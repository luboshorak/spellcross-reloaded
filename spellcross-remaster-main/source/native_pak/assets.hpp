#pragma once
// Typed native resource readers. No original archives or original_hex decoding.
#include "pixels.hpp"

namespace sc::native_pak {
struct LanguageTable {
    std::string language;
    std::vector<std::string> entries; // UTF-8, indexed by retail table ID
};
inline LanguageTable decode_language_table(const J& j){
    schema(j,"spellcross-language-table");
    LanguageTable out;out.language=j.at("language").str();
    const auto& entries=j.at("entries").arr();
    if(entries.size()>65536)fail("Language table exceeds limit");
    out.entries.reserve(entries.size());
    for(const auto& row:entries){
        if(row.at("id").num()!=out.entries.size())fail("Language table IDs out of order");
        out.entries.push_back(row.at("text").str());
    }
    return out;
}
struct FontData {
    unsigned first_code{},spacing{},default_height{};
    std::vector<std::vector<std::vector<std::uint8_t>>> glyphs;
};
inline FontData decode_font_header(const J& j,const Resource& r){
    schema(j,"spellcross-bitmap-font");
    if(j.at("id").str()!=r.id||j.at("atlas").str()!=r.atlas_entry)
        fail("Font metadata owner mismatch");
    auto first=j.at("first_code").num(),spacing=j.at("spacing").num(),
         height=j.at("default_height").num(),count=j.at("glyph_count").num();
    if(first>255||!count||count>256-first||spacing>255||!height||height>255)
        fail("Invalid native font dimensions or code range");
    FontData out;out.first_code=static_cast<unsigned>(first);
    out.spacing=static_cast<unsigned>(spacing);out.default_height=static_cast<unsigned>(height);
    out.glyphs.resize(static_cast<std::size_t>(count));return out;
}
inline std::vector<std::vector<std::uint8_t>> font_rows(const IndexedFrame& frame){
    if(!frame.width||!frame.height||frame.width>255||frame.height>255||
       frame.indices.size()!=std::size_t(frame.width)*frame.height)
        fail("Invalid native glyph geometry");
    std::vector<std::vector<std::uint8_t>> rows(frame.height);
    for(unsigned y=0;y<frame.height;++y){
        auto start=frame.indices.begin()+std::size_t(y)*frame.width;
        rows[y].assign(start,start+frame.width);
        if(std::any_of(rows[y].begin(),rows[y].end(),[](auto n){return n>1;}))
            fail("Invalid native glyph foreground index");
    }
    return rows;
}
class Assets {
    Provider& provider_;
public:
    explicit Assets(Provider& provider):provider_(provider){}
    const Resource& resolve(const std::string& archive,const std::string& name)const{
        return provider_.scoped_resource(archive,name);
    }
    J document(const std::string& id,const std::string& expected_schema){
        auto result=parse_bytes(provider_.payload(id));
        schema(result,expected_schema);return result;
    }
    std::string text(const std::string& id){
        const auto& r=provider_.resource(id);
        if(!r.payload.ends_with("/text.txt"))fail("Not a native UTF-8 text: "+r.id);
        auto b=provider_.payload(r.id);return std::string(b.begin(),b.end());
    }
    LanguageTable language(const std::string& id){
        return decode_language_table(document(id,"spellcross-language-table"));
    }
    IndexedFrame graphic(const std::string& id,std::uint64_t frame=0){
        return decode_frame(provider_.resource_frame(id,frame));
    }
    FontData font(const std::string& id){
        const auto& r=provider_.resource(id);
        auto out=decode_font_header(document(r.id,"spellcross-bitmap-font"),r);
        for(std::size_t i=0;i<out.glyphs.size();++i)
            out.glyphs[i]=font_rows(graphic(r.id,i));
        return out;
    }
    Bytes wave(const std::string& id,std::size_t limit=metadata_limit){
        const auto& r=provider_.resource(id);
        if(!r.payload.ends_with("/audio.wav"))fail("Not a native WAVE resource: "+r.id);
        auto b=provider_.payload(r.id,limit);
        if(b.size()<12||!std::equal(b.begin(),b.begin()+4,"RIFF")||
           !std::equal(b.begin()+8,b.begin()+12,"WAVE"))fail("Invalid WAVE resource");
        return b;
    }
    Bytes midi(const std::string& id){
        const auto& r=provider_.resource(id);
        if(!r.payload.ends_with("/music.mid"))fail("Not a native MIDI resource: "+r.id);
        auto b=provider_.payload(r.id);
        if(b.size()<14||!std::equal(b.begin(),b.begin()+4,"MThd"))fail("Invalid MIDI resource");
        return b;
    }
};
}
