#pragma once

// Standalone input policy: the held RMB is the retail movement overlay,
// while A/M are independent temporary keys with persistent SPACE fallback.
namespace spellcross_tactical
{
    enum class TemporaryRangeMode { None, Move, Attack };
    inline TemporaryRangeMode RequestedRangeMode(bool rightPreview,
        bool moveKey, bool attackKey, bool groupMode)
    {
        if (groupMode) return TemporaryRangeMode::None;
        if (rightPreview) return TemporaryRangeMode::Move;
        if (attackKey) return TemporaryRangeMode::Attack;
        if (moveKey) return TemporaryRangeMode::Move;
        return TemporaryRangeMode::None;
    }
}
