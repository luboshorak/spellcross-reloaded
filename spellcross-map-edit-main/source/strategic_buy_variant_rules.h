#pragma once

// Retail English/Czech JEDNOTKY.DEF uses the same type IDs. The genuine
// Alliance strategic roster lives in [0, 36]. Higher IDs are Other Side,
// scripted/scenario-only actors and temporary tactical transformations.
//
// Three Allied transform pairs confirmed from original special-action data:
//   7  <-> 8:  Mobile Infantry, ground <-> flying
//   17 <-> 73: Destructor, mobile <-> fortified
//   35 <-> 36: UDES, turret hidden <-> turret deployed
//
// Offer only the normal purchased configurations (7, 17, 36).
// Types 8 and 35 must still exist in JEDNOTKY.DEF for battle transformations.
// Type 73, although it has the same category and buy-id as 17 in units.json,
// is beyond the normal roster and must never enter the purchase/re-arm lists.
// Enemy Magotar 47 <-> 48 is likewise excluded as an enemy unit.
//
// Do not apply this rule to tactical loading, saves, mission events, or the
// unit definition database: both forms are needed in those contexts.
namespace spellcross_strategy
{
    inline bool IsStrategicPurchasableUnitType(int unitId)
    {
        return unitId >= 0 && unitId <= 36 && unitId != 8 && unitId != 35;
    }
}
