// Regression scenarios transcribed from retail COMMON.FS:
// LEVEL_09.DEF, RESEARCH.DEF, BONUSES.DEF and FORMACIE.DEF.
#include "../source/strategic_research_event_rules.h"
#include "../source/strategic_formation_bonus_rules.h"
#include <cassert>
#include <set>
#include <unordered_set>
#include <vector>

struct ScriptEvent
{
    int id;
    std::vector<int> research_flags;
    std::vector<int> wait_for_research;
    int set_player_territory = -1;
};

int main()
{
    using namespace spellcross_strategy;
    const std::vector<ScriptEvent> events{
        {11, {138}, {}, -1}, // M09_11A EndOKEvent(11)
        {12, {139}, {}, -1}, // M09_12A/B EndOKEvent(12)
        {13, {115}, {135, 136}, 4} // keys completed -> new passage
    };
    std::set<int> triggered;
    std::unordered_set<int> completed;

    // Neither key is offered before its corresponding mission.
    auto flags = EffectiveResearchFlags(std::vector<int>{}, events, triggered);
    assert(!AnyResearchPrerequisiteUnlocked({138}, completed, flags));
    assert(!AnyResearchPrerequisiteUnlocked({139}, completed, flags));
    assert(!FinalTerritoryResearchGateSatisfied(events, completed));

    // The flag for the first captured region unlocks ONLY its own key.
    triggered.insert(11);
    flags = EffectiveResearchFlags(std::vector<int>{}, events, triggered);
    assert(AnyResearchPrerequisiteUnlocked({138}, completed, flags));
    assert(!AnyResearchPrerequisiteUnlocked({139}, completed, flags));
    assert(!FinalTerritoryResearchGateSatisfied(events, completed));

    // After reload, event IDs alone must reconstruct both Special flags.
    triggered.insert(12);
    flags = EffectiveResearchFlags(std::vector<int>{}, events, triggered);
    assert(AnyResearchPrerequisiteUnlocked({138}, completed, flags));
    assert(AnyResearchPrerequisiteUnlocked({139}, completed, flags));
    assert(!AllResearchCompleted({135, 136}, completed));
    assert(!FinalTerritoryResearchGateSatisfied(events, completed));

    // Researching only one key is insufficient, even if both flags exist.
    completed.insert(135);
    assert(!AllResearchCompleted({135, 136}, completed));
    assert(!FinalTerritoryResearchGateSatisfied(events, completed));
    completed.insert(136);
    assert(AllResearchCompleted({135, 136}, completed));
    assert(FinalTerritoryResearchGateSatisfied(events, completed));

    // A Special flag unlocks a research choice, but is not research completion.
    assert(!AllResearchCompleted({138, 139}, completed));
    assert(AnyResearchPrerequisiteUnlocked({137, 139}, completed, flags)); // OR
    assert(!AnyResearchPrerequisiteUnlocked({137, 140}, completed, flags));
    assert(AnyResearchPrerequisiteUnlocked({}, completed, flags));
    assert(EffectiveResearchFlags(std::vector<int>{115}, events, std::set<int>{}).count(115) == 1);

    // Original FORMACIE.DEF: battalion +1/+1, regiment +2/+1,
    // brigade +4/+3. Highest formation bonus is used, not a sum.
    static_assert(FormationBonusForLevel(0).attack == 0);
    static_assert(FormationBonusForLevel(1).attack == 1);
    static_assert(FormationBonusForLevel(1).defence == 1);
    static_assert(FormationBonusForLevel(2).attack == 2);
    static_assert(FormationBonusForLevel(2).defence == 1);
    static_assert(FormationBonusForLevel(3).attack == 4);
    static_assert(FormationBonusForLevel(3).defence == 3);
    // A level 2 MLRS with a brigade can legitimately outperform a
    // level 5 ungrouped MLRS: +1 XP +4 formation > +2 XP +0.
    assert(1 + FormationBonusForLevel(3).attack >
           2 + FormationBonusForLevel(0).attack);
    return 0;
}
