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
    def test_folded_udes_only_is_excluded(self):
        self.assertRegex(RULES, r"unitId\s*!=\s*35")
        self.assertNotRegex(RULES, r"unitId\s*!=\s*36")

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
