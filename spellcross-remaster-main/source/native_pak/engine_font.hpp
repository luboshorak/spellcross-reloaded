#pragma once
// Engine-facing native font adapter. Construct the existing renderer's font
// object from modern glyph planes, without serializing an original FNT.
#include "assets.hpp"
#include "../spell_font.h"

namespace sc::native_pak {
inline std::unique_ptr<SpellFont> make_engine_font(Assets& assets,const std::string& id){
    auto f=assets.font(id);
    return std::make_unique<SpellFont>(std::move(f.glyphs),f.first_code,
        f.spacing,f.default_height,id);
}
}
