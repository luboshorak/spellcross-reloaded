// Portable integration check: real bundled ZIP importer and strategic parser.
#include "../source/demo_import.h"
#include "../source/spell_demo_profile.h"
#include "../source/level.h"
#include <iostream>
#include <stdexcept>
static void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
int main(int argc, char** argv) {
    if (argc != 3) return 2;
    try {
        namespace fs = std::filesystem;
        std::string error; fs::path data, second;
        Check(spellcross_demo::ImportPackage(argv[1], argv[2], data, error), error.c_str());
        Check(spellcross_demo::ImportPackage(argv[1], argv[2], second, error), error.c_str());
        Check(data != second && fs::exists(data / "COMMON.FS"), "Import replaced previous installation");
        for (const auto* path : {L"../bad", L"/bad", L"C:/bad", L"DATA/../../bad", L"DATA\\bad"})
            Check(!spellcross_demo::SafePackagePath(path), "Unsafe path accepted");
        for (bool demo : {false, true}) {
            Check(spellcross_demo::RequiredArchive(false,"PUST.FS",demo) == !demo, "Wrong terrain requirement");
            Check(spellcross_demo::RequiredArchive(false,"COMMON.FS",demo), "COMMON must be required");
        }
        std::ifstream input(data / "COMMON.FS", std::ios::binary);
        std::uint32_t count; input.read(reinterpret_cast<char*>(&count),4);
        fs::path levelPath = data / "LEVEL_02.DEF";
        for (std::uint32_t i=0; i<count; ++i) {
            char name[14]={}; std::uint32_t offset, size;
            input.read(name,13); input.read(reinterpret_cast<char*>(&offset),4); input.read(reinterpret_cast<char*>(&size),4);
            if (std::string(name) != "LEVEL_02.DEF") continue;
            input.seekg(offset); std::string content(size,'\0'); input.read(content.data(),size);
            std::ofstream output(levelPath,std::ios::binary);output.write(content.data(),size);break;
        }
        LevelData level; LevelLoader loader;
        Check(loader.LoadLevelDef(levelPath.string(),level,&error), error.c_str());
        Check(level.missions.size()==2,"Expected two original demo missions");
        std::cout << "DEMO_PACKAGE_OK missions=" << level.missions.size() << "\n";
    } catch (const std::exception& e) { std::cerr<<e.what()<<"\n"; return 1; }
}
