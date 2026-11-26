"""Do not inherit an interrupted enemy phase or mission markers when
switching from one tactical mission to the next (convoy deployment regression).

These source-level checks supplement native Windows compilation and exercise
the mission transitions that cannot be run without original ISO archives.
"""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
MAP = (ROOT / "source/map.cpp").read_text(encoding="utf-8")
MAIN = (ROOT / "source/main.cpp").read_text(encoding="utf-8")


class MissionSessionResetTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.close = MAP.split("void SpellMap::Close()", 1)[1].split(
            "// create blank map", 1
        )[0]
        cls.hud = MAP.split("// Render the original enemy-turn HUD", 1)[1].split(
            "// render side fillings", 1
        )[0]
        cls.mission = MAP.split("int SpellMap::MissionStartEvent()", 1)[1].split(
            "// execute list of given events", 1
        )[0]

    def test_new_map_resets_inherited_phase(self):
        for name in (
            "game_mode = false;",
            "enemy_turn_running = false;",
            "enemy_turn_list.clear();",
            "enemy_turn_idx = 0;",
            "enemy_turn_prev_selection = nullptr;",
            "panic_turn_running = false;",
            "panic_turn_list.clear();",
            "reaction_fire_queue.clear();",
            "reaction_fire_active = false;",
        ):
            with self.subTest(name=name):
                self.assertIn(name, self.close)

    def test_no_stale_goal_or_range_overlay(self):
        for marker in ("start.clear();", "escape.clear();", "target.clear();",
                       "unit_range_view_mode_lock = false;",
                       "unit_range_temp_active = 0;"):
            self.assertIn(marker, self.close)

    def test_enemy_hud_cannot_hide_deployment_confirmation(self):
        self.assertIn(
            "isGameMode() && enemy_turn_running && !deployment_mode", self.hud
        )

    def test_convoy_spawn_after_deployment_and_failure_visible(self):
        start = MAIN.split("void MainFrame::StartTacticalGame()", 1)[1].split(
            "// The original Spellcross allowed placement", 1
        )[0]
        finish = MAIN.split("void MainFrame::FinishDeployment()", 1)[1].split(
            "// File > Load custom map", 1
        )[0]
        self.assertIn("if (!m_deployment_active)", start)
        self.assertIn("spell_map->MissionStartEvent();", finish)
        self.assertIn("MissionStart spawn FAILED", self.mission)
        self.assertIn("expected_units=%llu placed_units=%llu failed_units=%llu",
                      self.mission)


if __name__ == "__main__":
    unittest.main()
