// Retail integration: DATA, auxiliary data, extracted COMMON, output directory.
#include "map.h"
#include "forms/form_level.h"
#include "main.h"
#include <wx/init.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>

struct SpellMapRestorationTestAccess
{
    static void Pump(SpellMap& map) { map.CheckCommanderDeathNotifications(); }
    static size_t Pending(SpellMap& map) { return map.m_commander_death_queue.size(); }
};

static void Check(bool ok, const char* message)
{
    if (!ok) throw std::runtime_error(message);
}

int main(int argc, char** argv)
{
    if (argc != 5) return 2;
    wxInitializer wx;
    if (!wx) return 2;
    wxDisableAsserts();
    wxInitAllImageHandlers();
    namespace fs = std::filesystem;
    try
    {
        const fs::path output(argv[4]);
        fs::create_directories(output);
        SpellData data(fs::path(argv[1]).wstring(), fs::path(argv[1]).wstring(), fs::path(argv[2]).wstring());
        LevelData level;
        LevelLoader loader;
        std::string error;
        Check(loader.LoadLevelDef((fs::path(argv[3]) / "LEVEL_03.DEF").string(), level, &error), "Retail level load");
        // Autosaves from simulated mouse input must stay inside the test fixture.
        fs::copy_file(fs::path(argv[3]) / "LEVEL_03.DEF", output / "LEVEL_03.DEF", fs::copy_options::overwrite_existing);
        level.source_path = (output / "LEVEL_03.DEF").string();
        SpellMap map;
        Check(!map.Load((fs::path(argv[3]) / "M02_04A.DEF").wstring(), &data), "Retail map load");
        map.HaltUnitRanging(true);
        MainFrame controller(&map, &data);
        StrategicLevelFrame strategy(&controller, level, true);
        strategy.m_playerUnits.clear();
        strategy.m_unitStates.clear();
        strategy.m_playerCommanders.clear();
        strategy.RestoreHierarchyAssignments({});
        std::unordered_set<uint32_t> participants;
        for (uint32_t uid = 1; uid <= 24; ++uid)
        {
            LevelData::PlayerUnitAdd company;
            company.unit_id = 1; company.count = 1; company.health = 100; company.extra = "-";
            strategy.m_playerUnits.push_back(company);
            StrategicLevelFrame::UnitInstanceState state;
            state.uid = uid;
            strategy.m_unitStates.push_back(state);
            participants.insert(uid);
        }
        uint32_t commanderUid = 100;
        auto commander = [&](const std::string& slot) {
            StrategicLevelFrame::CommanderRec c;
            c.uid = ++commanderUid; c.rank = 6; c.name = "Test commander " + std::to_string(c.uid);
            strategy.m_playerCommanders.push_back(c);
            Check(strategy.AssignCommanderToHierarchySlot(slot, c.uid, c.rank, wxString::FromUTF8(c.name)), "Commander assignment");
            return c.uid;
        };
        for (int b = 1; b <= 8; ++b)
        {
            const auto prefix = "battalion_" + std::to_string(b);
            for (int u = 1; u <= 3; ++u)
                Check(strategy.AssignUnitToHierarchySlot(prefix + "_unit_" + std::to_string(u), (b - 1) * 3 + u, "Company"), "Company assignment");
            Check(strategy.GetHierarchyBattleMeta((b - 1) * 3 + 1, participants).formation_level == 0, "Commander-free companies gained bonuses");
            commander(prefix + "_commander");
            Check(strategy.AssignUnitToHierarchySlot(prefix + "_commander_unit", (b - 1) * 3 + 1, "Company"), "Battalion host");
        }
        for (int r = 1; r <= 4; ++r)
        {
            const auto prefix = "regiment_" + std::to_string(r);
            commander(prefix + "_commander");
            Check(strategy.AssignUnitToHierarchySlot(prefix + "_unit", (r - 1) * 6 + 1, "Company"), "Regiment host");
        }
        for (int br = 1; br <= 2; ++br)
        {
            const auto prefix = "brigade_" + std::to_string(br);
            const auto uid = commander(prefix + "_commander");
            Check(strategy.AssignUnitToHierarchySlot(prefix + "_unit", (br - 1) * 12 + 1, "Company"), "Brigade host");
            Check(strategy.AssignCommanderToHierarchySlot(prefix + "_commander", uid, 6, "Test commander"), "Repeated commander pick");
            Check(strategy.GetHierarchyBattleMeta((br - 1) * 12 + 1, participants).commander_host_mask == 7, "Repeated pick detached host");
        }
        auto checkMeta = [&](uint32_t uid, int expectedLevel, int attack, int defence) {
            const auto meta = strategy.GetHierarchyBattleMeta(uid, participants);
            Check(meta.formation_level == expectedLevel && meta.attack_bonus == attack && meta.defence_bonus == defence, "Strategic formation bonus");
        };
        checkMeta(1, 3, 4, 3);
        checkMeta(13, 3, 4, 3);
        const fs::path strategicSave = output / "hierarchy.json";
        Check(strategy.SaveStrategicGameToPath(strategicSave, false), "Strategic save");
        strategy.RestoreHierarchyAssignments({});
        Check(strategy.LoadStrategicStateFromPath(strategicSave), "Strategic reload");
        checkMeta(1, 3, 4, 3);
        checkMeta(13, 3, 4, 3);
        std::string defPath, context;
        StrategicLevelFrame::PendingMissionResult pending;
        Check(strategy.ExportBattleSaveContext(defPath, context, pending), "Campaign context export");
        strategy.RestoreHierarchyAssignments({});
        Check(strategy.ImportBattleSaveContext(context, pending), "Campaign context import");
        checkMeta(1, 3, 4, 3);

        for (uint32_t uid : participants)
        {
            const auto meta = strategy.GetHierarchyBattleMeta(uid, participants);
            auto* unit = new MapUnit(&map);
            unit->unit = data.units->GetUnit(1);
            unit->id = 5000 + uid; unit->strategic_uid = uid;
            unit->coor = map.start.front(); unit->is_enemy = false; unit->is_active = true;
            unit->man = unit->unit->cnt;
            unit->formation_id = meta.formation_id;
            unit->formation_commander_mask = meta.commander_host_mask;
            unit->ResetAP();
            map.units.push_back(unit);
        }
        map.RecalculateTacticalFormations();
        auto checkBattle = [&](int id, int level, int attack, int defence) {
            auto* u = map.GetUnit(id);
            Check(u && u->formation_level == level && u->formation_attack_bonus == attack && u->formation_defence_bonus == defence, "Tactical formation bonus");
            const int attackBonus = u->formation_attack_bonus, defenceBonus = u->formation_defence_bonus;
            const int actualAttack = u->GetAttack(MapUnit::TARGET_TYPE::LIGHT), actualDefence = u->GetDefence();
            u->formation_attack_bonus = 0; u->formation_defence_bonus = 0;
            Check(actualAttack == u->GetAttack(MapUnit::TARGET_TYPE::LIGHT) + attackBonus && actualDefence == u->GetDefence() + defenceBonus, "Bonus missing from combat stats");
            u->formation_attack_bonus = attackBonus; u->formation_defence_bonus = defenceBonus;
        };
        checkBattle(5001, 3, 4, 3);
        const auto tacticalSave = output / "hierarchy.scsave";
        map.SortUnits();
        Check(!map.SaveGameStateToFile(tacticalSave.wstring()), "Tactical save");
        Check(!map.LoadGameStateFromFile(tacticalSave.wstring()), "Tactical reload");
        map.HaltUnitRanging(true);
        map.RecalculateTacticalFormations();
        checkBattle(5001, 3, 4, 3);
        checkBattle(5013, 3, 4, 3);
        Check(map.GetUnit(5001)->formation_commander_mask == 7, "Host mask lost after reload");
        // Losing the top-level host downgrades its sibling battalion to regiment.
        map.GetUnit(5001)->man = 0;
        map.RecalculateTacticalFormations();
        checkBattle(5004, 1, 1, 1);
        checkBattle(5007, 2, 2, 1);
        checkBattle(5013, 3, 4, 3);

        // Resolve all three real officers from campaign context after tactical reload.
        const auto commanderNames = strategy.GetHostedCommanderNames(1);
        Check(commanderNames.size() == 3, "Missing hosted commander identity");
        // Tactical Info must show the actual three campaign officers with
        // localized HODNOSTI rank prefixes; the casualty notices above must
        // still use the names without rank prefixes.
        const auto commanderLabels = strategy.GetHostedCommanderLabels(1);
        Check(commanderLabels.size() == commanderNames.size(),
            "Tactical Info lost hosted officers after campaign/tactical reload");
        const std::string expectedRank =
            wstring2stringCP895(strategy.GetRankAbbrev(6).ToStdWstring());
        for (size_t i = 0; i < commanderNames.size(); ++i)
            Check(commanderLabels[i] == expectedRank + " " + commanderNames[i],
                "Tactical Info displayed the wrong commander rank or identity");
        Check(strategy.GetHostedCommanderLabels(2).empty(),
            "Unit without a hosted officer gained a phantom commander");
        map.commander_death_names = [&](uint32_t uid) { return strategy.GetHostedCommanderNames(uid); };
        map.SetGameMode(true);
        map.HaltUnitRanging(true);
        bool messageBusy = true;
        std::vector<std::string> casualtyMessages;
        map.SetMessageInterface([&](SpellTextRec* text, bool yesno, std::function<void(bool)>) {
            Check(!yesno && text->name == "COMMANDER_DIED", "Wrong information message");
            casualtyMessages.push_back(text->raw_text);
            messageBusy = true;
        }, [&] { return messageBusy; });
        map.CleanupDeadUnits();
        Check(!map.GetUnit(5001), "Dead host not removed");
        Check(SpellMapRestorationTestAccess::Pending(map) == 3, "Expected one message per hosted officer");
        SpellMapRestorationTestAccess::Pump(map);
        Check(casualtyMessages.empty(), "Overwrote an existing information window");
        for (int i = 0; i < 3; ++i)
        {
            messageBusy = false;
            SpellMapRestorationTestAccess::Pump(map);
            Check(casualtyMessages.size() == static_cast<size_t>(i + 1), "Notification queue skipped an officer");
            Check(casualtyMessages.back() == data.GetOriginalText("OTHER", 41) + " " +
                commanderNames[i] + " " + data.GetOriginalText("OTHER", 40), "Non-retail casualty text or wrong name");
            SpellMapRestorationTestAccess::Pump(map);
            Check(casualtyMessages.size() == static_cast<size_t>(i + 1), "Repeated active notification");
        }
        messageBusy = false;
        SpellMapRestorationTestAccess::Pump(map);
        map.CleanupDeadUnits();
        Check(!SpellMapRestorationTestAccess::Pending(map), "Repeated notification after cleanup");
        map.GetUnit(5002)->man = 0; // ordinary company
        map.GetUnit(5013)->is_enemy = true; // enemy host
        map.GetUnit(5013)->man = 0;
        map.CleanupDeadUnits();
        Check(!SpellMapRestorationTestAccess::Pending(map), "Ordinary/enemy casualty announced as allied commander");
        checkBattle(5004, 1, 1, 1);
        checkBattle(5007, 2, 2, 1);
        const auto casualtySave = output / "commander-loss.scsave";
        Check(!map.SaveGameStateToFile(casualtySave.wstring()), "Post-casualty save");
        Check(!map.LoadGameStateFromFile(casualtySave.wstring()), "Post-casualty reload");
        map.HaltUnitRanging(true);
        map.CleanupDeadUnits();
        SpellMapRestorationTestAccess::Pump(map);
        Check(casualtyMessages.size() == 3, "Replayed casualty after reload");
        checkBattle(5007, 2, 2, 1);
        // The last allied host must still show its message before game mode ends.
        for (auto* u : map.units) u->is_enemy = true;
        auto* lastHost = map.GetUnit(5007);
        lastHost->is_enemy = false;
        lastHost->formation_commander_mask = 1;
        lastHost->man = 0;
        map.CleanupDeadUnits();
        map.Tick();
        Check(map.isGameMode() && casualtyMessages.size() == 4 && messageBusy,
            "Last allied commander notification was skipped by game over");
        messageBusy = false;
        map.Tick();
        Check(!map.isGameMode() && !SpellMapRestorationTestAccess::Pending(map),
            "Game over did not resume after the last commander notification");
        map.SetMessageInterface({}, {});

        // Exercise real native mouse handlers at two different render scales.
        strategy.m_originalStrategicScreen = StrategicLevelFrame::OriginalStrategicScreen::Hierarchy;
        strategy.m_originalHierarchyPage = 1;
        auto click = [&](bool right, int x, int y, int scale) {
            strategy.m_originalStrategicDrawRect = wxRect(11, 17, 640 * scale, 480 * scale);
            wxMouseEvent e(right ? wxEVT_RIGHT_DOWN : wxEVT_LEFT_DOWN);
            e.SetPosition(wxPoint(11 + x * scale, 17 + y * scale));
            if (right) strategy.OnOriginalStrategicRightDown(e);
            else strategy.OnOriginalStrategicLeftDown(e);
        };
        strategy.m_originalStrategicDrawRect = wxRect(11, 17, 640, 480);
        wxMouseEvent wheel(wxEVT_MOUSEWHEEL);
        wheel.SetPosition(wxPoint(441, 117));
        wheel.m_wheelRotation = -360;
        wheel.m_wheelDelta = 120;
        strategy.OnOriginalStrategicMouseWheel(wheel);
        Check(strategy.m_originalHierarchyUnitScroll == 9, "Unified roster did not scroll to final commander");
        click(false, 430, 448, 1); // fourteenth commander, beyond former eight-row limit
        Check(strategy.m_originalHierarchySelectedCommanderUid == commanderUid, "Scrolled commander hit test");
        click(false, 330, 450, 1); // page 2
        click(false, 270, 210, 1); // same brigade commander, preserve host
        Check(strategy.GetHierarchyBattleMeta(13, participants).commander_host_mask == 7, "Page 2 commander reassignment detached host");
        click(false, 330, 450, 1); // page 1
        strategy.m_originalHierarchyUnitScroll = 0;
        click(true, 20, 30, 2);
        Check(strategy.GetHierarchyBattleMeta(1, participants).formation_id == 0, "RMB did not remove company");
        for (const auto& record : strategy.CaptureHierarchyAssignments())
            Check(record.assigned_unit_uid != 1 && record.unit_uid != 1, "Removed host survived in an ancestor");
        checkMeta(7, 2, 2, 1);
        checkMeta(13, 3, 4, 3);
        click(false, 430, 30, 1); // first pool company
        click(false, 20, 30, 1); // first battalion company slot
        Check(strategy.GetHierarchyBattleMeta(1, participants).formation_id == 1, "LMB did not add company");
        click(true, 170, 70, 1); // detach battalion host, keep company and commander
        Check(strategy.GetHierarchyBattleMeta(1, participants).formation_id == 1, "Host detach removed company");
        Check(strategy.m_hierarchySlots[strategy.m_hierarchySlotIndex.at("battalion_1_commander")].commander_uid != 0, "Host detach removed commander");
        click(true, 170, 55, 1); // remove commander, keep subordinate companies
        Check(strategy.m_hierarchySlots[strategy.m_hierarchySlotIndex.at("battalion_1_commander")].commander_uid == 0, "RMB did not remove commander");
        Check(strategy.GetHierarchyBattleMeta(1, participants).formation_id == 1, "Commander removal deleted company");
        click(true, 20, 30, 1);
        click(false, 430, 30, 1);
        click(false, 20, 30, 1);
        Check(strategy.GetHierarchyBattleMeta(1, participants).formation_id == 1, "LMB cannot arrange companies before commander");
        Check(strategy.SaveStrategicGameToPath(strategicSave, false) && strategy.LoadStrategicStateFromPath(strategicSave), "Edited hierarchy reload");
        checkMeta(1, 0, 0, 0);
        checkMeta(13, 3, 4, 3);
        std::cout << "PASS: LMB/RMB, both pages, host cleanup, strategic/context/tactical reload, combat bonuses, loss downgrades and queued named commander-death notifications/reload\n";
        return 0;
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
