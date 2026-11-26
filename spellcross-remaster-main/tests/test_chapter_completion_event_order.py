"""Guard chapter-final mission and chapter-intro ordering in the strategic UI.

The source-level assertions cover integration order; native GUI/video playback
still requires manual validation in the Windows build.
"""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
STRATEGY = (ROOT / "source/forms/form_level.cpp").read_text(encoding="utf-8")
MAIN = (ROOT / "source/main.cpp").read_text(encoding="utf-8")


class ChapterCompletionEventOrderTest(unittest.TestCase):
    def test_final_victory_transitions_before_old_chapter_end_ok_events(self):
        result = STRATEGY.split(
            "void StrategicLevelFrame::HandleMissionResult(", 1
        )[1].split(
            "StrategicLevelFrame::LossBlock StrategicLevelFrame::CollectAndApplyBattleResults(", 1
        )[0]
        self.assertLess(result.index("if (isLevelComplete)"),
                        result.index("ActivateMissionEvent(mission->end_ok_event)"))
        self.assertLess(result.index("AdvanceToNextLevel();"),
                        result.index("ActivateMissionEvent(mission->end_ok_event)"))
        self.assertIn("if (!hasSuccessor && !wasCounterAttack)", result)
        self.assertIn("m_level.end_territory > 0", result)
        self.assertIn("ConquestTerritory(territory_id);", result)

    def test_normal_mission_outcomes_still_trigger_events(self):
        result = STRATEGY.split(
            "void StrategicLevelFrame::HandleMissionResult(", 1
        )[1].split(
            "StrategicLevelFrame::LossBlock StrategicLevelFrame::CollectAndApplyBattleResults(", 1
        )[0]
        self.assertIn("ActivateMissionEvent(mission->end_ok_event);", result)
        self.assertIn("ActivateMissionEvent(mission->end_bad_event);", result)
        self.assertGreaterEqual(result.count("ProcessLevelEvents();"), 2)

    def test_next_chapter_intro_events_are_after_complete_cutscene_queue(self):
        transition = STRATEGY.split(
            "void StrategicLevelFrame::AdvanceToNextLevel()", 1
        )[1]
        self.assertIn("newWin->m_deferredChapterIntroEvents = newWin->m_gameModeEnabled", transition)
        self.assertNotIn("newWin->ProcessLevelEvents();", transition)
        self.assertIn("newWin->RunDeferredChapterIntroEvents();", transition)
        self.assertIn("!m_main->HasPendingStrategicCutscenes()", transition)
        run_intro = STRATEGY.split(
            "void StrategicLevelFrame::RunDeferredChapterIntroEvents()", 1
        )[1].split("void StrategicLevelFrame::AdvanceToNextLevel()", 1)[0]
        self.assertLess(run_intro.index("m_deferredChapterIntroEvents = false"),
                        run_intro.index("ProcessLevelEvents();"))
        self.assertIn("SaveStrategicState();", run_intro)
        play_next = MAIN.split("void MainFrame::PlayNextStrategicCutscene()", 1)[1].split(
            "void MainFrame::OnStrategicCutsceneClosed(", 1
        )[0]
        self.assertLess(play_next.index("while (!m_strategicCutsceneQueue.empty())"),
                        play_next.index("m_strategicLevel->RunDeferredChapterIntroEvents();"))


if __name__ == "__main__":
    unittest.main()
