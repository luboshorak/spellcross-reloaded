"""RMB must pan the camera; only Escape cancels a tactical group."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "source" / "main.cpp"


class TacticalRightMouseTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = MAIN.read_text(encoding="utf-8")
        cls.rmb = source.split("void MainFrame::OnCanvasRMouse(wxMouseEvent& event)", 1)[1].split(
            "// select all tiles", 1)[0]
        cls.keyboard = source.split("void MainFrame::OnCanvasKeyDown(wxKeyEvent& event)", 1)[1].split(
            "// on canvas key up", 1)[0]
        cls.source = source

    def test_rmb_starts_and_stops_camera_drag(self):
        self.assertIn("scroller.SetRef(point.x, point.y)", self.rmb)
        self.assertIn("scroller.Idle()", self.rmb)
        self.assertIn("event.RightDown()", self.rmb)
        self.assertIn("event.RightUp()", self.rmb)

    def test_rmb_cannot_deselect_or_force_movement_mode(self):
        self.assertNotIn("SetActiveGroup(0)", self.rmb)
        self.assertNotIn("IsGroupCancelRightClick", self.rmb)
        self.assertNotIn("SetUnitRangeViewMode", self.rmb)

    def test_escape_cancels_active_group(self):
        escape = self.keyboard.split("if(key == WXK_ESCAPE)", 1)[1]
        escape = escape.split("wxCommandEvent evt;", 1)[0]
        self.assertIn("IsGroupMode()", escape)
        self.assertIn("SetActiveGroup(0)", escape)

    def test_selection_and_lost_release_watchdog_remain(self):
        self.assertIn("m_boxSelectPending", self.source)
        self.assertIn("ResetTransientTacticalPointerState(", self.source)
        self.assertIn("HasOrphanedPointerGesture(", self.source)
        self.assertNotIn("TickTacticalEdgeScroll", self.source)


if __name__ == "__main__":
    unittest.main()
