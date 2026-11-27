//=============================================================================
// Spellcross video data handling routines.
// 
// This code is part of Spellcross Map Editor project.
// (c) 2022, Stanislav Maslan, s.maslan@seznam.cz
// Distributed under MIT license, https://opensource.org/licenses/MIT.
//=============================================================================

#include "spell_video.h"
#include "native_pak/provider.hpp"
#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/utils.h>
#include <cmath>
#include <locale>
#include <sstream>
#include "other.h"
#include "LZ_spell.h"
#include "riff_loader.h"
#include "smacker_video_decoder.h"
#include "can_video_decoder.h"

#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cstdio>
#include <limits>
#include <stdexcept>

// ------------------------------------------------------------------------------------------------
// Video resources handling
// ------------------------------------------------------------------------------------------------

// search all available videos
SpellVideoResources::SpellVideoResources(std::vector<std::wstring> folders)
{	
	// make list of all possible source archives
	std::vector<std::filesystem::path> archives;
	for(auto &folder: folders)
	{		
		archives.push_back(std::filesystem::path(folder) / std::filesystem::path("SPEAKER.FS"));
		archives.push_back(std::filesystem::path(folder) / std::filesystem::path("MOVIE.FS"));
	}

	// for each candidate:
	for(auto &archive: archives)
	{
		if(!std::filesystem::exists(archive))
			continue;
				
		// load archive
		FSarchive* fs = NULL;
		try {
			fs = new FSarchive(archive.wstring(),FSarchive::Options::NO_LOAD);
		}
		catch(const runtime_error& error) {
			throw runtime_error(string_format("Loading archive file \"%ls\" failed (%s)!",archive.wstring().c_str(),error.what()));
		}

		std::vector<std::string> list;
		int fs_used = 0;

		// scan for CAN videos
		list = fs->GetFileNames("*.CAN");
		for(auto &vid: list)
			vid_list.emplace_back(vid, fs);
		fs_used += static_cast<int>(list.size());

		// scan for DPK videos
		list = fs->GetFileNames("*.DPK");
		for(auto& vid: list)
			vid_list.emplace_back(vid,fs);
		fs_used += static_cast<int>(list.size());

		// scan for DP2 videos
		list = fs->GetFileNames("*.DP2");
		for(auto& vid: list)
			vid_list.emplace_back(vid,fs);
		fs_used += static_cast<int>(list.size());

		// EN/PL/IT retail movies use Smacker v2 directly.
		list = fs->GetFileNames("*.SMK");
		for(auto& vid: list)
			vid_list.emplace_back(vid,fs);
		fs_used += static_cast<int>(list.size());

		// get rid of fs archive if nothing in there
		if(fs_used)
			fs_list.push_back(fs);
		else
			delete fs;
	}
}

// Index modern movie records only. The actual player must use their MKV
// payloads, not the legacy FS pointer (which is intentionally null).
SpellVideoResources::SpellVideoResources(sc::native_pak::Provider& provider)
{
    using namespace sc::native_pak;
    std::set<std::string> names;
    for(const auto& [id,r]:provider.resources()){
        if(r.status!="converted"||!r.payload.ends_with("/movie.json"))continue;
        auto name=leaf(r.name);
        if(!names.insert(name).second)fail("Ambiguous native movie name: "+name);
        vid_list.emplace_back(name,nullptr);
    }
}

// destructor
SpellVideoResources::~SpellVideoResources()
{
	vid_list.clear();
	for(auto fs: fs_list)
		delete fs;
}

// return list of wildcard filtered video names
std::vector<std::string> SpellVideoResources::GetNames(std::string wild)
{
	std::vector<std::string> list;
	for(auto &vid: vid_list)
		if(wildcmp(wild.c_str(), vid.name.c_str()))
			list.push_back(vid.name);
	return(list);
}


