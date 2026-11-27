#pragma once

#include "fs_archive.h"
#include "spell_sound.h"
#include "spell_font.h"
#include <string>

#include "spell_language.h"

class SpellTextChunk
{
public:
    std::wstring text;
    int pos_x;
    int pos_y;
    int size_x;

    SpellTextChunk(int xx, int yy, int xs, wstring str) {text = str; pos_x = xx; pos_y = yy; size_x = xs;};
};

typedef vector<SpellTextChunk> SpellTextChunks;

class SpellTextRec
{
public:
    std::string name;
    std::string raw_text;
    std::wstring text;
    SpellLang lang;    
    SpellSample *audio;

    SpellTextRec(std::string str,SpellLang lang,const char* name=NULL,SpellSample *audio=NULL);
    SpellTextRec(std::wstring display,SpellLang lang,const char* name,SpellSample* audio=nullptr);
    SpellTextChunks WordWrap(SpellFont *font, int x_limit);
};


namespace sc::native_pak { class Provider; }

class SpellTexts
{
private:

    SpellLang lang;
    vector<SpellTextRec*> list;

public:
    
    SpellTexts(FSarchive *fs,SpellLang lang,SpellSounds *sounds=NULL);
    SpellTexts(sc::native_pak::Provider& provider,SpellLang lang,SpellSounds* sounds=nullptr);
    ~SpellTexts();
    SpellTextRec* GetText(int index);
    SpellTextRec* GetText(const char* name);
    SpellTextRec *GetText(string &name);
    vector<SpellTextRec*> &GetTexts();
    int GetTextId(SpellTextRec* text);
};

