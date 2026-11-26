"""Issue #178: level-09 ending belongs to the tactical canvas, not wx strategic dialogs."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1] / "source"
MAIN = (ROOT / "main.cpp").read_text(encoding="utf-8")
STATE = (ROOT / "main.h").read_text(encoding="utf-8")
DIALOGS = (ROOT / "game_dialogs.h").read_text(encoding="utf-8")


class TacticalMissionDebriefRegression(unittest.TestCase):
    def test_original_strategy_message_boxes_are_not_replaced_with_scrollers(self):
        # The previous PR #179 accidentally changed these dialogs.
        self.assertNotIn("const bool longMessage", DIALOGS)
        self.assertNotIn("wxVSCROLL | wxBORDER_SIMPLE", DIALOGS)
        self.assertIn("auto* label = new wxStaticText", DIALOGS)

    def test_end_screen_uses_real_capped_frame_and_visible_text_only(self):
        draw = MAIN.split("void MainFrame::DrawMissionResultOverlay(", 1)[1].split(
            "void MainFrame::StartMissionEndFlow(", 1
        )[0]
        self.assertIn('m_mission_end_req.success ? "M_ACCOMP" : "M_FAILED"', draw)
        self.assertIn("spellcross_mission_debrief::Compute(", draw)
        self.assertIn("m_mission_debrief_lines[firstLine + i]", draw)
        self.assertIn("i < layout.visible_lines", draw)
        self.assertIn("m_mission_result_stage == MissionResultStage::Debrief", draw)
        self.assertIn("wm_frame_corner", draw)
        self.assertIn("wxMask(panel, maskColour)", draw)
        self.assertIn("layout.max_scroll_line > 0", draw)
        self.assertNotIn("32 + (int)lines.size() * lineH", draw)

    def test_every_story_line_is_kept_before_painting(self):
        show = MAIN.split("void MainFrame::ShowMissionResultOverlay()", 1)[1].split(
            "void MainFrame::DismissMissionResultOverlay()", 1
        )[0]
        self.assertIn("m_mission_debrief_scroll_line = 0", show)
        self.assertIn("m_mission_debrief_lines.clear()", show)
        self.assertIn("spellcross_mission_debrief::TextWidth", show)
        self.assertIn("m_mission_debrief_lines.emplace_back(line.text)", show)
        self.assertIn("m_mission_debrief_lines", STATE)

    def test_result_consumes_scroll_inputs_before_gameplay(self):
        wheel = MAIN.split("void MainFrame::OnCanvasMouseWheel(", 1)[1].split(
            "void MainFrame::OnCanvasKeyDown(", 1
        )[0]
        self.assertLess(wheel.index("if (m_mission_result_visible)"),
                        wheel.index("m_tacticalZoom.Wheel("))
        self.assertIn("m_mission_debrief_scroll_line", wheel)
        self.assertIn("event.GetWheelRotation()", wheel)
        self.assertIn("event.GetWheelAxis() == wxMOUSE_WHEEL_VERTICAL", wheel)

        keys = MAIN.split("void MainFrame::OnCanvasKeyDown(", 1)[1].split(
            "void MainFrame::OnCanvasKeyUp(", 1
        )[0]
        self.assertIn("case WXK_PAGEUP:", keys)
        self.assertIn("case WXK_PAGEDOWN:", keys)
        self.assertIn("case WXK_HOME:", keys)
        self.assertIn("case WXK_END:", keys)
        self.assertIn("DismissMissionResultOverlay()", keys)
        self.assertLess(keys.index("if (m_mission_result_visible)"),
                        keys.index("if(key == WXK_ESCAPE)"))

        click = MAIN.split("void MainFrame::OnCanvasLMouseDown(", 1)[1].split(
            "void MainFrame::OnCanvasLMouseUp(", 1
        )[0]
        self.assertIn("spellcross_mission_debrief::ScrollFromTrack(", click)
        self.assertIn("m_mission_debrief_dragging = true", click)
        self.assertIn("DismissMissionResultOverlay()", click)
        movement = MAIN.split("void MainFrame::OnCanvasMouseMove(", 1)[1].split(
            "void MainFrame::OnCanvasMouseWheel(", 1
        )[0] if "void MainFrame::OnCanvasMouseWheel(" in MAIN.split(
            "void MainFrame::OnCanvasMouseMove(", 1
        )[1] else MAIN.split("void MainFrame::OnCanvasMouseMove(", 1)[1]
        self.assertIn("if (m_mission_result_visible)", movement)
        self.assertIn("m_mission_debrief_dragging && event.LeftIsDown()", movement)
        self.assertIn("spellcross_mission_debrief::ScrollFromTrack(", movement)



if __name__ == "__main__":
    unittest.main()