namespace {
struct NativeMediaTemp {
    std::filesystem::path path;
    explicit NativeMediaTemp(const wxString& prefix){
        auto name=wxFileName::CreateTempFileName(prefix);
        if(name.empty())sc::native_pak::fail("Cannot create native video temporary file");
        path=std::filesystem::path(name.ToStdWstring());
    }
    ~NativeMediaTemp(){if(!path.empty()){std::error_code ec;std::filesystem::remove(path,ec);}}
};
void RunNativeFfmpeg(const std::vector<std::wstring>& args){
    namespace fs=std::filesystem;
    auto dir=fs::path(wxStandardPaths::Get().GetExecutablePath().ToStdWstring()).parent_path();
#ifdef _WIN32
    const auto exe=dir/L"ffmpeg.exe";
    std::wstring program=fs::is_regular_file(exe)?exe.wstring():L"ffmpeg.exe";
#else
    const auto exe=dir/L"ffmpeg";
    std::wstring program=fs::is_regular_file(exe)?exe.wstring():L"ffmpeg";
#endif
    std::vector<const wchar_t*> argv;argv.reserve(args.size()+2);
    argv.push_back(program.c_str());
    for(const auto& arg:args)argv.push_back(arg.c_str());
    argv.push_back(nullptr);
    wxArrayString output,errors;
    auto rc=wxExecute(argv.data(),output,errors,wxEXEC_SYNC);
    if(rc!=0){
        std::string msg="Native MKV decoding failed. Install FFmpeg or put it beside Spellcross.";
        if(!errors.empty())msg+=" "+errors.Last().ToStdString();
        sc::native_pak::fail(msg);
    }
}
}
SpellVideo::SpellVideo(sc::native_pak::Provider& provider,const std::string& requested)
    : x_size(0),y_size(0),fps(0),type(SpellVideoType::NativeRGB)
{
    using namespace sc::native_pak;
    namespace fs=std::filesystem;
    const auto name=leaf(requested);
    const auto* r=provider.optional_leaf(name);
    if(!r&&key(fs::path(name).extension().string())==".SMK"){
        auto stem=fs::path(name).stem().string();
        r=provider.optional_leaf(stem+".CAN");
        if(!r)r=provider.optional_leaf(stem+".DPK");
    }
    if(!r)fail("Native movie not found: "+requested);
    auto j=parse_bytes(provider.payload(r->id));schema(j,"spellcross-movie");
    auto w=j.at("width").num(),h=j.at("height").num(),count=j.at("frames").num();
    if(!w||!h||w>4096||h>4096||!count||count>1000000||w*h*count>512u*1024u*1024u)
        fail("Invalid native movie dimensions");
    auto rate=std::get_if<J::N>(&j.at("fps").v);
    if(!rate)fail("Invalid native movie frame rate");
    std::istringstream rate_stream(rate->value);rate_stream.imbue(std::locale::classic());
    if(!(rate_stream>>fps)||!rate_stream.eof()||!std::isfinite(fps)||fps<=0||fps>240)
        fail("Invalid native movie frame rate");
    x_size=static_cast<int>(w);y_size=static_cast<int>(h);native_frame_count=static_cast<int>(count);
    NativeMediaTemp input("spellcross-movie-"),raw("spellcross-rgb-"),pcm("spellcross-pcm-");
    {
        auto movie=provider.movie(r->id);
        std::ofstream out(input.path,std::ios::binary|std::ios::trunc);
        if(!out.write(reinterpret_cast<const char*>(movie.data()),movie.size()))fail("Cannot stage native MKV");
        out.close();if(!out)fail("Cannot close native MKV");
    }
    RunNativeFfmpeg({L"-nostdin",L"-hide_banner",L"-loglevel",L"error",L"-y",L"-i",input.path.wstring(),
        L"-map",L"0:v:0",L"-vsync",L"0",L"-frames:v",std::to_wstring(count),L"-f",L"rawvideo",L"-pix_fmt",L"rgb24",raw.path.wstring()});
    const auto frame_bytes=w*h*3;
    if(fs::file_size(raw.path)!=frame_bytes*count)fail("Native RGB frame count mismatch");
    if(!std::holds_alternative<std::nullptr_t>(j.at("audio_codec").v)){
        if(j.at("audio_codec").str()!="FLAC")fail("Unsupported native movie audio");
        auto sr=j.at("sample_rate").num(),ch=j.at("channels").num(),n=j.at("sample_frames").num();
        if(sr<4000||sr>192000||(ch!=1&&ch!=2)||!n||n>128u*1024u*1024u/ch)
            fail("Invalid native movie audio geometry");
        RunNativeFfmpeg({L"-nostdin",L"-hide_banner",L"-loglevel",L"error",L"-y",L"-i",input.path.wstring(),
            L"-map",L"0:a:0",L"-f",L"s16le",L"-acodec",L"pcm_s16le",pcm.path.wstring()});
        if(fs::file_size(pcm.path)!=n*ch*2)fail("Native audio length mismatch");
        std::ifstream in(pcm.path,std::ios::binary);
        audio.data.resize(n*ch);
        if(!in.read(reinterpret_cast<char*>(audio.data.data()),audio.data.size()*2))fail("Native movie PCM read failed");
        audio.fs=static_cast<int>(sr);audio.channels=static_cast<int>(ch);audio.samples=static_cast<int>(n);
    }
    native_rgb_frame.resize(frame_bytes);
    native_rgb_stream.open(raw.path,std::ios::binary);
    if(!native_rgb_stream)fail("Cannot read decoded native video frames");
    native_rgb_path=std::move(raw.path);raw.path.clear();
}
SpellVideo::~SpellVideo(){
    if(native_rgb_stream.is_open())native_rgb_stream.close();
    if(!native_rgb_path.empty()){std::error_code ec;std::filesystem::remove(native_rgb_path,ec);}
}
double SpellVideo::GetNativeDuration()const{
    const double video=native_frame_count/fps;
    const double sound=audio.fs>0?double(audio.samples)/audio.fs:0.0;
    return (std::max)(video,sound);
}

