#include "../../source/native_pak/pixels.hpp"
#include <wx/init.h>
#include <chrono>
#include <iostream>

using namespace sc::native_pak;
namespace fs=std::filesystem;
static void require(bool yes,const char* message){if(!yes)fail(message);}
template<class F> void rejects(F f,const char* message){
    try{f();}catch(const std::exception&){return;}fail(message);
}
struct Temp {fs::path path;~Temp(){std::error_code ec;fs::remove_all(path,ec);}};
static void write(const fs::path& path,const std::string& data){
    std::ofstream out(path,std::ios::binary);out<<data;out.close();require(bool(out),"fixture write failed");
}
static void entry(wxZipOutputStream& zip,const char* name,const Bytes& data){
    require(zip.PutNextEntry(wxString::FromUTF8(name)),"fixture ZIP entry");
    if(!data.empty()){zip.Write(data.data(),data.size());require(zip.LastWrite()==data.size(),"fixture ZIP write");}
    require(zip.CloseEntry(),"fixture ZIP close entry");
}
static void entry(wxZipOutputStream& zip,const char* name,std::string_view text){entry(zip,name,Bytes(text.begin(),text.end()));}
static Bytes png(const Bytes& pixels){
    wxImage image(2,2);
    for(std::size_t i=0;i<4;++i)for(int c=0;c<3;++c)image.GetData()[i*3+c]=pixels[i];
    wxMemoryOutputStream out;
    require(image.SaveFile(out,wxBITMAP_TYPE_PNG),"fixture PNG");
    Bytes b(out.GetSize());out.CopyTo(b.data(),b.size());return b;
}
static constexpr auto manifest=R"({"schema":"spellcross-dataset","version":1,"converter_version":"1.0.0","partial":false,"runtime_ready":false,"edition_language":"en","resource_catalog":"resources.json","packages":[{"file":"common.pak","category":"common","records":3}]})";
static constexpr auto catalog=R"({"schema":"spellcross-resource-catalog","version":1,"partial":false,"edition_language":"en","records":[
{"id":"DATA/COMMON.FS::MAINMENU.LZ#1","name":"DATA/COMMON.FS::MAINMENU.LZ","package":"common.pak","category":"common","format":"LZ","status":"converted","kind":"indexed-atlas","payload":"graphics/0/frames.json","atlas":{"entry":"graphics/0/frames.json","frames":1}},
{"id":"DATA/COMMON.FS::TEST.TXT#1","name":"DATA/COMMON.FS::TEST.TXT","package":"common.pak","category":"common","format":"TXT","status":"converted","kind":"utf8-text","payload":"records/1/text.txt"},
{"id":"DATA/OTHER.FS::TEST.TXT#1","name":"DATA/OTHER.FS::TEST.TXT","package":"common.pak","category":"common","format":"TXT","status":"failed","kind":"original-only"}],
"atlas_groups":[{"group":"DATA/COMMON.FS::MENU","package":"common.pak","entry":"graphics/0/frames.json","frames":1,"fsu_layout":false}]})";
static constexpr auto pack_manifest=R"({"schema":"spellcross-pak","version":1,"converter_version":"1.0.0","category":"common","partial":false,"records":[
{"id":"DATA/COMMON.FS::MAINMENU.LZ#1","name":"DATA/COMMON.FS::MAINMENU.LZ","format":"LZ","status":"converted","payload":"graphics/0/frames.json"},
{"id":"DATA/COMMON.FS::TEST.TXT#1","name":"DATA/COMMON.FS::TEST.TXT","format":"TXT","status":"converted","payload":"records/1/text.txt"},
{"id":"DATA/OTHER.FS::TEST.TXT#1","name":"DATA/OTHER.FS::TEST.TXT","format":"TXT","status":"failed"}],
"graphics":["graphics/0/frames.json"],"palettes":[{"rgb":"palettes/0.rgb","known":"palettes/0.known"}]})";
static constexpr auto atlas=R"({"schema":"spellcross-atlas","version":1,"group":"DATA/COMMON.FS::MENU","pages":[{"indices":"graphics/0/page-0-indices.png","opacity":"graphics/0/page-0-opacity.png"}],"frames":[{"id":"DATA/COMMON.FS::MAINMENU.LZ#1","frame":0,"page":0,"rect":[0,0,2,2],"origin":[-3,4],"palette":0,"shadow_index":253,"opaque_zero_display_index":254,"trailing_bytes":0}]})";
int main(){
    try{
        wxInitializer init;
        require(init.IsOk(),"wx init");
        if(!wxImage::FindHandler(wxBITMAP_TYPE_PNG))wxImage::AddHandler(new wxPNGHandler);
        Temp temp{fs::temp_directory_path()/("sr-native-pak-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))};
        require(fs::create_directory(temp.path),"fixture directory");
        write(temp.path/"manifest.json",manifest);write(temp.path/"resources.json",catalog);
        {
            wxFFileOutputStream file(wxString((temp.path/"common.pak").wstring()));
            require(file.IsOk(),"fixture PAK");
            wxZipOutputStream zip(file);zip.SetFormat(wxZIP_FORMAT_ZIP64);
            // Intentionally no original entries: this test must use only modern payloads.
            entry(zip,"graphics/0/page-0-indices.png",png({0,1,253,2}));
            entry(zip,"records/1/text.txt","native");
            entry(zip,"graphics/0/page-0-opacity.png",png({255,0,255,255}));
            entry(zip,"graphics/0/frames.json",atlas);
            Bytes palette(768,0);palette[254*3]=111;palette[253*3+1]=77;palette[2*3+2]=99;
            entry(zip,"palettes/0.rgb",palette);entry(zip,"palettes/0.known",Bytes(256,1));
            entry(zip,"manifest.json",pack_manifest);
            require(zip.Close()&&file.Close(),"fixture ZIP finalization");
        }
        {
            Provider p(temp.path);
            require(p.edition()=="en"&&p.converter_version()=="1.0.0"&&p.record_count()==3&&p.group_count()==1,"catalog");
            require(p.resource("mainmenu.lz").kind=="indexed-atlas","leaf lookup");
            require(p.payload("DATA/COMMON.FS::TEST.TXT#1")==Bytes({'n','a','t','i','v','e'}),"native text");
            rejects([&]{p.payload("TEST.TXT");},"ambiguous leaf was accepted");
            rejects([&]{p.payload("DATA/OTHER.FS::TEST.TXT#1");},"failed resource was accepted");
            rejects([&]{p.payload("missing");},"missing resource was accepted");
            auto frame=decode_frame(p.resource_frame("MAINMENU.LZ"));
            require(frame.width==2&&frame.height==2&&frame.origin_x==-3&&frame.origin_y==4,"frame geometry");
            require(frame.indices==Bytes({0,1,253,2})&&frame.opacity==Bytes({255,0,255,255}),"exact data planes");
            auto rgba=preview_rgba(frame);
            require(rgba.GetData()[0]==111&&rgba.GetAlpha()[1]==0&&rgba.GetData()[7]==77&&rgba.GetData()[11]==99,"display palette");
            require(p.frame("menu",0).known.size()==256,"random reread");
            require(p.payload("DATA/COMMON.FS::TEST.TXT#1").size()==6,"random reread text");
            frame.known[254]=0;rejects([&]{preview_rgba(frame);},"unknown palette accepted");
            rejects([&]{p.resource_frame("MAINMENU.LZ",99);},"invalid frame accepted");
        }
        {
            auto bad=std::string(manifest);bad.replace(bad.find("1.0.0"),5,"2.0.0");
            write(temp.path/"manifest.json",bad);
            Provider p(temp.path);
            rejects([&]{p.payload("DATA/COMMON.FS::TEST.TXT#1");},"mixed converter versions accepted");
            write(temp.path/"manifest.json",manifest);
        }
        {
            auto bad=std::string(catalog);auto pos=bad.find("records/1/text.txt");
            bad.replace(pos,std::string("records/1/text.txt").size(),"original/1.bin");
            write(temp.path/"resources.json",bad);
            Provider p(temp.path);
            rejects([&]{p.payload("DATA/COMMON.FS::TEST.TXT#1");},"raw locator accepted");
        }
        require(crc32(Bytes({'1','2','3','4','5','6','7','8','9'}))==0xcbf43926u,"CRC implementation");
        rejects([]{parse(R"({"x":1,"x":2})");},"duplicate JSON key accepted");
        std::cout<<"Native PAK lookup and indexed graphics passed without original data\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
