#include "../source/night_palette.h"
#include <cstdlib>
#include <cstring>
#include <iostream>

static void check(bool condition)
{
    if (!condition) std::abort();
}

int main()
{
    using namespace spellcross_night;
    Palette source{}, original{}, night{};
    for (int r = 0; r < 256; ++r)
        for (int g = 0; g < 256; ++g)
        {
            for (int b = 0; b < 256; ++b)
            {
                source[b][0] = r;
                source[b][1] = g;
                source[b][2] = b;
            }
            std::memcpy(original, source, sizeof(source));
            check(SelectPalette(source, night, false) == source);
            check(SelectPalette(source, night, true) == night);
            for (int b = 0; b < 256; ++b)
            {
                int expected = r > g ? r : g;
                if (b > expected) expected = b;
                check(night[b][0] == 0 && night[b][1] == expected && night[b][2] == 0);
            }
            check(std::memcmp(source, original, sizeof(source)) == 0);
            check(SelectPalette(source, night, false) == source);
        }
    std::cout << "Night palette: PASS (all 16777216 RGB combinations; day/night/day)\n";
}
