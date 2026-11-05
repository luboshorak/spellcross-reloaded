#include "../source/tactical_box_selection.h"

#include <cassert>

int main()
{
    using namespace spellcross_tactical;

    const auto forward = NormalizeSelectionRect(10, 20, 40, 70);
    assert(forward.left == 10 && forward.top == 20);
    assert(forward.right == 40 && forward.bottom == 70);
    assert(forward.Contains(10, 20));
    assert(forward.Contains(40, 70));
    assert(!forward.Contains(41, 70));

    const auto reverse = NormalizeSelectionRect(40, 70, 10, 20);
    assert(reverse.left == forward.left && reverse.top == forward.top);
    assert(reverse.right == forward.right && reverse.bottom == forward.bottom);

    assert(!IsSelectionDrag(100, 100, 105, 105, 6));
    assert(IsSelectionDrag(100, 100, 106, 100, 6));
    assert(IsSelectionDrag(100, 100, 100, 94, 6));
    assert(IsSelectionDrag(100, 100, 100, 100, 0));
}
