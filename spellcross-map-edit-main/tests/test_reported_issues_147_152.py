"""Source wiring regressions for the 2026-11 user-reported strategy bugs.

These checks keep native UI, mission outcome, archive-text parsing and JSON
persistence connected. They supplement, but do not replace, a Windows build
with real CS/EN game data and a reproducible tactical save.
"""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1] / "source"
STRATEGY = (ROOT / "forms/form_level.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "forms/form_level.h").read_text(encoding="utf-8")
MAP = (ROOT / "map.cpp").read_text(encoding="utf-8")
TEXT = (ROOT / "spell_texts.cpp").read_text(encoding="utf-8")


class ReportedIssuesRegression(unittest.TestCase):
    def test_151_long_mission_briefing_has_independent_scroll_state(self):
        render = STRATEGY.split("// Territory briefing text in the original lower frame.", 1)[1].split(
            "//", 1
        )[0] if False else STRATEGY.split(
            "// Territory briefing text in the original lower frame.", 1
        )[1].split("const int unitRowsY", 1)[0]
        self.assertIn("OriginalWrapText(font, briefing, 356, 4096)", render)
        self.assertIn("m_originalBriefingScroll", render)
        self.assertIn("firstLine + row", render)
        wheel = STRATEGY.split("void StrategicLevelFrame::OnOriginalStrategicMouseWheel(", 1)[1].split(
            "void StrategicLevelFrame::OnOriginalStrategicMouseMove(", 1
        )[0]
        self.assertIn("m_originalBriefingScroll - steps * 3", wheel)
        self.assertIn("lx >= 22 && lx < 396", wheel)
        self.assertIn("ly >= 326 && ly < 456", wheel)
        self.assertIn("int m_originalBriefingScroll = 0;", HEADER)
        self.assertIn("m_originalBriefingScroll = 0;", STRATEGY)

    def test_148_commander_battles_are_persistent_and_rank_uses_retail_thresholds(self):
        self.assertIn("int battles_fought = 0;", HEADER)
        self.assertIn('ParseJsonIntField(obj, "battles_fought", c.battles_fought)', STRATEGY)
        self.assertGreaterEqual(STRATEGY.count('<< c.battles_fought << "}";'), 2)
        battle = STRATEGY.split(
            "StrategicLevelFrame::LossBlock StrategicLevelFrame::CollectAndApplyBattleResults(", 1
        )[1].split("// Mission-created allied units", 1)[0]
        self.assertIn("RemoveCommandersHostedByDeadUnits(deadStrategicUids);", battle)
        self.assertIn("deployedHosts.count(slot.assigned_unit_uid)", battle)
        self.assertIn("survivingStrategicUids.count(slot.assigned_unit_uid)", battle)
        self.assertIn("countedCommanders.insert(slot.commander_uid)", battle)
        self.assertIn("commander->battles_fought", battle)
        self.assertIn("currentRank->actions_required", battle)
        self.assertIn("FindRankRec(commander->rank + 1)", battle)
        self.assertIn("UpdateCommanderHierarchyLabel(id)", battle)

    def test_149_two_stage_wall_destruction_does_not_revert_damaged_sprite(self):
        wall = MAP.split("int MapSprite::UpdateDestructible()", 1)[1].split(
            "Sprite* MapSprite::GetDestructible()", 1
        )[0]
        self.assertIn("const bool wasTwoStageWall", wall)
        self.assertIn("const bool isDamagedWall", wall)
        self.assertIn("destructible_alt->two_stage_desctruct", wall)
        self.assertLess(wall.index("wasTwoStageWall"), wall.index("*tile = (*tile)->destructible_alt;"))
        self.assertIn("wasTwoStageWall || isDamagedWall", wall)
        self.assertIn("*tile = NULL;", wall)

    def test_152_retail_text_control_bytes_are_not_converted_to_glyphs(self):
        parser = TEXT.split("SpellTextRec::SpellTextRec(", 1)[1].split(
            "SpellTextChunks SpellTextRec::WordWrap(", 1
        )[0]
        self.assertIn("static_cast<unsigned char>(*pstr)", parser)
        self.assertIn("byte < 0x20", parser)
        self.assertIn("byte == 0x7F", parser)
        self.assertIn("++pstr;", parser)
        self.assertIn("char2wstringCP895", parser)


if __name__ == "__main__":
    unittest.main()
