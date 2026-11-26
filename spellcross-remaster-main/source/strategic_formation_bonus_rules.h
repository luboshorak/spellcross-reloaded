#pragma once

// FORMACIE.DEF from the retail Spellcross COMMON.FS.
// Bonuses are for the single highest *active* formation and do not stack.
// Ranks govern which formations a commander may lead, not a separate direct
// +attack/+defence modifier. Shared by strategic launch and tactical updates.
namespace spellcross_strategy
{
struct FormationBonus
{
    int attack;
    int defence;
};

constexpr FormationBonus FormationBonusForLevel(int level)
{
    return level == 1 ? FormationBonus{1, 1} :
           level == 2 ? FormationBonus{2, 1} :
           level == 3 ? FormationBonus{4, 3} :
                        FormationBonus{0, 0};
}
} // namespace spellcross_strategy
