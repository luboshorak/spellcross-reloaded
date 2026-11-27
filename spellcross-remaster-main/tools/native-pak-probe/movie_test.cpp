#include "../../source/native_pak/movie_decoder.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
int main(int argc,char** argv){
    using namespace sc::native_pak::media;
    if(argc!=2)return 1;
    try{
        std::ifstream f(argv[1],std::ios::binary);
        std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(f)),{});
        auto result=decode(bytes,MovieSpec{16,16,4,8000,1,4000});
        if(result.rgb.size()!=16*16*3*4||result.pcm.size()!=4000)return 2;
        if(std::all_of(result.pcm.begin(),result.pcm.end(),[](auto n){return n==0;}))return 3;
        if(std::all_of(result.rgb.begin(),result.rgb.end(),[](auto n){return n==0;}))return 4;
        bool rejected=false;
        try{decode(bytes,MovieSpec{16,16,3,8000,1,4000});}
        catch(const std::runtime_error&){rejected=true;}
        if(!rejected)return 5;
        rejected=false;
        try{decode(std::vector<std::uint8_t>{1,2,3},MovieSpec{16,16,4});}
        catch(const std::runtime_error&){rejected=true;}
        return rejected?0:6;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 7;}
}
