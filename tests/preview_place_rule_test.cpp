// Where the config preview stands, beside the configs window.
//
// ⭐ WHAT THESE PROTECT (rhoquinn8217, 2026-11-11 and 12): right, above, left
// or below the window, in that order, the first with room, centred on it; in
// Advanced, on the side with the most room, over the window if it must be;
// never off the screen.
//
// WHAT THESE CANNOT DO. No browser starts here: the box standing and following
// where it was asked to is looked at with the listener running.

#include "harness.h"

#include "app/preview_place_rule.inl"

using namespace ctmtest;

int run_preview_place_rule_tests()
{
    using namespace preview_place_rule;

    // A 4K screen at 150%, its work area above a taskbar: 3840 x 2088, and the
    // box at 1000 x 720 there.
    const Rect work{0, 0, 3840, 2088};
    const int w = 1500, h = 1080, gap = 12;

    section("preview: above Simple at the bottom of the screen, centred on it");
    {
        // Simple at the bottom centre, 1323 x 647 at 150%
        const Place p = place(Rect{927, 1118, 2912, 2088}, work, w, h, gap, false);
        CTM_CHECK_EQ(p.x, 927 + (1985 - w) / 2);
        CTM_CHECK_EQ(p.y, 1118 - gap - h);
    }

    section("preview: above Quick too");
    {
        // Quick at the bottom centre, 977 x 338 at 150%
        const Place p = place(Rect{1187, 1581, 2652, 2088}, work, w, h, gap, false);
        CTM_CHECK_EQ(p.x, 1187 + (1465 - w) / 2);
        CTM_CHECK_EQ(p.y, 1581 - gap - h);
    }

    section("preview: right before above, where both have room (their order, 2026-11-12)");
    {
        // Quick moved to the bottom left
        const Place p = place(Rect{200, 1581, 1665, 2088}, work, w, h, gap, false);
        CTM_CHECK_EQ(p.x, 1665 + gap);
        CTM_CHECK_EQ(p.y, 2088 - h);   // level with its middle, kept on the screen
    }

    section("preview: above before left, where the right has no room");
    {
        const Place p = place(Rect{2400, 1581, 3800, 2088}, work, w, h, gap, false);
        // centred on the window would end 10 px past the screen: kept on it
        CTM_CHECK_EQ(p.x, 3840 - w);
        CTM_CHECK_EQ(p.y, 1581 - gap - h);
    }

    section("preview: left before below, where neither the right nor above has room");
    {
        const Place p = place(Rect{2400, 0, 3800, 500}, work, w, h, gap, false);
        CTM_CHECK_EQ(p.x, 2400 - gap - w);
        CTM_CHECK_EQ(p.y, 0);
    }

    section("preview: below a window with no room above");
    {
        const Place p = place(Rect{1000, 0, 2500, 500}, work, w, h, gap, false);
        CTM_CHECK_EQ(p.x, 1000);
        CTM_CHECK_EQ(p.y, 500 + gap);
    }

    section("preview: right of a window as tall as the screen, level with its middle");
    {
        const Place p = place(Rect{0, 0, 1400, 2088}, work, w, h, gap, false);
        CTM_CHECK_EQ(p.x, 1400 + gap);
        CTM_CHECK_EQ(p.y, (2088 - h) / 2);
    }

    section("preview: left when the right has no room either");
    {
        const Place p = place(Rect{2400, 0, 3840, 2088}, work, w, h, gap, false);
        CTM_CHECK_EQ(p.x, 2400 - gap - w);
        CTM_CHECK_EQ(p.y, (2088 - h) / 2);
    }

    section("preview: in Advanced, the side with the most room, over the window if it must be");
    {
        // Advanced at its opening size, most of the screen: the right covers least
        const Place p = place(Rect{490, 233, 3349, 1855}, work, w, h, gap, true);
        CTM_CHECK_EQ(p.x, 3840 - w);
        CTM_CHECK_EQ(p.y, 233 + (1622 - h) / 2);
        CTM_CHECK(covered(Rect{490, 233, 3349, 1855}, p, w, h) > 0);
    }

    section("preview: in Advanced, a side with room wins over one without");
    {
        // a small Advanced window: only the right has room
        const Place p = place(Rect{1000, 600, 2000, 1400}, work, w, h, gap, true);
        CTM_CHECK_EQ(p.x, 2000 + gap);
        CTM_CHECK_EQ(p.y, 600 + (800 - h) / 2);
        CTM_CHECK_EQ(covered(Rect{1000, 600, 2000, 1400}, p, w, h), 0);
    }

    section("preview: nowhere with room outside Advanced, the least covered, on the screen");
    {
        const Place p = place(Rect{490, 233, 3349, 1855}, work, w, h, gap, false);
        CTM_CHECK_EQ(p.x, 3840 - w);
        CTM_CHECK(p.y >= 0 && p.y + h <= 2088);
    }

    section("preview: a screen to the left of the first, in its own coordinates");
    {
        const Rect left{-3840, 0, 0, 2088};
        const Place p = place(Rect{-2653, 1581, -1188, 2088}, left, w, h, gap, false);
        CTM_CHECK_EQ(p.y, 1581 - gap - h);
        CTM_CHECK(p.x >= -3840 && p.x + w <= 0);
    }

    return 0;
}
