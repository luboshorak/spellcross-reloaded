#undef _HAS_STD_BYTE
#define _HAS_STD_BYTE 0

#include "spell_texts.h"
#include "native_pak/assets.hpp"
#include <memory>
#include "other.h"
#include <stdexcept>
#include <string>
#include <limits>

//using namespace std;

// basic text loader
SpellTextRec::SpellTextRec(std::string str, SpellLang lang, const char* name,SpellSample* audio)
{    
    if(name)
        this->name = name;
    this->lang = lang;    
    this->audio = audio;
    raw_text = str;
        
    // parse the string (remove control stuff and word wrapping)
    std::string buf(str.size()+1,'\0');
    char last_sym = '\0';
    char *data = buf.data();
    char *pstr = str.data();
    char *strend = &pstr[str.size()];
    while(pstr < strend)
    {
        if(*pstr == '~')
        {
            // possibly align mark - skip
            pstr++;
            continue;
        }
        // The retail text resources can contain non-printable DOS control
        // bytes (including SUB 0x1A). In the restored font they may render
        // as pictograms, e.g. a stray character after a research description.
        // Keep actual line boundaries; ignore controls, not CP895 letters.
        const unsigned char byte = static_cast<unsigned char>(*pstr);
        if ((byte < 0x20 && *pstr != '\r' && *pstr != '\n' && *pstr != '\t') ||
            byte == 0x7F)
        {
            ++pstr;
            continue;
        }        
        // Original text archives use hard display-width line breaks. Accept
        // CRLF, lone CR and lone LF as wrapping boundaries; none is part of
        // a word. Keep existing paragraph heuristics for sentence endings.
        if(*pstr == '\r' || *pstr == '\n')
        {
            if(*pstr == '\r' && &pstr[1] < strend && pstr[1] == '\n')
                ++pstr;
            if(last_sym == '.' || last_sym == '\n')
                *data++ = '\n';
            else if(data > buf.data() && data[-1] != ' ' && data[-1] != '\n')
                *data++ = ' ';
            last_sym = '\n';
            ++pstr;
            continue;
        }
        if(*pstr != ' ')
            last_sym = *pstr;
        *data++ = *pstr++;
    }
        
    // convert to unicode
    if(lang == SpellLang::ENG)
    {        
        // ENG: just copy, there should be no stuff above 127
        text = char2wstring(buf.c_str());
    }
    else if(lang == SpellLang::CZE)
    {
        // CZE: convert from weird CP895 to unicode
        text = char2wstringCP895(buf.c_str());
    }
    else if(lang == SpellLang::POL)
    {
        text = char2wstringCP1250(buf.c_str());
    }
    else if(lang == SpellLang::ITA)
    {
        text = char2wstringCP1252(buf.c_str());
    }
    else
    {
        throw runtime_error("Unknown language requested in text decoder!");
    }
}


// Native narrative payload is already display-normalized UTF-8 by the converter.
SpellTextRec::SpellTextRec(std::wstring display,SpellLang language,const char* resource,SpellSample* narration)
    : name(resource?resource:""),text(std::move(display)),lang(language),audio(narration)
{
    raw_text=wstring2stringCP895(text);
}

SpellTextChunks SpellTextRec::WordWrap(SpellFont* font,int x_limit)
{
    wchar_t *pstr = (wchar_t*)text.c_str();

    int y_step = font->GetHeight() + 1;

    SpellTextChunks lines;

    wstring chunk;
    size_t last_len = 0;
    int line_pix = 0;
    wchar_t *w_str = pstr;
    wchar_t p_sym = ' ';
    int pos_x = 0;
    int pos_y = 0;
    while(*pstr)
    {
        if(*pstr == '\n')
        {
            lines.emplace_back(0, pos_y, pos_x, chunk);
            chunk = L"";
            pos_x = 0;
            pos_y += y_step;            
        }
        else
        {
            if(*pstr != ' ' && (p_sym == ' ' || p_sym == '\n'))
            {
                // store word start
                last_len = chunk.size();
                w_str = pstr;
                line_pix = pos_x;
            }                
            pos_x += font->GetSymbolWidth(*pstr);
            if(pos_x >= x_limit)
            {
                // word won't fit, step back                
                chunk.resize(last_len);
                lines.emplace_back(0, pos_y, line_pix, chunk);
                chunk = L"";
                pos_x = 0;
                pos_y += y_step;
                pstr = w_str;                
            }
            chunk.insert(chunk.end(),*pstr);
        }
        p_sym = *pstr++;
    }
    if(!chunk.empty())
        lines.emplace_back(0,pos_y,pos_x,chunk);

    return(lines);
}


// load all text files from TEXTS.FS archive
SpellTexts::SpellTexts(FSarchive* fs, SpellLang lang,SpellSounds* sounds)
{
    this->lang = lang;

    // load all files (assuming they are all texts)
    for(auto & src : fs->GetFiles())    
    {        
        // try fetch message narration
        SpellSample *narration = NULL;
        if(sounds && src->name[0] == 'U')
            narration = sounds->GetSample(src->name.c_str());

        // try load
        std::string str(src->data.begin(), src->data.end());
        auto txt = new SpellTextRec(str,lang,src->name.c_str(),narration);
        list.push_back(txt);
    }
}

SpellTexts::SpellTexts(sc::native_pak::Provider& provider,SpellLang language,SpellSounds* sounds)
    : lang(language)
{
    using namespace sc::native_pak;
    Assets assets(provider);
    auto records=provider.archive_resources("TEXTS.FS");
    if(records.empty())fail("Missing native narrative text archive");
    std::vector<std::unique_ptr<SpellTextRec>> pending;
    pending.reserve(records.size());
    std::set<std::string> seen;
    for(auto* r:records){
        auto name=leaf(r->name);
        if(!seen.insert(name).second)fail("Duplicate native narrative name: "+name);
        auto utf8=assets.text(r->id);
        auto display=wxString::FromUTF8(utf8.data(),utf8.size());
        if(!utf8.empty()&&display.empty())fail("Invalid native narrative UTF-8");
        auto* narration=sounds&&name.starts_with("U")?sounds->GetSample(name.c_str()):nullptr;
        pending.push_back(std::make_unique<SpellTextRec>(display.ToStdWstring(),language,name.c_str(),narration));
    }
    list.reserve(pending.size());for(auto& r:pending)list.push_back(r.release());
}

// cleanup string table
SpellTexts::~SpellTexts()
{
    for(auto & txt : list)
        if(txt)
            delete txt;
    list.clear();
}

// try to get text by its name
SpellTextRec* SpellTexts::GetText(string &name)
{
    return(GetText(name.c_str()));
}
SpellTextRec* SpellTexts::GetText(const char *name)
{
    for(auto & txt : list)
        if(iequals(txt->name,name))
            return(txt);
    return(NULL);
}
// try get text by order index
SpellTextRec* SpellTexts::GetText(int index)
{
    if(index < 0 || index >= list.size())
        return(NULL);
    return(list[index]);
}

// get index of text resource in the list by record pointer
int SpellTexts::GetTextId(SpellTextRec* text)
{
    auto id = find(list.begin(), list.end(), text);
    if(id == list.end())
        return(-1);
    const auto index = id - list.begin();
    if(index > std::numeric_limits<int>::max())
        return(-1);
    return(static_cast<int>(index));
}

// get full list of texts
vector<SpellTextRec*>& SpellTexts::GetTexts()
{
    return(list);
}


