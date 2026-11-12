"""Source integration guards for player-reported tactical/recapture problems.

These regressions check wiring, not Windows runtime graphics and save loading.
"""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1] / "source"
MAIN = (ROOT / "main.cpp").read_text(encoding="utf-8")
MAP = (ROOT / "map.cpp").read_text(encoding="utf-8")
LEVEL = (ROOT / "forms/form_level.cpp").read_text(encoding="utf-8")


class ReportedIssues161164Tests(unittest.TestCase):
    def test_161_off_selection_takeoff_runs_and_group_waits(self):
        self.assertIn("spellcross_tactical::PendingUnitAnimation pending{", MAP)
        self.assertIn("spellcross_tactical::ShouldAdvanceBackgroundAnimation(pending)", MAP)
        self.assertIn("spellcross_tactical::WaitForGroupMemberActions(", MAP)
        self.assertLess(
            MAP.index("spellcross_tactical::ShouldAdvanceBackgroundAnimation(pending)"),
            MAP.index("if (!unit)", MAP.index("// Tactical actions belong to their original owner")),
        )

    def test_162_video_blocks_tactical_tick(self):
        start = MAIN.index("void MainFrame::OnTimer(")
        timer = MAIN[start:]
        video_guard = timer.index("if (form_video_box || HasPendingStrategicCutscenes())")
        tick = timer.index("if (spell_map->Tick())")
        self.assertLess(video_guard, tick)
        self.assertIn("if (m_mission_end_flow)", timer[video_guard:tick])

    def test_163_l5_is_the_primary_enemy_spawn_source(self):
        generated = MAIN.split("bool MainFrame::LoadGeneratedStrategicBattleFromDtaPath(", 1)[1].split(
            "void MainFrame::StartTacticalGame()", 1
        )[0]
        self.assertIn("spellcross_strategy::ChooseGeneratedEnemySeeds(", generated)
        self.assertIn("spell_map->counter_attack_post_enemy, fallbackSeeds,", generated)
        self.assertIn("spell_map->start = spell_map->counter_attack_post_player;", generated)

    def test_164_projectile_stays_owned_after_selection_change(self):
        tick = MAP.split("int SpellMap::Tick()", 1)[1]
        self.assertIn("owner->attack_target != nullptr", tick)
        self.assertIn("owner->attack_target_obj.IsSelected()", tick)
        self.assertIn("unit = owner;", tick)
        self.assertNotIn("unit_selection = owner;", tick)

    def test_156_duplicate_officers_are_normalized_before_hierarchy_restore(self):
        self.assertIn("void StrategicLevelFrame::NormalizeCommanderIdentities()", LEVEL)
        self.assertIn("spellcross_strategy::EnsureUniqueCommanderUids(", LEVEL)
        self.assertIn("std::unordered_set<uint32_t> restoredCommanderIds;", LEVEL)
        self.assertIn("restoredCommanderIds.insert(c->uid).second", LEVEL)
        self.assertGreaterEqual(LEVEL.count("NormalizeCommanderIdentities();"), 5)


if __name__ == "__main__":
    unittest.main()
