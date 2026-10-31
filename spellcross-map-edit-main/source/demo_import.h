#pragma once
#include <wx/wfstream.h>
#include <wx/zipstrm.h>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <set>
#include <string>

namespace spellcross_demo
{
    // Validate the known original demo, not an incomplete retail installation.
    inline bool ValidateFolder(const std::filesystem::path& data, std::string& error)
    {
        namespace fs = std::filesystem;
        for (const auto* name : { "COMMON.FS", "T11.FS", "UNITS.FSU", "TEXTS.FS", "INFO.FS", "RESEARCH.FS" })
        {
            std::error_code ec;
            if (!fs::is_regular_file(data / name, ec) || fs::file_size(data / name, ec) == 0 || ec)
            { error = std::string("Original demo is missing ") + name; return false; }
        }
        std::ifstream file(data / "COMMON.FS", std::ios::binary);
        const auto read32 = [&]() { std::uint32_t value = 0; file.read(reinterpret_cast<char*>(&value), 4); return value; };
        const auto count = read32();
        std::error_code ec;
        const auto size = fs::file_size(data / "COMMON.FS", ec);
        if (!file || ec || size < 4 || count == 0 || count > (size - 4) / 21)
        { error = "Invalid demo COMMON.FS index."; return false; }
        std::set<std::string> names;
        for (std::uint32_t i = 0; i < count; ++i)
        {
            char name[14] = {};
            file.read(name, 13);
            const auto offset = read32(), length = read32();
            if (!file || offset > size || length > size - offset)
            { error = "Invalid demo COMMON.FS entry."; return false; }
            names.insert(name);
        }
        for (const auto* name : { "LEVEL_02.DEF", "M02_03A.DEF", "M02_05A.DEF", "TUTORIAL.DEF", "UNITS.ENG" })
            if (!names.count(name))
            { error = std::string("Not the supported original demo: missing ") + name; return false; }
        for (const auto& name : names)
            if (name.rfind("LEVEL_", 0) == 0 && fs::path(name).extension() == ".DEF" && name != "LEVEL_02.DEF")
            { error = "The selected data contains a retail campaign, not the original demo."; return false; }
        return true;
    }

    inline bool SafePackagePath(const std::wstring& name)
    {
        if (name.empty() || name.find(L'\\') != std::wstring::npos ||
            name.find(L':') != std::wstring::npos || name.find(L'\0') != std::wstring::npos)
            return false;
        const std::filesystem::path path(name);
        if (path.is_absolute() || path.has_root_path()) return false;
        for (const auto& part : path)
            if (part == L".." || part == L".") return false;
        return true;
    }

    // demo.dat is a normal ZIP containing the original demo, including README.
    // Extract into a new directory; never overwrite a working installation.
    inline bool ImportPackage(const std::filesystem::path& package,
        const std::filesystem::path& runtimeRoot, std::filesystem::path& data,
        std::string& error)
    {
        namespace fs = std::filesystem;
        data.clear();
        wxFFileInputStream input(wxString(package.wstring()));
        if (!input.IsOk()) { error = "Cannot open bundled data/demo.dat."; return false; }
        wxZipInputStream zip(input);
        std::error_code ec;
        const auto imports = runtimeRoot / "game_data" / "demo_imports";
        fs::create_directories(imports, ec);
        if (ec) { error = "Cannot create the demo import directory."; return false; }
        fs::path destination;
        for (unsigned attempt = 0; attempt < 64; ++attempt)
        {
            const auto candidate = imports / (std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" + std::to_string(attempt));
            if (fs::create_directory(candidate, ec)) { destination = candidate; break; }
            if (ec) break;
        }
        if (destination.empty()) { error = "Cannot create a new demo directory."; return false; }
        const auto fail = [&](const std::string& message) {
            error = message;
            std::error_code cleanupError;
            fs::remove_all(destination, cleanupError); // Only this call's new directory.
            return false;
        };
        std::uint64_t total = 0;
        unsigned entries = 0;
        std::array<char, 65536> buffer;
        std::set<std::wstring> seen;
        for (;;)
        {
            std::unique_ptr<wxZipEntry> entry(zip.GetNextEntry());
            if (!entry) break;
            const auto name = entry->GetName(wxPATH_UNIX).ToStdWstring();
            if (++entries > 4096 || !SafePackagePath(name) || !seen.insert(name).second)
                return fail("Invalid or duplicate path in demo.dat.");
            const auto target = destination / fs::path(name);
            if (entry->IsDir())
            {
                fs::create_directories(target, ec);
                if (ec) return fail("Cannot create a demo subdirectory.");
                continue;
            }
            fs::create_directories(target.parent_path(), ec);
            if (ec) return fail("Cannot create a demo subdirectory.");
            std::ofstream output(target, std::ios::binary);
            if (!output) return fail("Cannot write a demo file.");
            std::uint64_t fileSize = 0;
            while (zip.IsOk() && !zip.Eof())
            {
                zip.Read(buffer.data(), buffer.size());
                const auto count = zip.LastRead();
                if (!count) break;
                fileSize += count;
                total += count;
                if (fileSize > 64ull * 1024 * 1024 || total > 128ull * 1024 * 1024)
                    return fail("demo.dat exceeds the supported package size.");
                output.write(buffer.data(), static_cast<std::streamsize>(count));
                if (!output) return fail("Writing a demo file failed.");
            }
            output.close();
            if (!output || (zip.GetLastError() != wxSTREAM_NO_ERROR && zip.GetLastError() != wxSTREAM_EOF))
                return fail("demo.dat is truncated or damaged.");
        }
        if (zip.GetLastError() != wxSTREAM_EOF || entries == 0)
            return fail("demo.dat is not a complete ZIP archive.");
        const auto candidate = destination / "DATA";
        if (!ValidateFolder(candidate, error)) return fail(error);
        data = candidate;
        return true;
    }
}
