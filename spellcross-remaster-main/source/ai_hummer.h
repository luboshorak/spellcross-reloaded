#pragma once
// Hummer-only neural sprite cache. No model or retail game assets are bundled.
// All source/cached data are local to the player's installation.
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <memory>
#include <utility>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <cstdlib>

namespace spellcross_ai
{
    namespace fs=std::filesystem;
    constexpr int scale=4;
    constexpr int maxSide=256;
    using RGB=std::array<std::uint8_t,3>;
    inline std::uint64_t Hash(const std::uint8_t* p,std::size_t n,
                              std::uint64_t h=14695981039346656037ull)
    {
        for(std::size_t i=0;i<n;++i) { h^=p[i]; h*=1099511628211ull; }
        return h;
    }
    inline fs::path CacheRoot()
    {
        if(const auto* env=std::getenv("SPELLCROSS_AI_CACHE"))
            if(*env) return fs::u8path(env);
        return fs::path("ai_hummer_cache");
    }
    inline void Put32(std::ostream& out,std::uint32_t n)
    {
        for(int i=0;i<4;++i) out.put(static_cast<char>((n>>(i*8))&255u));
    }
    inline bool Read32(std::istream& in,std::uint32_t& n)
    {
        n=0;
        for(int i=0;i<4;++i) { int c=in.get(); if(c<0) return false; n|=static_cast<std::uint32_t>(c)<<(i*8); }
        return true;
    }
    struct Source
    {
        int w=0,h=0;
        std::vector<std::uint8_t> indices;
    };
    template<class Fn>
    bool Rows(const std::uint8_t* bytes,std::size_t size,int rows,Fn fn)
    {
        if(!bytes||rows<=0||rows>maxSide) return false;
        std::size_t pos=0;
        for(int y=0;y<rows;++y)
        {
            if(size-pos<2*sizeof(int)) return false;
            int x,count;
            std::memcpy(&x,bytes+pos,sizeof(int)); pos+=sizeof(int);
            std::memcpy(&count,bytes+pos,sizeof(int)); pos+=sizeof(int);
            if(count<0||count>maxSide||static_cast<std::size_t>(count)>size-pos||
               x < -4096 || x > 4096) return false;
            fn(y,x,bytes+pos,count);
            pos+=static_cast<std::size_t>(count);
        }
        return true;
    }
    inline Source Decode(const std::uint8_t* bytes,std::size_t size,int rows,int xmin,int xmax)
    {
        Source src;
        if(xmin < -4096 || xmax > 4096 || xmax<=xmin ||
           xmax-xmin>maxSide || rows<=0 || rows>maxSide) return src;
        src.w=xmax-xmin; src.h=rows;
        src.indices.assign(static_cast<std::size_t>(src.w)*src.h,0);
        const bool ok=Rows(bytes,size,rows,[&](int y,int x,const std::uint8_t* p,int count){
            for(int i=0;i<count;++i)
            {
                const int lx=x+i-xmin;
                if(lx>=0&&lx<src.w) src.indices[static_cast<std::size_t>(y)*src.w+lx]=p[i];
            }
        });
        if(!ok) return {};
        return src;
    }
    inline bool Opaque(std::uint8_t index) { return index!=0 && index!=253; }
    inline std::string Key(const Source& src,const std::uint8_t* palette)
    {
        std::uint8_t dims[4]={static_cast<std::uint8_t>(src.w&255),
            static_cast<std::uint8_t>(src.w>>8),static_cast<std::uint8_t>(src.h&255),
            static_cast<std::uint8_t>(src.h>>8)};
        auto h=Hash(dims,4);
        h=Hash(palette,768,h); h=Hash(src.indices.data(),src.indices.size(),h);
        std::ostringstream s; s<<std::hex<<std::setfill('0')<<std::setw(16)<<h;
        return s.str();
    }
    struct Entry
    {
        Source src;
        std::vector<std::uint8_t> rgb;
        bool Load(const fs::path& path)
        {
            std::ifstream in(path,std::ios::binary);
            char magic[8]; std::uint32_t w=0,h=0;
            if(!in.read(magic,8)||std::memcmp(magic,"SCHDRGB1",8)||
               !Read32(in,w)||!Read32(in,h)||w!=static_cast<unsigned>(src.w*scale)||
               h!=static_cast<unsigned>(src.h*scale)) return false;
            std::vector<std::uint8_t> data(static_cast<std::size_t>(w)*h*3);
            if(!in.read(reinterpret_cast<char*>(data.data()),static_cast<std::streamsize>(data.size()))||
               in.peek()!=std::char_traits<char>::eof()) return false;
            rgb.swap(data);
            return true;
        }
        const std::uint8_t* Pixel(int x,int y,int fx,int fy) const
        {
            if(rgb.empty()||x<0||y<0||x>=src.w||y>=src.h) return nullptr;
            const auto idx=src.indices[static_cast<std::size_t>(y)*src.w+x];
            if(!Opaque(idx)) return nullptr;
            const int hx=x*scale+std::clamp(fx,0,255)*scale/256;
            const int hy=y*scale+std::clamp(fy,0,255)*scale/256;
            return rgb.data()+(static_cast<std::size_t>(hy)*src.w*scale+hx)*3;
        }
    };
    class Cache
    {
        std::map<std::string,Entry> entries;
        std::set<std::pair<std::uintptr_t,std::uint64_t>> warmed;
    public:
        bool WarmOnce(const void* resource,const std::uint8_t* pal)
        {
            return warmed.emplace(reinterpret_cast<std::uintptr_t>(resource),Hash(pal,768)).second;
        }
        Entry* Get(Source src,const std::uint8_t* pal)
        {
            if(!src.w||!src.h) return nullptr;
            const auto key=Key(src,pal);
            auto it=entries.find(key);
            if(it!=entries.end()) return &it->second;
            auto& e=entries[key]; e.src=std::move(src);
            try
            {
                const auto root=CacheRoot();
                std::error_code ec;
                fs::create_directories(root,ec);
                if(ec) return &e;
                if(e.Load(root/(key+".scrgb"))) return &e;
                const auto path=root/(key+".scsrc");
                if(!fs::exists(path,ec))
                {
                    const auto tmp=root/(key+".tmp");
                    std::ofstream out(tmp,std::ios::binary|std::ios::trunc);
                    out.write("SCHDSRC1",8);
                    Put32(out,e.src.w); Put32(out,e.src.h);
                    out.write(reinterpret_cast<const char*>(pal),768);
                    out.write(reinterpret_cast<const char*>(e.src.indices.data()),
                              static_cast<std::streamsize>(e.src.indices.size()));
                    out.close();
                    if(out) fs::rename(tmp,path,ec);
                    if(ec) fs::remove(tmp,ec);
                }
            }
            catch(const std::exception&) { /* read-only install: original sprites remain */ }
            return &e;
        }
    };
    inline Cache& SharedCache() { static Cache c; return c; }

