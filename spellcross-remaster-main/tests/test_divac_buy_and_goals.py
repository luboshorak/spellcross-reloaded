"""Regression tests for Divac1 #120 (UDES) and #122 (convoy goals).

Keep tests self-contained: the CI build does not ship the proprietary DOS
COMMON.FS/terrain archives. Their source semantics were verified separately.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
STRATEGY = (ROOT / "source/forms/form_level.cpp").read_text(encoding="utf-8")
MAP = (ROOT / "source/map.cpp").read_text(encoding="utf-8")
RULES = (ROOT / "source/strategic_buy_variant_rules.h").read_text(encoding="utf-8")

class DivacBuyAndGoalRegressions(unittest.TestCase):
    def test_all_tactical_transformations_excluded_from_buy_rule(self):
        self.assertRegex(RULES, r"unitId\s*<\s*37")
        self.assertRegex(RULES, r"unitId\s*>\s*86")
        self.assertRegex(RULES, r"unitId\s*!=\s*8")
        self.assertRegex(RULES, r"unitId\s*!=\s*35")
        # The extra Destructor 73 and enemy Magotar 48 are excluded by
        # the original Alliance type boundary, not by deleting game data.

    def test_rearming_must_not_offer_tactical_only_forms(self):
        rearm = STRATEGY.split(
            "std::vector<int> StrategicLevelFrame::GetAvailableUnitTypesForUpgrade", 1
        )[1].split("void StrategicLevelFrame::BuildHierarchyPage", 1)[0]
        self.assertIn("IsStrategicPurchasableUnitType(i)", rearm)
        can_upgrade = STRATEGY.split("bool StrategicLevelFrame::CanUpgradeUnitTo", 1)[1]
        can_upgrade = can_upgrade.split("std::vector<int> StrategicLevelFrame::GetAvailableUpgradesForUnit", 1)[0]
        self.assertIn("IsStrategicPurchasableUnitType(toUnitId)", can_upgrade)

    def test_native_and_legacy_buy_list_and_purchase_guards(self):
        self.assertGreaterEqual(
            STRATEGY.count("spellcross_strategy::IsStrategicPurchasableUnitType("), 4)
        for start, end in [
            ("BuildOriginalBuyRows()", "EnsureUpgradeDefsLoaded()"),
            ("RefreshBuyShopList()", "RefreshBuyInfo("),
        ]:
            # Only require both rendering paths to consult the shared rule.
            body = STRATEGY.split("void StrategicLevelFrame::" + start, 1)[-1]
            if body == STRATEGY:
                body = STRATEGY.split("std::vector<StrategicLevelFrame::OriginalBuyRow> StrategicLevelFrame::" + start, 1)[-1]
            self.assertIn("IsStrategicPurchasableUnitType", body[:7000])
    

    def test_radar_1r_and_2r_are_distinct_models_not_toggle_forms(self):
        import json
        all_units = json.loads((ROOT / "data/units.json").read_text(encoding="utf-8"))
        radar1, radar2 = all_units[30], all_units[31]
        self.assertEqual((radar1["name"], radar2["name"]), ("Radar 1R", "Radar 2R"))
        self.assertEqual((radar1["special_act_id"], radar2["special_act_id"]), (1, 1))
        self.assertEqual((radar1["special_act_params"][-1], radar2["special_act_params"][-1]), (11, 14))
        self.assertNotEqual(radar1["buy_id"], radar2["buy_id"])
        self.assertIn("30, 31", (ROOT / "tests/test_strategic_buy_variant_rules.cpp").read_text(encoding="utf-8"))
        self.assertIn("unitId < 37", RULES)

    def test_target_sprite_falls_back_to_original_ciel_art(self):
        self.assertEqual(MAP.count('target_sprite = terrain->GetSprite("TARGET");'), 2)
        self.assertEqual(MAP.count("target_sprite = escape_sprite;"), 2)
    
    def test_goal_markers_ignore_fog_but_start_does_not(self):
        special = MAP.split("// --- Render special tiles - START/ESCAPE/TARGET---", 1)[1]
        special = special.split("// --- Render selection tiles", 1)[0]
        self.assertIn("sid == 0 && unit_view->view[mxy] == 0", special)
        self.assertIn("sid != 0", special)
        self.assertIn("fil = default_filter", special)
        self.assertNotIn("if (use_view_mask && unit_view->view[mxy] == 0)", special)

if __name__ == "__main__":
    unittest.main()
