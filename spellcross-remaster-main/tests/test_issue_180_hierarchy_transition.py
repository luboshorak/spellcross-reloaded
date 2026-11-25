"""#180: stable command/formation assignments must survive campaign chapter changes."""
from pathlib import Path
import unittest

CODE = (Path(__file__).resolve().parents[1] /
        "source/forms/form_level.cpp").read_text(encoding="utf-8")


class HierarchyTransitionRegression(unittest.TestCase):
    def test_chapter_transition_restores_uid_based_hierarchy_before_save(self):
        transition = CODE.split(
            "void StrategicLevelFrame::AdvanceToNextLevel()", 1
        )[1]
        transition = transition.split("\n    Destroy();", 1)[0]

        self.assertIn("newWin->RestoreHierarchyAssignments(CaptureHierarchyAssignments());",
                      transition)
        self.assertLess(transition.index("newWin->NormalizeStrategicUnitInstances();"),
                        transition.index("newWin->RestoreHierarchyAssignments("))
        self.assertLess(transition.index("newWin->NormalizeCommanderIdentities();"),
                        transition.index("newWin->RestoreHierarchyAssignments("))
        self.assertLess(transition.index("newWin->RestoreHierarchyAssignments("),
                        transition.index("newWin->SaveStrategicState();"))

    def test_removed_temporary_units_are_not_reintroduced(self):
        transition = CODE.split(
            "void StrategicLevelFrame::AdvanceToNextLevel()", 1
        )[1].split("\n    Destroy();", 1)[0]
        self.assertIn("if (IsTemporaryUnitIndex(i))", transition)
        self.assertLess(transition.index("if (IsTemporaryUnitIndex(i))"),
                        transition.index("newWin->RestoreHierarchyAssignments("))

        restore = CODE.split(
            "void StrategicLevelFrame::RestoreHierarchyAssignments(", 1
        )[1].split("void StrategicLevelFrame::ApplyHierarchyDrop(", 1)[0]
        self.assertIn("FindUnitIndexByUid(uid)", restore)
        self.assertIn("findCommander(rec.commander_uid)", restore)
        self.assertIn("m_hierarchySlotIndex.find(rec.slot_id)", restore)

    def test_manual_load_from_previous_chapter_preserves_hierarchy_too(self):
        load = CODE.split(
            "void StrategicLevelFrame::LoadPlayerStateFromPreviousLevel()", 1
        )[1].split("void StrategicLevelFrame::SaveStrategicState()", 1)[0]
        self.assertIn("RestoreHierarchyAssignments(loadedHierarchy);", load)
        self.assertLess(load.index("RestoreHierarchyAssignments(loadedHierarchy);"),
                        load.index("SaveStrategicState();"))


if __name__ == "__main__":
    unittest.main()
