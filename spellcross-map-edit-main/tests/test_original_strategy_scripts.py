"""Source integration guard for original mission scripts and strategic defence.

This intentionally does not claim to simulate the DOS binary or native GUI.
Run with python -m unittest discover -s spellcross-map-edit-main/tests -p 'test_*.py'
"""
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
LEVEL_H = ROOT / "source" / "level.h"
LEVEL_CPP = ROOT / "source" / "level.cpp"
STRAT_H = ROOT / "source" / "forms" / "form_level.h"
STRAT_CPP = ROOT / "source" / "forms" / "form_level.cpp"


class OriginalStrategyScriptsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.level_h = LEVEL_H.read_text(encoding="utf-8")
        cls.level_cpp = LEVEL_CPP.read_text(encoding="utf-8")
        cls.strat_h = STRAT_H.read_text(encoding="utf-8")
        cls.strat = STRAT_CPP.read_text(encoding="utf-8")

    def test_mission_event_ids_preserved_from_def(self):
        self.assertIn("int end_ok_event = -1;", self.level_h)
        self.assertIn('cmd == "EndOKEvent"', self.level_cpp)
        self.assertIn('cmd == "EndBadEvent"', self.level_cpp)
        self.assertIn("int time_value = -1;", self.level_h)

    def test_both_mission_outcomes_activate_events(self):
        self.assertIn("ActivateMissionEvent(mission->end_ok_event)", self.strat)
        self.assertGreaterEqual(self.strat.count("ActivateMissionEvent(mission->end_bad_event)"), 2)
        self.assertIn("m_activatedEvents.emplace(eventId, m_turn)", self.strat)
        self.assertIn("ProcessLevelEvents();", self.strat)

    def test_timed_events_are_not_executed_as_immediate_runevents(self):
        self.assertIn("target->time_value <= 0", self.strat)
        self.assertIn("m_activatedEvents.emplace(run_id, m_turn)", self.strat)
        self.assertIn("m_turn >= target_turn", self.strat)

    def test_original_intro_events_fire_and_condition_only_events_wait(self):
        self.assertIn("evt.time_value == -1 ||", self.strat)
        self.assertIn("m_activatedEvents.count(evt.id) != 0", self.strat)
        self.assertIn("if (should_fire && !evt.wait_for_territories.empty())", self.strat)

    def test_mission_timeout_does_not_fake_territory_loss(self):
        portion = self.strat.split("void StrategicLevelFrame::CheckTimeouts()", 1)[1].split(
            "void StrategicLevelFrame::SurrenderCounterAttack(", 1)[0]
        self.assertNotIn("m_stats.territories_lost++", portion)
        self.assertIn("ActivateMissionEvent(mission->end_bad_event)", portion)

    def test_all_counterattack_declines_use_one_transition(self):
        self.assertIn("void StrategicLevelFrame::SurrenderCounterAttack(", self.strat)
        self.assertIn("SurrenderCounterAttack(ca)", self.strat)
        self.assertIn("SurrenderCounterAttack(activeAttack)", self.strat)
        self.assertIn("SurrenderCounterAttack(*attack)", self.strat)
        self.assertIn("if (answer != wxYES)\n                return;", self.strat)
        self.assertIn("attack.territory_lost = true;", self.strat)
        self.assertIn("SurrenderCounterAttack(CounterAttackState& attack)", self.strat_h)


if __name__ == "__main__":
    unittest.main()
