#pragma once
#include "assets.hpp"

namespace sc::native_pak {
inline unsigned bounded(const J& j,unsigned max){
    auto n=j.num();if(n>max)fail("Native campaign integer out of range");
    return static_cast<unsigned>(n);
}
inline std::vector<std::string> strings(const J& j,std::size_t limit){
    const auto& a=j.arr();if(a.size()>limit)fail("Native campaign list too long");
    std::vector<std::string> result;result.reserve(a.size());
    for(const auto& item:a)result.push_back(item.str());
    return result;
}
struct ScriptRecord {
    std::string kind,name,keyword,source;
    unsigned line{},depth{};
    std::vector<std::string> args;
};
inline std::vector<ScriptRecord> decode_script(const J& j){
    schema(j,"spellcross-script");
    if(j.at("source_encoding").str()!="UTF-8")fail("Native script requires UTF-8 export");
    std::vector<ScriptRecord> out;
    unsigned line=0,depth=0;
    for(const auto& r:j.at("records").arr()){
        ScriptRecord v;
        v.kind=r.at("kind").str();v.line=bounded(r.at("line"),16000000);
        v.depth=bounded(r.at("depth"),10000);v.source=r.at("source").str();
        if(v.line<=line)fail("Script source line ordering invalid");
        line=v.line;
        if(v.kind=="end"){
            if(!depth||v.depth!=depth-1)fail("Script end depth mismatch");
            --depth;
        }else{
            if(v.depth!=depth)fail("Script nesting mismatch");
            if(v.kind=="call"||v.kind=="block"){
                v.name=r.at("name").str();v.keyword=r.at("keyword").str();
                v.args=strings(r.at("args"),65536);
                if(v.name.empty()||v.keyword.empty())fail("Empty script keyword");
                if(v.kind=="block")++depth;
            }else if(v.kind!="unknown")fail("Invalid script record kind");
        }
        out.push_back(std::move(v));
    }
    if(depth)fail("Unterminated native script block");
    return out;
}
struct MapCell {
    unsigned ground{},elevation{},object{},flags{}; // object 0 = empty
};
struct AnimationPlacement {
    unsigned resource{},frame_offset{};
    int frame_limit{},x{},y{},x_offset{},y_offset{};
};
struct AnimationLayer {
    std::vector<std::string> resources;
    std::vector<AnimationPlacement> placements;
};
struct SoundPlacement {unsigned tile{},resource{};};
struct SoundLayer {
    std::vector<std::string> resources;
    std::vector<SoundPlacement> placements;
};
struct TacticalMap {
    unsigned width{},height{};
    std::string terrain;
    std::vector<std::string> ground_sprites,object_sprites;
    std::vector<MapCell> cells;
    AnimationLayer anm,pnm;
    std::vector<unsigned> enemy_posts,player_posts;
    SoundLayer loop_sounds,random_sounds;
};
inline TacticalMap decode_map(const J& j,const Bytes& ground,const Bytes& objects){
    schema(j,"spellcross-tactical-map");
    TacticalMap m;
    m.width=bounded(j.at("width"),65535);m.height=bounded(j.at("height"),65535);
    const auto cells=std::size_t(m.width)*m.height;
    if(!cells||cells>1000000||ground.size()!=cells*2||objects.size()!=cells*2)
        fail("Invalid native tactical map geometry");
    m.terrain=j.at("terrain").str();
    const auto terrain=key(m.terrain);
    if(terrain!="T11"&&terrain!="PUST"&&terrain!="DEVAST")fail("Invalid native terrain");
    const auto& l1=j.at("ground");const auto& l2=j.at("objects");
    if(l1.at("entry").str()!="ground.u16le"||l1.at("index_mask").num()!=4095||
       l1.at("elevation_shift").num()!=12||l2.at("entry").str()!="objects.u8x2"||
       !l2.at("zero_means_empty").boolean()||l2.at("index_base").num()!=1)
        fail("Unsupported native map layer encoding");
    m.ground_sprites=strings(l1.at("sprites"),4095);
    m.object_sprites=strings(l2.at("sprites"),255);
    m.cells.reserve(cells);
    for(std::size_t i=0;i<cells;++i){
        unsigned word=unsigned(ground[i*2])|(unsigned(ground[i*2+1])<<8);
        MapCell c{word&4095u,word>>12,unsigned(objects[i*2]),unsigned(objects[i*2+1])};
        if(c.ground>=m.ground_sprites.size()||c.object>m.object_sprites.size())
            fail("Native map sprite index out of bounds");
        m.cells.push_back(c);
    }
    auto animation=[&](const char* name,bool pnm){
        AnimationLayer a;const auto& l=j.at(name);
        a.resources=strings(l.at("resources"),65535);
        if(l.at("placements").arr().size()>1000000)fail("Animation placement limit");
        for(const auto& row:l.at("placements").arr()){
            AnimationPlacement v;
            v.resource=bounded(row.at("resource"),65535);v.frame_offset=bounded(row.at("frame_offset"),255);
            v.frame_limit=signed_origin(row.at("frame_limit"));
            v.x=signed_origin(row.at("x"));v.y=signed_origin(row.at("y"));
            if(v.resource>=a.resources.size())fail("Native animation index out of bounds");
            if(pnm){v.x_offset=signed_origin(row.at("x_offset"));v.y_offset=signed_origin(row.at("y_offset"));}
            a.placements.push_back(v);
        }
        return a;
    };
    m.anm=animation("anm",false);m.pnm=animation("pnm",true);
    auto posts=[&](const char* name){
        std::vector<unsigned> a;
        if(j.at(name).arr().size()>1000000)fail("Counterattack post limit");
        for(const auto& row:j.at(name).arr())a.push_back(bounded(row,static_cast<unsigned>(cells-1)));
        return a;
    };
    m.enemy_posts=posts("enemy_counterattack_posts");m.player_posts=posts("player_counterattack_posts");
    auto sounds=[&](const char* name){
        SoundLayer a;const auto& l=j.at(name);
        a.resources=strings(l.at("resources"),256);
        if(l.at("placements").arr().size()>1000000)fail("Map sound placement limit");
        for(const auto& row:l.at("placements").arr()){
            SoundPlacement v{bounded(row.at("tile"),static_cast<unsigned>(cells-1)),bounded(row.at("resource"),255)};
            if(v.resource>=a.resources.size())fail("Native map sound index out of bounds");
            a.placements.push_back(v);
        }
        return a;
    };
    m.loop_sounds=sounds("loop_sounds");m.random_sounds=sounds("random_sounds");
    return m;
}
inline TacticalMap load_map(Provider& p,const std::string& id){
    auto j=parse_bytes(p.payload(id));schema(j,"spellcross-tactical-map");
    auto w=bounded(j.at("width"),65535),h=bounded(j.at("height"),65535);
    auto count=std::size_t(w)*h;
    if(!count||count>1000000)fail("Invalid native map dimensions");
    auto ground=p.map_plane(id,j.at("ground").at("entry").str(),count*2);
    auto objects=p.map_plane(id,j.at("objects").at("entry").str(),count*2);
    return decode_map(j,ground,objects);
}
}
