#include "../source/strategic_battle_recovery_rules.h"
#include <cassert>

int main()
{
    using spellcross_strategy::RecoveredStrategicHealth;
    // #155: wounded survivors recover; the dead still require recruits.
    assert(RecoveredStrategicHealth(60, 40, 100, 0, false, true) == 100);
    assert(RecoveredStrategicHealth(60, 20, 100, 0, false, true) == 80);
    assert(RecoveredStrategicHealth(60, 20, 100, 0, false, false) == 60);
    assert(RecoveredStrategicHealth(9, 1, 10, 0, false, true) == 100);
    assert(RecoveredStrategicHealth(0, 100, 100, 0, false, true) == 100);
    assert(RecoveredStrategicHealth(1, 0, 1, 250, true, true) == 75);
    assert(RecoveredStrategicHealth(1, 0, 1, 250, true, false) == 75);
    return 0;
}
