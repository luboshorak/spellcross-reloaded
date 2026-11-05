# Extending Spellcross Reloaded

The restoration's guiding principles are [packaging/restoration-development-principles.md](../packaging/restoration-development-principles.md): distinguish original evidence, inferred behavior, robustness fixes and intentional modern deviations. Check both CZ/EN retail editions before claiming exact parity.

## Add or modify a unit

1. Identify the **unit type** in `JEDNOTKY.DEF` and `SpellUnits::SpellUnits` (`source/spell_units.cpp`). Inspect `SpellUnitRec` flags, attacks, range, air/ground category and special actions.
2. Inspect `UNITS.FSU` resource groups in `FSUarchive::LoadResource` (`source/fsu_archive.cpp`), `data/units.json` and `units.fsa` for any auxiliary unit sprite metadata.
3. Create **instances** with `SpellMap::CreateUnit`, `AddUnit` and `PlaceUnit` (`source/map.cpp`), preserving event/map indexes. Do not confuse `type_id` with tactical `MapUnit::id` or persistent strategic UID.
4. If the unit can be purchased or researched, update `StrategicLevelFrame::EnsureUnitCostsLoaded`, `GetUnitBuyCost`, `IsCampaignUnitUnlocked`, `EnsureResearchLoaded` and relevant upgrade rules (`forms/form_level.cpp`).
5. Test visibility, movement, slopes, attacks, death, AP, formations, temporary-company status, saved state and chapter transfer.

The strategic-to-tactical transfer object is `LevelData::PlayerUnitAdd` (`source/level.h`). Example:

```cpp
LevelData::PlayerUnitAdd company;
company.unit_id = unitTypeId;
company.count = 1;
company.health = 100;
company.strategic_uid = rosterUid; // only for a persistent roster company
company.temporary = chapterSupport;
```

Test coverage starts with `tests/test_campaign_unit_unlock.cpp`, `test_unit_health_display.cpp`, `test_temporary_companies.cpp` and `test_hierarchy_roundtrip.cpp`.

## Add terrain, an object or a mission

`SpellMap::Load/SaveDTA` owns tactical map data; `SpellDEF` and `SpellMapEvents::AddMissionObjective/AddSpecialEvent` own scripted tactical directives; `LevelLoader::LoadLevelDef` in `source/level.cpp` owns strategic `LEVEL_XX.DEF`. Add parse support **and** a caller that consumes the new field. Check `LevelData::unknown_lines` after parsing new directives.

For generated defensive/recapture battles `MainFrame::LoadGeneratedStrategicBattleFromDtaPath` differs from loading an authored DEF; it uses source DTA deployment data instead of resurrecting a story mission. Keep `tests/test_recapture_mission_lifecycle.cpp` and the existing `doc/STRATEGIC_ISSUE_57_DEFENCE_DEPLOYMENT.md` acceptance checklist applicable.

## Add graphics or a restored strategic page

First inventory the retail `COMMON.FS` asset and exact palette; do not redraw original chrome unless necessary. Static original-screen composition belongs in `StrategicOriginalRenderer::Render*` (`source/strategic_original_renderer.cpp`), while live company names, values, interactions and list scroll are in `StrategicLevelFrame::RefreshOriginalStrategicView` and `OnOriginalStrategicLeftDown` (`forms/form_level.cpp`). UI coordinates are authored in **640×480**, then mapped by `m_originalStrategicDrawRect`. Review the QH/CLK coordinate research under `source/spellcross_ui_coordinate_catalog/`.

```cpp
// Contract declared by StrategicOriginalRenderer:
using AssetLoader = std::function<bool(
    const std::string& name, std::vector<std::uint8_t>& out)>;
```

For tactical art use `Terrain::Load`/`Sprite::Render` or `SpellGraphics::AddICO/AddPNM`, respecting indexed palettes, fog-of-war filters, layer order and elevation. See [rendering](rendering.md) and [coordinates](coordinates.md).

## Add UI text or a language

Keep authored UI in `locales/en.lang` and `locales/cs.lang` with stable UTF-8 keys and call `spellcross_ui_locale::Tr(key, EnglishFallback)` (`source/spell_ui_locale.h`). Commercial narrative and research text comes from retail archives, not `locales/`. Run `tests/test_ui_localization_catalog.py` and test labels at real CZ and EN widths. See [localization guide](../spellcross-map-edit-main/locales/README.md).

## Add a C++ module or persistent field

- Add new `.cpp/.h` to `source/spellcross_map_editor.vcxproj` and `.vcxproj.filters`; there is no automatic project glob.
- Keep wx timer/input (`MainFrame`), model (`SpellMap`/`StrategicLevelFrame`) and rendering (graphics classes) responsibilities separate.
- Coordinate worker locks around view/range tasks; do not mutate the tactical model from `OnPaintCanvas`.
- Persistent tactical fields require consideration of `SpellMap::SaveGameStateToFile/LoadGameStateFromFile` and binary snapshot versioning. Persistent strategic fields require `SaveStrategicStateFile` / `LoadStrategicStateFromPath` and compatibility with embedded tactical campaign context.
- Prefer focused testable logic, as in `source/strategic_mission_flow.h` and `tests/test_original_strategy_mission_flow.cpp`, to further growing already very large controller translation units.
- Record original evidence or explicit uncertainty in [verification](verification.md), and add both a focused test and a realistic end-to-end regression.

A passing unit test alone does not prove original-game parity or successful Windows packaging.
