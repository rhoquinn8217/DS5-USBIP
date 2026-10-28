// The configs window's size and place, in the browser's units.
//
// ⭐ WHAT THESE PROTECT (rhoquinn8217, 2026-10-23): the window opens at its
// layout's size and place, so no larger window shows first. The browser reads
// a size and a place in scaled units, and the listener works in physical
// pixels: at 150% a window asked for in physical pixels came up half as large
// again.
//
// WHAT THESE CANNOT DO. No browser starts here: the window coming up where it
// was asked for is looked at with the listener running.

#include "harness.h"

#include "app/window_open_rule.inl"

using namespace ctmtest;

int run_window_open_rule_tests()
{
    using namespace window_open_rule;

    section("window open: a size or a place in the browser's units");
    {
        // 100%: as it is
        CTM_CHECK_EQ(browser_units(1201, 96), 1201);
        // 150%, this PC streaming: Quick's 1201 x 409 at 1321,1381
        CTM_CHECK_EQ(browser_units(1201, 144), 801);
        CTM_CHECK_EQ(browser_units(409, 144), 273);
        CTM_CHECK_EQ(browser_units(1321, 144), 881);
        CTM_CHECK_EQ(browser_units(1381, 144), 921);
        // what was measured: 1200 x 900 at 300,225 came from 800 x 600 at 200,150
        CTM_CHECK_EQ(browser_units(1200, 144), 800);
        CTM_CHECK_EQ(browser_units(225, 144), 150);
        // 125%
        CTM_CHECK_EQ(browser_units(1000, 120), 800);
        // a display to the left of the first: a place below zero, rounded the same way
        CTM_CHECK_EQ(browser_units(-1321, 144), -881);
        // a scale the display does not say: 100%
        CTM_CHECK_EQ(browser_units(640, 0), 640);
    }

    return 0;
}
