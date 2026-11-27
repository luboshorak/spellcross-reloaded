#pragma once
#include "json.hpp"
#include <algorithm>
#include <limits>
#include <cstring>

namespace sc::native_pak {
// Converter unit names are byte-preserving U+00xx, not UTF-8 localized text.
// Decode only those fields; never reconstruct the original binary record.
inline std::string unit_source_bytes(std::string_view value){
    std::string out;
    for(std::size_t i=0;i<value.size();++i){
        auto a=static_cast<unsigned char>(value[i]);
        if(a<128){if(!a)fail("Embedded NUL in native unit field");out+=char(a);continue;}
        if((a!=0xc2&&a!=0xc3)||i+1>=value.size())fail("Invalid byte-preserving native unit field");
        auto b=static_cast<unsigned char>(value[++i]);
        if((b&0xc0)!=0x80)fail("Invalid byte-preserving native unit field");
        out+=char(((a&3)<<6)|(b&63));
    }
    return out;
}
inline int unit_value(const J& j,const char* key,unsigned maximum=255){
    auto n=j.at(key).num();if(n>maximum)fail(std::string("Native unit integer out of range: ")+key);
    return static_cast<int>(n);
}
template<std::size_t N> inline void unit_name(char (&out)[N],const J& j,const char* key){
    auto s=unit_source_bytes(j.at(key).str());
    if(s.size()>=N)fail(std::string("Native unit name exceeds engine field: ")+key);
    std::fill(std::begin(out),std::end(out),0);
    std::copy(s.begin(),s.end(),out);
}
inline bool unit_edition(const J& j){
    schema(j,"spellcross-units");
    auto stride=j.at("record_size").num();
    if(stride!=206&&stride!=207)fail("Unsupported native unit record size");
    const auto& units=j.at("units").arr();
    if(units.empty()||units.size()>65535)fail("Invalid native unit count");
    return stride==207;
}
// Templated to allow exercising the exact mapping in independent CI without
// linking the legacy graphics/audio implementation.
template<class Unit> inline void assign_unit(Unit& unit,const J& row,unsigned expected_id,bool english){
    if(row.at("id").num()!=expected_id)fail("Native unit IDs must be contiguous");
    unit.type_id=static_cast<int>(expected_id);
    unit_name(unit.name,row,"name");
    unit_name(unit.info,row,"info");
    unit_name(unit.gra,row,"graphics");
    unit_name(unit.grb,row,"aux_graphics");
    unit_name(unit.icon,row,"icon");
    unit_name(unit.pnm_light_hit_name,row,"light_hit");
    unit_name(unit.anim_atack_light_name,row,"light_animation");
    unit_name(unit.pnm_light_shot_name,row,"light_shot");
    unit_name(unit.pnm_armored_hit_name,row,"armor_hit");
    unit_name(unit.anim_atack_armor_name,row,"armor_animation");
    unit_name(unit.pnm_armored_shot_name,row,"armor_shot");
    unit_name(unit.pnm_air_hit_name,row,"air_hit");
    unit_name(unit.anim_atack_air_name,row,"air_animation");
    unit_name(unit.pnm_air_shot_name,row,"air_shot");
    unit_name(unit.projetile_name,row,"projectile");
    unit_name(unit.action_fsu_name,row,"action_animation");
    unit_name(unit.die_anim_name,row,"die_animation");
    unit.utype=unit_value(row,"class_flags");
    unit.ap=unit_value(row,"ap");
    unit.apfw=unit_value(row,"forest_move_ap");
    unit.apw=unit_value(row,"move_ap");
    unit.cnt=unit_value(row,"count");
    unit.attack_light=unit_value(row,"attack_light");
    unit.attack_armored=unit_value(row,"attack_armored");
    unit.attack_air=unit_value(row,"attack_air");
    unit.attack_objects=unit_value(row,"attack_objects");
    unit.defence=unit_value(row,"defence");
    unit.res2=unit_value(row,"res2");
    unit.res3=unit_value(row,"initiative");
    unit.fire_range=unit_value(row,"fire_range");
    unit.fire_flags=unit_value(row,"fire_flags",english?65535u:255u);
    unit.aps=unit_value(row,"shot_ap");
    unit.anim_atack_light_frames=unit_value(row,"light_frames");
    unit.anim_atack_armor_frames=unit_value(row,"armor_frames");
    unit.anim_atack_air_frames=unit_value(row,"air_frames");
    unit.projectile_visible=unit_value(row,"projectile_visible");
    unit.action_id=unit_value(row,"action_id");
    unit.action_fsu_frames=unit_value(row,"action_frames");
    unit.action_ap=unit_value(row,"action_ap");
    unit.sdir=unit_value(row,"direct_sight");
    unit.vis=unit_value(row,"visible_count");
    unit.dig_turns=unit_value(row,"dig_turns");
    unit.die_anim_frames=unit_value(row,"die_frames");
    unit.die_action_id=unit_value(row,"die_action_id");
    unit.smov=unit_value(row,"sound_move");
    unit.slig=unit_value(row,"sound_light");
    unit.sarm=unit_value(row,"sound_armored");
    unit.sair=unit_value(row,"sound_air");
    unit.shit=unit_value(row,"sound_hit");
    unit.snd_action_id=unit_value(row,"sound_action");
    unit.ssel=unit_value(row,"sound_selection");
    unit.exp_min=unit_value(row,"exp_min",static_cast<unsigned>((std::numeric_limits<int>::max)()));
    unit.exp_max=unit_value(row,"exp_max",static_cast<unsigned>((std::numeric_limits<int>::max)()));
    for(auto key:{"action_params","die_action_params"}){
        const auto& a=row.at(key).arr();if(a.size()!=3)fail("Native unit action parameter count");
        auto* dest=std::string_view(key)=="action_params"?unit.action_params:unit.die_action_params;
        for(unsigned i=0;i<3;++i){auto n=a[i].num();if(n>255)fail("Native unit action parameter range");dest[i]=static_cast<int>(n);}
    }
    // Fields not assigned by the legacy binary decoder must not contain
    // uninitialized values in the native object.
    unit.res1=0;unit.sind=0;
    for(int e=0;e<12;++e)unit.exp_limits[e]=unit.CalcExperiencePts(e);
}
}
