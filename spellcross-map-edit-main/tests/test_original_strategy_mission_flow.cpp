// The exact transitions below follow retail English LEVEL_03.DEF:
//
 // Territory(4,m03_04a,m03_04,mus05)
 // Mission(M03_04A) { Time(5) EndBadMission(M03_04B) }
 // Mission(M03_04B) { EndBadMission(none) }
 // Territory(5,m03_05a,m03_05,mus05)
 // Event(5) { ChangeMission(5,M03_05B) }
 // Mission(M03_05B) { Time(4) EndBadMission(M03_05C) }
 // Territory(9,m03_09a,m03_09,mus05)
 // Mission(M03_09A) { Time(12) EndBadMission(M03_09C) }
//
// Regression: before the fix, with no battle launches, the original
// intro mission ALWAYS overrode EndBadMission. This made Time(n) repeat
// forever rather than remaining on the terminal B/C mission.
#include "../source/strategic_mission_flow.h"
#include <cassert>
#include <string>
#include <unordered_map>

struct MissionDef {
    int time;
    const char* fail;
};

struct Territory {
    std::string base;
    std::string intro;
    std::string current;
    int launches = 0;
    int deadline = -1;
    int timeouts = 0;
};

static const std::unordered_map<std::string, MissionDef> chapter3{
    {"m03_04a",{5,"m03_04b"}}, {"m03_04b",{0,""}},
    {"m03_05a",{0,""}}, {"m03_05b",{4,"m03_05c"}},
    {"m03_05c",{0,""}},
    {"m03_09a",{12,"m03_09c"}}, {"m03_09c",{0,""}}
};

std::string Token(const Territory& t)
{
    return spellcross_strategy::ResolveActiveMissionToken(
        t.base,t.intro,t.current,t.launches);
}

// Equivalent stage selection/deadline lifecycle to the production
// CheckTimeouts() path, with scripted events explicitly represented.
void Tick(Territory& t, int turn)
{
    const auto def = chapter3.find(Token(t));
    assert(def != chapter3.end());
    if (def->second.time <= 0)
    {
        t.deadline = -1;
        return;
    }
    if (t.deadline < 0)
    {
        t.deadline = turn + def->second.time;
        return;
    }
    if (turn >= t.deadline)
    {
        ++t.timeouts;
        t.current = def->second.fail;
        t.deadline = -1;
    }
}

int main()
{
    // Mission 4 starts with A despite initial base M03_04.
    Territory sector4{"m03_04","m03_04a","m03_04"};
    assert(Token(sector4) == "m03_04a");
    for (int turn=1;turn<=35;++turn)
        Tick(sector4,turn);
    assert(sector4.timeouts == 1);
    assert(Token(sector4) == "m03_04b");
    assert(sector4.current == "m03_04b");

    // Turn 1 + Time(12) -> successor C, without restarting
    // A on an unplayed territory.
    Territory sector9{"m03_09","m03_09a","m03_09"};
    for (int turn=1;turn<=35;++turn)
        Tick(sector9,turn);
    assert(sector9.timeouts == 1);
    assert(Token(sector9) == "m03_09c");

    // Event(5) changes an as-yet-unplayed sector 5 to B with Time(4).
    // It must never fall back to its initial A on subsequent turns.
    Territory sector5{"m03_05","m03_05a","m03_05"};
    sector5.current = "m03_05b"; // Event(5) ChangeMission(5,M03_05B)
    assert(Token(sector5) == "m03_05b");
    for(int turn=16;turn<=35;++turn)
        Tick(sector5,turn);
    assert(sector5.timeouts == 1);
    assert(Token(sector5) == "m03_05c");

    // Even on an imported original save, explicit C always overrides A.
    Territory imported{"m03_09","m03_09a","m03_09c",0};
    assert(Token(imported) == "m03_09c");

    // Original first attempt remains intact for legacy base-token saves.
    Territory fresh{"m03_04","m03_04a","m03_04",0};
    assert(Token(fresh) == "m03_04a");
    fresh.launches = 1;
    assert(Token(fresh) == "m03_04");
    return 0;
}
