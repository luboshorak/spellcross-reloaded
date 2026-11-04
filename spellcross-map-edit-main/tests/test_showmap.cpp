// Retail integration test. Arguments: DATA directory, auxiliary data directory,
// mission DEF path, temporary save path. No original resources are modified.
#include "map.h"
#include <wx/init.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <tuple>

static std::vector<unsigned char> Pixels(SpellMap& map, TScroll& scroll)
{
    wxBitmap bitmap(800, 600, 24);
    map.Render(bitmap, &scroll);
    wxImage image = bitmap.ConvertToImage();
    return {image.GetData(), image.GetData() + 800 * 600 * 3};
}

int main(int argc, char** argv)
{
    if (argc != 5) return 2;
    wxInitializer wx;
    if (!wx) return 2;
    namespace fs = std::filesystem;
    try
    {
        std::wstring root = fs::path(argv[1]).wstring();
        std::wstring aux = fs::path(argv[2]).wstring();
        SpellData data(root, root, aux);
        SpellMap map;
        auto path = fs::path(argv[3]).wstring();
        if (map.Load(path, &data)) throw std::runtime_error(map.GetLastError());
        map.SetGameMode(1);
        map.HaltUnitRanging(true);
        map.SelectUnit(nullptr);
        map.SetHUDstate(false);
        map.SetRender(true, true, true, true, false, true, false, false, false, false);
        MapUnit* enemy = nullptr;
        for (auto* unit : map.units)
            if (unit->is_enemy && !unit->isDead()) { enemy = unit; break; }
        if (!enemy) throw std::runtime_error("Test mission has no enemies");

        // Keep a valid, nonempty player view; the distant enemy remains hidden.
        map.unit_view->view.assign(map.unit_view->view.size(), 0);
        map.unit_view->view.at(0) = 2;
        for (auto* unit : map.units)
            if (unit->is_enemy) { unit->is_visible = 0; unit->was_seen = false; }
        const auto fog = map.unit_view->view;
        const auto explored = map.unit_view->view_mem;
        std::vector<std::tuple<int, int, bool>> visibility;
        for (auto* unit : map.units)
            visibility.emplace_back(unit->id, unit->is_visible, unit->was_seen);

        TScroll scroll;
        scroll.Reset();
        scroll.SetSurface(800, 600);
        map.ScrollToUnit(enemy, &scroll);
        const auto hidden = Pixels(map, scroll);
        if (map.IsShowMapCheatEnabled()) throw std::runtime_error("Cheat enabled by default");
        map.SetShowMapCheat(true);
        const auto revealed = Pixels(map, scroll);
        if (hidden == revealed) throw std::runtime_error("SHOWMAP did not reveal terrain");
        // In this enemy viewport, disabling unit rendering must remove the
        // enemy sprite even though its real is_visible state remains hidden.
        map.SetRender(true, true, true, true, false, false, false, false, false, false);
        const auto terrainOnly = Pixels(map, scroll);
        if (terrainOnly == revealed) throw std::runtime_error("SHOWMAP did not reveal units");
        map.SetRender(true, true, true, true, false, true, false, false, false, false);
        map.SetShowMapCheat(false);
        if (Pixels(map, scroll) != hidden) throw std::runtime_error("Fog not restored after second toggle");
        if (map.unit_view->view != fog || map.unit_view->view_mem != explored)
            throw std::runtime_error("SHOWMAP changed real fog/exploration");
        for (size_t i = 0; i < map.units.size(); ++i)
            if (visibility[i] != std::make_tuple(map.units[i]->id,
                map.units[i]->is_visible, (bool)map.units[i]->was_seen))
                throw std::runtime_error("SHOWMAP leaked enemy visibility");
        map.SetShowMapCheat(true);
        const auto save = fs::path(argv[4]);
        if (map.SaveGameStateToFile(save.wstring()) || map.LoadGameStateFromFile(save.wstring()))
            throw std::runtime_error("Save/load failed");
        if (map.IsShowMapCheatEnabled()) throw std::runtime_error("SHOWMAP leaked into loaded save");
        fs::remove(save);
        map.SetShowMapCheat(true);
        if (map.Load(path, &data) || map.IsShowMapCheatEnabled())
            throw std::runtime_error("SHOWMAP leaked into next mission");
        std::cout << "PASS: terrain/enemies revealed, exact fog restored, visibility untouched, save/mission reset\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
