// Where the config preview opens, beside the configs window.
//
// ⭐ WHAT THESE PROTECT (rhoquinn8217, 2026-11-11): the box opens to the right
// of the configs window, or to the left when there is no room, with its bottom
// level with the window's, and never off the screen.
//
// WHAT THESE CANNOT DO. No browser starts here: the box coming up where it was
// asked for is looked at with the listener running.

#include "harness.h"

#include "app/preview_place_rule.inl"

using namespace ctmtest;

int run_preview_place_rule_tests()
{
    using namespace preview_place_rule;

    // A 4K screen at 150%, its work area above a taskbar: 3840 x 2088.
    const Rect work{0, 0, 3840, 2088};
    const int w = 900, h = 900, gap = 12;   // 600 x 600 at 150%

    section("preview: on the right of the window when there is room");
    {
        // Simple at the bottom centre: 1323 wide, 1258..2581
        const Place p = place(Rect{1258, 1441, 2581, 2088}, work, w, h, gap);
        CTM_CHECK_EQ(p.x, 2581 + gap);
        // its bottom level with the window's
        CTM_CHECK_EQ(p.y, 2088 - h);
    }

    section("preview: on the left when the right has no room");
    {
        // a window near the right edge
        const Place p = place(Rect{2500, 1000, 3700, 1600}, work, w, h, gap);
        CTM_CHECK_EQ(p.x, 2500 - gap - w);
        CTM_CHECK_EQ(p.y, 1600 - h);
    }

    section("preview: at the screen's right edge, over the window, when neither side has room");
    {
        // Advanced's large, nearly the whole screen
        const Place p = place(Rect{58, 31, 3782, 2057}, work, w, h, gap);
        CTM_CHECK_EQ(p.x, 3840 - w);
        CTM_CHECK_EQ(p.y, 2057 - h);
    }

    section("preview: never above the screen, nor below it");
    {
        // a short window high on the screen: its bottom is above the box's height
        const Place high = place(Rect{1000, 0, 2000, 400}, work, w, h, gap);
        CTM_CHECK_EQ(high.y, 0);
        // a window hanging below the work area
        const Place low = place(Rect{1000, 1800, 2000, 2300}, work, w, h, gap);
        CTM_CHECK_EQ(low.y, 2088 - h);
    }

    section("preview: a screen to the left of the first, in its own coordinates");
    {
        const Rect left{-3840, 0, 0, 2088};
        const Place p = place(Rect{-2582, 1441, -1259, 2088}, left, w, h, gap);
        CTM_CHECK_EQ(p.x, -1259 + gap);
        CTM_CHECK(p.x + w <= 0);
    }

    return 0;
}
