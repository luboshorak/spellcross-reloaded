"""Static wiring regressions for original side-wide tactical reaction fire (#188)."""
from pathlib import Path
import unittest

SRC = (Path(__file__).resolve().parents[1] / "source" / "map.cpp").read_text(encoding="utf-8")


class TestReactionSharedSightIntegration(unittest.TestCase):
    def test_contact_uses_defending_side_and_weapon_area(self):
        start = SRC.index("void SpellMap::CheckReactionFire(")
        end = SRC.index("bool SpellMap::ProcessReactionFire(", start)
        block = SRC[start:end]
        self.assertIn("ReactionSideHasSight(this, defending_is_enemy, moving_unit->coor)", block)
        self.assertIn("ReactionSideHasSight(this, defending_is_enemy, previous_pos)", block)
        self.assertIn("spellcross_tactical::EntersSideSight(seen_now, seen_before)", block)
        self.assertIn("unit_view->CalcAttackRange(shooter, true);", block)
        self.assertIn("shooter->attack_map[target_mxy]", block)
        self.assertNotIn("HasDirectSight(shooter, moving_unit->coor)", block)
        self.assertNotIn("HasDirectSight(shooter, previous_pos)", block)

    def test_delayed_shots_keep_side_contact_without_personal_los(self):
        start = SRC.index("bool SpellMap::ProcessReactionFire(")
        end = SRC.index("// attack target (unit or object)", start)
        block = SRC[start:end]
        self.assertIn("ReactionSideHasSight(this, entry.shooter->is_enemy, entry.target->coor)", block)
        self.assertIn("unit_view->CalcAttackRange(entry.shooter, true);", block)
        self.assertNotIn("HasDirectSight(entry.shooter, entry.target->coor)", block)

    def test_side_visibility_does_not_borrow_player_fog_for_enemy(self):
        start = SRC.index("static bool ReactionSideHasSight(")
        end = SRC.index("// Original Spellcross initiative", start)
        block = SRC[start:end]
        self.assertIn("spellcross_tactical::SideHasSight(map->units, observer_is_enemy,", block)
        self.assertIn("HasDirectSight(observer, pos)", block)
        self.assertNotIn("unit_view->view[", block)


if __name__ == "__main__":
    unittest.main()
