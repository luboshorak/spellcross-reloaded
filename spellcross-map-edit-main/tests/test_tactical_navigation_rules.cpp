#include "../source/tactical_navigation_rules.h"
#include <cassert>
int main()
{
    using namespace spellcross_tactical;
    // Confirmed original LE traversal mask at 0x74852, repeated by class.
    assert(!IsWithinRetailNavigationEnvelope(1,78,56,104,true));
    assert(IsWithinRetailNavigationEnvelope(2,78,56,104,true));
    assert(IsWithinRetailNavigationEnvelope(52,85,56,104,true));
    assert(!IsWithinRetailNavigationEnvelope(53,78,56,104,true));
    assert(!IsWithinRetailNavigationEnvelope(0,78,56,104,true));
    // NO original vertical inset: original loops span whole map height.
    assert(IsWithinRetailNavigationEnvelope(20,0,56,104,true));
    assert(IsWithinRetailNavigationEnvelope(20,103,56,104,true));
    assert(IsWithinRetailNavigationEnvelope(20,90,56,104,true));
    assert(IsWithinRetailNavigationEnvelope(20,5,56,104,true));
    assert(!IsWithinRetailNavigationEnvelope(20,104,56,104,true));
    // Authored START/ESCAPE sites on x=1 or near right edge are handled
    // by SpellMap's explicit scenario overrides; new maps are unrestricted.
    assert(!IsWithinRetailNavigationEnvelope(97,25,100,84,true));
    assert(IsWithinRetailNavigationEnvelope(96,25,100,84,true));
    assert(IsWithinRetailNavigationEnvelope(0,0,56,104,false));

    // Original 0x7c9b8 zero-random hit-projection curve.
    assert(OriginalEstimatedHitChance(0,9) == 0);
    assert(OriginalEstimatedHitChance(10,10) == 16);
    assert(OriginalEstimatedHitChance(11,10) == 16);
    assert(OriginalEstimatedHitChance(24,10) == 100);
    assert(OriginalEstimatedHitChance(25,10) == 100);
    assert(OriginalEstimatedHitChance(9,10) == 16);
    assert(OriginalEstimatedHitChance(0,25) == 0);
    assert(OriginalEstimatedHitChance(1,12) == 1);
    assert(OriginalEstimatedHitChance(1,11) == 0);

    // 0x9c867: air-vs-ground ratios, zero-denominator 1e6 sentinel,
    // and strict > tie-break to preference 2.
    TacticalGroupFormEffect e{};
    assert(OriginalFormPreference(e) == 2);
    e.airUnits = 1;
    assert(OriginalFormPreference(e) == 1);
    e.groundUnits = 1;
    assert(OriginalFormPreference(e) == 2);
    e.airAttack = 70; e.airRetaliation = 20;
    e.groundAttack = 90; e.groundRetaliation = 50;
    assert(OriginalFormPreference(e) == 1); // 3.5 > 1.8
    e.airAttack = 20;
    assert(OriginalFormPreference(e) == 2); // 1.0 < 1.8
    e.airAttack = 0; e.airRetaliation = 0; e.groundAttack = 0;
    e.groundRetaliation = 0;
    assert(OriginalFormPreference(e) == 2);
    assert(OriginalMagotarAction(47,1) == MagotarDecision::TakeOff);
    assert(OriginalMagotarAction(47,2) == MagotarDecision::None);
    assert(OriginalMagotarAction(48,2) == MagotarDecision::Land);
    assert(OriginalMagotarAction(48,1) == MagotarDecision::None);
    assert(OriginalMagotarAction(7,1) == MagotarDecision::None);
    assert(OriginalMagotarAction(8,2) == MagotarDecision::None);
}
