#include "../source/tactical_background_action_rules.h"
#include <cassert>

int main()
{
    using namespace spellcross_tactical;
    PendingUnitAnimation a;
    a.alive = true; a.attacking = true; a.has_unit_target = true;
    assert(ShouldAdvanceBackgroundAnimation(a)); // MLRS projectile
    a.has_unit_target = false;
    assert(!ShouldAdvanceBackgroundAnimation(a)); // HIT-state victim
    a.has_object_target = true;
    assert(ShouldAdvanceBackgroundAnimation(a)); // scenery projectile
    a.attacking = false; a.has_object_target = false; a.acting = true;
    assert(ShouldAdvanceBackgroundAnimation(a)); // mobile infantry takeoff
    a.acting = false; a.moving = true;
    assert(ShouldAdvanceBackgroundAnimation(a)); // deselected mover
    a.enemy = true;
    assert(!ShouldAdvanceBackgroundAnimation(a)); // enemy phase owns AI
    a.enemy = false; a.alive = false;
    assert(!ShouldAdvanceBackgroundAnimation(a));
    assert(WaitForGroupMemberActions(true, false, true));
    assert(!WaitForGroupMemberActions(true, true, true));
    assert(!WaitForGroupMemberActions(true, false, false));
    return 0;
}
