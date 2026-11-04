// Retail regression: continue a completed rescue, refuse an OS attack, reload,
// fail and win the recapture. Arguments: DATA, auxiliary data, extracted COMMON.
#include <wx/wx.h>
#include "main.h"
#include "forms/form_level.h"
#include <wx/init.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>

static void Check(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

int main(int argc, char** argv)
{
    if (argc != 4) return 2;
    std::cout.setf(std::ios::unitbuf);
    wxInitializer wx;
    if (!wx) return 2;
    wxDisableAsserts(); // Existing native layout assertions are outside this regression.
    wxInitAllImageHandlers();
    namespace fs = std::filesystem;
    try
    {
        std::wstring root = fs::path(argv[1]).wstring();
        std::wstring aux = fs::path(argv[2]).wstring();
        SpellData data(root, root, aux);
        const fs::path common(argv[3]);
        LevelData level;
        LevelLoader loader;
        std::string error;
        Check(loader.LoadLevelDef((common / "LEVEL_04.DEF").string(), level, &error), "LEVEL_04 load failed");
        SpellMap map;
        auto initial = (common / "M04_10A.DEF").wstring();
        Check(!map.Load(initial, &data), "Rescue fixture load failed");
        MainFrame controller(&map, &data);
        StrategicLevelFrame strategy(&controller, level, true);
        strategy.m_gameModeEnabled = true;
        strategy.m_ownedTerritories.clear();
        for (const auto& territory : level.territories)
            if (territory.id != level.end_territory)
                strategy.m_ownedTerritories.push_back(territory.id);
        strategy.m_territoryCurrentMission[10] = "m04_10a";
        strategy.m_territoryLaunchCount[10] = 1;
        strategy.m_turn = 12;
        strategy.m_money = 500;
        strategy.m_player.experience = 800;
        std::string defPath, snapshot;
        StrategicLevelFrame::PendingMissionResult pending;
        Check(strategy.ExportBattleSaveContext(defPath, snapshot, pending), "Continue snapshot failed");
        Check(strategy.ImportBattleSaveContext(snapshot, pending), "Continue restore failed");

        StrategicLevelFrame::CounterAttackState army;
        army.attack_id = 901;
        army.territory_id = 10;
        army.counter_mission = "m04_10a"; // Actual legacy Army attack snapshot.
        army.triggered = true;
        army.enemy_units = {37,38,47};
        strategy.m_counterAttacks.push_back(army);
        strategy.SelectTerritoryById(10);
        const auto defenceBriefing = strategy.GetGeneratedBattleBriefing(10);
        Check(strategy.ResolveMissionTokenForTerritory(10) == "r0410",
            "Active OS attack reused the completed major-rescue mission");
        strategy.SurrenderCounterAttack(strategy.m_counterAttacks.back());
        Check(strategy.ResolveMissionTokenForTerritory(10) == "r0410",
            "Refusing defence resurrected the completed rescue");
        const auto recaptureBriefing = strategy.GetGeneratedBattleBriefing(10);
        Check(!recaptureBriefing.empty() && recaptureBriefing != defenceBriefing &&
            recaptureBriefing.Find("majora") == wxNOT_FOUND &&
            recaptureBriefing.Find("major") == wxNOT_FOUND,
            "Recapture briefing repeats the major rescue");
        Check(strategy.ExportBattleSaveContext(defPath, snapshot, pending), "Occupied snapshot failed");
        Check(strategy.ImportBattleSaveContext(snapshot, pending), "Occupied restore failed");
        Check(strategy.ResolveMissionTokenForTerritory(10) == "r0410", "Reload lost recapture state");
        const auto occupyingForce = strategy.m_counterAttacks.back().enemy_units;
        Check(occupyingForce == std::vector<int>({37,38,47}), "Reload lost the occupying OS army");
        strategy.m_turn += 20;
        strategy.CheckTimeouts();
        Check(strategy.GetTerritoryTimeoutRemaining(10) < 0, "Old rescue deadline restarted");
        Check(strategy.ResolveMissionTokenForTerritory(10) == "r0410", "Old EndBadMission replaced recapture");
        const auto token = strategy.ResolveMissionTokenForTerritory(10);
        Check(strategy.ResolveMapDefPathForMissionToken(token).empty(), "Recapture selected a story DEF");
        const auto dta = strategy.ResolveMapDtaPathForMissionToken(token);
        Check(fs::path(dta).filename() == "M04_10.DTA", "Recapture terrain mismatch");
        LevelData::PlayerUnitAdd company;
        company.unit_id = 0;
        company.health = 100;
        Check(controller.LoadGeneratedStrategicBattleFromDtaPath(dta, {company}, occupyingForce),
            "Generated recapture could not launch");
        int objectives = 0;
        for (const auto* event : map.events->GetEvents())
            if (event->is_objective)
            {
                ++objectives;
                Check(event->evt_type == SpellMapEventRec::EVT_DESTROY_ALL,
                    "Generated recapture contains the major rescue objective");
            }
        Check(objectives == 1, "Generated recapture objectives mismatch");
        const int money = strategy.m_money;
        const int experience = strategy.m_player.experience;
        strategy.HandleMissionResult(10, false, "m04_10a"); // Older tactical save token.
        Check(strategy.ResolveMissionTokenForTerritory(10) == "r0410", "Failed recapture reactivated rescue B");
        strategy.HandleMissionResult(10, true, "m04_10a");
        Check(strategy.m_money == money && strategy.m_player.experience == experience,
            "Recapture awarded the completed rescue's money/XP again");
        Check(strategy.ResolveMissionTokenForTerritory(10) == "r0410", "Victory reactivated rescue A");
        Check(strategy.ExportBattleSaveContext(defPath, snapshot, pending), "Victory snapshot failed");
        Check(strategy.ImportBattleSaveContext(snapshot, pending), "Victory restore failed");
        Check(strategy.ResolveMissionTokenForTerritory(10) == "r0410", "Victory reload reactivated rescue");
        army.attack_id = 902;
        strategy.m_counterAttacks.push_back(army);
        strategy.SurrenderCounterAttack(strategy.m_counterAttacks.back());
        Check(strategy.ResolveMissionTokenForTerritory(10) == "r0410", "Second occupation reactivated rescue");
        std::cout << "PASS: Continue, refuse defence, reload, timeout, generated objective, failed/successful recapture, second occupation\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
