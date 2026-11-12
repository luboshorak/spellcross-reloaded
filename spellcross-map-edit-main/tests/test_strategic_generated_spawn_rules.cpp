#include "../source/strategic_generated_spawn_rules.h"
#include <cassert>
#include <vector>

struct Pos { int x; int y; };
int main()
{
    auto valid = [](const Pos& p) { return p.x >= 0 && p.y >= 0; };
    const std::vector<Pos> enemy{{3,29},{3,39},{5,45},{3,51}};
    const std::vector<Pos> north{{10,9},{20,8}};
    // M08_04 has player deployment on the right/bottom; enemies are L5 left.
    auto chosen = spellcross_strategy::ChooseGeneratedEnemySeeds(enemy,north,valid);
    assert(chosen.size() == enemy.size());
    assert(chosen[0].x == 3 && chosen[0].y == 29);
    auto fall = spellcross_strategy::ChooseGeneratedEnemySeeds(
        std::vector<Pos>{{-1,-1}},north,valid);
    assert(fall.size() == north.size());
    auto none = spellcross_strategy::ChooseGeneratedEnemySeeds(
        std::vector<Pos>{}, std::vector<Pos>{}, valid);
    assert(none.empty());
    return 0;
}