// ------------------------------------------------------------------------------------------------
// Video file handling
// ------------------------------------------------------------------------------------------------

// load video file from path
SpellVideo::SpellVideo(std::wstring path)
{
	// try open file
	ifstream fr(path.c_str(),ios::in | ios::binary | ios::ate);
	if(!fr.is_open())
		throw runtime_error("Cannot open video file!");

	// Video decoders accept signed int lengths; reject unrepresentable input.
	const std::streamoff flen = fr.tellg();
	if(flen <= 0 || flen > std::numeric_limits<int>::max())
		throw runtime_error("Video file size is invalid or exceeds int range!");
	fr.seekg(0);
	vector<uint8_t> data(static_cast<size_t>(flen));
	if(!fr.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(flen)))
		throw runtime_error("Reading video file failed!");
	fr.close();
	const int video_size = static_cast<int>(flen);

	std::string ext = std::filesystem::path(path).extension().string();
	std::transform(ext.begin(), ext.end(), ext.begin(), ::toupper);

	if(ext == ".CAN" && DecodeCAN(data.data(), video_size))
	{
		throw runtime_error("Decoding CAN video file failed!");
	}	
	else if(ext == ".DPK" && DecodeDPK(data.data(),video_size))
	{
		throw runtime_error("Decoding DPK video file failed!");
	}
	else if(ext == ".DP2")
	{		
		// try open complement wave file
		std::wstring wave_path = std::filesystem::path(path).replace_extension("").wstring();
		ifstream frw(wave_path.c_str(),ios::in | ios::binary | ios::ate);
		if(!frw.is_open())
			throw runtime_error("Cannot open complement wave file!");

		// Complementary WAVE data uses the same signed int length limit.
		const std::streamoff wave_flen = frw.tellg();
		if(wave_flen <= 0 || wave_flen > std::numeric_limits<int>::max())
			throw runtime_error("Complement wave file size is invalid or exceeds int range!");
		frw.seekg(0);
		vector<uint8_t> wave_data(static_cast<size_t>(wave_flen));
		if(!frw.read(reinterpret_cast<char*>(wave_data.data()), static_cast<std::streamsize>(wave_flen)))
			throw runtime_error("Reading complement wave file failed!");
		frw.close();
		const int wave_size = static_cast<int>(wave_flen);

		// try decode
		if(DecodeDP2(data.data(), video_size, wave_data.data(), wave_size))
		{
			throw runtime_error("Decoding DP2 video file failed!");
		}
	}
	else if(ext == ".SMK" && DecodeSMK(data.data(), video_size))
	{
		throw runtime_error("Decoding SMK video file failed!");
	}
}

// load video file from FS archive
SpellVideo::SpellVideo(FSarchive* fs,std::string name)
{
	if(!fs)
		throw runtime_error("Loading video file: source FS archive not provided!");

	// try load video file
	uint8_t *v_data;
	int v_size;
	if(fs->GetFile(name, &v_data, &v_size))
		throw runtime_error(string_format("Loading video file: video file \"%s\" not found in archive!",name));
	
	std::string ext = std::filesystem::path(name).extension().string();
	std::transform(ext.begin(), ext.end(), ext.begin(), ::toupper);

	if(ext == ".CAN" && DecodeCAN(v_data, v_size))
	{
		throw runtime_error("Decoding CAN video file failed!");
	}
	else if(ext == ".DPK" && DecodeDPK(v_data,v_size))
	{
		throw runtime_error("Decoding DPK video file failed!");
	}
	else if(ext == ".DP2")
	{
		// try load complement audio file if needed
		uint8_t* a_data = NULL;
		int a_size = 0;
		if(ext == ".DP2")
		{
			std::string audio_name = std::filesystem::path(name).replace_extension("").string();
			if(fs->GetFile(audio_name,&a_data,&a_size))
				throw runtime_error(string_format("Loading video file: complement audio file \"%s\" not found in archive!",audio_name));
		}

		// try decode
		if(DecodeDP2(v_data,v_size, a_data,a_size))
		{
			throw runtime_error("Decoding DP2 video file failed!");
		}
	}
	else if(ext == ".SMK" && DecodeSMK(v_data, v_size))
	{
		throw runtime_error("Decoding SMK video file failed!");
	}
}

