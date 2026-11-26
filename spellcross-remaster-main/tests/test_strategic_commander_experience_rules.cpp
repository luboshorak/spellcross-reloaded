#include "../source/strategic_commander_experience_rules.h"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

struct RankRecord
{
    int rank;
    int actions_required;
};

// Exact cumulative officer thresholds from the retail HODNOSTI.DEF.
static const std::vector<RankRecord> retailRanks = {
    {0, 2}, {1, 6}, {2, 10}, {3, 18}, {4, 26},
    {5, 36}, {6, 48}, {7, 66}, {8, 84}
};

int main()
{
    using spellcross_strategy::InitialCommanderBattles;
    using spellcross_strategy::CommanderRankForBattles;
    using spellcross_strategy::ReadOriginalCommanderBattles;

    // Older JSON saves can contain zero despite officers' established
    // First Lieutenant rank. Restore only the minimum, not campaign totals.
    assert(spellcross_strategy::RestoreMinimumCommanderBattles(1, 0, retailRanks) == 6);
    assert(spellcross_strategy::RestoreMinimumCommanderBattles(1, 7, retailRanks) == 7);
    assert(spellcross_strategy::RestoreMinimumCommanderBattles(3, 0, retailRanks) == 18);
    assert(spellcross_strategy::RestoreMinimumCommanderBattles(7, 70, retailRanks) == 70);

    // A survivor matched by the existing UID-less tactical-save fallback
    // must count as returned for its commander. A different nonzero UID
    // must NEVER be accepted as that same company.
    std::unordered_set<std::uint32_t> returned;
    spellcross_strategy::RecordReturnedStrategicUnit(returned, 19, 0);
    spellcross_strategy::RecordReturnedStrategicUnit(returned, 22, 22);
    spellcross_strategy::RecordReturnedStrategicUnit(returned, 38, 55);
    spellcross_strategy::RecordReturnedStrategicUnit(returned, 0, 0);
    assert(returned.size() == 2 && returned.count(19) && returned.count(22));
    assert(!returned.count(38) && !returned.count(0));

    // The user's LEVEL_06 save has John rank 7 and first seven deployed
    // officers at 7 battles. Three more correctly attributed deployments
    // produce Captain at battle 10, regardless of his higher rank.
    assert(CommanderRankForBattles(1, 7, 7, retailRanks) == 1);
    assert(CommanderRankForBattles(1, 10, 7, retailRanks) == 2);

    // New officers already carry the six battles of a First Lieutenant.
    assert(InitialCommanderBattles(1, retailRanks) == 6);
    assert(InitialCommanderBattles(0, retailRanks) == 2);
    assert(InitialCommanderBattles(77, retailRanks) == 0);

    // No early promotion: thresholds describe a total, not an interval.
    assert(CommanderRankForBattles(1, 6, 8, retailRanks) == 1);
    assert(CommanderRankForBattles(1, 9, 8, retailRanks) == 1);
    assert(CommanderRankForBattles(1, 10, 8, retailRanks) == 2);
    assert(CommanderRankForBattles(2, 17, 8, retailRanks) == 2);
    assert(CommanderRankForBattles(2, 18, 8, retailRanks) == 3);
    assert(CommanderRankForBattles(3, 25, 8, retailRanks) == 3);
    assert(CommanderRankForBattles(3, 26, 8, retailRanks) == 4);
    assert(CommanderRankForBattles(4, 35, 8, retailRanks) == 4);
    assert(CommanderRankForBattles(4, 36, 8, retailRanks) == 5);
    assert(CommanderRankForBattles(5, 47, 8, retailRanks) == 5);
    assert(CommanderRankForBattles(5, 48, 8, retailRanks) == 6);
    assert(CommanderRankForBattles(6, 65, 8, retailRanks) == 6);
    assert(CommanderRankForBattles(6, 66, 8, retailRanks) == 7);

    // Subordinate officers never become the same rank as John Alexander.
    assert(CommanderRankForBattles(1, 100, 2, retailRanks) == 1);
    assert(CommanderRankForBattles(1, 100, 3, retailRanks) == 2);
    assert(CommanderRankForBattles(1, 100, 5, retailRanks) == 4);
    assert(CommanderRankForBattles(7, 100, 8, retailRanks) == 7);
    assert(CommanderRankForBattles(1, 100, 8, retailRanks) == 7);

    // Late player promotion unlocks previously-earned subordinate ranks
    // without requiring another battle for that same subordinate.
    assert(CommanderRankForBattles(2, 41, 3, retailRanks) == 2);
    assert(CommanderRankForBattles(2, 41, 5, retailRanks) == 4);

    // Selected real records from decoded retail BIG_MAP.SAV files:
    // name       fights  John  expected officer rank
    struct RetailRecord { int fights; int johnRank; int officerRank; };
    const std::vector<RetailRecord> retailSamples = {
        {39, 8, 5}, // Delton
        {62, 8, 6}, // Schein
        {76, 8, 7}, // Berger
        {82, 8, 7}, // Garowski
        {67, 5, 4}, // Berger, earlier save
        {45, 5, 4}, // Schein, earlier save
        {41, 3, 2}, // Berger, capped
        {31, 3, 2}, // Berger, capped
        { 7, 2, 1}, // Berger, early campaign
        { 8, 2, 1}, // Berger, early campaign
        { 9, 2, 1}, // Berger, early campaign
        {11, 3, 2}, // Berger, promoted
        {10, 5, 2}  // Delton, promoted
    };
    for (const auto& sample : retailSamples)
        assert(CommanderRankForBattles(1, sample.fights,
            sample.johnRank, retailRanks) == sample.officerRank);

    // Never change ranks stored by older Reloaded versions downward during
    // a load; legacy achievements and imported saves are preserved.
    assert(CommanderRankForBattles(5, 0, 3, retailRanks) == 5);

    // Original retail binary: 16-bit little-endian battle count at +36.
    std::vector<std::uint8_t> save(44 * 2, 0);
    save[36] = 0x34;
    save[37] = 0x12;
    save[44 + 36] = 84;
    assert(ReadOriginalCommanderBattles(save, 0) == 0x1234);
    assert(ReadOriginalCommanderBattles(save, 44) == 84);
    assert(ReadOriginalCommanderBattles(save, save.size() - 37) == 0);
    assert(ReadOriginalCommanderBattles(save, save.size() + 1) == 0);

    std::cout << "PASS: retail cumulative thresholds, John rank ceiling, "
                 "13 original save fixtures, import and legacy compatibility\n";
    return 0;
}
