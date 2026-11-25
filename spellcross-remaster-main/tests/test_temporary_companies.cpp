// Retail integration regression. Arguments: DATA directory, auxiliary directory,
// extracted COMMON directory, temporary tactical save filename.
#include "map.h"
#include "campaign_company_identity.h"
#include "forms/form_level.h"
#include "main.h"
#include <wx/init.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>

static void Check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

static std::vector<uint8_t> UnitImage(MapUnit& unit, SpellMap& map)
{
    std::vector<uint8_t> pixels(512 * 512, 0);
    unit.Render(map.terrain, pixels.data(), pixels.data() + pixels.size(),
        256, 256, 512, map.terrain->filter.nullpal, nullptr, map.tiles[0].L1, true);
    return pixels;
}

int main(int argc, char** argv)
{
    std::cout.setf(std::ios::unitbuf);
    if (argc != 5) return 2;
    wxInitializer wx;
    if (!wx) return 2;
    // Existing native page-layout assertions are unrelated to company data;
    // keep this noninteractive regression governed by the explicit checks.
    wxDisableAsserts();
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
        Check(loader.LoadLevelDef((common / "LEVEL_03.DEF").string(), level, &error), "Cannot load retail level");
        std::string rawName;
        for (const auto& event : level.events)
            for (const auto& company : event.add_units)
                if (spellcross_company::OriginalNameToUtf8(company.name) == u8"1.T\u011b\u017ek\u00e1 p\u011bchota")
                    rawName = company.name;
        Check(!rawName.empty(), "Czech retail support fixture missing");
        const std::string name = spellcross_company::OriginalNameToUtf8(rawName);
        Check(!wxString::FromUTF8(name).empty(), "Support name is blank in strategic UI");
        Check(spellcross_company::NormalizeStoredName(rawName) == name, "Legacy saved name not repaired");
        Check(spellcross_company::NormalizeStoredName(name) == name, "UTF-8 name converted twice");
        Check(spellcross_company::NormalizeStoredName(u8"Moje rota \u017dlu\u0165\u00e1sek") == u8"Moje rota \u017dlu\u0165\u00e1sek",
            "Player UTF-8 name damaged");
        Check(spellcross_company::TacticalName(name) == rawName, "Tactical CP895 name lost accents");
        Check(spellcross_company::OriginalNameToUtf8("Support company") == "Support company", "English name damaged");

        SpellMap map;
        auto mission = (common / "M02_04A.DEF").wstring();
        Check(!map.Load(mission, &data), "Cannot load retail tactical map");
        map.HaltUnitRanging(true);
        auto* unit = new MapUnit(&map);
        unit->unit = data.units->GetUnit(1);
        Check(unit->unit != nullptr && !map.start.empty(), "Unit/start fixture missing");
        unit->id = 2000;
        unit->strategic_uid = 777;
        unit->coor = map.start.front();
        unit->is_enemy = false;
        unit->is_active = true;
        unit->man = unit->unit->cnt;
        unit->ResetAP();
        LevelData::PlayerUnitAdd company;
        company.extra = rawName;
        company.temporary = true;
        spellcross_company::ApplyTacticalIdentity(*unit, company);
        Check(unit->spec_type == MapUnitType::VoluntUnit && unit->name == rawName,
            "Deployment lost support identity");
        const auto support = UnitImage(*unit, map);
        unit->spec_type = MapUnitType::MissionUnit;
        Check(support == UnitImage(*unit, map), "Support company lost the blue helper mark");
        unit->spec_type = MapUnitType::SpecUnit;
        Check(support != UnitImage(*unit, map), "Support company confused with red critical unit");
        company.temporary = false;
        company.extra = "-";
        spellcross_company::ApplyTacticalIdentity(*unit, company);
        Check(unit->spec_type == MapUnitType::NormalUnit && unit->name.empty(), "Permanent/default unit mislabelled");
        Check(support != UnitImage(*unit, map), "Support mark is missing or applied to permanent units");
        company.temporary = true;
        company.extra = rawName;
        spellcross_company::ApplyTacticalIdentity(*unit, company);
        map.units.push_back(unit);
        map.SortUnits();
        {
            // Exercise the real strategic save importer and repair a battle
            // from an older build, where deployment cleared the support type.
            MainFrame controller(&map, &data);
            StrategicLevelFrame strategy(&controller, level, true);
            const std::string legacy = std::string("{\"units\":[{\"unit_id\":1,\"count\":1,\"health\":89,") +
                "\"uid\":777,\"temporary\":true,\"custom_name\":\"" + rawName + "\"}]}";
            StrategicLevelFrame::PendingMissionResult pending;
            Check(strategy.ImportBattleSaveContext(legacy, pending), "Cannot import legacy strategic context");
            std::string defPath, normalized;
            Check(strategy.ExportBattleSaveContext(defPath, normalized, pending), "Cannot export strategic context");
            Check(normalized.find(name) != std::string::npos && normalized.find(rawName) == std::string::npos,
                "Strategic load/save did not repair the legacy CP895 name");
            unit->spec_type = MapUnitType::NormalUnit;
            unit->name.clear();
            strategy.RestoreTacticalCompanyIdentities(map);
            Check(unit->spec_type == MapUnitType::VoluntUnit && unit->name == rawName,
                "Old tactical save not repaired from its campaign UID");
            unit->spec_type = MapUnitType::SpecUnit;
            strategy.RestoreTacticalCompanyIdentities(map);
            Check(unit->spec_type == MapUnitType::SpecUnit, "Campaign repair overwrote a special mission unit");
            unit->spec_type = MapUnitType::NormalUnit;
            unit->strategic_uid = 778;
            strategy.RestoreTacticalCompanyIdentities(map);
            Check(unit->spec_type == MapUnitType::NormalUnit, "Campaign repair guessed an unmatched UID");
            unit->strategic_uid = 777;
            strategy.RestoreTacticalCompanyIdentities(map);
        }
        const fs::path save(argv[4]);
        Check(!map.SaveGameStateToFile(save.wstring()), "Save failed");
        Check(!map.LoadGameStateFromFile(save.wstring()), "Save load failed");
        const auto* restored = map.GetUnit(2000);
        Check(restored && restored->strategic_uid == 777 &&
            restored->spec_type == MapUnitType::VoluntUnit && restored->name == rawName,
            "Support identity lost in tactical save round trip");
        fs::remove(save);
        std::cout << "PASS: retail Czech/English names, legacy names, blue support mark, permanent/special units, tactical save round trip\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
