// Where the config preview stands, beside the configs window.
//
// ⭐ WHAT THESE PROTECT (rhoquinn8217, 2026-11-11 and 12): right, left, above
// or below the window, in that order, the first with room, centred on it; in
// Advanced, centred on the window, over it ("In advance view, center the
// preview on the config box"); never off the screen.
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

    section("preview: above Simple at the bottom of the screen, centred on it, neither side having room");
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

    section("preview: right before the rest, where it has room (their order, 2026-11-12)");
    {
        // Quick moved to the bottom left
        const Place p = place(Rect{200, 1581, 1665, 2088}, work, w, h, gap, false);
        CTM_CHECK_EQ(p.x, 1665 + gap);
        CTM_CHECK_EQ(p.y, 2088 - h);   // level with its middle, kept on the screen
    }

    section("preview: left before above, where the right has no room (their order, later on 2026-11-12)");
    {
        // Quick at the bottom right: above has room too, and left comes first
        const Place p = place(Rect{2400, 1581, 3800, 2088}, work, w, h, gap, false);
        CTM_CHECK_EQ(p.x, 2400 - gap - w);
        CTM_CHECK_EQ(p.y, 2088 - h);   // level with its middle, kept on the screen
    }

    section("preview: left at the top of the screen too, where the right has no room");
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

    section("preview: in Advanced, centred on the window, over it (their word, 2026-11-12)");
    {
        // Advanced at its opening size, most of the screen
        const Place p = place(Rect{490, 233, 3349, 1855}, work, w, h, gap, true);
        CTM_CHECK_EQ(p.x, 490 + (2859 - w) / 2);
        CTM_CHECK_EQ(p.y, 233 + (1622 - h) / 2);
        CTM_CHECK_EQ(covered(Rect{490, 233, 3349, 1855}, p, w, h), static_cast<long long>(w) * h);
    }

    section("preview: in Advanced, centred even where a side has room");
    {
        // a small Advanced window, the right with room: centred on it all the same
        const Place p = place(Rect{1000, 600, 2000, 1400}, work, w, h, gap, true);
        CTM_CHECK_EQ(p.x, 1000 + (1000 - w) / 2);
        CTM_CHECK_EQ(p.y, 600 + (800 - h) / 2);
        CTM_CHECK(covered(Rect{1000, 600, 2000, 1400}, p, w, h) > 0);
    }

    section("preview: in Advanced near the screen's corner, centred but kept on the screen");
    {
        const Place p = place(Rect{0, 0, 1200, 800}, work, w, h, gap, true);
        CTM_CHECK_EQ(p.x, 0);
        CTM_CHECK_EQ(p.y, 0);
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
