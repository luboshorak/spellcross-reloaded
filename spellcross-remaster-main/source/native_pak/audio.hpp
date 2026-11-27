#pragma once
#include "assets.hpp"
#include "units.hpp"

namespace sc::native_pak {
struct PcmData{
    unsigned rate{},channels{};
    std::vector<std::int16_t> samples;
};
inline PcmData decode_wave(const Bytes& b){
    auto tag=[&](std::size_t p,std::string_view s){
        return p<=b.size()&&s.size()<=b.size()-p&&std::equal(s.begin(),s.end(),b.begin()+p);
    };
    auto u16=[&](std::size_t p)->unsigned{
        if(p>b.size()||b.size()-p<2)fail("Truncated WAVE");
        return unsigned(b[p])|(unsigned(b[p+1])<<8);
    };
    auto u32=[&](std::size_t p)->std::uint32_t{
        return std::uint32_t(u16(p))|(std::uint32_t(u16(p+2))<<16);
    };
    if(b.size()<12||!tag(0,"RIFF")||!tag(8,"WAVE")||std::uint64_t(u32(4))+8!=b.size())
        fail("Invalid native WAVE header");
    PcmData out;bool fmt=false,data=false;std::size_t offset=0,length=0;
    for(std::size_t p=12;p<b.size();){
        if(b.size()-p<8)fail("Truncated WAVE chunk");
        auto n=u32(p+4);auto start=p+8;
        if(n>b.size()-start)fail("Invalid WAVE chunk length");
        if(tag(p,"fmt ")){
            if(fmt||n<16||u16(start)!=1||u16(start+14)!=16)fail("Expected native PCM16");
            out.channels=u16(start+2);out.rate=u32(start+4);
            if((out.channels!=1&&out.channels!=2)||out.rate<4000||out.rate>192000||
               u16(start+12)!=2*out.channels||u32(start+8)!=out.rate*2*out.channels)
                fail("Invalid PCM format");
            fmt=true;
        }else if(tag(p,"data")){
            if(data)fail("Duplicate WAVE data");
            offset=start;length=n;data=true;
        }
        p=start+n;
        if(n&1){if(p==b.size())fail("Missing WAVE chunk pad");++p;}
    }
    if(!fmt||!data||!length||length%(2*out.channels))fail("Invalid native PCM data");
    out.samples.reserve(length/2);
    for(std::size_t p=offset;p<offset+length;p+=2){
        auto v=u16(p);out.samples.push_back(static_cast<std::int16_t>(v>=32768?int(v)-65536:int(v)));
    }
    return out;
}
struct ClassRow {
    unsigned id{};std::string label;std::vector<std::string> values;
};
inline std::vector<ClassRow> decode_class_table(const J& j,const std::string& name){
    schema(j,"spellcross-class-table");
    if(key(j.at("table").str())!=key(name)||j.at("source_encoding").str()!="byte-preserving JSON escapes")
        fail("Native class table identity mismatch");
    std::vector<ClassRow> out;std::set<unsigned> ids;
    for(const auto& row:j.at("records").arr()){
        ClassRow v;
        auto id=row.at("id").num();if(id>65535||!ids.insert(static_cast<unsigned>(id)).second)fail("Invalid class ID");
        v.id=static_cast<unsigned>(id);v.label=unit_source_bytes(row.at("label").str());
        if(row.at("values").arr().size()>65535)fail("Class values exceed limit");
        for(const auto& value:row.at("values").arr())v.values.push_back(unit_source_bytes(value.str()));
        out.push_back(std::move(v));
    }
    return out;
}
}
