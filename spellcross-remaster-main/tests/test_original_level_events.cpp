// Standalone original-script parser regression (no wxWidgets required).
// Linux: g++ -std=c++17 -Wall -Wextra -o /tmp/level_event_test
//        spellcross-remaster-main/tests/test_original_level_events.cpp
//        spellcross-remaster-main/source/level.cpp && /tmp/level_event_test
// Windows: cl /std:c++17 /EHsc /I spellcross-remaster-main/source
//          spellcross-remaster-main/tests/test_original_level_events.cpp
//          spellcross-remaster-main/source/level.cpp
#include "../source/level.h"
#include "../source/predator_blast_rules.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

int main()
{
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "spellcross_level_events_regression.def";
    {
        std::ofstream f(path, std::ios::binary);
        assert(f);
        // Representative original CZ LEVEL_03.DEF commands; only the
        // minimum needed to catch the mission event/turn state regressions.
        f << "levelINIT {\n"
             " Start(1)\n Start(3)\n"
             " Territory(1,none,m03_01,mus05)\n"
             " Territory(3,none,m03_03,mus05)\n"
             " Territory(4,m03_04a,m03_04,mus05)\n"
             " SetSpecialTerritory(4)\n"
             "}\n"
             "mISSION(M03_04A) {\n"
             " Time(5)\n"
             " EndOKMission(none)\n"
             " EndBadMission(M03_04B)\n"
             " EndOkEvent(5)\n"
             " playStartMissionDeltaAnim(LEVEL9_1.SMK)\n"
             " PlayStartMissionCANAnim(clip,135,136)\n"
             " PlayStartMissionDeltaAnim(skip,invalid)\n"
             " EndBadEvent(5)\n"
             "}\n"
             "Event(0) {\n AbsTime(-1)\n EventText(MSG0014)\n}\n"
             "Event(5) {\n ChangeMission(5,M03_05B)\n}\n"
             "Event(6) {\n Time(2)\n EventText(E03_0006)\n}\n"
             "Event(9) {\n AbsTime(9)\n Army(61,61)\n}\n"
             // LEVEL_09: two research projects gate the way to its End(13).
             "Event(13) {\n WaitForResearch(135,136)\n"
             " SetPlayersTerritory(4)\n SetResearchFlag(115)\n}\n";
    }

    LevelData level;
    std::string error;
    const bool loaded = LevelLoader().LoadLevelDef(path.string(), level, &error);
    std::filesystem::remove(path);
    assert(loaded);
    assert(level.start_territory == 1);
    assert(level.territories.size() == 3);
    assert(!level.territories[0].is_special);
    assert(!level.territories[1].is_special);
    assert(level.territories[2].id == 4 && level.territories[2].is_special);
    assert(level.missions.size() == 1);
    const auto& mission = level.missions.front();
    assert(mission.name == "M03_04A");
    assert(mission.time_limit == 5);
    assert(mission.end_ok_event == 5);
    assert(mission.intro_videos.size() == 2);
    assert(mission.intro_videos[0].video == "LEVEL9_1.SMK");
    assert(mission.intro_videos[0].research_prerequisites.empty());
    assert(mission.intro_videos[1].video == "clip.CAN");
    assert((mission.intro_videos[1].research_prerequisites == std::vector<int>{135, 136}));
    assert(level.unknown_lines.size() == 1);
    assert(mission.end_bad_event == 5);
    assert(mission.end_bad_mission == "M03_04B");

    assert(level.events.size() == 5);
    assert(level.events[0].abs_time && level.events[0].time_value == -1);
    assert(!level.events[1].abs_time && level.events[1].time_value == -1);
    assert(!level.events[2].abs_time && level.events[2].time_value == 2);
    assert(level.events[3].abs_time && level.events[3].time_value == 9);
    assert(level.events[3].armies.size() == 1);
    assert((level.events[4].wait_for_research == std::vector<int>{135, 136}));
    assert(level.events[4].set_player_territory == 4);
    assert((level.events[4].research_flags == std::vector<int>{115}));
    // Predator retail action 13: explicit attack values, ground-only radius,
    // no ordinary-weapon casualty scaling or wounded conversion.
    using namespace spellcross_tactical::predator_blast;
    assert(Eligible(0x20, true, true, false, 1.414, 1));
    assert(!Eligible(0x04, true, true, false, 1.0, 1));
    assert(!Eligible(0x20, false, true, false, 1.0, 1));
    assert(!Eligible(0x20, true, false, false, 1.0, 1));
    assert(!Eligible(0x20, true, true, true, 1.0, 1));
    assert(!Eligible(0x20, true, true, false, 2.0, 1));
    assert(Attack(0x20, 26, 18) == 26);
    assert(Attack(0x10, 26, 18) == 18);
    assert(HitChance(18, 18) == 80);
    assert(HitChance(26, 18) == 100);
    assert(HitChance(18, 40) == 20);
    assert(Lethality(18, 18, 0, 0) == 16);
    assert(Lethality(26, 20, 0, 0) == 31);
    assert(Lethality(18, 40, 0, 0) == 4);
    auto damage = Apply(1, 1, 200, 30, false);
    assert(damage.active == 1 && damage.remainder == 500);
    damage = Apply(1, 1, 800, 20, false);
    assert(damage.active == 0 && damage.remainder == 0);
    damage = Apply(7, 10, 0, 40, false);
    assert(damage.active == 3 && damage.remainder == 0);
    damage = Apply(2, 10, 0, 80, true);
    assert(damage.active == 1);
    return 0;
}
