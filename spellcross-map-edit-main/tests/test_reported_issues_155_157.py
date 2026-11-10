"""Reported #155-157 regression wiring for the desktop integration branch.

Source checks supplement the stand-alone C++ battle recovery test; these do
not replace a Windows build or retail CZ/EN playthrough.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1] / "source"
STRATEGY = (ROOT / "forms" / "form_level.cpp").read_text(encoding="utf-8")
TACTICAL = (ROOT / "spell_units.cpp").read_text(encoding="utf-8")
AFTERMATH = (ROOT / "strategic_battle_recovery_rules.h").read_text(encoding="utf-8")


class ReportedIssues155157Tests(unittest.TestCase):
    def test_155_recovery_applies_to_both_existing_and_new_units(self):
        self.assertIn('#include "../strategic_battle_recovery_rules.h"', STRATEGY)
        results = STRATEGY.split("StrategicLevelFrame::LossBlock StrategicLevelFrame::CollectAndApplyBattleResults(", 1)[1]
        self.assertEqual(results.count("spellcross_strategy::RecoveredStrategicHealth("), 2)
        self.assertIn("survivor->wounded", results)
        self.assertIn("s->wounded", results)
        self.assertRegex(AFTERMATH, r"victory\s*\?\s*std::clamp\(wounded")

    def test_156_commander_label_cannot_resize_tree_nodes(self):
        self.assertIn("wxST_ELLIPSIZE_END | wxST_NO_AUTORESIZE", STRATEGY)
        label_method = STRATEGY.split("void StrategicLevelFrame::UpdateCommanderHierarchyLabel(", 1)[1].split(
            "void StrategicLevelFrame::OnHierarchyTogglePage(", 1)[0]
        self.assertNotIn('base += wxString::Format(" → %s"', label_method)
        self.assertIn("host.unit_uid = cs.assigned_unit_uid;", label_method)
        self.assertIn("cs.label->SetLabel(base);", label_method)

    def test_157_retail_marker_palette_separates_lifetime_types(self):
        marker = TACTICAL.split("// render special unit type mark", 1)[1].split(
            "// render fire count", 1
        )[0]
        self.assertRegex(marker, r"MapUnitType::MissionUnit\)\s*type_color\s*=\s*230")
        self.assertRegex(marker, r"MapUnitType::VoluntUnit\)\s*type_color\s*=\s*255")
        self.assertRegex(marker, r"MapUnitType::SpecUnit\)\s*type_color\s*=\s*253")


if __name__ == "__main__":
    unittest.main()
