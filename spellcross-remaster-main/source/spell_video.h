#pragma once

#include <string>
#include <tuple>
#include <array>
#include <filesystem>
#include <fstream>
#include "fs_archive.h"
#include "spell_sound.h"



// video resource
class SpellVideoResource
{
public:
    SpellVideoResource(std::string name,FSarchive* fs) { this->name = name; this->fs = fs; };
    std::string name;
    FSarchive* fs;
};

// list of all found spellcross video resources
namespace sc::native_pak { class Provider; }

class SpellVideoResources
{
private:

    std::vector<SpellVideoResource> vid_list;
    std::vector<FSarchive*> fs_list; // list of loaded FS archives
    
public:

    SpellVideoResources(std::vector<std::wstring> folders);
    explicit SpellVideoResources(sc::native_pak::Provider& provider);
    ~SpellVideoResources();

    std::vector<std::string> GetNames(std::string wild="*");

};


// vide file
class SpellVideo
{
private:

    enum class SpellVideoType {
        CAN = 1,
        DPK,
        DP2,
        SMK,
        NativeRGB
    };

    SpellSample audio;
    vector<vector<uint8_t>> frames;
    vector<array<uint8_t, 256 * 3>> frame_palettes;
    uint8_t pal[256][3];
    int x_size;
    int y_size;
    double fps;
    SpellVideoType type;
    std::vector<uint8_t> native_rgb_frames;
    int native_frame_count=0;

    int DecodeCAN(uint8_t* data,int len);
    int DecodeDPK(uint8_t* data,int len);
    int DecodeDP2(uint8_t* dp2,int dp2_len,uint8_t* riff,int riff_len);
    int DecodeSMK(uint8_t* data, int len);

public:       
    
    SpellVideo(FSarchive *fs, std::string name);
    SpellVideo(std::wstring path);
    SpellVideo(sc::native_pak::Provider& provider,const std::string& requested);
    ~SpellVideo();
    bool IsNativeRGB() const {return type==SpellVideoType::NativeRGB;}
    double GetNativeDuration() const;

    std::tuple<int,int> GetResolution();
    int GetFramesCount();
    int SaveFrames(std::wstring folder_path);
    uint8_t* GetFrame(int id);
    int GetFrameID(double time);
    uint8_t* GetPalette(int frame_id = -1);
    SpellSample *GetSound();
    bool isDPK() {return(type == SpellVideoType::DPK);};
    bool isCAN() { return(type == SpellVideoType::CAN); };
    bool isDP2() { return(type == SpellVideoType::DP2); };
    bool isSMK() { return(type == SpellVideoType::SMK); };


};
