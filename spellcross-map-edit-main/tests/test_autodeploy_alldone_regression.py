"""Autodeploy and alldone source wiring regressions.

The matcher itself is compiled and executed by test_autodeploy_rules.cpp.
"""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "source/main.cpp").read_text(encoding="utf-8")
MAP = (ROOT / "source/map.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "source/game_autodeploy.h").read_text(encoding="utf-8")
STRATEGIC = (ROOT / "source/forms/form_level.cpp").read_text(encoding="utf-8")
HOME = (ROOT / "source/forms/form_mmenu.cpp").read_text(encoding="utf-8")


class AutodeployAndAlldoneRegression(unittest.TestCase):
    def test_off_by_default_and_persistent(self):
        self.assertIn("static bool enabled = false;", HEADER)
        self.assertIn('ini.GetBoolValue("STATE", "autodeploy", false)', MAIN)
        self.assertIn('ini.SetBoolValue("STATE", "autodeploy"', MAIN)
        self.assertIn("autodeploy = false",
            (ROOT / "source/config.example.ini").read_text(encoding="utf-8"))

    def test_setting_visible_in_all_three_menus(self):
        for source in (MAIN, STRATEGIC, HOME):
            self.assertIn('spellcross_ui_locale::Tr("menu.autodeploy"', source)
            self.assertIn("spellcross_autodeploy::SetEnabled(ev.IsChecked())", source)
        for language in ("en", "cs"):
            text = (ROOT / "locales" / (language + ".lang")).read_text(encoding="utf-8")
            self.assertIn("menu.autodeploy=", text)

    def test_matching_planned_before_original_placement(self):
        autodeploy = MAIN.split("bool MainFrame::AutoDeployUnits()", 1)[1].split(
            "void MainFrame::FinishDeployment()", 1)[0]
        self.assertLess(autodeploy.index("spellcross_autodeploy::Assign("),
                        autodeploy.index("PlaceDeploymentUnit("))
        self.assertIn("FinishDeployment();", autodeploy)
        self.assertIn("fixed->unit->isAir() == available.is_air", autodeploy)
        start = MAIN.split("void MainFrame::StartTacticalGame()", 1)[1].split(
            "bool MainFrame::IsDeploymentSquare(", 1)[0]
        self.assertIn("spellcross_autodeploy::Enabled() && !AutoDeployUnits()", start)

    def test_alldone_executes_pending_event_actions(self):
        cheat = MAP.split("bool SpellMap::CompleteAllMissionObjectivesForCheat()", 1)[1].split(
            "void SpellMap::CheckObjectiveNotifications()", 1)[0]
        for marker in ("evt->is_done = true", "message.is_done = true",
                       "evt->HasPendingActions()", "ProcEventsList(pending)",
                       "CheckAndTriggerMissionEnd()"):
            self.assertIn(marker, cheat)
        console = MAIN.split('if (cmd == "ALLDONE")', 1)[1].split(
            'else if (cmd == "AGAINAGAIN")', 1)[0]
        self.assertIn("CompleteAllMissionObjectivesForCheat()", console)


if __name__ == "__main__":
    unittest.main()
