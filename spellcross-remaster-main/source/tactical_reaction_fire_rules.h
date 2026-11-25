#pragma once

namespace spellcross_tactical
{
    // Determine contact from the combined sight of the defending side, not
    // from the reacting shooter's individual sight. The weapon's legal attack
    // area is checked separately by the tactical engine.
    template<typename Units, typename SeesTile>
    bool SideHasSight(const Units& units, bool observer_is_enemy, SeesTile sees_tile)
    {
        for (auto* observer : units)
        {
            if (!observer || !observer->unit || observer->isDead())
                continue;
            if (observer->is_enemy != observer_is_enemy)
                continue;
            if (sees_tile(observer))
                return true;
        }
        return false;
    }

    // A move triggers opportunity fire only on entry into the defending
    // side's sight. An absent previous position (teleport) counts as unseen.
    inline bool EntersSideSight(bool seen_now, bool seen_before)
    {
        return seen_now && !seen_before;
    }
}
