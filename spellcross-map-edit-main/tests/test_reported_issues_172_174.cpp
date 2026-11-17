// Regression scenarios transcribed from retail COMMON.FS:
// LEVEL_09.DEF, RESEARCH.DEF, BONUSES.DEF and FORMACIE.DEF.
#include "../source/strategic_research_event_rules.h"
#include "../source/strategic_formation_bonus_rules.h"
#include <cassert>
#include <array>
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

    // Sanitized reproduction from issue #174's slot_09.json
    // (2026-11-16 13:49:10; LEVEL_09, turn 11, selected territory 12).
    // Actual save: both missions won, events 11+12 triggered, but neither
    // research 135 nor 136 completed, and Event(13) has NOT fired.
    // Never distribute the original reporter's complete save with the tests.
    const std::set<int> reportedTriggered{
        0, 1, 2, 4, 11, 12, 15, 16
    };
    const std::unordered_set<int> reportedCompleted{
        104, 115, 133, 134, 140
    }; // representative previously finished IDs; no 135/136
    const auto reportedFlags =
        EffectiveResearchFlags(std::vector<int>{}, events, reportedTriggered);
    assert(reportedFlags.count(138) == 1);
    assert(reportedFlags.count(139) == 1);
    assert(reportedFlags.count(115) == 0); // Event(13) still pending
    assert(AnyResearchPrerequisiteUnlocked({138}, reportedCompleted, reportedFlags));
    assert(AnyResearchPrerequisiteUnlocked({139}, reportedCompleted, reportedFlags));
    assert(!AllResearchCompleted({135, 136}, reportedCompleted));
    assert(!FinalTerritoryResearchGateSatisfied(events, reportedCompleted));
    auto afterFirstKey = reportedCompleted;
    afterFirstKey.insert(135);
    assert(!FinalTerritoryResearchGateSatisfied(events, afterFirstKey));
    afterFirstKey.insert(136);
    assert(FinalTerritoryResearchGateSatisfied(events, afterFirstKey));

    // Original FORMACIE.DEF: battalion +1/+1, regiment +2/+1,
    // brigade +4/+3. Highest formation bonus is used, not a sum.
    static_assert(FormationBonusForLevel(0).attack == 0);
    static_assert(FormationBonusForLevel(1).attack == 1);
    static_assert(FormationBonusForLevel(1).defence == 1);
    static_assert(FormationBonusForLevel(2).attack == 2);
    static_assert(FormationBonusForLevel(2).defence == 1);
    static_assert(FormationBonusForLevel(3).attack == 4);
    static_assert(FormationBonusForLevel(3).defence == 3);
    // Issue #174 snapshot has 13 MLRS (unit_id=28), all upgraded only
    // with engine #2 (Move +1), assigned across intact brigades 1 and 2.
    // JEDNOTKY.DEF base against armor = 15, defence = 8.
    // BONUS.DEF (experience level 5..8): attack 2/3/4/4, defence 3/4/4/5.
    // The expected values apply only if the full formations participate:
    // at saved turn 11 every MLRS has cooldown > 0 and cannot yet deploy.
    constexpr std::array<int, 13> reportedMlrsLevels{
        6, 8, 7, 6, 7, 7, 5, 8, 5, 6, 5, 5, 6
    };
    constexpr std::array<int, 13> expectedArmoredAttack{
        22, 23, 23, 22, 23, 23, 21, 23, 21, 22, 21, 21, 22
    };
    constexpr std::array<int, 13> expectedDefence{
        15, 16, 15, 15, 15, 15, 14, 16, 14, 15, 14, 14, 15
    };
    constexpr int xpAttack[] = {0, 0, 0, 0, 0, 2, 3, 4, 4};
    constexpr int xpDefence[] = {0, 0, 0, 0, 0, 3, 4, 4, 5};
    const auto brigade = FormationBonusForLevel(3);
    for (std::size_t i = 0; i < reportedMlrsLevels.size(); ++i)
    {
        const int lvl = reportedMlrsLevels[i];
        assert(15 + xpAttack[lvl] + brigade.attack == expectedArmoredAttack[i]);
        assert(8 + xpDefence[lvl] + brigade.defence == expectedDefence[i]);
    }

    // A level 2 MLRS with a brigade can legitimately outperform a
    // level 5 ungrouped MLRS: +1 XP +4 formation > +2 XP +0.
    assert(1 + FormationBonusForLevel(3).attack >
           2 + FormationBonusForLevel(0).attack);
    return 0;
}
