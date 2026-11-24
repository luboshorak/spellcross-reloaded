#include "../source/pnm_frame_rules.h"
#include <cstdlib>
#include <iostream>

static void check(bool condition)
{
    if (!condition) std::abort();
}

int main()
{
    using namespace pnm_frame_rules;
    int sprite = 42;
    std::vector<int*> frames(5, &sprite);
    check(limit(8, frames.size()) == 5);
    check(limit(-1, frames.size()) == 5);
    check(limit(3, frames.size()) == 3);
    check(limit(0, frames.size()) == 1);
    check(limit(8, 0) == 0);
    check(normalize(-1, 5) == 0);
    check(normalize(5, 5) == 0);
    check(normalize(2, 5) == 2);
    check(advance(4, 5) == 0);
    check(advance(1, 5) == 2);
    check(advance(5, 5) == 0);
    check(advance(-1, 5) == 0);
    check(advance(0, 0) == 0);
    check(advance(0, 1) == 0);
    check(get(frames, -1) == nullptr);
    check(get(frames, 5) == nullptr);
    check(get(frames, 4) == &sprite);
    check(get(std::vector<int*>{}, 0) == nullptr);
    frames[2] = nullptr;
    check(get(frames, 2) == nullptr);
    frames[2] = &sprite;
    for (int declared = -1; declared <= 255; ++declared)
    {
        const int count = limit(declared, frames.size());
        for (int start = -1; start <= 255; ++start)
        {
            int index = normalize(start, count);
            for (int tick = 0; tick < 20; ++tick)
            {
                check(index >= 0 && index < count);
                check(get(frames, index) == &sprite);
                index = advance(index, count);
            }
        }
    }
    std::cout << "PNM frame regression: PASS\n";
}
