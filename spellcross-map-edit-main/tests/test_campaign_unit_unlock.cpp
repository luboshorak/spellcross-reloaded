#include "../source/strategic_campaign_unlock_rules.h"
#include <cassert>
#include <set>
using namespace spellcross_strategy;

int main()
{
    // Scripted campaign flags are derived from every LEVEL_XX.DEF, not from
    // the current chapter or from a hardcoded list of base-unit IDs.
    const std::set<int> scripted = {0, 1, 2, 3, 5, 10, 11, 12, 18, 19, 20, 21};

    // Both truly unscripted base units are available without research.
    for (const int id : {25, 29})
        assert(IsUnscriptedBaseUnit(false, 0, false, true, scripted.count(id) != 0));

    // Equally Time(0)/Level(0) units introduced in a later chapter are not.
    for (const int id : {0, 1, 2, 3, 5, 10, 18, 21})
        assert(!IsUnscriptedBaseUnit(false, 0, false, true, scripted.count(id) != 0));

    // A NewUnit research dependency, nonzero research duration/level or an
    // incomplete set of campaign scripts must never grant default access.
    assert(!IsUnscriptedBaseUnit(false, 0, true, true, false));
    assert(!IsUnscriptedBaseUnit(true, 0, false, true, false));
    assert(!IsUnscriptedBaseUnit(false, 5, false, true, false));
    assert(!IsUnscriptedBaseUnit(false, 0, false, false, false));
    return 0;
}
