#pragma once

// The DTA grid extends beyond the authored combat area. Original retail
// mission coordinates were audited across 79 English maps. The original
// SPELCROS.EXE EN (SHA-256 ff6b6554...) routine 0x746FF excludes
// x < 2 and x >= mapWidth - 3 by writing 0xFFFF into its traversal mask.
// The loop spans ALL y rows. There is NO fixed vertical margin in this
// function: traversability depends on the per-tile terrain mask instead.
// A separate true game-mode check keeps the map editor fully navigable.
// Render and editor access must continue to cover the full DTA grid.
namespace spellcross_tactical
{
    inline bool IsWithinRetailNavigationEnvelope(int x, int y,
        int width, int height, bool hasRetailPadding)
    {
        if (x < 0 || y < 0 || x >= width || y >= height) return false;
        if (!hasRetailPadding || width < 10 || height < 25) return true;
        return x >= 2 && x < width - 3;
    }

    // SPELCROS.EXE 0x9c867 compares the ratios of expected attack strength
    // against retaliation for the AIR and GROUND portions of a player group.
    // It returns 1 iff AIR's ratio is strictly greater; otherwise 2.
    // Each damage contribution is max(0, 0x9b6c1(attacker, defender)).
    struct TacticalGroupFormEffect
    {
        int airAttack = 0, airRetaliation = 0, groundAttack = 0,
            groundRetaliation = 0;
        int airUnits = 0, groundUnits = 0;
    };

    inline int OriginalFormPreference(const TacticalGroupFormEffect& e)
    {
        auto ratio = [](int attack, int retaliation, int count) -> float
        {
            if (retaliation > 0)
                return static_cast<float>(attack) /
                       static_cast<float>(retaliation);
            return attack > 0 || count > 0 ? 1000000.0f : 0.0f;
        };
        return ratio(e.airAttack, e.airRetaliation, e.airUnits) >
               ratio(e.groundAttack, e.groundRetaliation, e.groundUnits) ?
               1 : 2;
    }

    // 0x7c9b8 without the random +/-5 rolls (AI estimation path passes 0):
    // quadratic hit chance for effective attack minus effective defence.
    inline int OriginalEstimatedHitChance(int attack, int defence)
    {
        if (attack <= 0) return 0;
        const int delta = attack - defence;
        if (delta > 14) return 100;
        if (delta >= 0)
            return 16 + (delta * delta * 84) / 196;
        if (delta < -10) return 1;
        const int mag = -delta;
        return 16 - (mag * mag * 16) / 100;
    }

    // Original branches 0x918f7 and 0x92dbe: action 7 (ground -> air)
    // when form comparison returns 1; action 8 (air -> ground) when 2.
    // A legal landing tile and enough AP are checked separately.
    enum class MagotarDecision { None, Land, TakeOff };
    inline MagotarDecision OriginalMagotarAction(
        int typeId, int formPreference)
    {
        if (typeId == 47 && formPreference == 1)
            return MagotarDecision::TakeOff;
        if (typeId == 48 && formPreference == 2)
            return MagotarDecision::Land;
        return MagotarDecision::None;
    }
}
