// Original strategic DTA-only recapture positioning.
// L5 explicitly marks Other Side counter-attack posts; fixed north/south
// heuristics are wrong on maps whose Alliance L6 deployment is in the north.
#pragma once
#include <vector>

namespace spellcross_strategy
{
    template<class Point, class Valid>
    std::vector<Point> ChooseGeneratedEnemySeeds(
        const std::vector<Point>& authoredEnemyPosts,
        const std::vector<Point>& heuristicFallback,
        Valid isValidPosition)
    {
        std::vector<Point> result;
        // Keep authoring order, since multiple posts describe distinct
        // enemy sectors; do not spread the first set across an arbitrary band.
        for (const Point& post : authoredEnemyPosts)
            if (isValidPosition(post))
                result.push_back(post);
        if (!result.empty())
            return result;

        // Custom or incomplete maps may omit L5; only then allow a fallback.
        for (const Point& post : heuristicFallback)
            if (isValidPosition(post))
                result.push_back(post);
        return result;
    }
}
