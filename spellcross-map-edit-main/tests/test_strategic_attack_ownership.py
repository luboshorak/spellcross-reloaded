"""Regression checks for strategic mission launch ownership policy.

Runs without wxWidgets; on hosts with a C++ compiler, extracts and compiles
the actual C++ eligibility method against synthetic campaign scenarios.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "source" / "forms" / "form_level.cpp"
HEADER = ROOT / "source" / "forms" / "form_level.h"


def extract_policy(source: str) -> str:
    anchor = "bool StrategicLevelFrame::CanLaunchTerritoryMission(int territory_id) const"
    begin = source.index(anchor)
    start = source.index("{", begin)
    depth = 0
    for i in range(start, len(source)):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0:
                return source[begin:i + 1]
    raise ValueError("Unterminated launch eligibility method")


class StrategicAttackOwnershipTests(unittest.TestCase):
    def test_every_launch_path_uses_the_same_policy(self):
        source = SOURCE.read_text(encoding="utf-8")
        header = HEADER.read_text(encoding="utf-8")
        self.assertIn("bool CanLaunchTerritoryMission(int territory_id) const;", header)
        self.assertIn("bool StrategicLevelFrame::CanLaunchTerritoryMission(", source)
        self.assertIn("const bool canAttack = CanLaunchTerritoryMission(m_selectedTerritory)", source)
        self.assertIn("m_btnLaunch->Enable(onStrategicMap && CanLaunchTerritoryMission(m_selectedTerritory)", source)
        self.assertEqual(source.count("m_btnLaunch->Enable(onStrategicMap && CanLaunchTerritoryMission("), 2)
        self.assertIn("if (!m_main || !CanLaunchTerritoryMission(m_selectedTerritory))", source)

    def test_policy_with_real_cpp_method(self):
        compiler = shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("C++ compiler not installed (run on CI/dev machine)")

        method = extract_policy(SOURCE.read_text(encoding="utf-8"))
        harness = """
#include <algorithm>
#include <cstdint>
#include <vector>
struct LevelTerritory { int id; };
struct LevelData { std::vector<LevelTerritory> territories; };
struct CounterAttackState {
    int territory_id; bool triggered; bool completed;
};
class StrategicLevelFrame {
public:
    LevelData m_level;
    bool m_gameModeEnabled = true;
    std::vector<std::uint8_t> m_visibleTerritory;
    std::vector<int> m_ownedTerritories;
    std::vector<CounterAttackState> m_counterAttacks;
    bool CanLaunchTerritoryMission(int territory_id) const;
};
""" + method + """
int main() {
    StrategicLevelFrame f;
    f.m_level.territories = {{1}, {2}, {3}};
    f.m_visibleTerritory = {0, 1, 1, 0};
    f.m_ownedTerritories = {1};
    if (f.CanLaunchTerritoryMission(0) || f.CanLaunchTerritoryMission(99)) return 1;
    if (f.CanLaunchTerritoryMission(1)) return 2;  // owned, no attack
    if (!f.CanLaunchTerritoryMission(2)) return 3; // revealed hostile
    if (f.CanLaunchTerritoryMission(3)) return 4;  // hidden
    f.m_counterAttacks = {{1, false, false}};
    if (f.CanLaunchTerritoryMission(1)) return 5;  // merely scheduled
    f.m_counterAttacks = {{1, true, false}};
    if (!f.CanLaunchTerritoryMission(1)) return 6; // active defence
    f.m_counterAttacks = {{1, true, true}};
    if (f.CanLaunchTerritoryMission(1)) return 7;  // finished attack
    f.m_ownedTerritories.clear();
    if (!f.CanLaunchTerritoryMission(1)) return 8; // enemy-held recapture
    f.m_gameModeEnabled = false;
    if (!f.CanLaunchTerritoryMission(3)) return 9; // editor mode
    return 0;
}
"""
        with tempfile.TemporaryDirectory() as d:
            source_path = Path(d) / "test.cpp"
            executable = Path(d) / ("test.exe" if os.name == "nt" else "test")
            source_path.write_text(harness, encoding="utf-8")
            subprocess.run(
                [compiler, "-std=c++17", str(source_path), "-o", str(executable)],
                check=True, capture_output=True, text=True,
            )
            subprocess.run([str(executable)], check=True, capture_output=True)


if __name__ == "__main__":
    unittest.main()
