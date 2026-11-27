//=============================================================================
// Unsorted Spellcross data handling routines.
// 
// This code is part of Spellcross Map Editor project.
// (c) 2021, Stanislav Maslan, s.maslan@seznam.cz
// Distributed under MIT license, https://opensource.org/licenses/MIT.
//=============================================================================
#pragma once

#include "windows.h"
#include "cstdint"
#include <vector>
#include <list>
#include <string>
#include <unordered_map>
#include <memory>
#include "fs_archive.h"
#include "fsu_archive.h"
#include "sprites.h"
#include "spell_units.h"
#include "spell_font.h"
#include "spell_graphics.h"
#include "spell_sound.h"
#include "spell_midi.h"
#include "spell_texts.h"
#include "spell_video.h"
#include "strategic_original_renderer.h"
#include "native_pak/ui_cache.hpp"

namespace sc::native_pak { struct ScriptRecord; class Provider; }
struct LevelData;

//using namespace std;

class SpellDefCmd
{
	public:
		// command name string
		std::string name;		
		vector<std::string> *parameters;
		std::string comment;
		std::string full_command;

		SpellDefCmd(std::string command, std::vector<std::string> args);

		SpellDefCmd(std::string command, std::string params, std::string comment);
		~SpellDefCmd();
};

class SpellDefSection
{
private:
	vector<SpellDefCmd*> list;

public:
	SpellDefSection();
	~SpellDefSection();
		
	void Add(SpellDefCmd* cmd);
	int Size();
	SpellDefCmd* operator[](int index);
	vector<SpellDefCmd*> &GetData();
};

class SpellDEF
{
	private: 
		string data;
		struct NativeSection {
			std::string header;
			std::vector<std::pair<std::string,std::vector<std::string>>> calls;
		};
		bool native_script = false;
		std::vector<NativeSection> native_sections;

	public:
		SpellDEF(string str);
		SpellDEF(const std::vector<sc::native_pak::ScriptRecord>& records);
		SpellDEF(wstring &path);
		SpellDEF(uint8_t *data, int size);
		~SpellDEF();

		SpellDefSection *GetSection(std::string section);
};

class SpellClassFileRec
{
private:
public:
	int index;
	vector<string> head;
	vector<string> items;
};

class SpellClassFile
{
private:
public:
	vector<SpellClassFileRec> list;
	SpellClassFile(string text,string regexp_head,int reg_index_item=-1);
};


class SpellStringTableRec
{
private:
public:
	string label;
	string raw;
	wstring text;
	SpellStringTableRec(string label, string raw, wstring text);
};
class SpellStringTable
{
private:
	string raw_text;
	string lang;
public:
	SpellStringTable(FSarchive *fs, string name);
	string &GetRaw();
	
	vector<SpellStringTableRec> list;
};




struct SpellDataFiles
{
	std::wstring data_root;
	std::wstring cd_root;
	std::wstring common_fs;
	std::wstring terrain_t11_fs;
	std::wstring terrain_pust_fs;
	std::wstring terrain_devast_fs;
	std::wstring units_fsu;
	std::wstring texts_fs;
	std::wstring info_fs;
	std::wstring samples_fs; // optional; empty = silent SFX mode
	std::wstring music_fs;   // optional; empty = silent music mode
};

class SpellData
{
private:
	int GenerateSpecialTiles();
	int LoadAuxGraphics(FSarchive* fs,std::function<void(std::string)> status_item=NULL);
    int LoadNativeAuxGraphics(sc::native_pak::Provider& provider,std::function<void(std::string)> status_item=NULL);
    int FinishAuxGraphics();
    void Release();
    std::unique_ptr<sc::native_pak::Provider> native_provider;
    mutable sc::native_pak::UiCache<StrategicOriginalRenderer::IndexedAsset> native_ui_frames{64u*1024u*1024u,2048};
    mutable sc::native_pak::UiCache<std::vector<uint8_t>> native_ui_palettes{1024u*1024u,512};

	SpellLang text_language = SpellLang::CZE;
	FSarchive* common_fs = nullptr;
	FSarchive* terrain_fs = nullptr;
	// Original retail CP895 tables, grouped by COMMON.FS resource stem.
	std::unordered_map<std::string, std::vector<std::string>> original_language_tables;

public:
	FSarchive* GetCommonFS() { return common_fs; }
    bool IsNativePak() const { return bool(native_provider); }
    sc::native_pak::Provider* NativeProvider() const { return native_provider.get(); }
    // Read modern indexed image or palette/filter bytes for existing UI
    // consumers. No original archive bytes or legacy decompression.
    std::wstring NativeCampaignPath(const std::string& filename) const;
    bool LoadCampaignLevel(const std::string& path,LevelData& out,std::string* error=nullptr) const;
    bool ReadNativeText(const std::string& archive,const std::string& name,std::wstring& text) const;
    bool ReadNativeFrame(const std::string& archive,const std::string& name,
        StrategicOriginalRenderer::IndexedAsset& frame) const;
    bool ReadNativeAdjacency(const std::string& name,std::vector<uint32_t>& out) const;
    bool ReadNativeMask(const std::string& name,int& width,int& height,std::vector<uint8_t>& out) const;
    bool ReadNativeIndexed(const std::string& archive,const std::string& name,std::vector<uint8_t>& out) const;
	SpellLang GetTextLanguage() const { return text_language; }
	std::string GetOriginalText(const char* resource_stem, size_t index) const;
	// data paths
	std::wstring data_path;
	std::wstring cd_data_path;

	// terrains data array
	vector<Terrain*> terrain;
	// special tile graphics
	struct {
		Sprite select[13];
		Sprite grid[13];
		Sprite solid[13];
	} special;
	// units graphics (FSU)
	FSUarchive *units_fsu = nullptr;
	// units
	SpellUnits* units = nullptr;
	// palettes
	uint8_t map_pal[256][3]; /* map environment common pal (index: 128 - 255) */
	uint8_t strategy_pal[256][3]; /* strategic screens and VM_* resources */
	// fonts
	SpellFont* font = nullptr;
	SpellFont* font7 = nullptr;
	// generic graphics
	SpellGraphics gres;
	// last path
	wstring spell_data_root;
	// info.fs
	FSarchive* info = nullptr;
	// sound stuff
	SpellSounds* sounds = nullptr;
	// sound stuff
	SpellMIDI* midi = nullptr;
	// texts.fs
	SpellTexts* texts = nullptr;
	// L2 object class parameters
	SpellL2classes *L2_classes = nullptr;
	// unit bonuses (BONUSES.DEF)
	UnitBonuses *unit_bonuses = nullptr;
	// videos
	SpellVideoResources *videos = nullptr;


	SpellData(const SpellDataFiles& files, wstring& spec_path,std::function<void(std::string)> status_list=NULL,std::function<void(std::string)> status_item=NULL);
	SpellData(wstring& data_path,wstring& cd_data_path,wstring& spec_path,std::function<void(std::string)> status_list=NULL,std::function<void(std::string)> status_item=NULL);
    // Experimental native constructor (no retail file fallback).
    SpellData(const std::wstring& pak_directory,const std::wstring& spec_path,
        std::function<void(std::string)> status_list=nullptr,
        std::function<void(std::string)> status_item=nullptr);
	~SpellData();	
	Terrain* GetTerrain(const char* name);
	Terrain* GetTerrain(int index);
	int GetTerrainCount();
	int BuildSpriteContextOfMaps(wstring folder,string terrain_name,std::function<void(std::string)> status_cb);	
	int BuildHouseObjectsOfMaps(wstring folder,string terrain_name,std::function<void(std::string)> status_cb);
};






