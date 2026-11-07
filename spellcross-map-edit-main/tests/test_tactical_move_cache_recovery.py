"""The real click must not depend on a possibly stale asynchronous move
range. Regression for intermittent 'MOVEMENT MODE but Hummer won't move'.
"""
from pathlib import Path
import unittest

SOURCE = Path(__file__).resolve().parents[1] / "source"
MAP = (SOURCE / "map.cpp").read_text(encoding="utf-8")
HEADER = (SOURCE / "map.h").read_text(encoding="utf-8")
MAIN = (SOURCE / "main.cpp").read_text(encoding="utf-8")
GRAPHICS = (SOURCE / "spellcross.cpp").read_text(encoding="utf-8")


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

    def test_action_points_and_position_invalidate_previous_move_range(self):
        worker = MAP.split("void SpellMap::MoveRange::Worker()", 1)[1].split(
            "// set default", 1
        )[0]
        self.assertIn("const int range_ap = unit->action_points;", worker)
        self.assertIn("const MapXY range_pos = unit->coor;", worker)
        self.assertEqual(worker.count("completed_range_ap = range_ap;"), 2)
        self.assertEqual(worker.count("completed_range_pos = range_pos;"), 2)
        self.assertIn("completed_range_ap == unit->action_points", MAP)
        self.assertIn("completed_range_pos.x == unit->coor.x", MAP)
        self.assertIn("completed_range_pos.y == unit->coor.y", MAP)
        self.assertIn("return HasCachedMoveRangeLocked(unit);", MAP)

    def test_finishing_deployment_initializes_range_before_first_player_input(self):
        deployment = MAIN.split("void MainFrame::FinishDeployment()", 1)[1].split(
            "// File > Load custom map", 1
        )[0]
        self.assertIn("spell_map->RefreshSelectedMovePreview()", deployment)
        self.assertLess(
            deployment.index("spell_map->MissionStartEvent();"),
            deployment.index("spell_map->RefreshSelectedMovePreview()"),
        )
        self.assertLess(
            deployment.index("spell_map->InvalidateUnitsView();"),
            deployment.index("spell_map->RefreshSelectedMovePreview()"),
        )
        self.assertIn("Initial movement preview unavailable after deployment", deployment)

    def test_other_units_rebuilding_occupancy_invalidates_same_unit_move_preview(self):
        # Selected-unit AP/position is unchanged when a friendly blocker moves.
        self.assertIn("std::atomic<uint64_t> unit_placement_revision{ 0 };", HEADER)
        self.assertIn("uint64_t completed_range_units_revision = 0;", HEADER)
        sorted_units = MAP.split("void SpellMap::SortUnits()", 1)[1].split(
            "// clear units layer array", 1
        )[0]
        self.assertIn("unit_placement_revision.fetch_add(1, std::memory_order_relaxed);", sorted_units)
        worker = MAP.split("void SpellMap::MoveRange::Worker()", 1)[1].split(
            "// Keep heap priorities", 1
        )[0]
        self.assertIn("const uint64_t range_units_revision =", worker)
        self.assertEqual(worker.count("completed_range_units_revision = range_units_revision;"), 2)
        validation = MAP.split("bool SpellMap::MoveRange::HasCachedMoveRangeLocked(", 1)[1].split(
            "bool SpellMap::MoveRange::HasCachedMoveRange(", 1
        )[0]
        self.assertIn(
            "completed_range_units_revision == map->unit_placement_revision.load(std::memory_order_relaxed)",
            validation,
        )
        self.assertIn("unit_range->HasCachedMoveRangeLocked(unit_selection)", MAP)
        self.assertIn("unit_range->GetCachedMoveAP(unit, npos)", MAP)

    def test_rmb_and_space_rebuild_stale_range_for_visual_preview(self):
        temporary = MAP.split("int SpellMap::BeginTemporaryUnitRangeViewMode(", 1)[1].split(
            "int SpellMap::EndTemporaryUnitRangeViewMode(", 1
        )[0]
        self.assertIn("if (mode == UNIT_RANGE_MOVE)", temporary)
        self.assertIn("RefreshSelectedMovePreview();", temporary)
        persistent = MAP.split("int SpellMap::SetUnitRangeViewMode(", 1)[1].split(
            "int SpellMap::BeginTemporaryUnitRangeViewMode(", 1
        )[0]
        self.assertIn("unit_range_view_mode == UNIT_RANGE_MOVE", persistent)
        self.assertIn("RefreshSelectedMovePreview();", persistent)
        refresh = MAP.split("bool SpellMap::RefreshSelectedMovePreview(", 1)[1].split(
            "// check if unit can move", 1
        )[0]
        self.assertIn("!unit_range->HasCachedMoveRange(selected)", refresh)
        self.assertIn("!unit_range->IsRangePending(selected)", refresh)
        self.assertIn("unit_range->WaitIdle();", refresh)
        self.assertIn("bool RefreshSelectedMovePreview();", HEADER)

    def test_move_overlay_does_not_show_other_unit_range(self):
        render = MAP.split("int SpellMap::Render(", 1)[1].split(
            "// render unit", 1
        )[0]
        self.assertIn("m_render_move_range_current = unit_range->HasCachedMoveRangeLocked(unit_selection)", render)
        palette = MAP.split("uint8_t* SpellMap::GetUnitRangeFilter(", 1)[1].split(
            "// set map view mode", 1
        )[0]
        self.assertIn("viewingUnitMoveRange() && m_render_move_range_current", palette)

    def test_preview_cursor_requires_current_range_owner(self):
        body = MAP.split("int SpellMap::CanUnitMove(", 1)[1].split(
            "// move unit (in game mode)", 1
        )[0]
        self.assertIn("unit_range->GetCachedMoveAP(unit, npos)", body)
        self.assertNotIn("unit_range->ap_left[npos]", body)

    def test_preview_refreshes_after_background_range_replaces_player_cache(self):
        options = MAP.split("int SpellMap::GetUnitOptions(", 1)[1].split(
            "// reset AP of all units", 1
        )[0]
        self.assertIn("!unit_range->HasCachedMoveRange(unit_selection)", options)
        self.assertLess(options.index("unit_range->FindRange(unit_selection);"),
                        options.index("CanUnitMove(select_pos)"))
        self.assertIn("unit_range->WaitIdle();", options)

    def test_inflight_range_is_reused_instead_of_restarted_per_mouse_move(self):
        options = MAP.split("int SpellMap::GetUnitOptions(", 1)[1].split(
            "// reset AP of all units", 1
        )[0]
        self.assertIn("bool IsRangePending(const MapUnit* unit);", HEADER)
        self.assertIn("MapUnit* requested_range_unit = nullptr", HEADER)
        self.assertIn("requested_range_unit = unit;", MAP)
        self.assertIn("requested_range_unit == unit", MAP)
        self.assertIn("!unit_range->IsRangePending(unit_selection)", options)
        self.assertIn("g_move_range_dirty_for == unit_selection", options)
        self.assertLess(options.index("!unit_range->IsRangePending(unit_selection)"),
                        options.rindex("unit_range->FindRange(unit_selection);"))

    def test_pending_range_still_waits_before_returning_to_input(self):
        # #129 stopped re-queuing an existing range (good), but also skipped
        # WaitIdle() while it was in flight. The previous successful build
        # always waited before handing control back to the tactical UI.
        options = MAP.split("int SpellMap::GetUnitOptions(", 1)[1].split(
            "// reset AP of all units", 1
        )[0]
        pending = options.split("// Tick() may already be computing", 1)[1].split(
            "if (unit_selection && g_attack_map_dirty_for", 1
        )[0]
        self.assertIn("if (!unit_range->IsRangePending(unit_selection))", pending)
        self.assertIn("unit_range->FindRange(unit_selection);", pending)
        self.assertIn("unit_range->WaitIdle();", pending)
        self.assertLess(pending.index("if (!unit_range->IsRangePending"),
                        pending.index("unit_range->FindRange(unit_selection);"))
        self.assertLess(pending.index("unit_range->FindRange(unit_selection);"),
                        pending.index("unit_range->WaitIdle();"))

    def test_visible_move_overlay_recovers_if_ai_overwrites_shared_cache(self):
        # A previous enemy/panic/reaction calculation can replace the only
        # MoveRange result without changing the selected unit. Refresh before
        # taking Render's map and result locks, never under them.
        render = MAP.split("int SpellMap::Render(wxBitmap", 1)[1].split(
            "// find selection tiles", 1
        )[0]
        self.assertIn("viewingUnitMoveRange()", render)
        self.assertIn("RefreshSelectedMovePreview();", render)
        self.assertIn("!enemy_turn_running", render)
        self.assertLess(render.index("RefreshSelectedMovePreview();"),
                        render.index("LockMap();") if "LockMap();" in render else 999999)
        rendered = MAP.split("int SpellMap::Render(wxBitmap", 1)[1].split(
            "// The shared worker can last have published", 1
        )[0]
        self.assertLess(rendered.index("RefreshSelectedMovePreview();"),
                        rendered.index("unit_range->ResultLock(true);"))

    def test_native_movement_boot_cursor_remains_wired_to_move_option(self):
        self.assertIn('gres.cur_move = gres.RenderCUR("DOJAZD.CUR");', GRAPHICS)
        mouse_move = MAIN.split("void MainFrame::OnCanvasMouseMove(", 1)[1].split(
            "// on canvas wheel", 1
        )[0]
        self.assertIn(
            "else if (options == SpellMap::UNIT_OPT_MOVE) cur = spell_data->gres.cur_move;",
            mouse_move,
        )

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
