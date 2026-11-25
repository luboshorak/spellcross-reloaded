// Integration regression: use retail resources to exercise the actual save/load
// path. Arguments: DATA directory, auxiliary data directory, extracted COMMON
// directory, and a writable temporary save filename. No source data is modified.
#include "map.h"
#include <wx/init.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

struct ObjectiveSnapshot
{
    std::wstring label;
    std::string type_name;
    int type;
    MapXY position;
    int done;
};

int main(int argc, char** argv)
{
    std::cout.setf(std::ios::unitbuf);
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
        const fs::path save(argv[4]);
        const char* missions[] = {
            "M01_01A.DEF", "M02_04A.DEF", "M03_09A.DEF",
            "M03_13A.DEF", "M04_10A.DEF"
        };
        for (const char* mission : missions)
        {
            auto path = (fs::path(argv[3]) / mission).wstring();
            if (map.Load(path, &data)) throw std::runtime_error(map.GetLastError());
            std::vector<ObjectiveSnapshot> expected;
            for (auto* event : map.events->GetEvents())
            {
                if (!event->is_objective) continue;
                if (event->label.empty()) throw std::runtime_error("Missing DEF label");
                event->is_done = (expected.size() % 2 == 0);
                expected.push_back({event->label, event->type_name,
                    (int)event->evt_type, event->position, event->is_done});
            }
            if (expected.empty()) throw std::runtime_error("No DEF objectives");
            // The existing binary format intentionally omits objective labels.
            // Thus this exercises compatibility with already affected saves too.
            for (int cycle = 0; cycle < 2; ++cycle)
            {
                if (cycle == 1)
                {
                    // Event cleanup after combat can shift serialized indices.
                    // Spatial objectives must also survive a missing sibling.
                    for (auto* event : map.events->GetEvents())
                        if (!event->is_objective && event->units.empty())
                        {
                            map.events->EraseEvent(event);
                            break;
                        }
                    for (auto* event : map.events->GetEvents())
                        if (event->is_objective && event->evt_type == SpellMapEventRec::EVT_SEE_PLACE)
                        {
                            for (auto it = expected.begin(); it != expected.end(); ++it)
                                if (it->type == (int)event->evt_type && it->position == event->position)
                                {
                                    expected.erase(it);
                                    break;
                                }
                            map.events->EraseEvent(event);
                            break;
                        }
                }
                if (map.SaveGameStateToFile(save.wstring()))
                    throw std::runtime_error("Save failed");
                auto other = (fs::path(argv[3]) / "M02_02A.DEF").wstring();
                if (map.Load(other, &data)) throw std::runtime_error("Other map load failed");
                if (map.LoadGameStateFromFile(save.wstring()))
                    throw std::runtime_error("Save load failed: " + map.GetLastError());
                size_t index = 0;
                for (auto* event : map.events->GetEvents())
                {
                    if (!event->is_objective) continue;
                    if (index >= expected.size()) throw std::runtime_error("Extra objective");
                    const auto& want = expected[index++];
                    if (event->label != want.label || event->type_name != want.type_name ||
                        (int)event->evt_type != want.type || event->position != want.position ||
                        event->is_done != want.done || event->objective_notified != (want.done != 0))
                        throw std::runtime_error(std::string(mission) + ": objective metadata/state lost");
                }
                if (index != expected.size()) throw std::runtime_error("Missing objective");
            }
            std::cout << "PASS " << mission << " (" << expected.size() << " objectives, two round trips)\n";
        }
        fs::remove(save);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
