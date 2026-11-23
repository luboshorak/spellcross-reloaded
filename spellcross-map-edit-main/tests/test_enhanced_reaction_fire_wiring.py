"""Static integration guard for the opt-in Enhanced defensive fire mode."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
def source(relative):
    return (ROOT / relative).read_text(encoding="utf-8")

class TestEnhancedReactionFireWiring(unittest.TestCase):
    def test_original_retains_retail_contact_rule(self):
        code = source("source/map.cpp")
        block = code.split("void SpellMap::CheckReactionFire(", 1)[1].split(
            "bool SpellMap::ProcessReactionFire(", 1)[0]
        self.assertIn("spellcross_tactical::EntersSideSight(seen_now, seen_before)", block)
        self.assertIn("const bool enhanced = reaction_fire_mode ==", block)
        self.assertIn("if (enhanced ? !seen_now :", block)
        self.assertIn("enhanced_reaction_attempts.Has(moving_unit->id, shooter->id)", block)
        self.assertIn("enhanced_reaction_attempts.Mark(moving_unit->id, shooter->id)", block)
        self.assertLess(block.index("enhanced_reaction_attempts.Mark"),
                        block.index("const int defender_roll"))
        self.assertLess(block.index("shooter->attack_map[target_mxy]"),
                        block.index("enhanced_reaction_attempts.Mark"))
        self.assertNotIn("type_id == 29", block)  # never hard-code Hummer exclusions

    def test_phase_boundaries_reset_attempts(self):
        code = source("source/map.cpp")
        for name in ("StartEnemyTurn", "EndEnemyTurn", "Close"):
            with self.subTest(name=name):
                block = code.split("void SpellMap::" + name + "(", 1)[1][:850]
                self.assertIn("enhanced_reaction_attempts.Clear()", block)

    def test_preference_is_opt_in_and_persisted(self):
        code = source("source/main.cpp")
        self.assertIn('ini.GetLongValue("STATE", "reaction_fire_mode", 0)', code)
        self.assertIn('ini.SetLongValue("STATE", "reaction_fire_mode",', code)
        self.assertIn("OnOptionsReactionFire", code)
        for file in ("source/forms/form_mmenu.cpp", "source/forms/form_level.cpp"):
            with self.subTest(file=file):
                self.assertIn("menu.reaction_enhanced", source(file))
                self.assertIn("menu.reaction_original", source(file))

    def test_locale_entries(self):
        for file in ("locales/en.lang", "locales/cs.lang"):
            with self.subTest(file=file):
                locale = source(file)
                self.assertEqual(locale.count("menu.reaction_fire="), 1)
                self.assertEqual(locale.count("menu.reaction_original="), 1)
                self.assertEqual(locale.count("menu.reaction_enhanced="), 1)

if __name__ == "__main__":
    unittest.main()
