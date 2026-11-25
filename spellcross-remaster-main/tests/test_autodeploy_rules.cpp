// Pure algorithm regression: no retail media or wxWidgets required.
#include "../source/game_autodeploy.h"
#include <cassert>
#include <vector>

int main()
{
    using spellcross_autodeploy::UnitSlots;
    using spellcross_autodeploy::Assign;
    using spellcross_autodeploy::Enabled;
    using spellcross_autodeploy::SetEnabled;
    assert(!Enabled()); // absent config defaults to classic manual placement
    SetEnabled(true);
    assert(Enabled());
    SetEnabled(false);

    std::vector<int> assignment;
    assert(Assign({{false, {0, 1}}, {false, {0}}}, 2, assignment));
    assert(assignment.size() == 2);
    assert(assignment[0] == 1 && assignment[1] == 0);

    // Air and ground may share one square; two ground troops may not.
    assert(Assign({{false, {0}}, {true, {0}}}, 1, assignment));
    assert(assignment[0] == 0 && assignment[1] == 0);
    assert(!Assign({{false, {0}}, {false, {0}}}, 1, assignment));

    // Failed planning must leave the previous result intact.
    assert(assignment.size() == 2 && assignment[0] == 0);
    assert(!Assign({{false, {}}}, 1, assignment));
    assert(!Assign({{true, {4}}}, 1, assignment));
    assert(!Assign({{false, {0}}}, 0, assignment));

    assert(Assign({}, 0, assignment) && assignment.empty());
    assert(Assign({{true, {2}}, {true, {1, 2}}, {false, {2}}},
                  3, assignment));
    assert(assignment[0] == 2);
    assert(assignment[1] == 1);
    assert(assignment[2] == 2);
}
