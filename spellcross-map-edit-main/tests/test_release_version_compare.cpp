#include "../source/release_version_compare.h"
#include <cassert>
using spellcross_release::CompareVersions;

int main()
{
    bool comparable = false;
    assert(CompareVersions("v0.9.2.3", "v0.9.3-RC1", comparable) < 0 && comparable);
    assert(CompareVersions("v0.9.3-RC1", "v0.9.3-RC2", comparable) < 0 && comparable);
    assert(CompareVersions("v0.9.3-RC2", "v0.9.3", comparable) < 0 && comparable);
    assert(CompareVersions("0.9.3.0", "v0.9.3", comparable) == 0 && comparable);
    assert(CompareVersions("v0.9.3-RC1", "v0.9.3.0", comparable) < 0 && comparable);
    assert(CompareVersions("v0.9.3", "v0.9.3-RC1", comparable) > 0 && comparable);
    assert(CompareVersions("v0.9.4-RC1", "v0.9.3", comparable) > 0 && comparable);
    assert(CompareVersions("no-version", "v0.9.3", comparable) == 0 && !comparable);
    return 0;
}
