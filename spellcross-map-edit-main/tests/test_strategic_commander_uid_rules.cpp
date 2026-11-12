#include "../source/strategic_commander_uid_rules.h"
#include <cassert>
#include <cstdint>
#include <set>
#include <vector>

struct Commander { std::uint32_t uid; int rank; };
int main()
{
    std::vector<Commander> owned{{9,1},{9,7},{0,0},{2,3}};
    std::vector<Commander> available{{9,0},{4,2},{0,1}};
    std::uint32_t next = 1;
    const auto repaired =
        spellcross_strategy::EnsureUniqueCommanderUids(owned,available,next);
    assert(repaired == 4);
    // The first UID remains stable for existing hierarchy records.
    assert(owned[0].uid == 9 && owned[1].uid != owned[0].uid);
    std::set<std::uint32_t> all;
    for (auto c:owned) assert(c.uid && all.insert(c.uid).second);
    for (auto c:available) assert(c.uid && all.insert(c.uid).second);
    assert(next > 0 && all.count(next)==0);
    const auto second =
        spellcross_strategy::EnsureUniqueCommanderUids(owned,available,next);
    assert(second == 0);
    assert(owned[1].rank == 7);

    // ID wraparound from edited saves must not create zero IDs.
    std::vector<Commander> maxOwned{{UINT32_MAX,8},{UINT32_MAX,8}};
    std::vector<Commander> noOffers;
    next = UINT32_MAX;
    assert(spellcross_strategy::EnsureUniqueCommanderUids(maxOwned,noOffers,next) == 1);
    assert(maxOwned[1].uid != 0 && maxOwned[1].uid != UINT32_MAX);
    return 0;
}
