#include "../source/unit_health_display.h"
#include "../source/destructible_health.h"
#include <stdexcept>
#include <iostream>

int main()
{
    using spellcross_health::GetBars;
    auto check = [](int maximum, int active, int wounded, int damage, int width,
        int green, int present) {
        const auto bars = GetBars(maximum, active, wounded, damage, width);
        if (bars.active != green || bars.present != present)
            throw std::runtime_error("Unit health display regression");
    };
    check(1, 1, 0, 0, 112, 112, 112);
    check(1, 1, 0, 1, 112, 111, 112);
    check(1, 1, 0, 250, 112, 84, 112);
    check(1, 1, 0, 500, 112, 56, 112);
    check(1, 1, 0, 999, 112, 0, 112);
    check(1, 0, 0, 0, 112, 0, 0);
    check(20, 20, 0, 999, 112, 112, 112); // infantry fractional remainder isn't HP
    check(20, 10, 5, 500, 112, 56, 84);
    check(20, 10, 10, 0, 112, 56, 112);
    check(20, 0, 0, 0, 112, 0, 0);
    check(10, 5, 0, 0, 112, 56, 56); // multi-vehicle company
    check(10, 10, 0, 0, 26, 26, 26); // over-unit bar
    check(1, 1, 0, 500, 26, 13, 26);
    check(1, 1, 0, -10, 112, 112, 112);
    check(0, 1, 0, 0, 112, 0, 0);
    check(20, 100, 100, 0, 112, 112, 112);
    check(20, 20, 0, 0, 0, 0, 0);
    // #142: object percent and destruction are separate states.
    using namespace spellcross_tactical;
    if (DestructibleHealthPercent(1, 140) != 1 ||
        DestructibleHealthPercent(1, 240) != 1 ||
        DestructibleHealthPercent(0, 240) != 0 ||
        DestructibleHealthPercent(120, 240) != 50 ||
        DestructibleHealthPercent(250, 240) != 100 ||
        DestructibleHealthPercent(1, 0) != 0)
        throw std::runtime_error("Object HUD shows 0% before destruction");
    if (ApplyDestructibleDamage(1, 1) != 0 ||
        ApplyDestructibleDamage(1, 0) != 1 ||
        ApplyDestructibleDamage(1, 100) != 0 ||
        ApplyDestructibleDamage(0, 5) != 0 ||
        !DestructibleIsDead(ApplyDestructibleDamage(1, 1)))
        throw std::runtime_error("Object HP did not transition to zero correctly");
    if (!ShouldFinishDestruction(0, false, false) ||
        ShouldFinishDestruction(0, false, true) ||
        !ShouldFinishDestruction(0, true, true) ||
        ShouldFinishDestruction(1, false, false))
        throw std::runtime_error("Repeated object destruction resurrects finished sprite");
    std::cout << "PASS: single-member fractional HP, infantry/vehicle counts, wounded/dead and bounds\n";
}