int SpellVideo::DecodeSMK(uint8_t* data, int len)
{
	SmackerDecodedVideo decoded;
	std::string error;
	if(len <= 0 || !DecodeSmackerVideo(data, static_cast<size_t>(len), decoded, error))
		return(1);

	type = SpellVideoType::SMK;
	x_size = decoded.width;
	y_size = decoded.height;
	fps = decoded.fps;
	frames = std::move(decoded.frames);
	frame_palettes = std::move(decoded.palettes);
	audio.channels = decoded.audio_channels;
	audio.fs = decoded.audio_rate;
	audio.data = std::move(decoded.audio_samples);
	audio.samples = audio.channels > 0
		? static_cast<int>(audio.data.size() / static_cast<size_t>(audio.channels)) : 0;
	std::snprintf(audio.name, sizeof(audio.name), "%s", "stream");
	return(frames.empty() || frame_palettes.size() != frames.size() || audio.samples <= 0);
}

int SpellVideo::DecodeCAN(uint8_t* data,int len)
{
    type = SpellVideoType::CAN;
    spellcross_can::Decoded decoded;
    if (len <= 0 || !spellcross_can::Decode(data, static_cast<size_t>(len), decoded))
        return 1;
    x_size = 320;
    y_size = 200;
    fps = 15.0;
    std::copy(decoded.palette.begin(), decoded.palette.end(), &pal[0][0]);
    frames = std::move(decoded.frames);
    audio.data = std::move(decoded.audio);
    audio.samples = static_cast<int>(audio.data.size());
    audio.fs = 11025;
    audio.channels = 1;
    strcpy_s(audio.name, sizeof(audio.name), "stream");
    return 0;
}

int SpellVideo::DecodeDPK(uint8_t* data,int len)
{
	type = SpellVideoType::DPK;

	uint8_t* p = data;
	uint8_t* pend = &data[len];

	// frame rate (estimate from smallest audio chunk)
	fps = 15.0;

	// read frames count
	if(&p[4] > pend)
	{
		// fail
		return(1);
	}
	int frames_count = *(uint32_t*)p; p += sizeof(uint32_t);

	// read palette data
	if(&p[256*3] > pend)
	{
		// fail
		return(1);
	}
	std::memcpy(&pal[0][0], p, 256*3);
	p += 256*3;
	
	// read frame resolution
	if(&p[8] > pend)
	{
		// fail
		return(1);
	}
	x_size = *(uint32_t*)p; p += sizeof(uint32_t);
	y_size = *(uint32_t*)p; p += sizeof(uint32_t);

	// make temp frame buffer holding current frame data
	vector<uint8_t> frame(x_size*y_size, 0x00);

	// initialize LZW decoder
	LZWexpand delz(1000000);

	// first audio block
	int audio_frame_size = 2*735;
	int audio_frames_count = 23;
	
	// init audio stream data
	audio.samples = 0;
	audio.fs = 11025;
	audio.channels = 1;
	strcpy_s(audio.name, sizeof(audio.name), "stream");
	
	
	// read frames
	while(p < pend)
	{
		// read audio samples till end of block detected
		
		int audio_block_size = audio_frame_size*audio_frames_count;
		if(&p[audio_block_size] > pend)
		{
			// fail
			return(1);
		}		
		
		// copy audio block
		uint16_t *smpl = (uint16_t*)p;		
		audio.data.resize(audio.samples + audio_block_size/2);
		for(int k = 0; k < audio_block_size/2; k++)
			audio.data[audio.samples++] = (int16_t)(*smpl++ - (uint16_t)0x8000u);
		p += audio_block_size;
		
		// next audio block will be one frame only
		audio_frames_count = 1;
					
		// get video frame data size		
		if(&p[4] > pend)
		{
			// failed
			return(1);
		}
		int video_chunk_size = *(uint32_t*)p; p += sizeof(uint32_t);
		if(&p[video_chunk_size] > pend)
		{
			// fail
			return(1);
		}
		if(!video_chunk_size)
		{
			// duplicate last frame
			frames.emplace_back(frame);
			continue;
		}

		// try deLZ
		uint8_t *deltas = NULL;
		int deltas_size;
		delz.Decode(p, &p[video_chunk_size], &deltas, &deltas_size);
		if(!deltas || deltas_size < 20)
		{
			// failed
			return(1);
		}
		p += video_chunk_size;
		uint8_t *dptr = deltas;
		uint8_t* dend = &deltas[deltas_size];
		
		// modified area range
		int min_x = *(uint16_t*)dptr; dptr += sizeof(uint16_t);
		int max_x = *(uint16_t*)dptr; dptr += sizeof(uint16_t);
		int min_y = *(uint16_t*)dptr; dptr += sizeof(uint16_t);
		int max_y = *(uint16_t*)dptr; dptr += sizeof(uint16_t);

		// frame data pointer
		uint8_t *fptr = frame.data();
		uint8_t* fend = &fptr[frame.size()];

		while(true)
		{
			// get pixels gap
			if(&dptr[4] > dend)
			{
				// failed
				delete deltas;
				return(1);
			}
			uint32_t gap_size = *(uint32_t*)dptr; dptr += sizeof(uint32_t);			
			// end mark?
			if(gap_size == 0xFFFFFFFFu)
				break;
			
			// get chunk size
			if(&dptr[4] > dend)
			{
				// failed
				delete deltas;
				return(1);
			}
			int chunk_size = *(uint32_t*)dptr; dptr += sizeof(uint32_t);
			if(!chunk_size)
				continue;

			// copy data chunk to current frame
			fptr += gap_size;
			if(&fptr[chunk_size] > fend || &dptr[chunk_size] > dend)
			{
				// failed
				delete deltas;
				return(1);
			}
			std::memcpy(fptr, dptr, chunk_size);
			dptr += chunk_size;
			fptr += chunk_size;			
		}

		// store to frame buffer
		frames.emplace_back(frame);
		
		delete deltas;
	}

	return(0);
}

