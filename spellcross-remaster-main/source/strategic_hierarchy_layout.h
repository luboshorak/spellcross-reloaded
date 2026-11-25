#pragma once

namespace spellcross_strategy
{
    struct HierarchyPageIds
    {
        int first_battalion;
        int first_regiment;
        int brigade;
    };

    constexpr HierarchyPageIds HierarchyPage(int page)
    {
        return page <= 1 ? HierarchyPageIds{1, 1, 1}
                         : HierarchyPageIds{5, 3, 2};
    }

    constexpr int HierarchyPoolMaxScroll(int unit_rows,
                                         int commander_rows,
                                         int visible_rows)
    {
        const int overflow = unit_rows + commander_rows + 2 - visible_rows;
        return overflow > 0 ? overflow : 0;
    }

    struct HierarchyOverlayGeometry
    {
        int width;
        int height;
        int x;
        int y;
    };

    // BRG_ACT contains one 138 px-wide brigade frame and the long connectors
    // from both regiment rows. Its 23,598 decoded pixels are 138x171, not
    // 207x114 (the same product with a corrupt row stride).
    inline constexpr HierarchyOverlayGeometry kBrigadeActiveOverlay{
        138, 171, 258, 136
    };

    inline constexpr int kBrigadeCommanderSlotX = 261;
    inline constexpr int kBrigadeCommanderSlotY = 207;
    inline constexpr int kBrigadeSlotWidth = 132;
    inline constexpr int kBrigadeOverlayFrameY = 68;

    static_assert(kBrigadeActiveOverlay.width * kBrigadeActiveOverlay.height == 23598,
        "BRG_ACT decoded size changed");
    static_assert(kBrigadeActiveOverlay.x + 3 == kBrigadeCommanderSlotX,
        "BRG_ACT frame must surround the brigade slot horizontally");
    static_assert(kBrigadeActiveOverlay.y + kBrigadeOverlayFrameY + 3 ==
        kBrigadeCommanderSlotY,
        "BRG_ACT frame must surround the brigade slot vertically");
    static_assert(kBrigadeActiveOverlay.width == kBrigadeSlotWidth + 6,
        "BRG_ACT frame must keep the native three-pixel slot inset");
}
