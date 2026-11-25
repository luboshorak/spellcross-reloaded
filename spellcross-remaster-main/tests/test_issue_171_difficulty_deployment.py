"""Issue #171: exercise production enemy-XP methods without the wxWidgets GUI.

The test extracts the real C++ method bodies from map.cpp, compiles them with
minimal MapUnit/SpellMap stand-ins, and verifies the deployment lifecycle and
experience levels. This guards against the bug where SetGameMode(1) runs
before any Alliance companies have been placed onto the map.
"""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
MAP_CPP = ROOT / "source" / "map.cpp"
MAIN_CPP = ROOT / "source" / "main.cpp"


def method(source: str, signature: str) -> str:
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 0
    for i in range(opening, len(source)):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0:
                return source[start:i + 1]
    raise AssertionError(f"Unclosed C++ method: {signature}")


STUB = r"""
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <vector>

struct MapUnit {
    bool is_enemy = false;
    bool difficulty_adjusted = false;
    bool dead = false;
    bool active = true;
    int experience_level = 1;
    bool isDead() const { return dead; }
    bool isActive() const { return active; }
    void InitExperience(int level) { experience_level = level; }
};

class SpellMap {
public:
    enum class GameDifficulty { EASY = 0, NORMAL = 1, HARD = 2 };
    GameDifficulty game_difficulty = GameDifficulty::NORMAL;
    bool game_mode = false;
    bool deployment_mode = false;
    std::vector<MapUnit*> units;
    void AdjustEnemyExperienceForDifficulty(MapUnit* enemy);
    void ApplyDifficultyEnemyExperience();
    void FinalizeEnemyDifficultyAfterDeployment();
};
"""

TEST_CASES = r"""
int main()
{
    std::srand(7);

    // The pre-deployment call sees only enemies; XP must remain untouched.
    SpellMap hard;
    hard.game_difficulty = SpellMap::GameDifficulty::HARD;
    hard.game_mode = true;
    hard.deployment_mode = true;
    MapUnit enemy;
    enemy.is_enemy = true;
    hard.units.push_back(&enemy);
    hard.ApplyDifficultyEnemyExperience();
    assert(enemy.experience_level == 1 && !enemy.difficulty_adjusted);

    // Even if one company has arrived, partial deployment is not final.
    MapUnit allied;
    allied.experience_level = 1;
    hard.units.push_back(&allied);
    hard.FinalizeEnemyDifficultyAfterDeployment();
    assert(enemy.experience_level == 1 && !enemy.difficulty_adjusted);

    // Once placement finishes, a rank-0 friendly force makes hard OS level 3-4.
    hard.deployment_mode = false;
    hard.FinalizeEnemyDifficultyAfterDeployment();
    assert(enemy.difficulty_adjusted);
    assert(enemy.experience_level >= 3 && enemy.experience_level <= 4);

    // A changed setting must not reroll an already-adjusted enemy/save.
    const int previous = enemy.experience_level;
    hard.game_difficulty = SpellMap::GameDifficulty::NORMAL;
    hard.FinalizeEnemyDifficultyAfterDeployment();
    assert(enemy.experience_level == previous);

    // Native scripted veterans are never downgraded by the difficulty floor.
    MapUnit veteran;
    veteran.is_enemy = true;
    veteran.experience_level = 8;
    hard.units.push_back(&veteran);
    hard.AdjustEnemyExperienceForDifficulty(&veteran);
    assert(veteran.experience_level == 8 && veteran.difficulty_adjusted);

    // Normal mode on the same rookie force has no hard-mode XP bonus.
    SpellMap normal;
    normal.game_mode = true;
    normal.game_difficulty = SpellMap::GameDifficulty::NORMAL;
    MapUnit normal_enemy;
    normal_enemy.is_enemy = true;
    normal.units = {&allied, &normal_enemy};
    normal.FinalizeEnemyDifficultyAfterDeployment();
    assert(normal_enemy.experience_level >= 1 && normal_enemy.experience_level <= 2);

    // Difficulty has no effect on a cancelled deployment / editor session.
    SpellMap editor;
    MapUnit editor_enemy;
    editor_enemy.is_enemy = true;
    editor.units = {&allied, &editor_enemy};
    editor.FinalizeEnemyDifficultyAfterDeployment();
    assert(editor_enemy.experience_level == 1 && !editor_enemy.difficulty_adjusted);

    // Reinforcements arriving after mission start use the same rule.
    MapUnit spawned_enemy;
    spawned_enemy.is_enemy = true;
    hard.game_difficulty = SpellMap::GameDifficulty::HARD;
    hard.units.push_back(&spawned_enemy);
    hard.AdjustEnemyExperienceForDifficulty(&spawned_enemy);
    assert(spawned_enemy.experience_level >= 3);
    return 0;
}
"""


class Issue171DifficultyRegression(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.map_src = MAP_CPP.read_text(encoding="utf-8")
        cls.main_src = MAIN_CPP.read_text(encoding="utf-8")

    def test_finalization_runs_before_first_save(self):
        # Check actual UI integration, not only the isolated algorithm.
        finish = method(self.main_src, "void MainFrame::FinishDeployment()")
        self.assertLess(finish.index("SetDeploymentMode(false)"),
                        finish.index("FinalizeEnemyDifficultyAfterDeployment()"))
        self.assertLess(finish.index("FinalizeEnemyDifficultyAfterDeployment()"),
                        finish.index("SaveInitial()"))
        self.assertLess(finish.index("SaveInitial()"),
                        finish.index("MissionStartEvent()"))
        start = method(self.main_src, "void MainFrame::StartTacticalGame()")
        self.assertLess(start.index("SetGameMode(1)"),
                        start.index("SetDeploymentMode(m_deployment_active)"))

    def test_real_cpp_experience_rule_after_deployment(self):
        compiler = shutil.which(os.environ.get("CXX", "c++"))
        if not compiler:
            self.skipTest("C++ compiler unavailable")
        bodies = "\n\n".join(method(self.map_src, "void SpellMap::" + name + "(")
                           for name in ("AdjustEnemyExperienceForDifficulty",
                                        "ApplyDifficultyEnemyExperience",
                                        "FinalizeEnemyDifficultyAfterDeployment"))
        with tempfile.TemporaryDirectory(prefix="spellcross-issue171-") as temp:
            source_file = Path(temp) / "test.cpp"
            executable = Path(temp) / "test"
            source_file.write_text(STUB + bodies + TEST_CASES, encoding="utf-8")
            subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra",
                            "-Werror", str(source_file), "-o", str(executable)],
                           check=True, capture_output=True, text=True)
            subprocess.run([str(executable)], check=True, capture_output=True, text=True)


if __name__ == "__main__":
    unittest.main()
