#include "../source/strategic_hierarchy_layout.h"

#include <cassert>

int main()
{
    using namespace spellcross_strategy;

    const auto first = HierarchyPage(1);
    assert(first.first_battalion == 1);
    assert(first.first_regiment == 1);
    assert(first.brigade == 1);

    const auto second = HierarchyPage(2);
    assert(second.first_battalion == 5);
    assert(second.first_regiment == 3);
    assert(second.brigade == 2);

    assert(HierarchyPoolMaxScroll(12, 4, 31) == 0);
    assert(HierarchyPoolMaxScroll(24, 14, 31) == 9);

    assert(kBrigadeActiveOverlay.width == 138);
    assert(kBrigadeActiveOverlay.height == 171);
    assert(kBrigadeActiveOverlay.x == 258);
    assert(kBrigadeActiveOverlay.y == 136);
    return 0;
}
