// Standalone original-script parser regression (no wxWidgets required).
// Linux: g++ -std=c++17 -Wall -Wextra -o /tmp/level_event_test
//        spellcross-map-edit-main/tests/test_original_level_events.cpp
//        spellcross-map-edit-main/source/level.cpp && /tmp/level_event_test
// Windows: cl /std:c++17 /EHsc /I spellcross-map-edit-main/source
//          spellcross-map-edit-main/tests/test_original_level_events.cpp
//          spellcross-map-edit-main/source/level.cpp
#include "../source/level.h"
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
        f << "LevelInit {\n"
             " Start(1)\n Start(3)\n"
             " Territory(1,none,m03_01,mus05)\n"
             " Territory(3,none,m03_03,mus05)\n"
             " Territory(4,m03_04a,m03_04,mus05)\n"
             "}\n"
             "Mission(M03_04A) {\n"
             " Time(5)\n"
             " EndOKMission(none)\n"
             " EndBadMission(M03_04B)\n"
             " EndOKEvent(5)\n"
             " EndBadEvent(5)\n"
             "}\n"
             "Event(0) {\n AbsTime(-1)\n EventText(MSG0014)\n}\n"
             "Event(5) {\n ChangeMission(5,M03_05B)\n}\n"
             "Event(6) {\n Time(2)\n EventText(E03_0006)\n}\n"
             "Event(9) {\n AbsTime(9)\n Army(61,61)\n}\n";
    }

    LevelData level;
    std::string error;
    const bool loaded = LevelLoader().LoadLevelDef(path.string(), level, &error);
    std::filesystem::remove(path);
    assert(loaded);
    assert(level.start_territory == 1);
    assert(level.territories.size() == 3);
    assert(level.missions.size() == 1);
    const auto& mission = level.missions.front();
    assert(mission.name == "M03_04A");
    assert(mission.time_limit == 5);
    assert(mission.end_ok_event == 5);
    assert(mission.end_bad_event == 5);
    assert(mission.end_bad_mission == "M03_04B");

    assert(level.events.size() == 4);
    assert(level.events[0].abs_time && level.events[0].time_value == -1);
    assert(!level.events[1].abs_time && level.events[1].time_value == -1);
    assert(!level.events[2].abs_time && level.events[2].time_value == 2);
    assert(level.events[3].abs_time && level.events[3].time_value == 9);
    assert(level.events[3].armies.size() == 1);
    return 0;
}
