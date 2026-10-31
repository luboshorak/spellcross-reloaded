#include "../source/spellcross.h"
#include "../source/demo_import.h"
#include "../source/level.h"
#include "../source/map.h"
#include <iostream>

int wmain(int argc, wchar_t** argv)
{
    if (argc != 4) return 2; // package, isolated writable runtime directory, support data
    try
    {
        auto package = std::filesystem::absolute(argv[1]);
        auto runtime = std::filesystem::absolute(argv[2]);
        auto spec = std::filesystem::absolute(argv[3]).wstring();
        std::filesystem::create_directories(runtime);
        const auto retailRoot = std::filesystem::current_path();
        spellcross_demo::ActiveProfile() = false;
        if (spellcross_demo::RuntimeRoot() != retailRoot) return 3;
        for (bool demo : { false, true })
        {
            if (spellcross_demo::RequiredArchive(false, "COMMON.FS", demo) != true) return 4;
            if (spellcross_demo::RequiredArchive(false, "PUST.FS", demo) != !demo) return 5;
            if (spellcross_demo::RequiredArchive(true, "MUSIC.FS", demo)) return 6;
        }
        for (const auto* bad : { L"../bad", L"/bad", L"C:/bad", L"DATA/../../bad", L"DATA\\bad" })
            if (spellcross_demo::SafePackagePath(bad)) return 7;
        std::filesystem::path data;
        std::string error;
        if (!spellcross_demo::ImportPackage(package, runtime, data, error))
            throw std::runtime_error(error);
        std::filesystem::current_path(runtime);
        SpellDataFiles files;
        files.original_demo = true;
        files.data_root = data.wstring();
        files.cd_root = data.wstring();
        const auto resource = [&](const wchar_t* name) { return (data / name).wstring(); };
        files.common_fs = resource(L"COMMON.FS");
        files.terrain_t11_fs = resource(L"T11.FS");
        files.units_fsu = resource(L"UNITS.FSU");
        files.texts_fs = resource(L"TEXTS.FS");
        files.info_fs = resource(L"INFO.FS");
        files.samples_fs = resource(L"SAMPLES.FS");
        SpellData assets(files, spec,
            [](std::string s) { std::cout << "STEP " << s << std::endl; },
            [](std::string s) { std::cout << "ITEM " << s << std::endl; });
        if (!assets.IsOriginalDemo() || assets.terrain.size() != 1) return 8;
        if (spellcross_demo::RuntimeRoot() != runtime / "demo_profile") return 9;
        const auto common = spellcross_demo::RuntimeRoot() / "temp" / "COMMON";
        LevelData level;
        LevelLoader loader;
        if (!loader.LoadLevelDef((common / "LEVEL_02.DEF").string(), level, &error))
            throw std::runtime_error(error);
        if (level.missions.size() != 2) throw std::runtime_error("Unexpected demo mission count");
        for (const auto* name : { L"M02_03A.DEF", L"M02_05A.DEF", L"TUT01.DEF", L"TUT02.DEF", L"TUT03.DEF" })
        {
            auto path = (common / name).wstring();
            SpellMap map;
            if (map.Load(path, &assets)) throw std::runtime_error(map.GetLastError());
            std::cout << "DEMO_MAP_OK " << std::filesystem::path(name).string()
                << " " << map.x_size << "x" << map.y_size << std::endl;
        }
        {
            SpellMap unsupported;
            auto path = (common / L"TUT04.DEF").wstring();
            if (!unsupported.Load(path, &assets) ||
                unsupported.GetLastError().find("does not include graphics") == std::string::npos)
                throw std::runtime_error("TUT04 must report missing demo graphics");
        }
        std::cout << "DEMO_LOAD_OK units=" << assets.units->GetUnits().size()
            << " texts=" << assets.texts->GetTexts().size() << " missions=" << level.missions.size() << std::endl;
        return 0;
    }
    catch (const std::exception& error) { std::cerr << "DEMO_ERROR " << error.what() << std::endl; return 1; }
}
