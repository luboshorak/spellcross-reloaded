"""Regression checks for range-worker lifecycle and task-queue synchronization."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1] / "source"
HEADER = (ROOT / "map.h").read_text(encoding="utf-8")
CODE = (ROOT / "map.cpp").read_text(encoding="utf-8")
EVENTS = (ROOT / "spell_map_event.cpp").read_text(encoding="utf-8")
EVENT_HEADER = (ROOT / "spell_map_event.h").read_text(encoding="utf-8")


class WorkerSafetyContract(unittest.TestCase):
    def test_no_volatile_cross_thread_state(self):
        self.assertNotIn("volatile ThreadCtrl state", HEADER)
        self.assertEqual(HEADER.count("std::atomic<ThreadCtrl> state{ IDLE };"), 2)
        self.assertEqual(HEADER.count("std::atomic<bool> is_halted{ true };"), 2)

    def test_control_fields_initialized_before_threads_start(self):
        for klass in ("MoveRange", "ViewRange"):
            with self.subTest(klass=klass):
                start = CODE.index(f"SpellMap::{klass}::{klass}(SpellMap* map)")
                end = CODE.index(f"SpellMap::{klass}::~{klass}()", start)
                ctor = CODE[start:end]
                self.assertRegex(ctor, re.compile(
                    rf"is_halted\s*=\s*true;\s*was_halted\s*=\s*1;\s*"
                    rf"thread\s*=\s*new std::thread\(&SpellMap::{klass}::Worker", re.S))

    def test_nested_halts_do_not_collapse_to_a_boolean(self):
        self.assertNotIn("was_halted = is_halted;", CODE)
        self.assertGreaterEqual(CODE.count("is_halted = was_halted > 0;"), 2)

    def test_queue_inspection_uses_control_lock(self):
        start = CODE.index("int SpellMap::ViewRange::isIdle()")
        end = CODE.index("int SpellMap::ViewRange::WaitIdle()", start)
        self.assertIn("std::lock_guard<std::mutex> guard(ctrl_lock)", CODE[start:end])

        start = CODE.index("void SpellMap::MoveRange::WaitIdle()")
        end = CODE.index("void SpellMap::MoveRange::ResultLock(", start)
        self.assertIn("std::lock_guard<std::mutex> guard(ctrl_lock)", CODE[start:end])

    def test_deployment_unit_addition_uses_safe_range_worker_lock_order(self):
        # Deployment calls AddUnit() repeatedly on the UI thread while the
        # view and movement range workers may still be finishing old tasks.
        # Never hold map_lock while waiting for workers to halt or resuming
        # those workers; otherwise lock-order inversion / races are possible.
        add = CODE.split("int SpellMap::AddUnit(MapUnit* unit)", 1)[1].split(
            "// try to place new unit", 1
        )[0]
        stages = [
            "HaltUnitRanging(true);",
            "LockMap();",
            "units.push_back(unit);",
            "ReleaseMap();",
            "ResumeUnitRanging(false);",
        ]
        for previous, following in zip(stages, stages[1:]):
            with self.subTest(before=previous, after=following):
                self.assertLess(add.index(previous), add.index(following))

    def test_tactical_save_restoration_does_not_recursively_lock_map(self):
        restore = CODE.split("int SpellMap::Saves::Load(", 1)[1].split(
            "// save initial state", 1
        )[0]
        self.assertLess(restore.index("map->HaltUnitRanging(true);"),
                        restore.index("map->LockMap();"))
        self.assertNotIn("map->AddUnit(copy);", restore)
        self.assertIn("map->units.push_back(copy);", restore)
        self.assertLess(restore.index("map->ReleaseMap();"),
                        restore.index("map->ResumeUnitRanging(false);"))
        self.assertIn("map->RecalculateTacticalFormations();", restore)

    def test_initial_snapshot_does_not_reenter_map_lock_via_events(self):
        save = CODE.split("int SpellMap::Saves::Save(", 1)[1].split(
            "// load save position", 1
        )[0]
        self.assertLess(save.index("map->HaltUnitRanging(true);"),
                        save.index("map->LockMap();"))
        self.assertIn("save->events->AddEventAlreadyLocked(", save)
        self.assertNotIn("save->events->AddEvent(", save)
        self.assertLess(save.index("map->ReleaseMap();"),
                        save.index("map->ResumeUnitRanging(false);"))

    def test_restore_events_do_not_reenter_map_lock(self):
        restore = CODE.split("int SpellMap::Saves::Load(", 1)[1].split(
            "// save initial state", 1
        )[0]
        for safe, unsafe in (
            ("map->events->ClearEventsAlreadyLocked();",
             "map->events->ClearEvents();"),
            ("map->events->AddEventAlreadyLocked(",
             "map->events->AddEvent("),
            ("map->events->RelinkUnitsAlreadyLocked();",
             "map->events->RelinkUnits();"),
        ):
            self.assertIn(safe, restore)
            self.assertNotIn(unsafe, restore)

    def test_event_lock_free_primitives_and_wrapper_order(self):
        for name, invoked in (
            ("AddEvent", "AddEventAlreadyLocked("),
            ("ClearEvents", "ClearEventsAlreadyLocked("),
            ("RelinkUnits", "RelinkUnitsAlreadyLocked("),
        ):
            with self.subTest(name=name):
                start = EVENTS.index(f"SpellMapEvents::{name}(")
                body = EVENTS[start:EVENTS.index("\n}", start)]
                stages = ["HaltUnitRanging(true);", "LockMap();", invoked,
                          "ReleaseMap();", "ResumeUnitRanging(false);"]
                for prev, nxt in zip(stages, stages[1:]):
                    self.assertLess(body.index(prev), body.index(nxt))
                self.assertIn(f"{name}AlreadyLocked(", EVENT_HEADER)

    def test_reset_and_event_unit_linking_do_not_lock_twice(self):
        reset = EVENTS.split("void SpellMapEvents::ResetEvents()", 1)[1].split(
            "// build links to particular units", 1
        )[0]
        self.assertIn("RelinkUnitsAlreadyLocked();", reset)
        self.assertNotIn("\n\tRelinkUnits();", reset)
        unit_add = EVENTS.split("int SpellMapEventRec::AddUnit(", 1)[1].split(
            "// try extract unit", 1
        )[0]
        self.assertIn("map->events->RelinkUnitsAlreadyLocked();", unit_add)

    def test_tactical_load_logs_stage_boundaries(self):
        body = CODE.split("int SpellMap::LoadGameStateFromFile(", 1)[1].split(
            "SpellMap::~SpellMap()", 1
        )[0]
        for stage in (
            "Tactical load stage: reloading base map",
            "Tactical load stage: base map loaded",
            "Tactical load stage: restoring units=%llu",
            "Tactical load stage: restored state applied",
        ):
            self.assertIn(stage, body)


if __name__ == "__main__":
    unittest.main()
