// Spellcross tactical animation scheduler: one animation owner per timer tick.
// Never confuse a HIT-state victim with the unit actually firing its weapon.
#pragma once

namespace spellcross_tactical
{
    struct PendingUnitAnimation
    {
        bool alive = false;
        bool enemy = false;
        bool moving = false;
        bool attacking = false;
        bool acting = false;
        bool has_unit_target = false;
        bool has_object_target = false;
    };

    inline bool ShouldAdvanceBackgroundAnimation(const PendingUnitAnimation& a)
    {
        if (!a.alive || a.enemy)
            return false;
        // The attacker's flight/damage state is driven by its own target.
        // A victim can temporarily have attack_state=HIT but no target.
        return a.moving || a.acting ||
            (a.attacking && (a.has_unit_target || a.has_object_target));
    }

    inline bool WaitForGroupMemberActions(bool groupMoveActive,
        bool groupMoverAlreadyRunning, bool anyMemberStillActing)
    {
        return groupMoveActive && !groupMoverAlreadyRunning &&
            anyMemberStillActing;
    }
}
