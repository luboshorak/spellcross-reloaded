#pragma once
// Experimental native PAK v1 data access. No legacy archive dependency.
// One provider per dataset; archive streams are serialized by the caller.
// The full converter audit remains the authoritative whole-dataset check.
#include "json.hpp"
#include <wx/wfstream.h>
#include <wx/zipstrm.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <memory>
#include <set>
#include <limits>
#include <iterator>

namespace sc::native_pak {
using Bytes=std::vector<std::uint8_t>;
inline constexpr std::size_t metadata_limit=64u*1024u*1024u;
inline bool safe_entry(std::string_view s){
    if(s.empty()||s.size()>1024||s.front()=='/'||s.back()=='/')return false;
    for(unsigned char c:s)if(c<32||c>=127||c=='\\'||c==':')return false;
    while(!s.empty()){
        auto p=s.find('/');auto part=s.substr(0,p);
        if(part.empty()||part=="."||part=="..")return false;
        if(p==s.npos)break;s.remove_prefix(p+1);
    }
    return true;
}
inline std::string key(std::string s){
    for(auto& c:s){if(c=='\\')c='/';else if(c>='a'&&c<='z')c=char(c-'a'+'A');}
    return s;
}
inline std::string leaf(std::string_view s){
    auto p=s.find_last_of("/\\:");return key(std::string(p==s.npos?s:s.substr(p+1)));
}
inline void regular(const std::filesystem::path& p){
    if(!std::filesystem::is_regular_file(p)||std::filesystem::is_symlink(p))fail("Missing or nonregular dataset file: "+p.filename().string());
}
inline J read_json(const std::filesystem::path& p){
    regular(p);auto n=std::filesystem::file_size(p);
    if(n>metadata_limit)fail("Dataset metadata too large");
    std::ifstream in(p,std::ios::binary);std::string s(static_cast<std::size_t>(n),'\0');
    if(!in||!in.read(s.data(),static_cast<std::streamsize>(s.size()))||in.peek()!=std::char_traits<char>::eof())fail("Dataset metadata read failed");
    return parse(s);
}
inline std::uint32_t crc32(const Bytes& data){
    static const auto table=[](){
        std::array<std::uint32_t,256> t{};
        for(unsigned i=0;i<256;++i){auto c=std::uint32_t(i);for(int j=0;j<8;++j)c=(c>>1)^((c&1)?0xedb88320u:0);t[i]=c;}
        return t;
    }();
    std::uint32_t c=0xffffffffu;
    for(auto b:data)c=(c>>8)^table[(c^b)&255];return c^0xffffffffu;
}
inline J parse_bytes(const Bytes& b){
    return parse(std::string_view(b.empty()?"":reinterpret_cast<const char*>(b.data()),b.size()));
}
class Package {
    wxFFileInputStream file_;
    wxZipInputStream zip_;
    std::map<std::string,std::unique_ptr<wxZipEntry>> entries_;
    std::map<std::string,std::size_t> records_;
    J manifest_;
public:
    Package(const std::filesystem::path& path,const std::string& category,const std::string& version,bool partial,std::uint64_t count)
        :file_(wxString(path.wstring())),zip_(file_){
        if(!file_.IsOk()||!file_.IsSeekable())fail("Cannot open seekable PAK: "+category);
        while(auto entry=std::unique_ptr<wxZipEntry>(zip_.GetNextEntry())){
            if(entries_.size()>=300000)fail("PAK directory too large");
            const wxString name=entry->GetName(wxPATH_UNIX);auto u=name.utf8_str();
            std::string n(u.data(),u.length());
            if(entry->IsDir()||!safe_entry(n)||!entries_.emplace(n,std::move(entry)).second)
                fail("Unsafe or duplicate PAK entry: "+n);
        }
        if(zip_.GetLastError()!=wxSTREAM_EOF)fail("Invalid PAK directory");
        manifest_=parse_bytes(read("manifest.json",metadata_limit));
        schema(manifest_,"spellcross-pak");
        if(manifest_.at("category").str()!=category||manifest_.at("partial").boolean()!=partial)
            fail("PAK category/selection mismatch");
        if(auto v=manifest_.get("converter_version");!version.empty()&&(!v||v->str()!=version))
            fail("PAK converter version mismatch");
        const auto& records=manifest_.at("records").arr();
        if(records.size()!=count)fail("PAK record count mismatch");
        for(std::size_t i=0;i<records.size();++i)
            if(!records_.emplace(records[i].at("id").str(),i).second)fail("Duplicate package record ID");
    }
    const J& manifest() const{return manifest_;}
    const J& record(const std::string& id) const{
        auto i=records_.find(id);if(i==records_.end())fail("Record absent from package: "+id);
        return manifest_.at("records").arr()[i->second];
    }
    Bytes read(const std::string& name,std::size_t limit){
        if(!safe_entry(name)||name.starts_with("original/")||name.ends_with("/original.bin")||name.ends_with("/decoded.bin"))
            fail("Legacy/raw entry access forbidden: "+name);
        auto it=entries_.find(name);if(it==entries_.end())fail("Missing PAK entry: "+name);
        auto& e=*it->second;
        if(e.GetSize()<0||static_cast<std::uint64_t>(e.GetSize())>limit)fail("PAK payload size limit: "+name);
        if(!zip_.OpenEntry(e))fail("Cannot open PAK entry: "+name);
        Bytes out;out.reserve(static_cast<std::size_t>(e.GetSize()));
        std::array<std::uint8_t,65536> buffer{};
        for(;;){
            zip_.Read(buffer.data(),buffer.size());auto n=zip_.LastRead();
            if(n>limit-out.size()||n>static_cast<std::size_t>(e.GetSize())-out.size())fail("PAK entry exceeds size: "+name);
            out.insert(out.end(),buffer.begin(),buffer.begin()+n);
            if(zip_.GetLastError()==wxSTREAM_EOF)break;
            if(zip_.GetLastError()!=wxSTREAM_NO_ERROR||!n)fail("PAK entry read failed: "+name);
        }
        if(out.size()!=static_cast<std::size_t>(e.GetSize())||crc32(out)!=e.GetCrc())fail("PAK entry length/CRC mismatch: "+name);
        return out;
    }
};
struct Resource {
    std::string id,name,package,format,status,kind,payload,atlas_entry;
};
struct Group {
    std::string name,package,entry;
    std::uint64_t frames{};
};
struct FrameData {
    J metadata;
    Bytes indices_png,opacity_png,rgb,known;
};
class Provider {
    struct PackageInfo {std::string category;std::uint64_t count;};
    std::filesystem::path root_;
    std::string version_,edition_;
    bool partial_{};
    std::map<std::string,PackageInfo> packages_;
    std::map<std::string,std::unique_ptr<Package>> opened_;
    std::map<std::string,Resource> records_;
    std::map<std::string,std::vector<std::string>> names_,leaves_;
    std::vector<Group> groups_;
    std::map<std::string,std::vector<std::size_t>> group_names_,group_leaves_;
    template<class T> static T unique(const std::vector<T>& matches,const std::string& name){
        if(matches.size()!=1)fail("Ambiguous resource/group: "+name+" (use exact ID or full name)");
        return matches.front();
    }
    Package& package(const std::string& file){
        auto p=packages_.find(file);if(p==packages_.end())fail("Undeclared PAK: "+file);
        if(auto i=opened_.find(file);i!=opened_.end())return *i->second;
        auto path=root_/file;regular(path);
        auto value=std::make_unique<Package>(path,p->second.category,version_,partial_,p->second.count);
        return *opened_.emplace(file,std::move(value)).first->second;
    }
    Package& checked(const Resource& resource){
        auto& p=package(resource.package);const auto& r=p.record(resource.id);
        if(r.at("name").str()!=resource.name||r.at("format").str()!=resource.format||
           r.at("status").str()!=resource.status||r.at("payload").str()!=resource.payload)
            fail("Catalog/PAK resource mismatch: "+resource.id);
        return p;
    }
public:
    explicit Provider(std::filesystem::path directory):root_(std::move(directory)){
        const auto root=read_json(root_/"manifest.json");schema(root,"spellcross-dataset");
        partial_=root.at("partial").boolean();
        (void)root.at("runtime_ready").boolean(); // not a claim of gameplay support
        if(root.at("resource_catalog").str()!="resources.json")fail("Unsupported resource catalog");
        if(auto v=root.get("converter_version")){version_=v->str();if(version_.empty())fail("Empty converter version");}
        if(auto e=root.get("edition_language"))edition_=e->str();
        std::uint64_t total=0;
        for(const auto& p:root.at("packages").arr()){
            auto file=p.at("file").str(),category=p.at("category").str();auto count=p.at("records").num();
            if(!safe_entry(file)||file.find('/')!=file.npos||file!=category+".pak"||count>300000-total||
               !packages_.emplace(file,PackageInfo{category,count}).second)fail("Invalid dataset package");
            regular(root_/file);total+=count;
        }
        if(packages_.empty()||packages_.size()>100)fail("Invalid package count");
        const auto cat=read_json(root_/"resources.json");schema(cat,"spellcross-resource-catalog");
        if(cat.at("partial").boolean()!=partial_)fail("Catalog selection mismatch");
        if(auto e=cat.get("edition_language");e&&e->str()!=edition_)fail("Catalog edition mismatch");
        if(cat.at("records").arr().size()!=total)fail("Catalog record count mismatch");
        std::map<std::string,std::uint64_t> counts;
        for(const auto& r:cat.at("records").arr()){
            Resource x{r.at("id").str(),r.at("name").str(),r.at("package").str(),r.at("format").str(),r.at("status").str(),r.at("kind").str(),{}};
            if(auto p=r.get("payload"))x.payload=p->str();
            if(auto a=r.get("atlas"))x.atlas_entry=a->at("entry").str();
            auto owner=packages_.find(x.package);
            if(x.id.empty()||owner==packages_.end()||r.at("category").str()!=owner->second.category||
               (!x.payload.empty()&&!safe_entry(x.payload))||
               (!x.atlas_entry.empty()&&(!safe_entry(x.atlas_entry)||!x.atlas_entry.starts_with("graphics/")||!x.atlas_entry.ends_with("/frames.json")))||
               !records_.emplace(x.id,x).second)fail("Invalid catalog record");
            ++counts[x.package];names_[key(x.name)].push_back(x.id);leaves_[leaf(x.name)].push_back(x.id);
        }
        for(const auto& [file,p]:packages_)if(counts[file]!=p.count)fail("Catalog package count mismatch");
        std::set<std::pair<std::string,std::string>> locators;
        for(const auto& g:cat.at("atlas_groups").arr()){
            Group x{g.at("group").str(),g.at("package").str(),g.at("entry").str(),g.at("frames").num()};
            if(!packages_.contains(x.package)||!safe_entry(x.entry)||!x.entry.starts_with("graphics/")||
               !x.entry.ends_with("/frames.json")||!x.frames||!locators.emplace(x.package,x.entry).second)
                fail("Invalid atlas group");
            group_names_[key(x.name)].push_back(groups_.size());group_leaves_[leaf(x.name)].push_back(groups_.size());
            groups_.push_back(std::move(x));
        }
    }
    const std::string& edition()const{return edition_;}
    const std::string& converter_version()const{return version_;}
    bool partial()const{return partial_;}
    std::size_t record_count()const{return records_.size();}
    std::size_t group_count()const{return groups_.size();}
    const Resource& resource(const std::string& name)const{
        if(auto i=records_.find(name);i!=records_.end())return i->second;
        if(auto i=names_.find(key(name));i!=names_.end())return records_.at(unique(i->second,name));
        if(auto i=leaves_.find(key(name));i!=leaves_.end())return records_.at(unique(i->second,name));
        fail("Missing resource: "+name);
    }
    // Resolve a retail basename in its original archive scope, independent of
    // whether the dataset came from an ISO, installer or installed directory.
    // Never use conversion status to silently choose between duplicate records.
    const Resource& scoped_resource(const std::string& archive,const std::string& name)const{
        auto i=leaves_.find(key(name));
        if(i==leaves_.end())fail("Missing scoped resource: "+archive+"::"+name);
        std::vector<std::string> matches;
        for(const auto& id:i->second){
            const auto& r=records_.at(id);
            auto n=key(r.name);auto split=n.rfind("::");
            if(split==n.npos)continue;
            auto owner=n.substr(0,split);
            if(owner==key(archive)||leaf(owner)==key(archive))matches.push_back(id);
        }
        if(matches.empty())fail("Missing scoped resource: "+archive+"::"+name);
        return records_.at(unique(matches,archive+"::"+name));
    }
    const Group& group(const std::string& name)const{
        if(auto i=group_names_.find(key(name));i!=group_names_.end())return groups_.at(unique(i->second,name));
        if(auto i=group_leaves_.find(key(name));i!=group_leaves_.end())return groups_.at(unique(i->second,name));
        fail("Missing atlas group: "+name);
    }
    Bytes payload(const std::string& name,std::size_t limit=metadata_limit){
        const auto& r=resource(name);
        if(r.status!="converted"||r.payload.empty()||
           !(r.payload.starts_with("records/")||r.payload.starts_with("graphics/")))
            fail("Resource has no native payload: "+r.id);
        return checked(r).read(r.payload,limit);
    }
private:
    J atlas_group(const Group& g){
        auto& p=package(g.package);
        const auto& declared=p.manifest().at("graphics").arr();
        if(std::none_of(declared.begin(),declared.end(),[&](const J& j){return j.str()==g.entry;}))
            fail("Atlas not declared in package");
        auto a=parse_bytes(p.read(g.entry,metadata_limit));schema(a,"spellcross-atlas");
        if(a.at("group").str()!=g.name||a.at("frames").arr().size()!=g.frames)fail("Atlas catalog mismatch");
        return a;
    }
    FrameData group_frame(const Group& g,const J& a,std::size_t ordinal){
        auto& p=package(g.package);
        const auto& f=a.at("frames").arr().at(ordinal);
        const auto& owner=resource(f.at("id").str());
        if(owner.package!=g.package||owner.atlas_entry!=g.entry||owner.status!="converted")
            fail("Atlas frame owner mismatch or unsupported resource");
        checked(owner);
        if(auto synthetic=f.get("synthetic_placeholder");synthetic&&synthetic->boolean())fail("Synthetic frame is not an original graphic");
        const auto& rect=f.at("rect").arr();
        if(rect.size()!=4||!rect[2].num()||!rect[3].num()||rect[0].num()>2048||rect[1].num()>2048||
           rect[2].num()>2048-rect[0].num()||rect[3].num()>2048-rect[1].num())fail("Invalid frame rectangle");
        const auto& page=a.at("pages").arr().at(static_cast<std::size_t>(f.at("page").num()));
        const auto& pal=p.manifest().at("palettes").arr().at(static_cast<std::size_t>(f.at("palette").num()));
        auto prefix=g.entry.substr(0,g.entry.find_last_of('/')+1);
        auto image=[&](const std::string& n){
            if(!n.starts_with(prefix)||!n.ends_with(".png"))fail("Invalid atlas plane locator");
            auto b=p.read(n,metadata_limit);
            constexpr std::uint8_t sig[]={137,80,78,71,13,10,26,10};
            if(b.size()<33||!std::equal(std::begin(sig),std::end(sig),b.begin())||
               !std::equal(b.begin()+12,b.begin()+16,"IHDR")||b[24]!=8)fail("Invalid atlas PNG header");
            auto be=[&](int i){return std::uint32_t(b[i])<<24|std::uint32_t(b[i+1])<<16|std::uint32_t(b[i+2])<<8|b[i+3];};
            auto w=be(16),h=be(20);
            if(w>2048||h>2048||rect[0].num()+rect[2].num()>w||rect[1].num()+rect[3].num()>h)fail("Frame outside atlas PNG");
            return b;
        };
        auto palette=[&](const std::string& n,std::size_t size){
            if(!n.starts_with("palettes/"))fail("Invalid palette locator");
            auto b=p.read(n,size);if(b.size()!=size)fail("Invalid palette size");return b;
        };
        FrameData result{f,image(page.at("indices").str()),image(page.at("opacity").str()),
                         palette(pal.at("rgb").str(),768),palette(pal.at("known").str(),256)};
        if(!std::equal(result.indices_png.begin()+16,result.indices_png.begin()+24,result.opacity_png.begin()+16))
            fail("Atlas plane dimensions differ");
        if(std::any_of(result.known.begin(),result.known.end(),[](auto n){return n>1;}))fail("Invalid known palette mask");
        return result;
    }
public:
    J atlas(const std::string& name){return atlas_group(group(name));}
    FrameData frame(const std::string& name,std::size_t ordinal){
        const auto& g=group(name);auto a=atlas_group(g);return group_frame(g,a,ordinal);
    }
    // Resource-local frame ordinal, not an ordinal in the whole atlas group.
    FrameData resource_frame(const std::string& name,std::uint64_t ordinal=0){
        const auto& r=resource(name);
        if(r.status!="converted"||r.atlas_entry.empty())fail("Resource has no native graphic");
        for(const auto& g:groups_)if(g.package==r.package&&g.entry==r.atlas_entry){
            auto a=atlas_group(g);const auto& frames=a.at("frames").arr();
            std::size_t match=frames.size();
            for(std::size_t i=0;i<frames.size();++i)
                if(frames[i].at("id").str()==r.id&&frames[i].at("frame").num()==ordinal){
                    if(match!=frames.size())fail("Duplicate resource frame");
                    match=i;
                }
            if(match==frames.size())fail("Resource frame not found");
            return group_frame(g,a,match);
        }
        fail("Resource atlas missing from catalog");
    }
};
}
