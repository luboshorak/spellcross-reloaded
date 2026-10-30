// Regression cases grounded in original English BIG_MAP.SAV and LEVEL_XX.DEF.
// Deliberately self-contained: no GUI or proprietary retail data at CI time.
#include "../source/strategic_countdown_rules.h"
#include <cassert>

int main()
{
    using spellcross_strategy::PauseDeadlineForInactiveTurn;

    // Retail chapter 05: inaccessible territory 6, M05_06A Time(10).
    // Two independent original BIG_MAP.SAV snapshots at turns 6 and 9
    // both record remain_time=10 and ownership flags=0x00.
    int deadline = 6 + 10;
    for (int turn = 6; turn < 9; ++turn)
        deadline = PauseDeadlineForInactiveTurn(deadline, false);
    assert(deadline - 9 == 10);

    // Once the territory becomes available the deadline advances normally.
    deadline = PauseDeadlineForInactiveTurn(deadline, true);
    assert(deadline - 10 == 9);

    // Original LEVEL_03.DEF: M03_09A Time(12) can remain inactive.
    deadline = 3 + 12;
    for (int turn = 3; turn < 22; ++turn)
        deadline = PauseDeadlineForInactiveTurn(deadline, false);
    assert(deadline - 22 == 12);
    assert(PauseDeadlineForInactiveTurn(deadline, true) - 23 == 11);

    // Sentinel for an already resolved/expired timer is never revived.
    assert(PauseDeadlineForInactiveTurn(-1, false) == -1);
    assert(PauseDeadlineForInactiveTurn(0, false) == 0);
    return 0;
}
