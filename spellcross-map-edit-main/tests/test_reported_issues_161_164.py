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

    def test_164_rapid_fire_never_replaces_an_inflight_projectile(self):
        # Repeated clicks while SHOT/FLIGHT/HIT is active must not reset
        # the pending attack's target, animation, or already-spent AP.
        player_attack = MAP.split("int SpellMap::Attack(MapXY pos, int prefer_air)", 1)[1].split(
            "// Find the live, controllable Alliance unit", 1
        )[0]
        self.assertLess(
            player_attack.index("IsUnitBusy(GetSelectedUnit())"),
            player_attack.index("unit->attack_target = NULL;"),
        )

        ai_attack = MAP.split("bool SpellMap::StartAttack_NoHUD(", 1)[1].split(
            "// DOS helper used by morale shock routines", 1
        )[0]
        self.assertLess(
            ai_attack.index("IsUnitBusy(attacker)"),
            ai_attack.index("attacker->attack_target = NULL;"),
        )

        # Keep the cursor and actual command in agreement while busy.
        target_checks = MAP.split("int SpellMap::CanUnitAttack(MapUnit* target)", 1)[1].split(
            "// can selected unit attack land unit", 1
        )[0]
        object_checks = MAP.split("int SpellMap::CanUnitAttackObject(MapXY pos)", 1)[1].split(
            "\n}\n", 1
        )[0]
        self.assertIn("IsUnitBusy(attacker)", target_checks)
        self.assertIn("IsUnitBusy(unit)", object_checks)

        # Movement may be redirected while moving, but it must not
        # starve an attack after AP has been consumed on SHOT.
        move = MAP.split("int SpellMap::MoveUnit(MapXY target, bool reserve_one_shot)", 1)[1].split(
            "// get unit action options for cursor position", 1
        )[0]
        self.assertIn("unit->attack_state != MapUnit::ATTACK_STATE::IDLE", move)
        self.assertIn("unit->action_state != MapUnit::ACTION_STATE::IDLE", move)
        can_move = MAP.split("int SpellMap::CanUnitMove(MapXY target)", 1)[1].split(
            "// move unit (in game mode)", 1
        )[0]
        self.assertIn("unit->attack_state != MapUnit::ATTACK_STATE::IDLE", can_move)


    def test_156_duplicate_officers_are_normalized_before_hierarchy_restore(self):
        self.assertIn("void StrategicLevelFrame::NormalizeCommanderIdentities()", LEVEL)
        self.assertIn("spellcross_strategy::EnsureUniqueCommanderUids(", LEVEL)
        self.assertIn("std::unordered_set<uint32_t> restoredCommanderIds;", LEVEL)
        self.assertIn("restoredCommanderIds.insert(c->uid).second", LEVEL)
        self.assertGreaterEqual(LEVEL.count("NormalizeCommanderIdentities();"), 5)


if __name__ == "__main__":
    unittest.main()