int SpellVideo::DecodeDP2(uint8_t* dp2,int dp2_len, uint8_t *riff, int riff_len)
{
	type = SpellVideoType::DP2;

	uint8_t* p = dp2;
	uint8_t* pend = &dp2[dp2_len];

	// frame rate (estimate from smallest audio chunk)
	fps = 15.0;

	// read frames count
	if(&p[4] > pend)
	{
		// fail
		return(1);
	}
	int frames_count = *(uint32_t*)p; p += sizeof(uint32_t);

	// read palette data
	if(&p[256*3] > pend)
	{
		// fail
		return(1);
	}
	std::memcpy(&pal[0][0],p,256*3);
	p += 256*3;

	// read frame resolution
	if(&p[8] > pend)
	{
		// fail
		return(1);
	}
	x_size = *(uint32_t*)p; p += sizeof(uint32_t);
	y_size = *(uint32_t*)p; p += sizeof(uint32_t);

	// make temp frame buffer holding current frame data
	vector<uint8_t> frame(x_size*y_size,0x00);

	// initialize LZW decoder
	LZWexpand delz(1000000);

	// read frames
	while(p < pend)
	{
		// read audio samples till end of block detected

		// get video frame data size		
		if(&p[4] > pend)
		{
			// failed
			return(1);
		}
		int video_chunk_size = *(uint32_t*)p; p += sizeof(uint32_t);
		if(&p[video_chunk_size] > pend)
		{
			// fail
			return(1);
		}
		if(!video_chunk_size)
		{
			// duplicate last frame
			frames.emplace_back(frame);
			continue;
		}

		// try deLZ
		auto &deltas = delz.Decode(p,&p[video_chunk_size]);
		if(deltas.empty() || deltas.size() < 20)
		{
			// failed
			return(1);
		}
		p += video_chunk_size;
		uint8_t* dptr = deltas.data();
		uint8_t* dend = &dptr[deltas.size()];

		// modified area range
		int min_x = *(uint16_t*)dptr; dptr += sizeof(uint16_t);
		int max_x = *(uint16_t*)dptr; dptr += sizeof(uint16_t);
		int min_y = *(uint16_t*)dptr; dptr += sizeof(uint16_t);
		int max_y = *(uint16_t*)dptr; dptr += sizeof(uint16_t);

		// frame data pointer
		uint8_t* fptr = frame.data();
		uint8_t* fend = &fptr[frame.size()];

		while(true)
		{
			// get pixels gap
			if(&dptr[4] > dend)
			{
				// failed
				return(1);
			}
			uint32_t gap_size = *(uint32_t*)dptr; dptr += sizeof(uint32_t);
			// end mark?
			if(gap_size == 0xFFFFFFFFu)
				break;

			// get chunk size
			if(&dptr[4] > dend)
			{
				// failed
				return(1);
			}
			int chunk_size = *(uint32_t*)dptr; dptr += sizeof(uint32_t);
			if(!chunk_size)
				continue;

			// copy data chunk to current frame
			fptr += gap_size;
			if(&fptr[chunk_size] > fend || &dptr[chunk_size] > dend)
			{
				// failed
				return(1);
			}
			std::memcpy(fptr,dptr,chunk_size);
			dptr += chunk_size;
			fptr += chunk_size;
		}

		// store to frame buffer
		frames.emplace_back(frame);
	}

	// load complementary audio data			
	try {
		// try parse RIFF WAVE
		RIFF riff_cont(riff,riff_len);
		
		// store PCM data
		strcpy_s(audio.name, sizeof(audio.name), "stream");
		audio.channels = riff_cont.channels;
		audio.fs = riff_cont.fs;
		audio.samples = riff_cont.samples;
		riff_cont.ConvertPCM(audio.data);
	}
	catch(const runtime_error&) {
		return(1);
	}

	return(0);
}

