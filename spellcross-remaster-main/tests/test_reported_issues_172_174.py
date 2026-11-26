"""Regression wiring for original issues #172-174 (no wx runtime required)."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1] / "source"
LEVEL = (ROOT / "level.cpp").read_text(encoding="utf-8")
STRATEGY = (ROOT / "forms/form_level.cpp").read_text(encoding="utf-8")
TACTICAL = (ROOT / "map.cpp").read_text(encoding="utf-8")
INFO = (ROOT / "forms/tactical_unit_info.h").read_text(encoding="utf-8")


class OriginalIssues172174(unittest.TestCase):
    def test_retail_wait_for_research_is_parsed(self):
        self.assertIn('cmd == "WaitForResearch"', LEVEL)
        self.assertIn("curEvent.wait_for_research.push_back(v)", LEVEL)

    def test_special_flags_survive_save_load_and_unlock_keys(self):
        self.assertIn("EffectiveResearchFlags(", STRATEGY)
        self.assertIn("m_triggeredLevelEvents", STRATEGY)
        self.assertIn("AnyResearchPrerequisiteUnlocked(", STRATEGY)
        self.assertIn("AllResearchCompleted(", STRATEGY)
        self.assertIn("ProcessLevelEvents();\n        CheckTimeouts();", STRATEGY)

    def test_174_final_map_is_gated_both_in_ui_and_launch(self):
        gate = "FinalTerritoryResearchGateSatisfied("
        self.assertGreaterEqual(STRATEGY.count(gate), 2)
        start = STRATEGY.index("bool StrategicLevelFrame::CanLaunchTerritoryBattle(")
        section = STRATEGY[start:STRATEGY.index("void StrategicLevelFrame::SurrenderCounterAttack(", start)]
        self.assertLess(section.index(gate), section.index("if (!owned)"))
        self.assertIn("Toto území zatím není přístupné.", STRATEGY)

    def test_174_scripted_noncombat_sector_cannot_launch_or_be_attacked(self):
        # LEVEL_09 retail marks territory 4 with SetSpecialTerritory(4).
        # The event after both keys awards it without running M09_04A.DEF.
        self.assertIn('cmd == "SetSpecialTerritory"', LEVEL)
        self.assertIn("t.is_special = std::find(specialTerritoryIds.begin()", LEVEL)
        start = STRATEGY.index("bool StrategicLevelFrame::CanLaunchTerritoryBattle(")
        end = STRATEGY.index("void StrategicLevelFrame::SurrenderCounterAttack(", start)
        section = STRATEGY[start:end]
        self.assertLess(section.index("if (territory->is_special)"),
                        section.index("if (!owned)"))
        # Cover native button, wxWidgets launch path and both enemy-AI attack
        # producers; script-granted sector must not have a normal combat map.
        self.assertIn("!CanLaunchTerritoryBattle(m_selectedTerritory)", STRATEGY)
        self.assertIn("if (territory == m_level.territories.end() || territory->is_special)", STRATEGY)
        self.assertIn("return t.id == id && !t.is_special;", STRATEGY)
        self.assertIn("ca.completed = true;", STRATEGY)

    def test_172_stat_formula_consistent_in_strategic_and_tactical(self):
        self.assertIn("FormationBonusForLevel(meta.formation_level)", STRATEGY)
        self.assertIn("FormationBonusForLevel(level)", TACTICAL)
        self.assertIn("unit->formation_attack_bonus", INFO)
        self.assertIn("Bonus formace:", INFO)


if __name__ == "__main__":
    unittest.main()
