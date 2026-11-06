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
