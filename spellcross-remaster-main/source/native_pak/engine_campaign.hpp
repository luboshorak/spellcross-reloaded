#pragma once
#include "campaign.hpp"
#include "../level.h"

namespace sc::native_pak {
// String conversion is supplied by the existing engine so CP895 is applied
// only at its boundary, not inside the PAK reader or the campaign semantics.
template<class Encode>
inline bool load_level(Provider& p,const std::string& id,LevelData& out,Encode encode){
    const auto& r=p.resource(id);
    auto script=decode_script(parse_bytes(p.payload(r.id)));
    std::vector<LevelSyntaxRecord> records;records.reserve(script.size());
    for(const auto& row:script){
        LevelSyntaxRecord v;
        v.kind=row.kind=="block"?LevelSyntaxRecord::Kind::Block:
            row.kind=="call"?LevelSyntaxRecord::Kind::Call:
            row.kind=="end"?LevelSyntaxRecord::Kind::End:LevelSyntaxRecord::Kind::Unknown;
        v.line=static_cast<int>(row.line);v.source=encode(row.source);
        v.name=row.name;v.args.reserve(row.args.size());
        for(const auto& arg:row.args)v.args.push_back(encode(arg));
        records.push_back(std::move(v));
    }
    LevelLoader loader;
    return loader.LoadLevelRecords(leaf(r.name),records,out);
}
}
