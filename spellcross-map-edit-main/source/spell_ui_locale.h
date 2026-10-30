#pragma once

// Localization of Reloaded-authored UI only. Retail mission texts remain in TEXTS.FS.
// UTF-8 .lang catalogs live in <runtime-root>/locales and can be extended without
// recompiling. Keys are stable identifiers, never translated source strings.
#include <wx/string.h>
#include <wx/stdpaths.h>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace spellcross_ui_locale
{
    using Dictionary = std::unordered_map<std::string, wxString>;

    struct Catalog
    {
        std::string language = "en";
        bool loaded = false;
        Dictionary english;
        Dictionary selected;
    };

    inline Catalog& State()
    {
        static Catalog state;
        return state;
    }

    inline std::string NormalizeLanguage(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        // Both the retail ISO names and BCP-47 region tags select the base language.
        const auto separator = value.find_first_of("-_");
        if (separator != std::string::npos)
            value.resize(separator);
        if (value == "cz") value = "cs";
        if (value.empty()) value = "en";
        return value;
    }

    inline void ReadCatalog(const std::filesystem::path& file, Dictionary& dictionary)
    {
        std::ifstream stream(file, std::ios::binary);
        if (!stream) return;
        std::string line;
        bool first = true;
        while (std::getline(stream, line))
        {
            if (first && line.compare(0, 3, "\xEF\xBB\xBF") == 0)
                line.erase(0, 3);
            first = false;
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            const auto begin = line.find_first_not_of(" \t");
            if (begin == std::string::npos || line[begin] == '#' || line[begin] == ';')
                continue;
            const auto delimiter = line.find('=', begin);
            if (delimiter == std::string::npos || delimiter <= begin) continue;
            const auto keyEnd = line.find_last_not_of(" \t", delimiter - 1);
            if (keyEnd == std::string::npos || keyEnd < begin) continue;
            const std::string key = line.substr(begin, keyEnd - begin + 1);
            // Preserve spaces in values; only strip optional indentation after '='.
            const auto valueStart = line.find_first_not_of(" \t", delimiter + 1);
            const std::string utf8 = valueStart == std::string::npos
                ? std::string() : line.substr(valueStart);
            if (utf8.empty()) continue; // Empty translation means: use fallback.
            const wxString translated = wxString::FromUTF8(utf8.c_str());
            if (!translated.empty())
                dictionary[key] = translated;
        }
    }

    inline std::vector<std::filesystem::path> CatalogDirectories()
    {
        namespace fs = std::filesystem;
        std::vector<fs::path> paths;
        const fs::path exe(wxStandardPaths::Get().GetExecutablePath().ToStdWstring());
        if (!exe.empty())
        {
            paths.push_back(exe.parent_path() / "locales");
            paths.push_back(exe.parent_path().parent_path() / "locales");
        }
        std::error_code ec;
        const fs::path cwd = fs::current_path(ec);
        if (!ec)
        {
            paths.push_back(cwd / "locales");
            paths.push_back(cwd / "spellcross-map-edit-main" / "locales");
        }
        return paths;
    }

    inline void LoadCatalogs()
    {
        Catalog& state = State();
        state.english.clear();
        state.selected.clear();
        for (const auto& dir : CatalogDirectories())
        {
            std::error_code ec;
            if (!std::filesystem::is_regular_file(dir / "en.lang", ec))
                continue;
            ReadCatalog(dir / "en.lang", state.english);
            if (state.language != "en")
                ReadCatalog(dir / (state.language + ".lang"), state.selected);
            break; // Never mix catalogs from different installations.
        }
        state.loaded = true;
    }

    inline void SetLanguage(const std::string& requestedLanguage)
    {
        Catalog& state = State();
        const auto normalized = NormalizeLanguage(requestedLanguage);
        if (state.loaded && normalized == state.language) return;
        state.language = normalized;
        state.loaded = false;
        LoadCatalogs();
    }

    inline void SetLanguageFromEdition(bool englishRetailEdition)
    {
        SetLanguage(englishRetailEdition ? "en" : "cs");
    }

    inline std::string GetLanguage()
    {
        return State().language;
    }

    inline wxString Tr(const char* key, const char* englishFallback)
    {
        Catalog& state = State();
        if (!state.loaded) LoadCatalogs();
        if (key)
        {
            if (auto it = state.selected.find(key); it != state.selected.end())
                return it->second;
            if (auto it = state.english.find(key); it != state.english.end())
                return it->second;
        }
        return wxString::FromUTF8(englishFallback ? englishFallback : "");
    }
}
