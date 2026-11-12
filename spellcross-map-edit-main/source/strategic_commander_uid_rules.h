// Stable strategic commander identities, including imported/hand-edited saves.
// A rank is mutable; the UID must identify exactly one commander.
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <unordered_set>
#include <vector>

namespace spellcross_strategy
{
    template<typename Commander>
    std::size_t EnsureUniqueCommanderUids(std::vector<Commander>& owned,
        std::vector<Commander>& available, std::uint32_t& nextUid)
    {
        std::unordered_set<std::uint32_t> reserved;
        std::uint32_t highest = 0;
        for (const auto& officer : owned)
            if (officer.uid != 0)
            {
                reserved.insert(officer.uid);
                highest = std::max(highest, static_cast<std::uint32_t>(officer.uid));
            }
        for (const auto& officer : available)
            if (officer.uid != 0)
            {
                reserved.insert(officer.uid);
                highest = std::max(highest, static_cast<std::uint32_t>(officer.uid));
            }

        // Do not reuse an ID from the available pool after buying an officer.
        if (highest != UINT32_MAX)
            nextUid = std::max(nextUid, highest + 1u);
        if (nextUid == 0)
            nextUid = 1;

        std::unordered_set<std::uint32_t> seen;
        std::size_t repaired = 0;
        const auto normalize = [&](std::vector<Commander>& officers)
        {
            for (auto& officer : officers)
            {
                if (officer.uid != 0 && seen.insert(officer.uid).second)
                    continue;
                // Preserve the FIRST occurrence of each original UID (old
                // hierarchy references are keyed only by that value). Allocate
                // new IDs for later duplicates/legacy zero records, never an
                // existing/announced UID from either pool.
                while (nextUid == 0 || reserved.count(nextUid) != 0)
                    ++nextUid;
                officer.uid = nextUid++;
                reserved.insert(officer.uid);
                seen.insert(officer.uid);
                ++repaired;
            }
        };

        normalize(owned);
        normalize(available);
        while (nextUid == 0 || reserved.count(nextUid) != 0)
            ++nextUid;
        return repaired;
    }
}
