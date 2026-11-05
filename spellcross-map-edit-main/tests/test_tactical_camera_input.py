"""Regression guard for retail-style tactical camera and group input separation."""
from pathlib import Path
import unittest

SOURCE = Path(__file__).resolve().parents[1] / "source" / "main.cpp"
HELP = Path(__file__).resolve().parents[1] / "source" / "game_controls.h"


class TacticalCameraInputTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = SOURCE.read_text(encoding="utf-8")
        cls.help = HELP.read_text(encoding="utf-8")

    def test_right_mouse_no_longer_arms_camera_drag(self):
        # An orphaned native RMB-up must never latch TScroll::state again.
        self.assertNotIn("scroller.SetRef(", self.source)
        self.assertNotIn("m_rightPressActive", self.source)
        self.assertNotIn("m_rightDragged", self.source)

    def test_rmb_still_deselects_group(self):
        method = self.source.split("void MainFrame::OnCanvasRMouse", 1)[1]
        method = method.split("// select all tiles", 1)[0]
        self.assertIn("event.RightUp()", method)
        self.assertIn("IsGroupMode()", method)
        self.assertIn("SetActiveGroup(0)", method)
        self.assertNotIn("UNIT_RANGE_MOVE", method)

    def test_edge_scroll_uses_timer_not_drag_event(self):
        self.assertIn("TickTacticalEdgeScroll();", self.source)
        scroll = self.source.split("void MainFrame::TickTacticalEdgeScroll()", 1)[1]
        scroll = scroll.split("// map animation periodic refresh tick", 1)[0]
        self.assertIn("EdgeScrollAt(", scroll)
        self.assertIn("m_boxSelectPending", scroll)
        self.assertIn("RenderPrepare()", scroll)

    def test_help_matches_controls(self):
        self.assertIn("help.key.edge_scroll", self.help)
        self.assertIn("help.key.right_cancel", self.help)
        self.assertNotIn("help.key.drag", self.help)


if __name__ == "__main__":
    unittest.main()
