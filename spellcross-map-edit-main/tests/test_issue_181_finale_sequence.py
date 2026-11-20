"""#181: the retail chapter-10 ending must drain every clip before completion."""
from pathlib import Path
import unittest

SRC = Path(__file__).resolve().parents[1] / "source"
MAIN = (SRC / "main.cpp").read_text(encoding="utf-8")
LEVEL = (SRC / "forms" / "form_level.cpp").read_text(encoding="utf-8")
HEADER = (SRC / "main.h").read_text(encoding="utf-8")


class CampaignFinaleRegression(unittest.TestCase):
    def test_terminal_level_not_presented_as_a_stuck_strategy_map(self):
        advance = LEVEL.split("void StrategicLevelFrame::AdvanceToNextLevel()", 1)[1]
        final = advance.split("if (currentNumber == 10 && requestedNumber == 11)", 1)[1]
        final = final.split("\n        spellcross_dialog_ui::ShowThemedMessage(", 1)[0]
        self.assertIn("m_pendingMission.valid = false;", final)
        self.assertIn("SaveStrategicState();", final)
        self.assertIn("m_main->CompleteCampaignAfterCutscenes();", final)
        self.assertNotIn('"Campaign Complete"', final)
        self.assertLess(
            advance.index("for (const auto& clip : m_level.outro_videos)"),
            advance.index("if (currentNumber == 10 && requestedNumber == 11)"),
        )

    def test_outro_queue_is_preserved_through_finale(self):
        self.assertIn("void CompleteCampaignAfterCutscenes();", HEADER)
        self.assertIn("bool m_campaignCompletionPending = false;", HEADER)

        play = MAIN.split("void MainFrame::PlayNextStrategicCutscene()", 1)[1].split(
            "void MainFrame::OnStrategicCutsceneClosed(", 1
        )[0]
        self.assertLess(play.index("while (!m_strategicCutsceneQueue.empty())"),
                        play.index("if (m_campaignCompletionPending)"))
        self.assertIn("m_campaignCompletionPending = false;", play)
        self.assertIn('"Campaign Complete"', play)
        self.assertIn("ShowMainMenuWindow();", play)
        self.assertIn("m_strategicLevel = nullptr;", play)
        self.assertIn("finished->Destroy();", play)
        self.assertLess(play.index('spellcross_dialog_ui::ShowThemedMessage('),
                        play.index("ShowMainMenuWindow();"))
        self.assertLess(play.index("if (m_campaignCompletionPending)"),
                        play.index("// Only the final clip returns to the new/current strategic frame."))

    def test_finale_suppresses_intermediate_strategic_map(self):
        mission = MAIN.split("void MainFrame::OpenStrategicAndLoadNext()", 1)[1].split(
            "// Mission failed without strategic level", 1
        )[0]
        self.assertIn("if (m_strategicLevel && !m_campaignCompletionPending)", mission)
        self.assertIn("m_mission_end_flow = false;", mission)

        marker = MAIN.split("void MainFrame::CompleteCampaignAfterCutscenes()", 1)[1].split(
            "void MainFrame::PlayNextStrategicCutscene()", 1
        )[0]
        self.assertIn("m_campaignCompletionPending = true;", marker)
        self.assertIn("CallAfter([this]()", marker)
        self.assertIn("!m_strategicCutscenePlaying && !m_strategicCutsceneScheduled", marker)


if __name__ == "__main__":
    unittest.main()
