// Original chapter 5-8 cinematic sequences, including Speaker DP2 commands.
#include "../source/level.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <string>
#include <vector>

static LevelData ParseLevel(const char* name, const char* levelInit)
{
    const auto path = std::filesystem::temp_directory_path() / name;
    {
        std::ofstream f(path, std::ios::binary);
        assert(f);
        f << levelInit;
    }

    LevelData level;
    std::string error;
    const bool ok = LevelLoader().LoadLevelDef(path.string(), level, &error);
    std::filesystem::remove(path);
    assert(ok);
    return level;
}

static void Expect(const std::vector<std::string>& actual,
                   std::initializer_list<const char*> expected)
{
    assert(actual.size() == expected.size());
    std::size_t i = 0;
    for (const char* item : expected)
        assert(actual[i++] == item);
}

int main()
{
    const LevelData level5 = ParseLevel(
        "spellcross_level_05_cinematics.def",
        "LevelInit {\n"
        " PlayStartCANAnim(L_05.CAN)\n"
        " PlayStartDeltaAnim(LEVEL5_1.DPK)\n"
        " ;\n"
        " PlayEndCANAnim(RUJNY_T.CAN)\n"
        " PlayEndDeltaAnim(LEVEL5_2.DPK)\n"
        "}\n");
    Expect(level5.intro_videos, {"L_05.CAN", "LEVEL5_1.DPK"});
    Expect(level5.outro_videos, {"RUJNY_T.CAN", "LEVEL5_2.DPK"});

    const LevelData level6 = ParseLevel(
        "spellcross_level_06_cinematics.def",
        "LevelInit {\n"
        " PlayStartCANAnim(L_06.CAN)\n"
        " PlayStartDeltaAnim(LEVEL6_1.DPK)\n"
        " PlayStartCANAnim(KEFTACI.CAN)\n"
        " ;\n"
        " PlayEndDeltaAnim(LEVEL6_2.DPK)\n"
        "}\n");
    Expect(level6.intro_videos, {"L_06.CAN", "LEVEL6_1.DPK", "KEFTACI.CAN"});
    Expect(level6.outro_videos, {"LEVEL6_2.DPK"});

    std::vector<std::string> transition56 = level5.outro_videos;
    transition56.insert(transition56.end(), level6.intro_videos.begin(), level6.intro_videos.end());
    Expect(transition56, {
        "RUJNY_T.CAN", "LEVEL5_2.DPK", "L_06.CAN", "LEVEL6_1.DPK", "KEFTACI.CAN"
    });

    const LevelData level7 = ParseLevel(
        "spellcross_level_07_cinematics.def",
        "LevelInit {\n"
        " PlayStartCANAnim(L_07.CAN)\n"
        " StartSpeaker(S_07A,S_07A.DP2)\n"
        " ;\n"
        " PlayEndDeltaAnim(BRANA.DPK)\n"
        " PlayEndDeltaAnim(LEVEL7_1.DPK)\n"
        " EndSpeaker(S_07B,S_07B.DP2)\n"
        "}\n");
    Expect(level7.intro_videos, {"L_07.CAN", "S_07A.DP2"});
    Expect(level7.outro_videos, {"BRANA.DPK", "LEVEL7_1.DPK", "S_07B.DP2"});

    const LevelData level8 = ParseLevel(
        "spellcross_level_08_cinematics.def",
        "LevelInit {\n"
        " PlayStartCANAnim(L_08.CAN)\n"
        " StartSpeaker(S_08A,S_08A.DP2)\n"
        " PlayStartCANAnim(MAG.CAN)\n"
        " ;\n"
        " PlayEndDeltaAnim(LEVEL8_1.DPK)\n"
        " EndSpeaker(S_08B,S_08B.DP2)\n"
        "}\n");
    Expect(level8.intro_videos, {"L_08.CAN", "S_08A.DP2", "MAG.CAN"});
    Expect(level8.outro_videos, {"LEVEL8_1.DPK", "S_08B.DP2"});

    // Scalar compatibility fields still identify the first legacy CAN/Delta.
    assert(level8.intro_video == "L_08.CAN");
    assert(level7.outro_video == "BRANA.DPK");
    return 0;
}
