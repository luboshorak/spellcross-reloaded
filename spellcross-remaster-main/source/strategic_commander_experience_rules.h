#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <unordered_set>

namespace spellcross_strategy
{
// HODNOSTI.DEF's actions_required is the TOTAL number of battles for a rank,
// not the number of additional battles required while holding the previous rank.
// Retail recruits start as First Lieutenant with this rank's existing battle
// count (six in the unmodified original), not with zero.
template<typename RankRecords>
inline int InitialCommanderBattles(int initialRank, const RankRecords& ranks)
{
    for (const auto& record : ranks)
        if (record.rank == initialRank)
            return std::max(0, record.actions_required);
    return 0;
}

// Some older Reloaded JSON saves stored officers at rank 1 with a zero
// counter. Retail First Lieutenants start at six battles, so restore only
// the minimum inherent in their EXISTING rank. Do not infer extra deployments
// from total campaign missions, which may predate an officer's recruitment.
template<typename RankRecords>
inline int RestoreMinimumCommanderBattles(int rank, int persistedBattles,
    const RankRecords& ranks)
{
    return std::max(std::max(0, persistedBattles),
        InitialCommanderBattles(rank, ranks));
}

// The tactical save fallback can successfully associate a UID-less survivor
// with a specific deployed strategic roster entry. Give the commander the
// SAME survivor association used for unit XP/health recovery instead of
// silently ignoring this battle because tactical UID metadata was absent.
// Never credit a survivor bearing somebody else's nonzero UID.
inline void RecordReturnedStrategicUnit(
    std::unordered_set<std::uint32_t>& returned,
    std::uint32_t rosterUid, std::uint32_t tacticalUid)
{
    if (rosterUid != 0 && (tacticalUid == 0 || tacticalUid == rosterUid))
        returned.insert(rosterUid);
}

// An officer may only advance below the current rank of John Alexander.
// Do not demote existing imported or legacy officers if a saved rank exceeds
// that limit: only apply the rank ceiling to FUTURE promotions.
template<typename RankRecords>
inline int CommanderRankForBattles(int currentRank, int battlesFought,
    int playerRank, const RankRecords& ranks)
{
    const int maximumNewRank = playerRank - 1;
    int result = currentRank;
    for (const auto& record : ranks)
    {
        if (record.rank > result && record.rank <= maximumNewRank &&
            record.actions_required > 0 && battlesFought >= record.actions_required)
            result = record.rank;
    }
    return result;
}

// Decompressed retail BIG_MAP.SAV stores a commander's battle counter as
// little-endian uint16 at offset +36 in each 44-byte commander record.
inline int ReadOriginalCommanderBattles(
    const std::vector<std::uint8_t>& data, std::size_t recordOffset)
{
    if (recordOffset > data.size() || data.size() - recordOffset < 38)
        return 0;
    return static_cast<int>(data[recordOffset + 36]) |
        (static_cast<int>(data[recordOffset + 37]) << 8);
}
} // namespace spellcross_strategy
