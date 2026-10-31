#include "../source/spell_language.h"
#include "../source/spell_font.h"
#include "../source/spell_texts.h"
#include "../source/other.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>

static void Check(bool ok, const char* message)
{
    if (!ok) throw std::runtime_error(message);
}

static std::map<std::string, std::vector<uint8_t>> ReadArchive(const char* path)
{
    std::ifstream file(path, std::ios::binary);
    Check(bool(file), "Cannot open test archive");
    std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(file), {}};
    const auto u32 = [&](size_t offset) {
        Check(offset + 4 <= bytes.size(), "Truncated archive index");
        uint32_t value;
        std::memcpy(&value, bytes.data() + offset, 4);
        return value;
    };
    const auto count = u32(0);
    Check(count <= (bytes.size() - 4) / 21, "Invalid archive count");
    std::map<std::string, std::vector<uint8_t>> result;
    for (size_t i = 0; i < count; ++i)
    {
        const auto record = 4 + i * 21;
        const char* name = reinterpret_cast<const char*>(bytes.data() + record);
        const auto length = std::find(name, name + 13, '\0') - name;
        const size_t offset = u32(record + 13), size = u32(record + 17);
        Check(offset <= bytes.size() && size <= bytes.size() - offset, "Invalid archive bounds");
        result.emplace(std::string(name, length),
            std::vector<uint8_t>(bytes.begin() + offset, bytes.begin() + offset + size));
    }
    return result;
}

int main(int argc, char** argv)
{
    try
    {
        // Exhaust all resource-presence combinations and both existing DEF layouts.
        for (int english = 0; english < 2; ++english)
            for (int mask = 0; mask < 8; ++mask)
            {
                const auto selected = SelectSpellLanguage(english != 0,
                    (mask & 1) != 0, (mask & 2) != 0, (mask & 4) != 0);
                const auto legacy = english ? SpellLang::ENG : SpellLang::CZE;
                Check(selected == (mask == 4 ? SpellLang::POL : legacy),
                    "CZ/ENG or mixed-archive selection changed");
            }
        Check(std::string(SpellMainFontResource(SpellLang::CZE)) == "FONT_001.FNT", "CZ font changed");
        Check(std::string(SpellMainFontResource(SpellLang::ENG)) == "FONT_001.FNT", "EN font changed");
        Check(std::string(SpellMainFontResource(SpellLang::POL)) == "FONT_PL.FNT", "PL font incorrect");
        Check(SpellTextRec("English text", SpellLang::ENG).text == L"English text", "EN text regression");
        Check(SpellTextRec("Cesky text", SpellLang::CZE).text == L"Cesky text", "CZ text regression");
        Check(SpellTextRec("\x8C" "ciana", SpellLang::POL).text == L"\u015Aciana", "PL text decoding failed");
        for (int i = 1; i < argc; ++i)
        {
            auto archive = ReadArchive(argv[i]);
            auto& definitions = archive.at("JEDNOTKY.DEF");
            const auto language = SelectSpellLanguage(definitions.size() % 207 == 0,
                archive.count("UNITS.CZ") != 0, archive.count("UNITS.ENG") != 0,
                archive.count("UNITS.PL") != 0);
            const auto resource = SpellMainFontResource(language);
            auto& bytes = archive.at(resource);
            SpellFont font(bytes.data(), static_cast<int>(bytes.size()));
            Check(font.GetHeight() > 0 && font.GetSymbolWidth(L'A') > 0, "Retail font failed to decode");
            Check(archive.count(std::string("UNITS") + SpellLanguageExtension(language)) != 0,
                "Selected unit-name table missing");
            std::cout << "PASS: " << argv[i] << " -> " << SpellLanguageExtension(language)
                << ", " << resource << ", decoded font height " << font.GetHeight() << '\n';
        }
        std::cout << "PASS: all 16 edition-selection cases and CZ/ENG/PL text paths.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
