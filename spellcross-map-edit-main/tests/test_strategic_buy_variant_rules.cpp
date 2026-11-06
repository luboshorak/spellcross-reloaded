#include "../source/strategic_buy_variant_rules.h"
#include <cassert>
#include <initializer_list>

int main()
{
    using spellcross_strategy::IsStrategicPurchasableUnitType;

    // Standard retail Alliance units, including distinct upgrade/research
    // vehicles that only look similar, remain available.
    for (int id : { 0, 1, 6, 7, 10, 12, 13, 15, 16, 17, 21, 24,
                    29, 30, 31, 32, 34, 36 })
        assert(IsStrategicPurchasableUnitType(id));

    // Every alternate state in the Alliance transform pairs is hidden.
    assert(!IsStrategicPurchasableUnitType(8));  // flying mobile infantry
    assert(!IsStrategicPurchasableUnitType(35)); // folded UDES turret
    assert(!IsStrategicPurchasableUnitType(73)); // fortified Destructor

    // Enemy Magotar (47/48), Renegades, convoy and scripted bosses never
    // appear in player shop even if raw unit data has a nonzero cost.
    for (int id : { -1, 37, 47, 48, 74, 75, 79, 80, 86, 87 })
        assert(!IsStrategicPurchasableUnitType(id));

    // Exhaustively verify all 87 original JEDNOTKY.DEF identifiers:
    // 0..36 except the airborne/folded tactical forms.
    for (int id = 0; id < 87; ++id)
        assert(IsStrategicPurchasableUnitType(id) ==
               (id <= 36 && id != 8 && id != 35));
}
