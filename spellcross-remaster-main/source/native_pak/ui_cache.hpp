#pragma once
#include <list>
#include <map>
#include <string>
#include <cstddef>
#include <utility>
namespace sc::native_pak {
// GUI-thread cache, owned by a single immutable dataset. Limits both payload
// bytes and entry count, including negative results. Never persists to disk.
template<class T> class UiCache {
    struct Entry { T value; bool ok; std::size_t bytes; };
    using List=std::list<std::pair<std::string,Entry>>;
    List entries_;
    std::map<std::string,typename List::iterator> index_;
    std::size_t budget_,max_entries_,used_=0;
public:
    UiCache(std::size_t budget,std::size_t max_entries):budget_(budget),max_entries_(max_entries){}
    UiCache(const UiCache&)=delete;
    UiCache& operator=(const UiCache&)=delete;
    bool find(const std::string& key,T& out,bool& ok){
        auto i=index_.find(key);if(i==index_.end())return false;
        entries_.splice(entries_.begin(),entries_,i->second);
        ok=i->second->second.ok;out=i->second->second.value;return true;
    }
    void put(const std::string& key,const T& value,bool ok,std::size_t bytes){
        if(auto i=index_.find(key);i!=index_.end()){
            used_-=i->second->second.bytes;entries_.erase(i->second);index_.erase(i);
        }
        if(bytes>budget_||!max_entries_)return;
        while(!entries_.empty()&&(bytes>budget_-used_||entries_.size()>=max_entries_)){
            used_-=entries_.back().second.bytes;index_.erase(entries_.back().first);entries_.pop_back();
        }
        entries_.emplace_front(key,Entry{value,ok,bytes});
        try{index_.emplace(entries_.front().first,entries_.begin());}
        catch(...){entries_.pop_front();throw;}
        used_+=bytes;
    }
    std::size_t bytes()const{return used_;}
    std::size_t size()const{return entries_.size();}
};
}
