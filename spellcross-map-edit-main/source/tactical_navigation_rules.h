#pragma once

// The DTA grid extends beyond the authored combat area. Original retail
// mission coordinates were audited across 79 English maps. The original
// SPELCROS.EXE EN (SHA-256 ff6b6554...) routine 0x746FF explicitly excludes
// x < 2 and x >= mapWidth - 3 by writing 0xFFFF into its traversal mask
// (0x74852..0x74868; repeated for each movement class). The vertical
// y=5..height-15 envelope remains a *provisional* data-derived safety guard,
// NOT a recovered DOS rule. Keep this PR draft until it is verified.
// Render and editor access must continue to cover the full DTA grid.
namespace spellcross_tactical
{
    inline bool IsWithinRetailNavigationEnvelope(int x, int y,
        int width, int height, bool hasRetailPadding)
    {
        if (x < 0 || y < 0 || x >= width || y >= height) return false;
        if (!hasRetailPadding || width < 10 || height < 25) return true;
        return x >= 2 && x < width - 3 && y >= 5 && y <= height - 15;
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
