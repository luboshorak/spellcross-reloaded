// Original cinematic sequence parser: chapter 5/6/7/8 multi-video commands.
#include "../source/level.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

int main()
{
    const auto path = std::filesystem::temp_directory_path() /
        "spellcross_ordered_cinematics_regression.def";
    {
        std::ofstream f(path, std::ios::binary);
        assert(f);
        f << "LevelInit {\n"
             " PlayStartCANAnim(L_08.SMK)\n"
             " StartSpeaker(S_08A,S_08A.DP2)\n"
             " PlayStartCANAnim(MAG.SMK)\n"
             " PlayStartDeltaAnim(LEVEL8_EXTRA.SMK)\n"
             " PlayEndCANAnim(RUJNY_T.SMK)\n"
             " PlayEndDeltaAnim(BRANA.SMK)\n"
             " PlayEndDeltaAnim(LEVEL7_1.SMK)\n"
             " Start(1)\n"
             " Territory(1,none,m08_01,none)\n"
             "}\n";
    }
    LevelData level;
    std::string error;
    const bool ok = LevelLoader().LoadLevelDef(path.string(), level, &error);
    std::filesystem::remove(path);
    assert(ok);
    assert(level.intro_videos.size() == 3);
    assert(level.intro_videos[0] == "L_08.SMK");
    assert(level.intro_videos[1] == "MAG.SMK");
    assert(level.intro_videos[2] == "LEVEL8_EXTRA.SMK");
    assert(level.intro_video == "L_08.SMK");
    assert(level.outro_videos.size() == 3);
    assert(level.outro_videos[0] == "RUJNY_T.SMK");
    assert(level.outro_videos[1] == "BRANA.SMK");
    assert(level.outro_videos[2] == "LEVEL7_1.SMK");
    assert(level.outro_can_video == "RUJNY_T.SMK");
    assert(level.outro_video == "BRANA.SMK");
    return 0;
}
