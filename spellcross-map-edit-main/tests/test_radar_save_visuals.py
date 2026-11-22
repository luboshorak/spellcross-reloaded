"""Regression: a deployed tactical radar must stay visibly deployed after load.

The binary tactical save stores radar_up, but not FSU_resource pointers. This
test guards the deserializer-to-renderer wiring used by both Czech and English
radars (unit types 30 and 31) without requiring proprietary game assets.
"""
from pathlib import Path
import unittest


SOURCE_ROOT = Path(__file__).resolve().parents[1] / "source"
MAP = (SOURCE_ROOT / "map.cpp").read_text(encoding="utf-8")
UNITS = (SOURCE_ROOT / "spell_units.cpp").read_text(encoding="utf-8")


class RadarSaveVisualRegressionTests(unittest.TestCase):
    def test_disk_save_still_persists_radar_state_and_frame(self):
        write = MAP.split("static void scsave_write_unit(", 1)[1].split(
            "static bool scsave_read_unit(", 1
        )[0]
        self.assertIn("scsave::write(os, (uint8_t)u->radar_up);", write)
        self.assertIn("scsave::write(os, (int32_t)u->frame);", write)

    def test_load_reattaches_deployed_radar_sprite_without_cost(self):
        read = MAP.split("static bool scsave_read_unit(", 1)[1].split(
            "// A click can follow a selection change", 1
        )[0]
        self.assertIn("u->radar_up = radar_up;", read)
        self.assertIn("u->frame = frame;", read)
        restore = read.split("// The save stores the deployed radar flag", 1)[1].split(
            "u->spec_type = ", 1
        )[0]
        self.assertIn("u->radar_up && u->unit && u->unit->isActionToggleRadar()", restore)
        self.assertIn("u->unit->gr_action && u->unit->gr_action->anim.frames > 0", restore)
        self.assertIn("u->in_animation = u->unit->gr_action;", restore)
        self.assertIn("u->frame = u->frame_stop - 1;", restore)
        self.assertIn("(std::min)(u->unit->action_fsu_frames, u->in_animation->anim.frames)", restore)
        self.assertNotIn("action_points -=", restore)
        self.assertNotIn("PlayAction()", restore)

    def test_live_radar_and_renderer_use_same_action_sprite(self):
        toggle = MAP.split("void SpellMap::OnHUDradarToggle()", 1)[1].split(
            "// aircraft land/takeoff HUD button pressed", 1
        )[0]
        self.assertIn("unit->in_animation = unit->unit->gr_action;", toggle)
        self.assertIn("unit->action_state = MapUnit::ACTION_STATE::RADAR_UP;", toggle)
        render = UNITS.split("int MapUnit::Render(Terrain* data,", 1)[1].split(
            "// render unit preview for map units list", 1
        )[0]
        self.assertIn("if(!in_animation)", render)
        self.assertIn("loc_frame = -1;", render)
        self.assertIn("loc_frame, in_animation, altitude", render)

    def test_only_live_deployment_finishes_with_radar_flag(self):
        tick = MAP.split("else if (unit->action_state == MapUnit::ACTION_STATE::RADAR_UP)", 1)[1].split(
            "else if (unit->action_state == MapUnit::ACTION_STATE::RADAR_DOWN)", 1
        )[0]
        self.assertIn("unit->radar_up = true;", tick)
        self.assertIn("unit->frame = unit->frame_stop - 1;", tick)


if __name__ == "__main__":
    unittest.main()
