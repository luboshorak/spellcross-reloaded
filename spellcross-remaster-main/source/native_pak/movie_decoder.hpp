#pragma once
// In-process MKV (FFV1/FLAC) decoding. FFmpeg libraries are dynamically linked;
// the retail game video decoders do not use this code.
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/error.h>
#include <libavutil/mem.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

namespace sc::native_pak::media {
struct MovieSpec {
    int width, height, frames;
    int sample_rate=0, channels=0, samples=0; // zero for silent movies
};
struct DecodedMovie {
    std::vector<std::uint8_t> rgb;
    std::vector<std::int16_t> pcm;
};
inline void check(int rc,const char* operation) {
    if(rc>=0)return;
    char error[AV_ERROR_MAX_STRING_SIZE]{};
    av_strerror(rc,error,sizeof(error));
    throw std::runtime_error(std::string("Native MKV: ")+operation+": "+error);
}
inline void require(bool condition,const char* message) {
    if(!condition)throw std::runtime_error(std::string("Native MKV: ")+message);
}
struct MemoryInput {
    const std::vector<std::uint8_t>& bytes;
    std::size_t offset=0;
    static int read(void* opaque,std::uint8_t* dst,int size) {
        auto& self=*static_cast<MemoryInput*>(opaque);
        if(size<=0)return AVERROR(EINVAL);
        auto n=(std::min)(static_cast<std::size_t>(size),self.bytes.size()-self.offset);
        if(!n)return AVERROR_EOF;
        std::memcpy(dst,self.bytes.data()+self.offset,n);
        self.offset+=n;return static_cast<int>(n);
    }
    static std::int64_t seek(void* opaque,std::int64_t delta,int whence) {
        auto& self=*static_cast<MemoryInput*>(opaque);
        const auto size=static_cast<std::int64_t>(self.bytes.size());
        whence&=~AVSEEK_FORCE;
        if(whence==AVSEEK_SIZE)return size;
        std::int64_t base=0;
        if(whence==SEEK_CUR)base=static_cast<std::int64_t>(self.offset);
        else if(whence==SEEK_END)base=size;
        else if(whence!=SEEK_SET)return AVERROR(EINVAL);
        if(delta < -base || delta > size-base)return AVERROR(EINVAL);
        self.offset=static_cast<std::size_t>(base+delta);
        return static_cast<std::int64_t>(self.offset);
    }
};
struct Input {
    MemoryInput memory;
    AVIOContext* io=nullptr;
    AVFormatContext* format=nullptr;
    explicit Input(const std::vector<std::uint8_t>& bytes):memory{bytes}{}
    void open() {
        require(!memory.bytes.empty(),"empty payload");
        auto* buffer=static_cast<unsigned char*>(av_malloc(32768));
        if(!buffer)throw std::bad_alloc();
        io=avio_alloc_context(buffer,32768,0,&memory,MemoryInput::read,nullptr,MemoryInput::seek);
        if(!io){av_free(buffer);throw std::bad_alloc();}
        format=avformat_alloc_context();
        if(!format)throw std::bad_alloc();
        format->pb=io;
        format->flags|=AVFMT_FLAG_CUSTOM_IO;
        const auto* demuxer=av_find_input_format("matroska");
        require(demuxer!=nullptr,"Matroska demuxer not available");
        check(avformat_open_input(&format,nullptr,demuxer,nullptr),"open Matroska");
        check(avformat_find_stream_info(format,nullptr),"read stream information");
    }
    ~Input(){
        if(format)avformat_close_input(&format);
        if(io){av_freep(&io->buffer);avio_context_free(&io);}
    }
    Input(const Input&)=delete;
};
struct CodecDeleter { void operator()(AVCodecContext* p)const{avcodec_free_context(&p);} };
struct FrameDeleter { void operator()(AVFrame* p)const{av_frame_free(&p);} };
struct PacketDeleter { void operator()(AVPacket* p)const{av_packet_free(&p);} };
struct ScaleDeleter { void operator()(SwsContext* p)const{sws_freeContext(p);} };
struct ResampleDeleter { void operator()(SwrContext* p)const{swr_free(&p);} };
using Codec=std::unique_ptr<AVCodecContext,CodecDeleter>;
inline Codec decoder(AVStream* stream,AVCodecID expected) {
    require(stream->codecpar->codec_id==expected,"unexpected codec in movie");
    auto* codec=avcodec_find_decoder(expected);
    require(codec!=nullptr,"required decoder not available");
    Codec ctx(avcodec_alloc_context3(codec));
    if(!ctx)throw std::bad_alloc();
    check(avcodec_parameters_to_context(ctx.get(),stream->codecpar),"copy codec parameters");
    ctx->thread_count=1;
    check(avcodec_open2(ctx.get(),codec,nullptr),"open decoder");
    return ctx;
}
inline DecodedMovie decode(const std::vector<std::uint8_t>& bytes,const MovieSpec& spec) {
    require(spec.width>0&&spec.width<=4096&&spec.height>0&&spec.height<=4096
        &&spec.frames>0&&spec.frames<=1000000,"invalid video geometry");
    const auto pixels=static_cast<std::uint64_t>(spec.width)*spec.height*spec.frames;
    require(pixels<=512ull*1024*1024,"video exceeds decode limit");
    require((spec.channels==0&&spec.samples==0&&spec.sample_rate==0)||
        ((spec.channels==1||spec.channels==2)&&spec.sample_rate>=4000&&spec.sample_rate<=192000
        &&spec.samples>0&&static_cast<std::uint64_t>(spec.samples)*spec.channels<=128ull*1024*1024),
        "invalid audio geometry");
    Input input(bytes);input.open();
    int vi=-1,ai=-1;
    for(unsigned i=0;i<input.format->nb_streams;++i){
        auto t=input.format->streams[i]->codecpar->codec_type;
        if(t==AVMEDIA_TYPE_VIDEO&&vi<0)vi=static_cast<int>(i);
        if(t==AVMEDIA_TYPE_AUDIO&&ai<0)ai=static_cast<int>(i);
    }
    require(vi>=0,"missing video stream");
    require((ai>=0)==(spec.channels>0),"audio stream differs from catalog");
    auto video=decoder(input.format->streams[vi],AV_CODEC_ID_FFV1);
    Codec audio;
    if(ai>=0)audio=decoder(input.format->streams[ai],AV_CODEC_ID_FLAC);
    std::unique_ptr<AVFrame,FrameDeleter> frame(av_frame_alloc()),rgb(av_frame_alloc());
    std::unique_ptr<AVPacket,PacketDeleter> packet(av_packet_alloc());
    if(!frame||!rgb||!packet)throw std::bad_alloc();
    rgb->format=AV_PIX_FMT_RGB24;rgb->width=spec.width;rgb->height=spec.height;
    check(av_frame_get_buffer(rgb.get(),32),"allocate RGB frame");
    std::unique_ptr<SwsContext,ScaleDeleter> scale;
    std::unique_ptr<SwrContext,ResampleDeleter> resample;
    int input_audio_format=-1;
    DecodedMovie result;
    result.rgb.reserve(static_cast<std::size_t>(pixels)*3);
    result.pcm.reserve(static_cast<std::size_t>(spec.samples)*spec.channels);
    const auto row_bytes=static_cast<std::size_t>(spec.width)*3;
    int frames=0;
    auto video_frame=[&]{
        require(frame->width==spec.width&&frame->height==spec.height&&frames<spec.frames,
            "decoded video geometry/frame count differs from catalog");
        // sws_getCachedContext takes ownership of the old context on reconfiguration.
        scale.reset(sws_getCachedContext(scale.release(),spec.width,spec.height,
            static_cast<AVPixelFormat>(frame->format),spec.width,spec.height,
            AV_PIX_FMT_RGB24,SWS_BICUBIC,nullptr,nullptr,nullptr));
        require(scale!=nullptr,"RGB conversion unavailable");
        require(sws_scale(scale.get(),frame->data,frame->linesize,0,spec.height,
            rgb->data,rgb->linesize)==spec.height,"incomplete RGB conversion");
        for(int y=0;y<spec.height;++y){
            const auto* row=rgb->data[0]+static_cast<std::size_t>(y)*rgb->linesize[0];
            result.rgb.insert(result.rgb.end(),row,row+row_bytes);
        }
        ++frames;
    };
    auto append_audio=[&](const std::uint8_t** samples,int count){
        const int capacity=swr_get_out_samples(resample.get(),count);
        check(capacity,"calculate PCM buffer size");
        require(capacity<=1024*1024,"audio frame exceeds decode limit");
        if(capacity==0)return 0;
        std::vector<std::int16_t> temp(static_cast<std::size_t>(capacity)*spec.channels);
        std::uint8_t* dst[]={reinterpret_cast<std::uint8_t*>(temp.data())};
        const int n=swr_convert(resample.get(),dst,capacity,samples,count);
        check(n,"convert PCM");
        const auto used=static_cast<std::size_t>(n)*spec.channels;
        const auto expected=static_cast<std::size_t>(spec.samples)*spec.channels;
        require(used<=expected-result.pcm.size(),"decoded audio exceeds catalog sample count");
        result.pcm.insert(result.pcm.end(),temp.begin(),temp.begin()+used);
        return n;
    };
    auto audio_frame=[&]{
        require(frame->sample_rate==spec.sample_rate&&frame->ch_layout.nb_channels==spec.channels,
            "decoded audio geometry differs from catalog");
        if(!resample){
            AVChannelLayout out_layout{},fallback{};
            av_channel_layout_default(&out_layout,spec.channels);
            av_channel_layout_default(&fallback,spec.channels);
            auto* in_layout=frame->ch_layout.order==AV_CHANNEL_ORDER_UNSPEC?&fallback:&frame->ch_layout;
            SwrContext* raw=nullptr;
            int rc=swr_alloc_set_opts2(&raw,&out_layout,AV_SAMPLE_FMT_S16,spec.sample_rate,
                in_layout,static_cast<AVSampleFormat>(frame->format),frame->sample_rate,0,nullptr);
            av_channel_layout_uninit(&out_layout);av_channel_layout_uninit(&fallback);
            resample.reset(raw);check(rc,"configure PCM conversion");
            check(swr_init(resample.get()),"initialize PCM conversion");
            input_audio_format=frame->format;
        }
        require(frame->format==input_audio_format,"audio format changed mid-stream");
        const int planes=av_sample_fmt_is_planar(static_cast<AVSampleFormat>(frame->format))?spec.channels:1;
        const std::uint8_t* samples[2]{};
        for(int i=0;i<planes;++i)samples[i]=frame->extended_data[i];
        append_audio(samples,frame->nb_samples);
    };
    auto send=[&](AVCodecContext* ctx,AVPacket* p,auto&& consume){
        check(avcodec_send_packet(ctx,p),"send decoder packet");
        for(;;){
            int rc=avcodec_receive_frame(ctx,frame.get());
            if(rc==AVERROR(EAGAIN)||rc==AVERROR_EOF)break;
            check(rc,"receive decoded frame");
            consume();av_frame_unref(frame.get());
        }
    };
    for(;;){
        int rc=av_read_frame(input.format,packet.get());
        if(rc==AVERROR_EOF)break;
        check(rc,"read Matroska packet");
        if(packet->stream_index==vi)send(video.get(),packet.get(),video_frame);
        else if(audio&&packet->stream_index==ai)send(audio.get(),packet.get(),audio_frame);
        av_packet_unref(packet.get());
    }
    send(video.get(),nullptr,video_frame);
    if(audio){
        send(audio.get(),nullptr,audio_frame);
        if(resample)while(append_audio(nullptr,0)>0){}
    }
    require(frames==spec.frames,"decoded frame count differs from catalog");
    require(result.pcm.size()==static_cast<std::size_t>(spec.samples)*spec.channels,
        "decoded audio sample count differs from catalog");
    return result;
}
} // namespace sc::native_pak::media
