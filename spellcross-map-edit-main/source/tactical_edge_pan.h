#pragma once

// Standalone tactical edge-pan math. No wxWidgets dependency: camera motion
// must depend on elapsed time, never on the number of mouse-move messages.
#include <algorithm>
#include <cmath>
#include <utility>

namespace spellcross_tactical
{
    struct EdgePanDirection
    {
        double x = 0.0;
        double y = 0.0;
    };

    // The far edge is exclusive. Constrain the hot zone on small viewports
    // so opposing edges never overlap and the centre always stays neutral.
    inline double EdgePanAxis(int position, int begin, int end, int zone)
    {
        if (position < begin || position >= end || zone <= 0 || end <= begin + 2)
            return 0.0;
        const int active = (std::min)(zone, (end - begin) / 3);
        if (active <= 0)
            return 0.0;

        const int fromNear = position - begin;
        if (fromNear < active)
        {
            const double t = static_cast<double>(active - fromNear) / active;
            return -t * t;
        }
        const int fromFar = end - position - 1;
        if (fromFar < active)
        {
            const double t = static_cast<double>(active - fromFar) / active;
            return t * t;
        }
        return 0.0;
    }

    // battlefieldBottom is the *actual* top of the HUD at the cursor's x.
    // A cursor over the HUD never pans, including near the left/right edges.
    inline EdgePanDirection EdgePanAt(int x, int y, int width, int battlefieldBottom, int zone)
    {
        if (x < 0 || x >= width || y < 0 || y >= battlefieldBottom)
            return {};
        return { EdgePanAxis(x, 0, width, zone),
            EdgePanAxis(y, 0, battlefieldBottom, zone) };
    }

    class EdgePanAccumulator
    {
        double m_xFraction = 0.0;
        double m_yFraction = 0.0;

        static int Integrate(double direction, double& fraction,
            double seconds, double logicalPixelsPerScreenPixel, double screenPixelsPerSecond)
        {
            if (direction == 0.0)
            {
                fraction = 0.0;
                return 0;
            }
            // A direction reversal must not inherit the opposite direction's
            // fractional remainder, which otherwise feels like sticky edges.
            if (fraction * direction < 0.0)
                fraction = 0.0;
            const double distance = fraction + direction * seconds *
                logicalPixelsPerScreenPixel * screenPixelsPerSecond;
            // Avoid a lost pixel at exact integer boundaries caused by
            // binary floating-point rounding (e.g. 0.48 * 100).
            const double rounding = distance >= 0.0 ? 1e-9 : -1e-9;
            const int whole = static_cast<int>(std::trunc(distance + rounding));
            fraction = distance - whole;
            return whole;
        }

    public:
        void Reset() { m_xFraction = m_yFraction = 0.0; }

        std::pair<int, int> Advance(EdgePanDirection direction, double elapsedSeconds,
            double logicalPixelsPerScreenPixel, double maxScreenPixelsPerSecond = 480.0)
        {
            // Do not catch up several seconds after a breakpoint, modal dialog,
            // window drag, or unexpectedly slow paint.
            const double seconds = (std::clamp)(elapsedSeconds, 0.0, 0.05);
            return {
                Integrate(direction.x, m_xFraction, seconds,
                    logicalPixelsPerScreenPixel, maxScreenPixelsPerSecond),
                Integrate(direction.y, m_yFraction, seconds,
                    logicalPixelsPerScreenPixel, maxScreenPixelsPerSecond)
            };
        }
    };
}
