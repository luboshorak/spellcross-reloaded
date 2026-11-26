// Spellcross PAK v1 compatibility import.
// JSON parser and CRC implementation adapted from Spellcross Asset Converter
// (Luboš Horák, MIT). Modern derived data is not interpreted by this bridge.
#pragma once
#include <wx/wfstream.h>
#include <wx/zipstrm.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace spellcross_pak {
namespace fs = std::filesystem;
namespace {
[[noreturn]] void fail(const std::string& msg){throw std::runtime_error(msg);}
struct J {
    using A=std::vector<J>;using O=std::map<std::string,J>;
    // JSON numbers kept as strings with a distinct type, avoiding rounding.
    struct N{std::string value;};
    std::variant<std::nullptr_t,bool,N,std::string,A,O> v;
    const O& obj()const{if(auto p=std::get_if<O>(&v))return *p;fail("Expected JSON object");}
    const A& arr()const{if(auto p=std::get_if<A>(&v))return *p;fail("Expected JSON array");}
    const std::string& str()const{if(auto p=std::get_if<std::string>(&v))return *p;fail("Expected JSON string");}
    const J& at(const std::string& k)const{auto& o=obj();auto i=o.find(k);if(i==o.end())fail("Missing JSON key: "+k);return i->second;}
    const J* get(const std::string& k)const{auto& o=obj();auto i=o.find(k);return i==o.end()?nullptr:&i->second;}
    std::uint64_t num()const{
        auto p=std::get_if<N>(&v);if(!p)fail("Expected JSON integer");
        std::uint64_t n{};auto s=p->value;auto r=std::from_chars(s.data(),s.data()+s.size(),n);
        if(r.ec!=std::errc{}||r.ptr!=s.data()+s.size())fail("Invalid unsigned JSON integer");return n;
    }
    bool boolean()const{if(auto p=std::get_if<bool>(&v))return *p;fail("Expected JSON boolean");}
};
struct Parser{
    std::string_view s;std::size_t p{},nodes{};
    void ws(){while(p<s.size()&&(s[p]==' '||s[p]=='\r'||s[p]=='\n'||s[p]=='\t'))++p;}
    bool eat(char c){ws();if(p<s.size()&&s[p]==c){++p;return true;}return false;}
    void expect(char c){if(!eat(c))fail("Malformed JSON at "+std::to_string(p));}
    unsigned hex4(){unsigned u=0;for(int i=0;i<4;++i){
        if(p==s.size())fail("Truncated JSON escape");char c=s[p++];unsigned x;
        if(c>='0'&&c<='9')x=c-'0';else if(c>='a'&&c<='f')x=c-'a'+10;else if(c>='A'&&c<='F')x=c-'A'+10;else fail("Invalid JSON escape");u=(u<<4)|x;
    }return u;}
    std::string string(){
        expect('"');std::string o;
        while(p<s.size()){
            unsigned char c=s[p++];if(c=='"')return o;
            if(c<32)fail("Control byte in JSON string");
            if(c!='\\'){o+=char(c);continue;}
            if(p==s.size())fail("Truncated JSON string");c=s[p++];
            switch(c){
            case '"':case '\\':case '/':o+=char(c);break;
            case 'b':o+='\b';break;case 'f':o+='\f';break;case 'n':o+='\n';break;case 'r':o+='\r';break;case 't':o+='\t';break;
            case 'u':{
                unsigned u=hex4();
                if(u>=0xd800&&u<=0xdbff){if(p+2>s.size()||s.substr(p,2)!="\\u")fail("Missing JSON low surrogate");p+=2;auto l=hex4();if(l<0xdc00||l>0xdfff)fail("Invalid JSON surrogate");u=0x10000+((u-0xd800)<<10)+l-0xdc00;}
                else if(u>=0xdc00&&u<=0xdfff)fail("Unexpected JSON low surrogate");
                if(u<128)o+=char(u);else if(u<2048){o+=char(0xc0|(u>>6));o+=char(0x80|(u&63));}
                else if(u<65536){o+=char(0xe0|(u>>12));o+=char(0x80|((u>>6)&63));o+=char(0x80|(u&63));}
                else{o+=char(0xf0|(u>>18));o+=char(0x80|((u>>12)&63));o+=char(0x80|((u>>6)&63));o+=char(0x80|(u&63));}break;}
            default:fail("Invalid JSON escape");
            }
        }fail("Unterminated JSON string");
    }
    J value(unsigned depth=0){
        if(depth>32||++nodes>3000000)fail("JSON complexity limit");ws();if(p==s.size())fail("Missing JSON value");
        if(s[p]=='"')return J{string()};
        if(eat('{')){J::O o;if(!eat('}'))for(;;){auto k=string();expect(':');if(!o.emplace(k,value(depth+1)).second)fail("Duplicate JSON key: "+k);if(eat('}'))break;expect(',');}return J{std::move(o)};}
        if(eat('[')){J::A a;if(!eat(']'))for(;;){a.push_back(value(depth+1));if(eat(']'))break;expect(',');}return J{std::move(a)};}
        for(auto literal:{"true","false","null"}){std::string_view l=literal;if(s.substr(p,l.size())==l){p+=l.size();if(l=="null")return J{nullptr};return J{l=="true"};}}
        auto start=p;if(s[p]=='-')++p;
        auto digits=[&](){auto begin=p;while(p<s.size()&&s[p]>='0'&&s[p]<='9')++p;return p>begin;};
        if(p<s.size()&&s[p]=='0')++p;else if(!digits())fail("Invalid JSON number");
        if(p<s.size()&&s[p]=='.'){++p;if(!digits())fail("Invalid JSON fraction");}
        if(p<s.size()&&(s[p]=='e'||s[p]=='E')){++p;if(p<s.size()&&(s[p]=='+'||s[p]=='-'))++p;if(!digits())fail("Invalid JSON exponent");}
        return J{J::N{std::string(s.substr(start,p-start))}};
    }
};
J parse(std::string_view s){if(s.size()>64u*1024u*1024u)fail("JSON exceeds limit");Parser p{s};auto j=p.value();p.ws();if(p.p!=s.size())fail("Trailing JSON data");return j;}
void schema(const J& j,const std::string& name){if(j.at("schema").str()!=name||j.at("version").num()!=1)fail("Unsupported schema: "+name);}

bool safe_entry(std::string_view s){
    if(s.empty()||s.size()>1024||s.front()=='/'||s.back()=='/')return false;
    for(unsigned char c:s)if(c<32||c>=127||c=='\\'||c==':')return false;
    while(!s.empty()){auto p=s.find('/');auto part=s.substr(0,p);if(part.empty()||part=="."||part=="..")return false;if(p==s.npos)break;s.remove_prefix(p+1);}
    return true;
}

std::uint32_t crc32_update(std::uint32_t state,const unsigned char* d,std::size_t n){
    static const auto table=[](){std::vector<std::uint32_t> t(256);for(unsigned i=0;i<256;++i){auto c=std::uint32_t(i);for(int j=0;j<8;++j)c=(c>>1)^((c&1)?0xedb88320u:0);t[i]=c;}return t;}();
    for(std::size_t i=0;i<n;++i)state=(state>>8)^table[(state^d[i])&255];return state;
}
}
struct EntryInfo {
    std::uint64_t size{};
    std::uint32_t crc{};
};
struct Package {
    fs::path file;
    std::map<std::string,EntryInfo> entries;
    std::string manifest;
};
struct OriginalArchive {
    std::size_t package{};
    std::string entry;
    EntryInfo info;
};
struct Dataset {
    std::vector<Package> packages;
    std::map<std::string,OriginalArchive> archives;
};
using Progress=std::function<void(const std::string&)>;
constexpr std::uint64_t max_entry=8ull*1024*1024*1024;
constexpr std::uint64_t max_package=32ull*1024*1024*1024;
constexpr std::uint64_t max_manifest=64ull*1024*1024;
inline bool regular(const fs::path& p){
    auto st=fs::symlink_status(p);return fs::is_regular_file(st);
}
inline std::string read_manifest(const fs::path& p){
    if(!regular(p))fail("Missing or nonregular dataset manifest");
    const auto n=fs::file_size(p);
    if(!n||n>max_manifest)fail("Invalid dataset manifest size");
    std::string s(static_cast<std::size_t>(n),'\0');
    std::ifstream in(p,std::ios::binary);
    if(!in.read(s.data(),static_cast<std::streamsize>(s.size())))fail("Cannot read dataset manifest");
    return s;
}
inline EntryInfo consume(wxZipInputStream& zip,wxZipEntry& item,std::uint64_t limit,
                         const std::function<void(const unsigned char*,std::size_t)>& sink={}){
    std::array<unsigned char,65536> buf{};
    EntryInfo result;std::uint32_t crc=0xffffffffu;
    for(;;){
        zip.Read(buf.data(),buf.size());auto n=zip.LastRead();
        if(n>limit-result.size)fail("PAK uncompressed size limit exceeded");
        crc=crc32_update(crc,buf.data(),n);
        if(n&&sink)sink(buf.data(),n);
        result.size+=n;
        if(zip.GetLastError()==wxSTREAM_EOF)break;
        if(zip.GetLastError()!=wxSTREAM_NO_ERROR||!n)fail("PAK ZIP stream read error");
    }
    result.crc=crc^0xffffffffu;
    if(item.GetSize()<0||static_cast<std::uint64_t>(item.GetSize())!=result.size||
        item.GetCrc()!=result.crc)fail("PAK ZIP size or CRC mismatch");
    return result;
}
inline std::string entry_name(wxZipEntry& item){
    const auto utf=item.GetName(wxPATH_UNIX).utf8_str();
    std::string n(utf.data(),utf.length());
    if(item.IsDir()||!safe_entry(n))fail("Unsafe PAK ZIP entry: "+n);
    return n;
}
inline Package inspect_package(const fs::path& p){
    if(!regular(p))fail("Missing or nonregular PAK package");
    Package out;out.file=p;
    wxFFileInputStream file(wxString(p.wstring()));
    if(!file.IsOk())fail("Cannot open PAK package");
    wxZipInputStream zip(file);std::uint64_t total=0;
    while(auto item=std::unique_ptr<wxZipEntry>(zip.GetNextEntry())){
        auto name=entry_name(*item);
        if(out.entries.size()>=300000||out.entries.count(name))fail("Duplicate or too many PAK entries");
        auto limit=std::min(max_package-total,name=="manifest.json"?max_manifest:max_entry);
        EntryInfo info;
        if(name=="manifest.json")info=consume(zip,*item,limit,[&](const unsigned char* data,std::size_t n){
            out.manifest.append(reinterpret_cast<const char*>(data),n);
        });
        else info=consume(zip,*item,limit);
        total+=info.size;out.entries.emplace(std::move(name),info);
    }
    if(zip.GetLastError()!=wxSTREAM_EOF||out.manifest.empty())fail("Invalid ZIP end or missing PAK manifest");
    return out;
}
inline const EntryInfo& lookup(const Package& p,const std::string& entry){
    if(!safe_entry(entry))fail("Unsafe manifest entry reference");
    auto it=p.entries.find(entry);
    if(it==p.entries.end())fail("Missing PAK entry: "+entry);
    return it->second;
}
inline std::string uppercase(std::string n){
    for(auto& c:n)if(c>='a'&&c<='z')c=static_cast<char>(c-'a'+'A');return n;
}
inline std::string archive_name(std::string source,const std::string& format){
    // Accept top-level archive records and direct children of INSTALL.DTA,
    // never an arbitrary FS member that happens to have an archive suffix.
    const auto sep=source.find("::");
    if(sep!=std::string::npos){
        const auto parent=uppercase(source.substr(0,sep));
        const auto slash=parent.find_last_of("/\\");
        if(parent.substr(slash==std::string::npos?0:slash+1)!="INSTALL.DTA"||
            source.find("::",sep+2)!=std::string::npos)return {};
        source=source.substr(sep+2);
    }
    const auto slash=source.find_last_of("/\\");
    auto leaf=uppercase(source.substr(slash==std::string::npos?0:slash+1));
    static const std::set<std::string> fs_names={"COMMON.FS","T11.FS","PUST.FS","DEVAST.FS","TEXTS.FS","INFO.FS","SAMPLES.FS","MUSIC.FS","MOVIE.FS","MOVIES.FS","SPEAKER.FS"};
    if(format=="FS"&&fs_names.count(leaf))return leaf=="MOVIES.FS"?"MOVIE.FS":leaf;
    if(format=="FSU"&&leaf=="UNITS.FSU")return leaf;
    return {};
}
inline Dataset inspect(const fs::path& directory,const Progress& progress={}){
    auto root=parse(read_manifest(directory/"manifest.json"));
    schema(root,"spellcross-dataset");
    if(root.at("partial").boolean())fail("A partial PAK dataset cannot start the game");
    // Runtime_ready refers to native modern assets; this importer deliberately
    // uses retained originals and accepts the current false value.
    root.at("runtime_ready").boolean();
    const auto& packages=root.at("packages").arr();
    if(packages.empty()||packages.size()>100)fail("Invalid PAK package count");
    Dataset result;std::set<std::string> files,categories;
    for(const auto& declaration:packages){
        auto name=declaration.at("file").str(),category=declaration.at("category").str();
        if(!safe_entry(name)||name.find('/')!=std::string::npos||name!=category+".pak"||
            !files.insert(uppercase(name)).second||!categories.insert(category).second)
            fail("Invalid or duplicate PAK package name");
        if(progress)progress("Validating "+name);
        auto package=inspect_package(directory/name);
        auto j=parse(package.manifest);schema(j,"spellcross-pak");
        if(j.at("partial").boolean()||j.at("category").str()!=category)fail("PAK category/partial mismatch");
        const auto& records=j.at("records").arr();
        if(records.size()!=declaration.at("records").num())fail("PAK record count mismatch");
        std::set<std::string> ids;
        std::map<std::string,std::uint64_t> counts;
        static const std::set<std::string> statuses={"converted","decoded","raw","failed","container"};
        for(const auto& r:records){
            auto id=r.at("id").str(),status=r.at("status").str(),fmt=r.at("format").str();
            if(id.empty()||!ids.insert(id).second||!statuses.count(status))fail("Invalid PAK record ID/status");
            ++counts[status];
            auto& orig=r.at("original");auto loc=orig.at("entry").str();
            auto& info=lookup(package,loc);
            auto offset=orig.at("offset").num(),size=orig.at("size").num();
            if(offset>info.size||size>info.size-offset)fail("PAK original range outside entry: "+id);
            if(auto fb=r.get("fallback"))if(lookup(package,fb->str()).size!=size)fail("PAK fallback size mismatch");
            if(auto payload=r.get("payload"))lookup(package,payload->str());
            else if(status=="converted"||status=="decoded")fail("PAK converted payload missing");
            auto target=archive_name(r.at("name").str(),fmt);
            if(target.empty())continue;
            if(status!="container"||offset||size!=info.size||!size)fail("Incomplete original archive: "+target);
            if(!result.archives.emplace(target,OriginalArchive{result.packages.size(),loc,info}).second)
                fail("Ambiguous duplicate archive in PAK dataset: "+target);
        }
        for(const auto& status:statuses)if(counts[status]!=declaration.at(status=="container"?"containers":status).num())
            fail("PAK status totals mismatch: "+name);
        // Keep only the entry index; package manifest DOM is released here.
        package.manifest.clear();
        result.packages.push_back(std::move(package));
    }
    for(auto name:{"COMMON.FS","T11.FS","PUST.FS","DEVAST.FS","UNITS.FSU","TEXTS.FS","INFO.FS"})
        if(!result.archives.count(name))fail(std::string("Required original archive missing: ")+name);
    return result;
}
inline void materialize(const Dataset& dataset,const fs::path& fresh_dir,const Progress& progress={}){
    // Caller owns a newly created empty directory and removes it on failure.
    if(!fs::is_directory(fresh_dir)||!fs::is_empty(fresh_dir))fail("PAK import destination must be empty");
    for(std::size_t i=0;i<dataset.packages.size();++i){
        std::map<std::string,std::pair<std::string,EntryInfo>> wanted;
        for(const auto& kv:dataset.archives)if(kv.second.package==i){
            if(!wanted.emplace(kv.second.entry,std::make_pair(kv.first,kv.second.info)).second)
                fail("Ambiguous shared archive entry");
        }
        if(wanted.empty())continue;
        const auto& p=dataset.packages[i];
        if(!regular(p.file))fail("PAK package changed after validation");
        wxFFileInputStream file(wxString(p.file.wstring()));if(!file.IsOk())fail("Cannot reopen PAK package");
        wxZipInputStream zip(file);
        std::set<std::string> seen;
        while(auto item=std::unique_ptr<wxZipEntry>(zip.GetNextEntry())){
            auto name=entry_name(*item);
            if(!seen.insert(name).second)fail("Duplicate ZIP entry during import");
            auto it=wanted.find(name);if(it==wanted.end())continue;
            auto& target=it->second.first;auto& expected=it->second.second;
            if(progress)progress("Importing "+target);
            std::ofstream out(fresh_dir/target,std::ios::binary);
            if(!out)fail("Cannot create imported archive: "+target);
            auto actual=consume(zip,*item,expected.size,[&](const unsigned char* data,std::size_t n){
                out.write(reinterpret_cast<const char*>(data),static_cast<std::streamsize>(n));
                if(!out)fail("Cannot write imported archive: "+target);
            });
            out.close();
            if(!out||actual.size!=expected.size||actual.crc!=expected.crc)fail("Original archive changed after validation: "+target);
            wanted.erase(it);
        }
        if(zip.GetLastError()!=wxSTREAM_EOF||!wanted.empty())fail("Incomplete PAK archive import");
    }
}
} // namespace spellcross_pak
