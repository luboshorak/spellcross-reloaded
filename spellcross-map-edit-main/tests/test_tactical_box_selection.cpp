#include "../source/tactical_box_selection.h"

#include <cassert>

int main()
{
    using namespace spellcross_tactical;

	static_assert(!BOX_SELECTION_ENABLED_BY_DEFAULT);

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

    // In tactical group mode, a normal RMB click exits group mode, even if
    // Windows reports a small mouse movement between down and up events.
    assert(IsGroupCancelRightClick(100, 100, 100, 100, false, 6));
    assert(IsGroupCancelRightClick(100, 100, 101, 99, false, 6));
    assert(IsGroupCancelRightClick(100, 100, 105, 105, false, 6));
    assert(!IsGroupCancelRightClick(100, 100, 106, 100, false, 6));
    assert(!IsGroupCancelRightClick(100, 100, 100, 93, false, 6));
    // The user dragged beyond the threshold, then returned to press position.
    assert(!IsGroupCancelRightClick(100, 100, 100, 100, true, 6));

    // Windows fullscreen/focus transitions can discard button-up. A stale
    // capture or RMB map pan must no longer block subsequent tactical input.
    assert(!HasOrphanedPointerGesture(false, false, false, false));
    assert(!HasOrphanedPointerGesture(true, false, true, false));
    assert(!HasOrphanedPointerGesture(false, true, false, true));
    assert(!HasOrphanedPointerGesture(true, true, true, true));
    assert(HasOrphanedPointerGesture(true, false, false, false));
    assert(HasOrphanedPointerGesture(false, true, false, false));
    assert(HasOrphanedPointerGesture(true, true, false, true));
    assert(HasOrphanedPointerGesture(true, true, true, false));
}
