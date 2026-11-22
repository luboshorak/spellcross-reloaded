// Independent regression for shared-side reaction contact (no wxWidgets).
#include "../source/tactical_reaction_fire_rules.h"
#include <cassert>
#include <vector>

struct Observer
{
    bool is_enemy = false;
    bool dead = false;
    int unit_present = 1;
    int* unit = &unit_present;
    bool old_sight = false;
    bool new_sight = false;
    bool isDead() const { return dead; }
};

int main()
{
    using spellcross_tactical::SideHasSight;
    using spellcross_tactical::EntersSideSight;

    Observer artillery; // Eligible shooter, no own direct sight.
    Observer scout;
    scout.new_sight = true; // An allied forward observer spots the mover.
    Observer enemy;
    enemy.is_enemy = true;
    enemy.old_sight = true;
    enemy.new_sight = true;
    std::vector<Observer*> observers{&artillery, &scout, &enemy};
    auto new_sight = [](Observer* o) { return o->new_sight; };
    auto old_sight = [](Observer* o) { return o->old_sight; };

    assert(!artillery.new_sight);
    assert(SideHasSight(observers, false, new_sight));
    assert(!SideHasSight(observers, false, old_sight));
    assert(EntersSideSight(
        SideHasSight(observers, false, new_sight),
        SideHasSight(observers, false, old_sight)));

    // Sight of a unit on the wrong side must not enable a reaction.
    scout.new_sight = false;
    assert(!SideHasSight(observers, false, new_sight));
    assert(SideHasSight(observers, true, new_sight));

    // A mover already visible to the defending side is not a new contact,
    // even when an additional member first gains direct sight on this step.
    scout.new_sight = true;
    artillery.old_sight = true;
    assert(!EntersSideSight(
        SideHasSight(observers, false, new_sight),
        SideHasSight(observers, false, old_sight)));

    // Dead or invalid observers cannot supply team sight.
    artillery.old_sight = false;
    scout.dead = true;
    assert(!SideHasSight(observers, false, new_sight));
    scout.dead = false;
    scout.unit = nullptr;
    assert(!SideHasSight(observers, false, new_sight));

    // Existing teleport contract: unknown previous tile means no old sight.
    scout.unit = &scout.unit_present;
    assert(EntersSideSight(SideHasSight(observers, false, new_sight), false));
    assert(!EntersSideSight(false, false));
    return 0;
}
