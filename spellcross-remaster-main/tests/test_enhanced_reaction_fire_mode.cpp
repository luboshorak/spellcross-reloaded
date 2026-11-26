// Enhanced opportunity-fire: each defender receives at most one real
// initiative contest per moving enemy per faction phase, regardless of
// which unit spotted the enemy first. No wxWidgets required.
#include "../source/tactical_reaction_fire_mode.h"
#include "../source/tactical_reaction_fire_rules.h"
#include <cassert>

int main()
{
    using namespace spellcross_tactical;
    assert(static_cast<int>(ReactionFireMode::Original) == 0);
    assert(static_cast<int>(ReactionFireMode::Enhanced) == 1);
    assert(EntersSideSight(true, false)); // untouched Original rule
    assert(!EntersSideSight(true, true));

    EnhancedReactionAttempts attempts;
    constexpr int wolf = 50, commando = 68, howitzer = 69;
    // Hummer spots the wolf too early: no eligible Commando shot yet,
    // so no initiative draw is recorded.
    assert(!attempts.Has(wolf, commando));

    // Wolf enters Commando's weapon area while still continuously spotted.
    assert(attempts.Mark(wolf, commando));
    assert(attempts.Has(wolf, commando));
    assert(!attempts.Mark(wolf, commando)); // even if first roll lost

    // Independent shooter can still react to the same wolf.
    assert(attempts.Mark(wolf, howitzer));
    assert(attempts.Mark(wolf + 1, commando));

    // New faction phase renews opportunity once.
    attempts.Clear();
    assert(!attempts.Has(wolf, commando));
    assert(attempts.Mark(wolf, commando));

    // High/negative IDs should not accidentally collide.
    assert(EnhancedReactionAttempts::Key(-1, 1) != EnhancedReactionAttempts::Key(1, -1));
}
