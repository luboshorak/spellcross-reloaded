#include "../../source/native_pak/engine_campaign.hpp"
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
            Assets assets(p);
            require(assets.resolve("COMMON.FS","test.txt").id=="DATA/COMMON.FS::TEST.TXT#1","scoped lookup");
            require(assets.text(assets.resolve("COMMON.FS","test.txt").id)=="native","typed UTF-8 resource");
            rejects([&]{assets.resolve("MISSING.FS","TEST.TXT");},"missing scope accepted");
            rejects([&]{assets.wave("DATA/COMMON.FS::TEST.TXT#1");},"wrong media type accepted");
            auto language=decode_language_table(parse(R"({"schema":"spellcross-language-table","version":1,"language":"cz","entries":[{"id":0,"text":"A"},{"id":1,"text":"B"}]})"));
            require(language.entries==std::vector<std::string>({"A","B"}),"language table ordering");
            rejects([]{decode_language_table(parse(R"({"schema":"spellcross-language-table","version":1,"language":"en","entries":[{"id":1,"text":"A"}]})"));},"unordered language table accepted");
            IndexedFrame glyph;glyph.width=2;glyph.height=2;glyph.indices={0,1,1,0};
            require(font_rows(glyph)==std::vector<std::vector<std::uint8_t>>({{0,1},{1,0}}),"font planes");
            glyph.indices[0]=2;rejects([&]{font_rows(glyph);},"nonbinary glyph accepted");
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
        auto script=decode_script(parse(R"SC({"schema":"spellcross-script","version":1,"source_encoding":"UTF-8","records":[{"line":1,"depth":0,"kind":"block","source":"MissionData {","name":"MissionData","keyword":"missiondata","args":[]},{"line":2,"depth":1,"kind":"call","source":"MissionMap(M01_01)","name":"MissionMap","keyword":"missionmap","args":["M01_01"]},{"line":3,"depth":1,"kind":"call","source":"MissionMap(M01_02)","name":"MissionMap","keyword":"missionmap","args":["M01_02"]},{"line":4,"depth":0,"kind":"end","source":"}"}]})SC"));
        require(script.size()==4&&script[1].args[0]=="M01_01"&&script[2].args[0]=="M01_02","ordered script");
        LevelLoader level_loader;LevelData level;
        using Kind=LevelSyntaxRecord::Kind;
        require(level_loader.LoadLevelRecords("LEVEL_02.DEF",{
            {Kind::Block,1,"LevelInit {","LevelInit",{}},
            {Kind::Call,2,"Start(2)","Start",{"2"}},
            {Kind::Call,3,"Start(3)","Start",{"3"}},
            {Kind::Call,4,"Territory(2,none,m02_01,mus00)","Territory",{"2","none","m02_01","mus00"}},
            {Kind::Call,5,"DefineStrategicPoints(2,30,4)","DefineStrategicPoints",{"2","30","4"}},
            {Kind::End,6,"}","",{}},
            {Kind::Block,7,"Mission(M02_01A) {","Mission",{"M02_01A"}},
            {Kind::Call,8,"Time(6)","Time",{"6"}},
            {Kind::End,9,"}","",{}}
        },level),"engine level syntax");
        require(level.start_territory==2&&level.territories.size()==1&&level.territories[0].strategic_points_total==30&&level.territories[0].timeout_turns==6&&level.next_level_def=="LEVEL_03.DEF","shared strategic gameplay interpretation");
        auto map_json=parse(R"({"schema":"spellcross-tactical-map","version":1,"width":2,"height":1,"terrain":"T11","ground":{"entry":"ground.u16le","sprites":["PLA00"],"index_mask":4095,"elevation_shift":12},"objects":{"entry":"objects.u8x2","sprites":["TREE"],"zero_means_empty":true,"index_base":1},"anm":{"resources":["A"],"placements":[{"resource":0,"frame_offset":1,"frame_limit":3,"x":1,"y":0}]},"pnm":{"resources":["P"],"placements":[{"resource":0,"frame_offset":0,"frame_limit":1,"x":0,"y":0,"x_offset":-3,"y_offset":4}]},"enemy_counterattack_posts":[0],"player_counterattack_posts":[1],"loop_sounds":{"resources":["S"],"placements":[{"tile":1,"resource":0}]},"random_sounds":{"resources":[],"placements":[]}})");
        auto map=decode_map(map_json,Bytes{0,32,0,0},Bytes{1,7,0,0});
        require(map.cells.size()==2&&map.cells[0].elevation==2&&map.cells[0].object==1&&map.cells[0].flags==7&&map.pnm.placements[0].x_offset==-3&&map.loop_sounds.placements[0].tile==1&&map.player_posts[0]==1,"eight map layers");
        rejects([&]{decode_map(map_json,Bytes{1,0,0,0},Bytes{1,7,0,0});},"invalid map ground index accepted");
        require(crc32(Bytes({'1','2','3','4','5','6','7','8','9'}))==0xcbf43926u,"CRC implementation");
        rejects([]{parse(R"({"x":1,"x":2})");},"duplicate JSON key accepted");
        std::cout<<"Native PAK lookup and indexed graphics passed without original data\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
