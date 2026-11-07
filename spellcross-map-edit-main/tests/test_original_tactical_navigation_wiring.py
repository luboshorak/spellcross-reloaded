"""Guard the recovered DOS edge mask and Magotar stance against regressions.

The binary-equivalent decision inputs are independently exercised by
test_tactical_navigation_rules.cpp. These checks ensure the runtime actually
*uses* those rules, including AI and human A* search, without requiring ISO
assets or a running wxWidgets GUI.
"""
from pathlib import Path
import unittest

BASE = Path(__file__).resolve().parents[1]
CPP = (BASE / "source/map.cpp").read_text(encoding="utf-8")
HEADER = (BASE / "source/tactical_navigation_rules.h").read_text(encoding="utf-8")


class OriginalTacticalNavigationWiring(unittest.TestCase):
    def test_retail_map_detection_not_grey_tile_percentage(self):
        load = CPP.split("int SpellMap::Load(", 1)[1].split(
            "//////////////\n\t///// L2", 1
        )[0]
        self.assertIn("IsOriginalMissionMapStem(", load)
        self.assertNotIn("paddedEdge", load)
        self.assertNotIn("filler * 10", load)

    def test_both_path_search_and_move_preview_reject_outside(self):
        self.assertGreaterEqual(CPP.count(
            "map->IsTacticalMovementTile("), 3)
        boundary = CPP.split(
            "bool SpellMap::IsTacticalMovementTile(", 1
        )[1].split("// init move range finder", 1)[0]
        self.assertIn("IsWithinRetailNavigationEnvelope(", boundary)
        for name in ("start", "escape", "target"):
            self.assertIn("explicitSquare(" + name + ")", boundary)

    def test_generated_battle_seeds_and_fallbacks_remain_pathfindable(self):
        main = (BASE / "source/main.cpp").read_text(encoding="utf-8")
        create = main.split("bool MainFrame::LoadGeneratedStrategicBattleFromDtaPath(", 1)[1].split(
            "void MainFrame::StartTacticalGame()", 1
        )[0]
        # Spawning happens before SetGameMode(1): always request the full
        # combat boundary even though the editor/map loader is still active.
        self.assertIn("IsTacticalMovementTile(MapXY(x, y), true)", create)
        self.assertIn("spell_map->PlaceUnit(unit, true)", create)
        place = CPP.split("int SpellMap::PlaceUnit(", 1)[1].split(
            "// store new unit", 1
        )[0]
        self.assertIn("require_tactical_tile && !IsTacticalMovementTile(cand, true)", place)
        mask = CPP.split("bool SpellMap::IsTacticalMovementTile(", 1)[1].split(
            "// init move range finder", 1
        )[0]
        self.assertIn("!game_mode && !enforce_combat_bounds", mask)
        self.assertIn("IsWithinRetailNavigationEnvelope(", mask)

    def test_scripted_runtime_spawn_paths_enforce_combat_navigation(self):
        creator = CPP.split("MapUnit* SpellMap::CreateUnit(", 1)[1].split(
            "// add unit to map list", 1
        )[0]
        self.assertIn("PlaceUnit(unit, isGameMode())", creator)

        mission_start = CPP.split("int SpellMap::MissionStartEvent()", 1)[1].split(
            "// execute list of given events", 1
        )[0]
        self.assertIn("PlaceUnit(new_unit, true)", mission_start)

        scripted_events = CPP.split("int SpellMap::ProcEventsList(", 1)[1].split(
            "// get first event at cursor", 1
        )[0]
        self.assertIn("PlaceUnit(new_unit, true)", scripted_events)
        self.assertIn("Scripted event spawn FAILED", scripted_events)

    def test_border_marker_requires_exit_to_regular_pathfinding_area(self):
        placement = CPP.split("int SpellMap::PlaceUnit(", 1)[1].split(
            "// store new unit", 1
        )[0]
        self.assertIn("require_tactical_tile && !IsTacticalMovementTile(cand, true)", placement)
        self.assertIn("auto has_navigation_exit =", placement)
        self.assertIn("IsWithinRetailNavigationEnvelope(", placement)
        self.assertIn("GetNeighborTile8D(current, dir)", placement)
        self.assertIn("if (!IsTacticalMovementTile(next, true))", placement)
        self.assertIn("if (!has_navigation_exit(cand))", placement)
        self.assertLess(placement.index("if (!has_navigation_exit(cand))"),
                        placement.index("unit->coor = cand;"))

    def test_dos_top_bottom_and_left_right_mask(self):
        self.assertIn("x >= 2 && x < width - 3", HEADER)
        self.assertIn("y >= 8 && y < height - 14", HEADER)

    def test_original_magotar_decision_replaces_distance_constants(self):
        code = CPP.split("bool SpellMap::TryEnemyMagotarStance(", 1)[1].split(
            "bool SpellMap::EnemyTurnStep()", 1
        )[0]
        self.assertIn("OriginalFormPreference(strength)", code)
        self.assertIn("OriginalMagotarAction(type, preference)", code)
        self.assertIn("BuildAllianceClusters()", code)
        self.assertIn("path.back().g_cost", code)
        self.assertIn("StartMove_NoRangeCheck(enemy, cheapestSite)", code)
        self.assertIn("unit->action_ap", code.replace("enemy->", "unit->"))
        self.assertNotIn("groundTargetDistance <= 1.5", code)
        self.assertNotIn("goalDistance >= 4.5", code)

    def test_animation_and_ap_not_instant_morph(self):
        code = CPP.split("bool SpellMap::TryEnemyMagotarStance(", 1)[1].split(
            "bool SpellMap::EnemyTurnStep()", 1
        )[0]
        self.assertIn("enemy->CanSpecAction()", code)
        self.assertIn("enemy->action_points -= enemy->unit->action_ap;", code)
        self.assertIn("MapUnit::ACTION_STATE::AIR_LAND", code)
        self.assertIn("MapUnit::ACTION_STATE::AIR_TAKEOFF", code)
        self.assertIn("enemy->PlayAction()", code)
        enemy = CPP.split("bool SpellMap::EnemyTurnStep()", 1)[1]
        self.assertLess(enemy.index("TryEnemyMagotarStance(enemy, *group)"),
                        enemy.index("SelectGroupAttackTarget(enemy, *group)"))


if __name__ == "__main__":
    unittest.main()
