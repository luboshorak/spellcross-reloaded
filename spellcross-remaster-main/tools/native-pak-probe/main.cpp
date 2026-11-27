#include "../../source/native_pak/pixels.hpp"
#include <wx/init.h>
#include <iostream>

int main(int argc,char** argv){
    using namespace sc::native_pak;
    if(argc<2){
        std::cerr<<"Usage: native_pak_probe DATASET [inspect | resource NAME | image NAME OUTPUT.png]\n";
        return 2;
    }
    try{
        wxInitializer init;
        if(!init.IsOk())fail("wxWidgets initialization failed");
        if(!wxImage::FindHandler(wxBITMAP_TYPE_PNG))wxImage::AddHandler(new wxPNGHandler);
        Provider pak(std::filesystem::u8path(argv[1]));
        std::string command=argc>2?argv[2]:"inspect";
        if(command=="inspect"&&argc<=3){
            std::cout<<"Native PAK v1 | converter "<<pak.converter_version()<<" | edition "<<pak.edition()
                     <<" | "<<(pak.partial()?"partial":"full")<<" | "<<pak.record_count()<<" records | "
                     <<pak.group_count()<<" atlas groups\n";
        }else if(command=="resource"&&argc==4){
            const auto& r=pak.resource(argv[3]);
            auto bytes=pak.payload(r.id);
            std::cout<<r.id<<"\n"<<r.package<<" / "<<r.payload<<"\n"<<r.kind<<": "<<bytes.size()<<" bytes read; CRC verified\n";
        }else if(command=="image"&&argc==5){
            auto frame=decode_frame(pak.resource_frame(argv[3]));
            auto image=preview_rgba(frame);
            auto out=std::filesystem::u8path(argv[4]);
            if(out.extension()!=".png"||std::filesystem::exists(out))fail("Choose a new .png output file");
            if(!image.SaveFile(wxString(out.wstring()),wxBITMAP_TYPE_PNG))fail("PNG output failed");
            std::cout<<"Native graphic "<<argv[3]<<": "<<frame.width<<" x "<<frame.height
                     <<", origin ("<<frame.origin_x<<", "<<frame.origin_y<<")\n"
                     <<"Read indexed PNG, opacity and palette directly; no FS/FSU/LZ decoder used.\n";
        }else{
            fail("Invalid command or arguments");
        }
        return 0;
    }catch(const std::exception& e){std::cerr<<"Native PAK: "<<e.what()<<'\n';return 1;}
}
