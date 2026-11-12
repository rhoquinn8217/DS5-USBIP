// Where the config preview opens, beside the configs window.
//
// ⭐ WHAT THESE PROTECT (rhoquinn8217, 2026-11-11): the box opens to the right
// of the configs window, or to the left when there is no room, with its bottom
// level with the window's; above the window when neither side has room, which
// at its large text is Simple's case on a screen at 150%; never off the screen.
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
    const int w = 1290, h = 990, gap = 12;   // 860 x 660 at 150%

    section("preview: on the right of the window when there is room");
    {
        // Quick at the bottom centre, 977 x 338 at 150%: 1465 x 507
        const Place p = place(Rect{1187, 1581, 2652, 2088}, work, 900, 900, gap);
        CTM_CHECK_EQ(p.x, 2652 + gap);
        // its bottom level with the window's
        CTM_CHECK_EQ(p.y, 2088 - 900);
    }

    section("preview: on the left when the right has no room");
    {
        // a window near the right edge
        const Place p = place(Rect{2500, 1000, 3700, 1600}, work, w, h, gap);
        CTM_CHECK_EQ(p.x, 2500 - gap - w);
        CTM_CHECK_EQ(p.y, 1600 - h);
    }

    section("preview: above the window, centred on it, when neither side has room");
    {
        // Simple at the bottom centre, 1323 x 647 at 150%: 1985 x 970
        const Rect simple{927, 1118, 2912, 2088};
        const Place p = place(simple, work, w, h, gap);
        CTM_CHECK_EQ(p.x, 927 + (1985 - w) / 2);
        CTM_CHECK_EQ(p.y, 1118 - gap - h);
        CTM_CHECK(p.y + h <= simple.top);
    }

    section("preview: at the screen's right edge, over the window, when there is no room anywhere");
    {
        // Advanced at its opening size, most of the screen
        const Place p = place(Rect{487, 268, 3346, 1889}, work, w, h, gap);
        CTM_CHECK_EQ(p.x, 3840 - w);
        CTM_CHECK_EQ(p.y, 1889 - h);
    }

    section("preview: never off the screen");
    {
        // a short window high on the screen, the box beside it: not above the top
        const Place high = place(Rect{1000, 0, 2000, 400}, work, 900, 900, gap);
        CTM_CHECK_EQ(high.x, 2000 + gap);
        CTM_CHECK_EQ(high.y, 0);
        // a window hanging below the work area
        const Place low = place(Rect{1000, 1800, 2000, 2300}, work, 900, 900, gap);
        CTM_CHECK_EQ(low.y, 2088 - 900);
    }

    section("preview: a screen to the left of the first, in its own coordinates");
    {
        const Rect left{-3840, 0, 0, 2088};
        const Place p = place(Rect{-2652, 1581, -1187, 2088}, left, 900, 900, gap);
        CTM_CHECK_EQ(p.x, -1187 + gap);
        CTM_CHECK(p.x + 900 <= 0);
    }

    return 0;
}
