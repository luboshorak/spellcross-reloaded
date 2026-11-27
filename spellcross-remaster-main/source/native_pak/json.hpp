#pragma once
// Bounded JSON reader adapted from Spellcross Asset Converter 1.0.0,
// src/core/validation.cpp (MIT, Lubos Horak). Kept independent of legacy loaders.
// https://github.com/luboshorak/spellcross-asset-converter
#include <charconv>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>
namespace sc::native_pak {
[[noreturn]] inline void fail(const std::string& msg){throw std::runtime_error(msg);}
struct J {
    using A=std::vector<J>;using O=std::map<std::string,J>;
    // JSON numbers kept as strings with a distinct type, avoiding rounding.
    struct N{std::string value;bool operator==(const N&)const=default;};
    std::variant<std::nullptr_t,bool,N,std::string,A,O> v;
    bool operator==(const J&)const=default;
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
inline J parse(std::string_view s){if(s.size()>64u*1024u*1024u)fail("JSON exceeds limit");Parser p{s};auto j=p.value();p.ws();if(p.p!=s.size())fail("Trailing JSON data");return j;}
inline void schema(const J& j,const std::string& name){if(j.at("schema").str()!=name||j.at("version").num()!=1)fail("Unsupported schema: "+name);}

}
