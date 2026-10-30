"""Regression guards for strategic ownership, enemy attack, and active defence.

Run: python -m unittest discover -s spellcross-map-edit-main/tests -p 'test_*.py'
Source-level tests, not a replacement for a Windows/MSVC build or in-game test.
"""
from pathlib import Path
import re
import unittest

SOURCE = (Path(__file__).resolve().parents[1] / "source" / "forms" / "form_level.cpp")
HEADER = SOURCE.with_suffix(".h")


class StrategicContestedTerritoryRegression(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = SOURCE.read_text(encoding="utf-8")
        cls.header = HEADER.read_text(encoding="utf-8")

    def test_active_counterattack_is_not_a_scheduled_attack(self):
        self.assertIn("bool StrategicLevelFrame::HasActiveCounterAttack(int territoryId) const",
                      self.source)
        self.assertIn("attack.territory_id == territoryId && attack.triggered && !attack.completed",
                      self.source)

    def test_active_defence_is_hatched_without_changing_ownership(self):
        self.assertIn("const bool contested = m_gameModeEnabled && HasActiveCounterAttack(tid);",
                      self.source)
        self.assertRegex(self.source, r"visible && \(!owned \|\| contested\)\s*\?"
                         r"\s*StrategicOriginalRenderer::TerritoryVisualState::EnemyHatched")
        self.assertIn("owned ? StrategicOriginalRenderer::TerritoryVisualState::Revealed",
                      self.source)
        self.assertIn("if (visible && (!owned || HasActiveCounterAttack(tid)))",
                      self.source)

    def test_attack_command_checks_actual_battle_availability(self):
        self.assertIn("bool StrategicLevelFrame::CanLaunchTerritoryBattle(int territoryId) const",
                      self.source)
        self.assertIn("return !owned || HasActiveCounterAttack(territoryId);", self.source)
        self.assertIn("if (!CanLaunchTerritoryBattle(m_selectedTerritory))", self.source)
        self.assertIn("CanLaunchTerritoryBattle(m_selectedTerritory) && hasSelection",
                      self.source)
        self.assertIn("const bool canAttack = CanLaunchTerritoryBattle(m_selectedTerritory)",
                      self.source)

    def test_editor_mode_and_visibility_are_preserved(self):
        self.assertRegex(self.source, r"if \(!m_gameModeEnabled\)\s*return true;")
        self.assertIn("m_visibleTerritory[territoryId] == 0", self.source)

    def test_battle_logic_is_shared_by_renderer_and_controls(self):
        self.assertIn("bool HasActiveCounterAttack(int territoryId) const;", self.header)
        self.assertIn("bool CanLaunchTerritoryBattle(int territoryId) const;", self.header)
        self.assertGreaterEqual(
            self.source.count("CanLaunchTerritoryBattle(m_selectedTerritory)"), 4
        )


if __name__ == "__main__":
    unittest.main()
