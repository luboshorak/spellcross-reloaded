#pragma once
// Global user preference and pure deterministic START-square assignment.
// Neither the setting nor the matcher modifies campaign save files.
#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace spellcross_autodeploy
{
    inline bool& Preference()
    {
        static bool enabled = false;
        return enabled;
    }
    inline bool Enabled() { return Preference(); }
    inline void SetEnabled(bool enabled) { Preference() = enabled; }

    struct UnitSlots
    {
        bool is_air = false;
        std::vector<std::size_t> allowed;
    };

    // Bipartite matching for ground and air independently; one of each may
    // share a START square. No partial results are published on failure.
    inline bool Assign(const std::vector<UnitSlots>& units,
                       std::size_t square_count,
                       std::vector<int>& assignment)
    {
        if (units.empty())
        {
            assignment.clear();
            return true;
        }
        if (!square_count)
            return false;
        std::vector<int> ground_owner(square_count, -1), air_owner(square_count, -1);
        for (std::size_t i = 0; i < units.size(); ++i)
        {
            std::vector<unsigned char> visited(square_count, 0);
            std::function<bool(std::size_t)> place;
            place = [&](std::size_t index) {
                auto& owners = units[index].is_air ? air_owner : ground_owner;
                for (std::size_t square : units[index].allowed)
                {
                    if (square >= square_count || visited[square])
                        continue;
                    visited[square] = 1;
                    int previous = owners[square];
                    if (previous < 0 || place(static_cast<std::size_t>(previous)))
                    {
                        owners[square] = static_cast<int>(index);
                        return true;
                    }
                }
                return false;
            };
            if (!place(i))
                return false;
        }
        std::vector<int> result(units.size(), -1);
        for (std::size_t i = 0; i < square_count; ++i)
        {
            if (ground_owner[i] >= 0)
                result[static_cast<std::size_t>(ground_owner[i])] = static_cast<int>(i);
            if (air_owner[i] >= 0)
                result[static_cast<std::size_t>(air_owner[i])] = static_cast<int>(i);
        }
        assignment = std::move(result);
        return true;
    }
}
