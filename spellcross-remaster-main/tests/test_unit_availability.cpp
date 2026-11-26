#include "../source/strategic_unit_timer_rules.h"
#include <cassert>
using namespace spellcross_strategy;

int main()
{
    int pending[3] = {-1, -1, -1};
    assert(CanStartExclusiveUnitOperation(0, 0, pending));

    // Newly purchased unit: two end-turns, never one.
    int remaining = 2;
    assert(!CanStartExclusiveUnitOperation(remaining, 0, pending));
    assert(!CanQueueTechUpgrade(remaining, 0, pending, 0));
    remaining = AdvanceUnitCooldown(remaining);
    assert(remaining == 1);
    remaining = AdvanceUnitCooldown(remaining);
    assert(remaining == 0);

    // Independent upgrade categories may be queued during an upgrade lockout.
    pending[0] = 10; // Engine, two turns.
    remaining = ExtendUnitCooldown(0, 2);
    assert(CanQueueTechUpgrade(remaining, 0, pending, 1));
    assert(CanQueueTechUpgrade(remaining, 0, pending, 2));
    assert(!CanQueueTechUpgrade(remaining, 0, pending, 0));
    pending[1] = 11; // Weapon, four turns.
    remaining = ExtendUnitCooldown(remaining, 4);
    assert(remaining == 4);
    assert(!CanStartExclusiveUnitOperation(remaining, 0, pending));

    // Both installed upgrades finish at the one shared deadline.
    for (int n = 3; n >= 0; --n)
    {
        remaining = AdvanceUnitCooldown(remaining);
        assert(remaining == n);
    }
    pending[0] = pending[1] = -1;
    assert(CanStartExclusiveUnitOperation(remaining, 0, pending));

    // A rearm/recruit order excludes all additional upgrades.
    assert(!CanQueueTechUpgrade(2, 1, pending, 2));
    assert(!CanQueueTechUpgrade(2, 3, pending, 2));
    assert(CanQueueTechUpgrade(0, 0, pending, 2));
    return 0;
}
