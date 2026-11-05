// Retail integration. Arguments: DATA, auxiliary data, extracted COMMON, output.
#include "map.h"
#include "forms/form_level.h"
#include "main.h"
#include "unit_health_display.h"
#include <wx/init.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

struct SpellMapRestorationTestAccess
{
    static bool StartAttack(SpellMap& map, MapUnit* attacker, MapUnit* target)
    { return map.StartAttack_NoHUD(attacker, target); }
};

static void Check(bool ok, const char* message)
{
    if (!ok) throw std::runtime_error(message);
}
static std::vector<unsigned char> Read(const std::filesystem::path& p)
{
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), {}};
}

int main(int argc, char** argv)
{
    std::cout.setf(std::ios::unitbuf);
    if (argc != 5) return 2;
    wxInitializer wx;
    if (!wx) return 2;
    wxDisableAsserts();
    wxInitAllImageHandlers();
    namespace fs = std::filesystem;
    try
    {
        const fs::path common(argv[3]), output(argv[4]);
        fs::create_directories(output);
        // Match a packaged game's data/ layout. Pricing lookup uses cwd/data
        // when the unbundled integration executable has no adjacent data/.
        fs::current_path(fs::absolute(fs::path(argv[2])).parent_path());
        Check(fs::exists("data/units.json"), "Bundled units.json fixture missing");
        SpellData data(fs::path(argv[1]).wstring(), fs::path(argv[1]).wstring(), fs::path(argv[2]).wstring());
        LevelData level;
        LevelLoader loader;
        std::string error;
        Check(loader.LoadLevelDef((common / "LEVEL_03.DEF").string(), level, &error), "Load retail level");
        fs::copy_file(common / "LEVEL_03.DEF", output / "LEVEL_03.DEF", fs::copy_options::overwrite_existing);
        level.source_path = (output / "LEVEL_03.DEF").string();
        SpellMap map;
        Check(!map.Load((common / "M02_04A.DEF").wstring(), &data), "Load retail battle");
        map.HaltUnitRanging(true);
        MainFrame controller(&map, &data);
        StrategicLevelFrame strategy(&controller, level, true);
        strategy.m_playerUnits.clear(); strategy.m_unitStates.clear();
        strategy.RestoreHierarchyAssignments({});
        for (int i = 0; i < 5; ++i)
        {
            LevelData::PlayerUnitAdd company;
            company.unit_id = 1; company.count = 1; company.health = 100; company.extra = "-";
            strategy.m_playerUnits.push_back(company);
            StrategicLevelFrame::UnitInstanceState state;
            state.uid = i + 1; state.temporary = i == 4;
            strategy.m_unitStates.push_back(state);
            if (i < 4)
                Check(strategy.AssignUnitToHierarchySlot("battalion_1_unit_" + std::to_string(i+1), i+1, "Company"), "Arrange hierarchy");
        }
        const auto palette = Read(common / "_SHARED1.PAL");
        Check(palette.size() == 384, "Retail shared palette");
        const char* assets[] = {nullptr, "UF_ATACK.ICO", "UF_RECRU.ICO", "UF_UPGRD.ICO", "UF_BUY.ICO"};
        using Screen = StrategicLevelFrame::OriginalStrategicScreen;
        auto render = [&](Screen screen) {
            strategy.m_originalStrategicScreen = screen;
            strategy.m_originalStrategicDirty = true;
            strategy.RefreshOriginalStrategicView();
            Check(strategy.m_originalStrategicBitmap.IsOk(), "Original strategic render");
            return strategy.m_originalStrategicBitmap.ConvertToImage();
        };
        auto pixel = [&](const wxImage& image, int x, int y, int index) {
            Check(index >= 0 && index < 128, "UF symbol uses unexpected palette bank");
            Check(image.GetRed(x,y) == palette[index*3] && image.GetGreen(x,y) == palette[index*3+1] &&
                image.GetBlue(x,y) == palette[index*3+2], "Retail pixel mismatch");
        };
        int testedPixels = 0;
        for (Screen screen : {Screen::Hierarchy, Screen::Units, Screen::Buy})
        {
            std::cout << "Testing screen " << static_cast<int>(screen) << '\n';
            strategy.m_unitsSelectedUnit = -1;
            strategy.m_playerUnits[0].health = 0;
            const wxImage emptyBar = render(screen);
            for (int reason = 1; reason <= 4; ++reason)
                for (int health : {0, 1, 25, 50, 75, 99, 100})
                {
                    strategy.m_playerUnits[0].health = health;
                    strategy.m_unitStates[0].cooldown_turns = 3;
                    strategy.m_unitStates[0].cooldown_reason = reason;
                    Check(std::string(strategy.GetUnitStatusIconAsset(0)) == assets[reason], "State-to-icon mapping");
                    const wxImage image = render(screen);
                    const bool hierarchy = screen == Screen::Hierarchy;
                    const int cellX = hierarchy ? 12 : 16, cellY = hierarchy ? 25 : 11;
                    const int barX = 38, barY = cellY - 1;
                    const int width = health * 27 / 100;
                    for (int y = 0; y < 3; ++y)
                        for (int x = 0; x < width; ++x) { pixel(image, barX+x, barY+y, 122); ++testedPixels; }
                    for (int y = 0; y < 3; ++y)
                        for (int x = width; x < 27; ++x)
                            Check(image.GetRed(barX+x,barY+y) == emptyBar.GetRed(barX+x,barY+y) &&
                                image.GetGreen(barX+x,barY+y) == emptyBar.GetGreen(barX+x,barY+y) &&
                                image.GetBlue(barX+x,barY+y) == emptyBar.GetBlue(barX+x,barY+y), "Health bar extends past current strength");
                    // Decode the original sparse stream independently at its DOS
                    // draw origin. Every opaque icon pixel must match exactly.
                    const auto icon = Read(common / assets[reason]);
                    Check(icon.size() >= 7, "UF icon fixture");
                    size_t pos = 7;
                    const int yOffset = static_cast<int16_t>(icon[0] | (icon[1] << 8));
                    for (int y = 0; y < icon[6]; ++y)
                    {
                        int x = 0;
                        while (icon.at(pos) != 255)
                        {
                            x += icon.at(pos++);
                            const int count = icon.at(pos++);
                            for (int n = 0; n < count; ++n)
                            {
                                const int value = icon.at(pos++);
                                pixel(image, cellX+1+x++, cellY+1+yOffset+y, value);
                                ++testedPixels;
                            }
                        }
                        ++pos;
                    }
                    if (health == 50)
                        image.SaveFile((output / (std::to_string(static_cast<int>(screen)) + "-" + assets[reason] + ".png")).wstring(), wxBITMAP_TYPE_PNG);
                }
        }
        // Core and support rosters use the same native indicator geometry.
        strategy.m_unitStates[4].cooldown_turns = 2;
        strategy.m_unitStates[4].cooldown_reason = 4;
        strategy.m_playerUnits[4].health = 50;
        pixel(render(Screen::Units), 38, 320, 122);
        for (int reason = 1; reason <= 4; ++reason)
        {
            strategy.m_unitStates[0].cooldown_turns = 2;
            strategy.m_unitStates[0].cooldown_reason = reason;
            const fs::path save = output / "status.json";
            Check(strategy.SaveStrategicGameToPath(save, false), "Save status reason");
            strategy.m_unitStates[0].cooldown_reason = 0;
            Check(strategy.LoadStrategicStateFromPath(save), "Reload status reason");
            Check(std::string(strategy.GetUnitStatusIconAsset(0)) == assets[reason], "Reason lost after reload");
            std::string path, context;
            StrategicLevelFrame::PendingMissionResult pending;
            Check(strategy.ExportBattleSaveContext(path, context, pending) && strategy.ImportBattleSaveContext(context, pending), "Campaign context status roundtrip");
            Check(std::string(strategy.GetUnitStatusIconAsset(0)) == assets[reason], "Embedded reason lost");
            strategy.ApplyUnitsCooldownTick(); strategy.ApplyUnitsCooldownTick();
            Check(!strategy.GetUnitStatusIconAsset(0), "Expired status still visible");
        }
        strategy.m_unitStates[0].cooldown_turns = 2;
        strategy.m_unitStates[0].cooldown_reason = 0;
        Check(!strategy.GetUnitStatusIconAsset(0), "Guessed an ambiguous legacy reason");
        strategy.m_unitStates[0].pending_kind = 1;
        Check(std::string(strategy.GetUnitStatusIconAsset(0)) == "UF_RECRU.ICO", "Legacy recruit metadata ignored");
        strategy.m_unitStates[0].pending_kind = 2;
        Check(std::string(strategy.GetUnitStatusIconAsset(0)) == "UF_UPGRD.ICO", "Legacy upgrade metadata ignored");
        strategy.m_unitStates[0].pending_kind = 1;
        strategy.m_unitStates[0].pending_value = 2;
        strategy.m_playerUnits[0].health = 40;
        strategy.m_unitStates[0].cooldown_turns = 1;
        strategy.ApplyUnitsCooldownTick();
        Check(strategy.m_playerUnits[0].health == 100 && !strategy.GetUnitStatusIconAsset(0), "Recruitment did not restore health/status");

        MapUnit* single = nullptr;
        // The actual rescued/special officer type, rather than a generic monster.
        for (int id : {79})
        {
            auto* type = data.units->GetUnit(id);
            if (type && type->isSingleMan() && type->isLand())
            {
                single = new MapUnit(&map); single->unit = type;
            }
        }
        Check(single != nullptr, "Single-member retail fixture");
        single->id = 7000; single->man = 1; single->coor = map.start.front();
        single->is_enemy = false; single->is_active = true; single->ResetAP();
        map.units.push_back(single); map.SelectUnit(single);
        map.SetHUDstate(true);
        auto checkHud = [&](MapUnit* u, int expected) {
            map.SelectUnit(u);
            std::vector<uint8_t> pixels(640*480, 0);
            MapXY cursor(-1,-1);
            map.RenderHUD(pixels.data(), pixels.data()+pixels.size(), 640, &cursor, nullptr, 640,480);
            const int top = 480 - data.gres.wm_hud->y_size;
            for (int y = 0; y < 7; ++y)
            {
                int green = 0;
                for (int x = 0; x < 112; ++x) green += pixels[(top+54+y)*640+118+x] == 235;
                Check(green == expected, "Tactical HUD health pixels");
            }
        };
        for (int damage : {0,1,250,500,999})
        {
            single->man = 1; single->wounded = 0; single->damage_remainder = damage;
            checkHud(single, 112*(1000-damage)/1000);
        }
        single->damage_remainder = 500;
        Check(single->HasWounded() && single->Heal() == 500, "Single-member healing");
        checkHud(single,112);
        single->damage_remainder = 375;
        map.SortUnits();
        const fs::path tacticalSave = output / "health.scsave";
        Check(!map.SaveGameStateToFile(tacticalSave.wstring()) && !map.LoadGameStateFromFile(tacticalSave.wstring()), "Tactical health save/load");
        map.HaltUnitRanging(true);
        single = map.GetUnit(7000);
        Check(single && single->damage_remainder == 375, "Fractional damage lost");
        checkHud(single,70);
        single->man = 0; checkHud(single,0); single->man = 1;
        MapUnit multi(&map); multi.unit = data.units->GetUnit(1); multi.is_enemy = false;
        multi.man = multi.unit->cnt / 2; multi.wounded = 2; multi.ResetAP();
        checkHud(&multi, multi.man*112/multi.unit->cnt);
        multi.man = 0; multi.wounded = 0; checkHud(&multi,0);
        map.SelectUnit(single);

        Sprite* flat = nullptr; Sprite* slope = nullptr;
        for (const auto& tile : map.tiles)
            if (tile.L1) { if (tile.L1->GetSlope() == 'A') flat = tile.L1; else slope = tile.L1; }
        Check(flat && slope, "Retail flat/slope sprites");
        MapUnit attacker(&map), target(&map);
        target.unit = data.units->GetUnit(1); target.man = target.unit->cnt; target.is_enemy = true;
        attacker.is_enemy = false; attacker.coor = map.start.front(); target.coor = attacker.coor;
        target.coor.x += 5;
        const int tileIndex = map.ConvXY(attacker.coor);
        const auto originalTile = map.tiles[tileIndex];
        for (int id : {5,24,25,26,27,28,54})
        {
            attacker.unit = data.units->GetUnit(id); attacker.man = attacker.unit->cnt;
            attacker.ResetAP();
            const bool restricted = (attacker.unit->fire_flags & SpellUnitRec::FIRE_NOT_SLOPES) != 0;
            for (bool enemy : {false,true})
            for (int elevation : {0,5})
                for (bool onSlope : {false,true})
                {
                    attacker.is_enemy = enemy; target.is_enemy = !enemy;
                    map.tiles[tileIndex].L1 = onSlope ? slope : flat;
                    map.tiles[tileIndex].elev = elevation;
                    Check(map.CanUnitFireFromTerrain(&attacker) == !(restricted && onSlope), "Retail fire flag / terrain rule");
                    const bool started = SpellMapRestorationTestAccess::StartAttack(map, &attacker, &target);
                    Check(started == !(restricted && onSlope), "AI/reaction entry bypassed terrain rule");
                    attacker.attack_state = MapUnit::ATTACK_STATE::IDLE; attacker.attack_target = nullptr;
                    if (restricted && onSlope)
                    {
                        map.SelectUnit(&attacker);
                        map.unit_view->attack_map.assign(map.x_size*map.y_size,2); // deliberately stale cache
                        Check(!map.CanUnitAttack(&target), "Player entry trusted stale range on a slope");
                    }
                }
        }
        map.tiles[tileIndex] = originalTile;
        map.SelectUnit(single);
        // Exercise the asynchronous range worker too, not only attack entry.
        attacker.unit = data.units->GetUnit(25); attacker.man = attacker.unit->cnt;
        map.tiles[tileIndex].L1 = slope;
        map.ResumeUnitRanging(true);
        map.unit_view->CalcAttackRange(&attacker, true);
        Check(std::all_of(map.unit_view->attack_map.begin(), map.unit_view->attack_map.end(),
            [](int value) { return value == 0; }), "Artillery range worker exposed sloped shots");
        map.HaltUnitRanging(true);
        map.tiles[tileIndex] = originalTile;
        std::cout << "PASS: " << testedPixels << " retail strategic pixels; all four icons; strategic/context/tactical saves; single/multi health, healing/death; artillery/mortars flat/slope/elevation\n";
        return 0;
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}

