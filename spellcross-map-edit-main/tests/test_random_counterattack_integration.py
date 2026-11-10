"""Static wiring guards for retail random Other Side counterattacks (#154)."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1] / "source"
CPP = (ROOT / "forms" / "form_level.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "forms" / "form_level.h").read_text(encoding="utf-8")


class TestRandomCounterattackIntegration(unittest.TestCase):
    def test_scripts_take_priority_over_random_attacks(self):
        sequence = CPP[CPP.index("void StrategicLevelFrame::OnEndTurn("):]
        self.assertRegex(
            sequence,
            r"ProcessLevelEvents\(\);[\s\S]*?ProcessRandomCounterAttack\(\);"
            r"[\s\S]*?CheckCounterAttacks\(\);",
        )
        scheduler = CPP[CPP.index("void StrategicLevelFrame::ProcessRandomCounterAttack()"):
                        CPP.index("void StrategicLevelFrame::CheckCounterAttacks()")]
        self.assertIn("m_lastEnemyAttackTurn >= m_turn", scheduler)
        self.assertIn("m_counterAttacks.push_back(std::move(attack))", scheduler)
        self.assertIn("RandomCounterAttackRoll(", scheduler)
        self.assertIn("FindCounterAttackFrontier(", scheduler)

    def test_old_saves_migrate_and_cooldown_round_trips(self):
        self.assertIn('"last_enemy_attack_turn"', CPP)
        self.assertIn("lastEnemyAttackTurn = turn;", CPP)
        self.assertEqual(CPP.count("mfsv.lastEnemyAttackTurn = &m_lastEnemyAttackTurn;"), 2)
        self.assertEqual(CPP.count("mflv.lastEnemyAttackTurn = &m_lastEnemyAttackTurn;"), 2)
        self.assertIn("m_lastEnemyAttackTurn = m_turn;", CPP)
        self.assertIn("int m_lastEnemyAttackTurn = 1;", HEADER)

    def test_random_army_reuses_generated_battle_lifecycle(self):
        scheduler = CPP[CPP.index("void StrategicLevelFrame::ProcessRandomCounterAttack()"):
                        CPP.index("void StrategicLevelFrame::CheckCounterAttacks()")]
        self.assertIn("BuildRandomCounterAttackForce(", scheduler)
        self.assertIn("ResolveMapDtaPathForMissionToken(mission)", scheduler)
        self.assertIn("attack.enemy_units = std::move(enemyForce)", scheduler)
        self.assertIn("attack.trigger_turn = m_turn;", scheduler)
        self.assertIn("ca.triggered = true;", CPP)
        self.assertIn("SurrenderCounterAttack(ca);", CPP)
        self.assertIn("m_lastEnemyAttackTurn = m_turn;", CPP)


if __name__ == "__main__":
    unittest.main()
