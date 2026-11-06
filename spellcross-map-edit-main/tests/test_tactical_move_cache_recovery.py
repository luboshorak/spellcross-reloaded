"""The real click must not depend on a possibly stale asynchronous move
range. Regression for intermittent 'MOVEMENT MODE but Hummer won't move'.
"""
from pathlib import Path
import unittest

SOURCE = Path(__file__).resolve().parents[1] / "source"
MAP = (SOURCE / "map.cpp").read_text(encoding="utf-8")
HEADER = (SOURCE / "map.h").read_text(encoding="utf-8")
MAIN = (SOURCE / "main.cpp").read_text(encoding="utf-8")


class TacticalMoveCacheRecovery(unittest.TestCase):
    def test_result_owner_is_published_atomically_with_range(self):
        worker = MAP.split("void SpellMap::MoveRange::Worker()", 1)[1].split(
            "// set default", 1
        )[0]
        self.assertGreaterEqual(worker.count("completed_range_unit = unit;"), 2)
        self.assertIn("MapUnit* completed_range_unit = nullptr", HEADER)
        cache = MAP.split("int SpellMap::MoveRange::GetCachedMoveAP(", 1)[1].split(
            "AStarNode::AStarNode()", 1
        )[0]
        self.assertIn("std::lock_guard<std::mutex> guard(result_lock)", cache)
        self.assertIn("completed_range_unit != unit", cache)

    def test_preview_cursor_requires_current_range_owner(self):
        body = MAP.split("int SpellMap::CanUnitMove(", 1)[1].split(
            "// move unit (in game mode)", 1
        )[0]
        self.assertIn("unit_range->GetCachedMoveAP(unit, npos)", body)
        self.assertNotIn("unit_range->ap_left[npos]", body)

    def test_real_command_checks_path_before_clearing_current_move(self):
        body = MAP.split("int SpellMap::MoveUnit(", 1)[1].split(
            "// get unit action options", 1
        )[0]
        self.assertNotIn("if (!CanUnitMove(target))", body)
        self.assertLess(body.index("auto path = unit_range->FindPath(unit, target);"),
                        body.index("unit->move_nodes.clear();"))
        self.assertIn("path.size() < 2", body)
        self.assertIn("path.back().pos != target", body)
        self.assertIn("path.back().g_cost > unit->action_points", body)
        self.assertIn("return(1);", body)
        self.assertIn("Recovered stale MOVE preview", body)

    def test_click_only_fallback_when_menu_has_no_actions(self):
        click = MAIN.split("void MainFrame::HandleCanvasLeftClick(", 1)[1].split(
            "void MainFrame::ResetBoxSelectionGesture()", 1
        )[0]
        self.assertIn("if (action_count == 0 && select_pos.IsSelected()", click)
        self.assertIn("spell_map->MoveUnit(select_pos, event.AltDown()) == 0",
                      click)
        self.assertIn('GetTacticalInputBlockReason()) == "none"', click)
        mouse_move = MAIN.split("void MainFrame::OnCanvasMouseMove(", 1)[1].split(
            "// on canvas wheel", 1
        )[0]
        self.assertNotIn("spell_map->MoveUnit(", mouse_move)


if __name__ == "__main__":
    unittest.main()
