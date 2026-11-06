#pragma once

// The DTA grid extends beyond the authored combat area. Original retail
// mission coordinates were audited across 79 English maps: no scripted
// unit, START, ESCAPE or TARGET occurs beyond this envelope. This is a
// conservative gameplay restoration, not a byte-exact DOS AI disassembly.
// Render and editor access must continue to cover the full DTA grid.
namespace spellcross_tactical
{
    inline bool IsWithinRetailNavigationEnvelope(int x, int y,
        int width, int height, bool hasRetailPadding)
    {
        if (x < 0 || y < 0 || x >= width || y >= height) return false;
        if (!hasRetailPadding || width < 10 || height < 25) return true;
        return x >= 1 && x <= width - 3 && y >= 5 && y <= height - 15;
    }

    enum class MagotarDecision { None, Land, TakeOff };

    // Ground Magotar 47 <-> flying Magotar 48 (retail JEDNOTKY.DEF).
    // Different approach/retreat thresholds prevent oscillating forms.
    inline MagotarDecision DecideMagotarAction(int typeId, bool canAct,
        bool seesGroundTarget, double groundTargetDistance,
        bool clearLandingTile, bool hasMovementGoal, double goalDistance)
    {
        if (!canAct) return MagotarDecision::None;
        if (typeId == 48 && seesGroundTarget && clearLandingTile &&
            groundTargetDistance <= 1.5)
            return MagotarDecision::Land;
        if (typeId == 47 && hasMovementGoal && goalDistance >= 4.5 &&
            (!seesGroundTarget || groundTargetDistance > 3.5))
            return MagotarDecision::TakeOff;
        return MagotarDecision::None;
    }
}