std::tuple<int,int> SpellVideo::GetResolution()
{
	return std::tuple(x_size,y_size);
}

int SpellVideo::GetFramesCount()
{
    if(IsNativeRGB())return native_frame_count;
	if(isDP2())
		return((int)(fps*audio.samples/audio.fs));
	if(frames.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
		throw std::length_error("Video frame count exceeds int range");
	return(static_cast<int>(frames.size()));
}

uint8_t* SpellVideo::GetFrame(int id)
{
    if(IsNativeRGB()){
        if(id<0||id>=native_frame_count)return nullptr;
        native_rgb_stream.clear();native_rgb_stream.seekg(std::streamoff(id)*native_rgb_frame.size());
        if(!native_rgb_stream.read(reinterpret_cast<char*>(native_rgb_frame.data()),native_rgb_frame.size()))return nullptr;
        return native_rgb_frame.data();
    }
	if(id < 0 || frames.empty())
		return(NULL);
	const size_t frame_index = isDP2()
		? static_cast<size_t>(id) % frames.size() // loop frames for DP2 format
		: static_cast<size_t>(id);
	if(frame_index >= frames.size())
		return(NULL);
	return(frames[frame_index].data());
}

// get frame matching timestamp
int SpellVideo::GetFrameID(double time)
{
    if(IsNativeRGB())return std::clamp(static_cast<int>(time*fps),0,native_frame_count-1);
	if(frames.empty())
		return(-1);
	int id;
	if(isDP2())
		id = (std::max)((int)(time*fps),0);
	else
		id = (std::min)((std::max)((int)(time*fps), 0), (int)frames.size()-1);	
	return(id);
}

uint8_t* SpellVideo::GetPalette(int frame_id)
{
	if(type == SpellVideoType::SMK && !frame_palettes.empty())
	{
		const size_t index = frame_id < 0 ? 0 :
			(std::min)(static_cast<size_t>(frame_id), frame_palettes.size() - 1);
		return(frame_palettes[index].data());
	}
	return(&pal[0][0]);
}

SpellSample* SpellVideo::GetSound()
{
	return(&audio);
}

int SpellVideo::SaveFrames(std::wstring folder_path)
{
    if(IsNativeRGB())return 1; // legacy indexed frame export is not an RGB export
	for(int fid = 0; fid < frames.size(); fid++)
	{
		auto frame = &frames[fid];

		// frame path
		wstring path = std::filesystem::path(folder_path) / std::filesystem::path(string_format("frame_%04d.raw",fid));

		// store frame file
		ofstream fw(path,ios::out | ios::binary);
		if(!fw.is_open())
			return(1);
		fw.write((char*)frame->data(),frame->size());
		fw.close();		
	}

	// store frame file
	wstring path = std::filesystem::path(folder_path) / std::filesystem::path("audio.raw");
	ofstream fw(path,ios::out | ios::binary);
	if(!fw.is_open())
		return(1);
	fw.write((char*)audio.data.data(),audio.samples*sizeof(int16_t));
	fw.close();

	return(0);
}
