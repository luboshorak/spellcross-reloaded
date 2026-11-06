#pragma once

// The retail JEDNOTKY.DEF contains TWO UDES forms: 35 is the lowered
// turret, attack-less high-defence tactical transformation, while 36
// is the normal armed UDES. Research item 35 unlocks both, but the
// buy menu must not offer the folded form as a second vehicle.
namespace spellcross_strategy
{
    inline bool IsStrategicPurchasableUnitType(int unitId)
    {
        return unitId >= 0 && unitId != 35;
    }
}
