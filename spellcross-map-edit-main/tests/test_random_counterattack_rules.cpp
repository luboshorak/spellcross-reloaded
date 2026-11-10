// Retail random Other Side counterattack rule regression (no wxWidgets).
#include "../source/strategic_random_attack_rules.h"
#include <cassert>
#include <climits>
#include <cstdint>
#include <set>
#include <vector>

using namespace spellcross_strategy;

int main()
{
    // LEVEL_06 frequency (20,6): increasing conditional probability.
    const std::vector<int> chance = {0,0,0,0,0,0,0,20,40,60,80,100,100};
    for (int elapsed = 0; elapsed < static_cast<int>(chance.size()); ++elapsed)
        assert(RandomCounterAttackChance(1 + elapsed, 1, 20, 6) == chance[elapsed]);
    assert(!RandomCounterAttackRoll(9, 1, 20, 6, 20));
    assert(RandomCounterAttackRoll(9, 1, 20, 6, 19));
    assert(!RandomCounterAttackRoll(9, 1, 20, 6, -1));
    assert(!RandomCounterAttackRoll(9, 1, 20, 6, 100));
    assert(RandomCounterAttackChance(INT_MAX, 0, 20, 6) == 100);
    assert(RandomCounterAttackChance(10, 1, 0, 6) == 0);
    assert(RandomCounterAttackChance(10, 1, 20, -1) == 0);
    assert(RandomCounterAttackChance(5, 20, 20, 6) == 0);

    // Two owned sectors (1 and 3) touch enemy sector 2 in the same
    // retail CLK; both qualify, including the initial command foothold.
    const std::vector<uint8_t> clk = {
          1,   1, 130,
          1,   3,   2,
          0,   3,   2
    };
    const auto front = FindCounterAttackFrontier(clk, 3, 3, {1, 3});
    assert((front.player_targets == std::vector<int>{1, 3}));
    assert((front.enemy_sources == std::vector<int>{2}));
    assert((front.borders == std::vector<std::pair<int,int>>{{1,2}, {3,2}}));
    const auto lost = FindCounterAttackFrontier(clk, 3, 3, {1});
    assert((lost.player_targets == std::vector<int>{1}));
    assert((lost.enemy_sources == std::vector<int>{2, 3}));
    assert(FindCounterAttackFrontier(clk, 2, 3, {1, 3}).borders.empty());
    assert(FindCounterAttackFrontier(clk, 3, 3, {}).borders.empty());

    // Randomised force stays inside AttackFlags[0..1], picks valid
    // AttackUnits and at most one AttackSpecialUnits.
    auto zero = [](int) { return 0; };
    auto high = [](int bound) { return bound - 1; };
    const std::vector<int> units{39, 40, 47};
    const std::vector<int> special{59, 68};
    const std::vector<int> flags{18, 24, 6, 10, 20, 6};
    const auto lowForce = BuildRandomCounterAttackForce(units, special, flags, zero);
    const auto highForce = BuildRandomCounterAttackForce(units, special, flags, high);
    assert(lowForce.size() == 18);
    assert(highForce.size() == 24);
    assert(lowForce[0] == 59);
    assert(std::set<int>(highForce.begin(), highForce.end()) == std::set<int>{47});
    assert(BuildRandomCounterAttackForce({}, special, flags, zero).empty());
    assert(BuildRandomCounterAttackForce(units, special, {}, zero).empty());
    return 0;
}
