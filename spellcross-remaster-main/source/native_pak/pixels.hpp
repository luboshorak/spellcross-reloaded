#pragma once
#include "provider.hpp"
#include <optional>
#include <wx/image.h>
#include <wx/mstream.h>

namespace sc::native_pak {
struct IndexedFrame {
    unsigned width{},height{};
    int origin_x{},origin_y{};
    Bytes indices,opacity,rgb,known;
    std::optional<unsigned> shadow_index,opaque_zero_display_index;
};
inline int signed_origin(const J& j){
    auto n=std::get_if<J::N>(&j.v);
    if(!n)fail("Invalid frame origin");
    int value{};const auto& s=n->value;
    auto r=std::from_chars(s.data(),s.data()+s.size(),value);
    if(r.ec!=std::errc{}||r.ptr!=s.data()+s.size())fail("Invalid frame origin");
    return value;
}
inline std::optional<unsigned> optional_index(const J& frame,const char* name){
    auto p=frame.get(name);
    if(!p||std::holds_alternative<std::nullptr_t>(p->v))return std::nullopt;
    auto value=p->num();if(value>255)fail("Invalid frame palette index");
    return static_cast<unsigned>(value);
}
inline wxImage decode_plane(const Bytes& data){
    wxMemoryInputStream stream(data.data(),data.size());
    wxImage image;
    if(!image.LoadFile(stream,wxBITMAP_TYPE_PNG)||image.HasAlpha()||image.HasMask()||
       image.GetWidth()<=0||image.GetHeight()<=0||image.GetWidth()>2048||image.GetHeight()>2048)
        fail("Invalid atlas PNG data plane");
    auto rgb=image.GetData();
    for(std::size_t i=0;i<std::size_t(image.GetWidth())*image.GetHeight();++i)
        if(rgb[3*i]!=rgb[3*i+1]||rgb[3*i]!=rgb[3*i+2])fail("Atlas plane must contain grayscale data values");
    return image;
}
// The caller must register the wx PNG handler. No color interpolation,
// palette remapping or shadow effect is applied to stored data planes.
inline IndexedFrame decode_frame(const FrameData& data){
    const auto& rect=data.metadata.at("rect").arr();
    const auto& origin=data.metadata.at("origin").arr();
    if(rect.size()!=4||origin.size()!=2||data.rgb.size()!=768||data.known.size()!=256)
        fail("Invalid frame metadata");
    auto indices=decode_plane(data.indices_png),opacity=decode_plane(data.opacity_png);
    auto x=rect[0].num(),y=rect[1].num(),w=rect[2].num(),h=rect[3].num();
    if(indices.GetSize()!=opacity.GetSize()||!w||!h||x>unsigned(indices.GetWidth())||y>unsigned(indices.GetHeight())||
       w>unsigned(indices.GetWidth())-x||h>unsigned(indices.GetHeight())-y)fail("Frame outside atlas");
    IndexedFrame out;
    out.width=static_cast<unsigned>(w);out.height=static_cast<unsigned>(h);
    out.origin_x=signed_origin(origin[0]);out.origin_y=signed_origin(origin[1]);
    out.shadow_index=optional_index(data.metadata,"shadow_index");
    out.opaque_zero_display_index=optional_index(data.metadata,"opaque_zero_display_index");
    out.rgb=data.rgb;out.known=data.known;
    out.indices.reserve(w*h);out.opacity.reserve(w*h);
    auto a=indices.GetData(),b=opacity.GetData();
    for(std::uint64_t row=0;row<h;++row)for(std::uint64_t col=0;col<w;++col){
        auto pos=3*((y+row)*indices.GetWidth()+x+col);
        out.indices.push_back(a[pos]);out.opacity.push_back(b[pos]);
    }
    return out;
}
// Diagnostic output only. Preserve the mask and apply the documented opaque
// zero display remap. A shadow index stays an index; engine compositing is
// deliberately not implemented here. Unknown opaque palette values fail.
inline wxImage preview_rgba(const IndexedFrame& frame){
    const auto count=std::size_t(frame.width)*frame.height;
    if(!count||frame.width>2048||frame.height>2048||frame.indices.size()!=count||
       frame.opacity.size()!=count||frame.rgb.size()!=768||frame.known.size()!=256)
        fail("Invalid indexed frame");
    wxImage image(static_cast<int>(frame.width),static_cast<int>(frame.height));
    image.InitAlpha();
    auto rgb=image.GetData(),alpha=image.GetAlpha();
    for(std::size_t i=0;i<count;++i){
        unsigned index=frame.indices[i];
        if(!index&&frame.opacity[i]&&frame.opaque_zero_display_index)index=*frame.opaque_zero_display_index;
        if(index>255||(frame.opacity[i]&&!frame.known[index]))fail("Opaque pixel has unknown palette entry");
        for(unsigned c=0;c<3;++c)rgb[3*i+c]=frame.rgb[index*3+c];
        alpha[i]=frame.opacity[i];
    }
    return image;
}
}
