"""Regression checks for range-worker lifecycle and task-queue synchronization."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1] / "source"
HEADER = (ROOT / "map.h").read_text(encoding="utf-8")
CODE = (ROOT / "map.cpp").read_text(encoding="utf-8")


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


if __name__ == "__main__":
    unittest.main()
