#include "../source/tactical_navigation_rules.h"
#include <cassert>
int main()
{
    using spellcross_tactical::IsWithinRetailNavigationEnvelope;
    using spellcross_tactical::DecideMagotarAction;
    using D = spellcross_tactical::MagotarDecision;
    // Original EN M06_04A has authored START at (1,78): on the DOS
    // traversal mask this column is blocked. SpellMap's explicit START
    // exception remains necessary so scripted deployment stays functional.
    assert(!IsWithinRetailNavigationEnvelope(1,78,56,104,true));
    assert(IsWithinRetailNavigationEnvelope(2,78,56,104,true));
    assert(IsWithinRetailNavigationEnvelope(52,85,56,104,true));
    assert(!IsWithinRetailNavigationEnvelope(0,78,56,104,true));
    assert(!IsWithinRetailNavigationEnvelope(55,78,56,104,true));
    assert(!IsWithinRetailNavigationEnvelope(53,78,56,104,true));
    assert(!IsWithinRetailNavigationEnvelope(20,103,56,104,true));
    assert(!IsWithinRetailNavigationEnvelope(20,90,56,104,true));
    assert(!IsWithinRetailNavigationEnvelope(20,0,56,104,true));
    // Last original M04_04 start row and M01_01 escape near right border.
    assert(IsWithinRetailNavigationEnvelope(24,74,36,90,true));
    // Authored objectives on the right border can be occupied but should
    // not open the entire filler strip for pathfinding.
    assert(!IsWithinRetailNavigationEnvelope(97,25,100,84,true));
    assert(IsWithinRetailNavigationEnvelope(96,25,100,84,true));
    assert(IsWithinRetailNavigationEnvelope(0,0,56,104,false));
    assert(IsWithinRetailNavigationEnvelope(9,9,10,10,true));

    assert(DecideMagotarAction(48,true,true,1.0,true,true,2.0)==D::Land);
    assert(DecideMagotarAction(48,true,true,1.4,true,true,2.0)==D::Land);
    assert(DecideMagotarAction(48,true,true,1.4,false,true,2.0)==D::None);
    assert(DecideMagotarAction(48,false,true,1.0,true,true,2.0)==D::None);
    assert(DecideMagotarAction(48,true,true,2.0,true,true,2.0)==D::None);
    assert(DecideMagotarAction(47,true,true,1.0,true,true,8.0)==D::None);
    assert(DecideMagotarAction(47,true,true,5.0,true,true,8.0)==D::TakeOff);
    assert(DecideMagotarAction(47,true,false,0.0,true,true,8.0)==D::TakeOff);
    assert(DecideMagotarAction(47,true,false,0.0,true,false,8.0)==D::None);
    assert(DecideMagotarAction(47,false,false,0.0,true,true,8.0)==D::None);
    assert(DecideMagotarAction(7,true,false,0.0,true,true,8.0)==D::None);
    assert(DecideMagotarAction(8,true,true,1.0,true,true,2.0)==D::None);
}
