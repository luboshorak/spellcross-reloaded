// Verify tactical/strategic save chapter recovery without retail game data.
#include "../source/portable_level_def_path.h"
#include <cassert>

int main()
{
    using namespace spellcross_save;
    assert(CanonicalLevelDefName(R"(C:\Games\Spellcross Reloaded v0.9.6.7 ENG\temp\COMMON\LEVEL_03.DEF)") == "LEVEL_03.DEF");
    assert(CanonicalLevelDefName("/mnt/games/temp/COMMON/level_09.def") == "LEVEL_09.DEF");
    assert(CanonicalLevelDefName("C:\\stuff\\LEVEL_03.DEF.bad").empty());
    assert(CanonicalLevelDefName("C:\\stuff\\SECRET.DEF").empty());

    assert(LevelDefFromMissionToken("r0303") == "LEVEL_03.DEF");
    assert(LevelDefFromMissionToken("R0502.MIS") == "LEVEL_05.DEF");
    assert(LevelDefFromMissionToken("M03_04A.DEF") == "LEVEL_03.DEF");
    assert(LevelDefFromMissionToken("/tmp/M11_02.DTA") == "LEVEL_11.DEF");
    assert(LevelDefFromMissionToken("INVALID").empty());

    assert(ExpectedLevelDefName("C:\\A\\LEVEL_03.DEF", "r0303") == "LEVEL_03.DEF");
    assert(ExpectedLevelDefName("", "r0303") == "LEVEL_03.DEF");
    assert(PortableStem("C:\\A\\LEVEL_03.DEF") == "LEVEL_03");
    assert(PortableStem("/tmp/level_03.def") == "LEVEL_03");
}
