// Regression cases grounded in original English BIG_MAP.SAV and LEVEL_XX.DEF.
// Deliberately self-contained: no GUI or proprietary retail data at CI time.
#include "../source/strategic_countdown_rules.h"
#include <cassert>

int main()
{
    using spellcross_strategy::PauseDeadlineForInactiveTurn;

    // Retail chapter 05: inaccessible territory 6, M05_06A Time(10).
    // Two independent original BIG_MAP.SAV snapshots at turns 6 and 9
    // both record remain_time=10 and ownership flags=0x00.
    int deadline = 6 + 10;
    for (int turn = 6; turn < 9; ++turn)
        deadline = PauseDeadlineForInactiveTurn(deadline, false);
    assert(deadline - 9 == 10);

    // Once the territory becomes available the deadline advances normally.
    deadline = PauseDeadlineForInactiveTurn(deadline, true);
    assert(deadline - 10 == 9);

    // Original LEVEL_03.DEF: M03_09A Time(12) can remain inactive.
    deadline = 3 + 12;
    for (int turn = 3; turn < 22; ++turn)
        deadline = PauseDeadlineForInactiveTurn(deadline, false);
    assert(deadline - 22 == 12);
    assert(PauseDeadlineForInactiveTurn(deadline, true) - 23 == 11);

    // The retail COMMON.FS VM_0..VM_9 assets are separate glyphs; VM_1
    // is 11px wide while VM_0 is 16px. The overlay must combine them,
    // not clamp Time(10/12/15) to a VM_9 graphic.
    const std::array<int, 10> spriteWidths = {
        16, 11, 16, 16, 18, 17, 16, 16, 16, 16
    };
    const auto one = spellcross_strategy::LayoutCountdownDigits(9, spriteWidths, 1);
    assert(one.size() == 1 && one[0].digit == 9);
    assert(one[0].x_offset == -8);

    const auto ten = spellcross_strategy::LayoutCountdownDigits(10, spriteWidths, 1);
    assert(ten.size() == 2);
    assert(ten[0].digit == 1 && ten[1].digit == 0);
    assert(ten[0].x_offset == -14);
    assert(ten[1].x_offset == -2);
    assert(ten[1].x_offset + ten[1].scaled_width == 14);

    for (const int value : {12, 15, 99, 100})
    {
        const auto glyphs = spellcross_strategy::LayoutCountdownDigits(value, spriteWidths, 2);
        assert(glyphs.size() == std::to_string(value).size());
        for (size_t i = 1; i < glyphs.size(); ++i)
            assert(glyphs[i].x_offset ==
                glyphs[i - 1].x_offset + glyphs[i - 1].scaled_width + 2);
    }
    auto withMissingZero = spriteWidths;
    withMissingZero[0] = 0;
    assert(spellcross_strategy::LayoutCountdownDigits(10, withMissingZero, 1).empty());
    assert(spellcross_strategy::LayoutCountdownDigits(-1, spriteWidths, 1).empty());

    // Sentinel for an already resolved/expired timer is never revived.
    assert(PauseDeadlineForInactiveTurn(-1, false) == -1);
    assert(PauseDeadlineForInactiveTurn(0, false) == 0);
    return 0;
}