    struct Mark { const Entry* entry=nullptr; std::uint16_t x=0,y=0; std::uint8_t index=0; };
    class Frame
    {
        int w=0,h=0,left=0,top=0;
        const std::uint8_t* palette=nullptr;
        // Sparse 32x32 blocks: no full viewport-sized Mark array even on
        // a 4K monitor at minimum zoom. An empty cache allocates no blocks.
        static constexpr int blockSide=32;
        using Block=std::array<Mark,blockSide*blockSide>;
        int blocksX=0;
        std::vector<std::unique_ptr<Block>> blocks;
        bool hasMarks=false;
        const std::uint8_t* target=nullptr;
        Mark* At(int x,int y,bool create)
        {
            if(x<0||y<0||x>=w||y>=h) return nullptr;
            auto& block=blocks[static_cast<std::size_t>(y/blockSide)*blocksX+x/blockSide];
            if(!block&&create) block=std::make_unique<Block>();
            return block?&(*block)[(y%blockSide)*blockSide+x%blockSide]:nullptr;
        }
    public:
        void Reset(int width,int height,int x,int y,const std::uint8_t* pal,
                   const std::uint8_t* buffer=nullptr)
        {
            w=width; h=height; left=x; top=y; palette=pal; target=buffer;
            blocksX=(w+blockSide-1)/blockSide;
            blocks.clear();
            blocks.resize(static_cast<std::size_t>(blocksX)*((h+blockSide-1)/blockSide));
            hasMarks=false;
        }
        const std::uint8_t* Palette() const { return palette; }
        bool Matches(const std::uint8_t* buffer) const { return target==buffer; }
        void Write(int x,int y,Mark mark)
        {
            x-=left; y-=top;
            if(x<0||y<0||x>=w||y>=h) return;
            if(auto* slot=At(x,y,mark.entry!=nullptr)) *slot=mark;
            if(mark.entry) hasMarks=true;
        }
        void EraseRect(int x,int y,int width,int height)
        {
            if(!hasMarks) return;
            for(int py=(std::max)(y,top);py<(std::min)(y+height,top+h);++py)
                for(int px=(std::max)(x,left);px<(std::min)(x+width,left+w);++px)
                    Write(px,py,{});
        }
        void ClearRows(const std::uint8_t* bytes,std::size_t size,int rows,int x,int y)
        {
            if(!hasMarks) return;
            Rows(bytes,size,rows,[&](int dy,int dx,const std::uint8_t* p,int count){
                for(int i=0;i<count;++i) if(p[i]) Write(x+dx+i,y+dy,{});
            });
        }
        void Record(const Entry* e,int x,int y)
        {
            if(!e||e->rgb.empty()) return;
            for(int sy=0;sy<e->src.h;++sy) for(int sx=0;sx<e->src.w;++sx)
            {
                const auto idx=e->src.indices[static_cast<std::size_t>(sy)*e->src.w+sx];
                if(Opaque(idx)) Write(x+sx,y+sy,{e,static_cast<std::uint16_t>(sx),
                    static_cast<std::uint16_t>(sy),idx});
                else if(idx) Write(x+sx,y+sy,{});
            }
        }
        void Finalize(const std::uint8_t* frame,std::size_t len,int stride)
        {
            if(!hasMarks) return;
            for(std::size_t b=0;b<blocks.size();++b)
            {
                if(!blocks[b]) continue;
                const int bx=static_cast<int>(b%blocksX)*blockSide;
                const int by=static_cast<int>(b/blocksX)*blockSide;
                for(int y=0;y<blockSide&&by+y<h;++y)
                    for(int x=0;x<blockSide&&bx+x<w;++x)
                    {
                        auto& mark=(*blocks[b])[y*blockSide+x];
                        if(!mark.entry) continue;
                        const auto pos=static_cast<std::size_t>(top+by+y)*stride+left+bx+x;
                        if(pos>=len||frame[pos]!=mark.index) mark={};
                    }
            }
        }
        const std::uint8_t* Pixel(int x,int y,int fx,int fy) const
        {
            if(!hasMarks||x<0||y<0||x>=w||y>=h) return nullptr;
            const auto& block=blocks[static_cast<std::size_t>(y/blockSide)*blocksX+x/blockSide];
            if(!block) return nullptr;
            const auto& mark=(*block)[(y%blockSide)*blockSide+x%blockSide];
            return mark.entry?mark.entry->Pixel(mark.x,mark.y,fx,fy):nullptr;
        }
        bool HasMarks() const { return hasMarks; }
    };
    inline Frame*& Active() { static thread_local Frame* active=nullptr; return active; }
    class Scope
    {
        Frame* previous;
        bool open=true;
    public:
        explicit Scope(Frame* frame):previous(Active()) { Active()=frame; }
        Scope(const Scope&)=delete;
        Scope& operator=(const Scope&)=delete;
        void End() { if(open) { Active()=previous; open=false; } }
        ~Scope() { End(); }
    };
}
