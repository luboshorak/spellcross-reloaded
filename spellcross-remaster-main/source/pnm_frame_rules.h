#pragma once

#include <cstddef>
#include <limits>
#include <vector>

// DTA animation metadata can disagree with the actual PNM frame count.
namespace pnm_frame_rules
{
    inline int limit(int declared, std::size_t available)
    {
        const auto max_int = static_cast<std::size_t>((std::numeric_limits<int>::max)());
        const int count = static_cast<int>(available > max_int ? max_int : available);
        // A zero limit previously held the animation on frame zero.
        if (count == 0)
            return 0;
        if (declared < 0 || declared > count)
            return count;
        return declared == 0 ? 1 : declared;
    }

    inline int normalize(int index, int count)
    {
        return index >= 0 && index < count ? index : 0;
    }

    inline int advance(int index, int count)
    {
        return index >= 0 && count > 0 && index < count - 1 ? index + 1 : 0;
    }

    template<class T>
    T* get(const std::vector<T*>& frames, int index)
    {
        return index >= 0 && static_cast<std::size_t>(index) < frames.size()
            ? frames[static_cast<std::size_t>(index)] : nullptr;
    }
}
