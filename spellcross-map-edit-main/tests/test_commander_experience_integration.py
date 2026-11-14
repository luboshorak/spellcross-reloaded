"""Guard the C++ wiring for original officer experience and promotion rules.

The pure C++ test verifies the calculation; these assertions ensure the
production strategic lifecycle continues to call those tested helpers.
"""
from pathlib import Path
import unittest


SOURCE = (Path(__file__).resolve().parents[1] / "source" / "forms" / "form_level.cpp").read_text(encoding="utf-8")


class CommanderExperienceIntegrationTests(unittest.TestCase):
    def test_new_officer_starts_with_first_lieutenant_battles(self):
        self.assertIn(
            "rec.battles_fought = spellcross_strategy::InitialCommanderBattles(rec.rank, m_ranks);",
            SOURCE,
        )

    def test_original_import_reads_fight_counter(self):
        self.assertIn(
            "commander.battles_fought = spellcross_strategy::ReadOriginalCommanderBattles(raw, off);",
            SOURCE,
        )

    def test_count_only_once_per_surviving_deployed_commander(self):
        collector = SOURCE.split(
            "StrategicLevelFrame::CollectAndApplyBattleResults(bool success)", 1
        )[1]
        self.assertIn("deployedHosts.count(slot.assigned_unit_uid) == 0", collector)
        self.assertIn("survivingStrategicUids.count(slot.assigned_unit_uid) == 0", collector)
        self.assertIn("!countedCommanders.insert(slot.commander_uid).second", collector)
        self.assertEqual(collector.count("commander->battles_fought = std::min("), 1)

    def test_promotion_uses_original_threshold_and_player_rank(self):
        rank_fn = SOURCE.split("void StrategicLevelFrame::ApplyCommanderRankPromotions()", 1)[1]
        rank_fn = rank_fn.split(
            "const StrategicLevelFrame::CommanderRankRec* StrategicLevelFrame::FindRankRec", 1
        )[0]
        self.assertIn("spellcross_strategy::CommanderRankForBattles(", rank_fn)
        self.assertIn("commander.battles_fought, m_player.rank, m_ranks", rank_fn)

    def test_promotion_after_player_reward_even_at_chapter_end(self):
        result_handler = SOURCE.split(
            "void StrategicLevelFrame::HandleMissionResult(", 1
        )[1].split(
            "StrategicLevelFrame::LossBlock StrategicLevelFrame::CollectAndApplyBattleResults", 1
        )[0]
        success = result_handler.index("RecomputePlayerRank();")
        promotion = result_handler.index("ApplyCommanderRankPromotions();")
        chapter_end = result_handler.index("if (isLevelComplete)")
        self.assertLess(success, promotion)
        self.assertLess(promotion, chapter_end)
        self.assertGreaterEqual(result_handler.count("ApplyCommanderRankPromotions();"), 2)

    def test_json_roundtrip_preserves_counter(self):
        self.assertIn('ParseJsonIntField(obj, "battles_fought", c.battles_fought)', SOURCE)
        self.assertIn('<< ", \\"battles_fought\\": " << c.battles_fought', SOURCE)


if __name__ == "__main__":
    unittest.main()
