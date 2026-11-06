#include "../source/tactical_range_input.h"
#include <cassert>
int main()
{
    using namespace spellcross_tactical;
    using R = TemporaryRangeMode;
    assert(RequestedRangeMode(false, false, false, false) == R::None);
    assert(RequestedRangeMode(true, false, false, false) == R::Move);
    assert(RequestedRangeMode(false, true, false, false) == R::Move);
    assert(RequestedRangeMode(false, false, true, false) == R::Attack);
    assert(RequestedRangeMode(false, true, true, false) == R::Attack);
    assert(RequestedRangeMode(true, false, true, false) == R::Move);
    assert(RequestedRangeMode(true, true, true, false) == R::Move);
    assert(RequestedRangeMode(false, false, true, false) == R::Attack);
    assert(RequestedRangeMode(false, false, false, false) == R::None);
    assert(RequestedRangeMode(true, true, true, true) == R::None);
    return 0;
}
