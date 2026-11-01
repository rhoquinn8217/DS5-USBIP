// The configs window's browser is asked for its size and place in the
// browser's own units, not the listener's.
//
// ⭐ rhoquinn8217, 2026-10-23: *"when the window opens in simple or quick mode,
// a larger window blips open and close then the simple or quick mode window
// opens after.  Can we prevent that first larger window open blip?"* The log
// had it: the browser was started at Advanced's opening size, 1664 x 1298 in
// the listener's pixels, and its window came up 2496 x 1947 at 15,15 -- most of
// the screen -- before the restore put Quick's 1201 x 409 at its place.
// ⛔ THE BROWSER READS --window-size AND --window-position IN SCALED UNITS. The
// listener is per-monitor DPI aware and works in physical pixels; at the 150%
// this PC streams at, a window asked for 800 x 600 at 200,150 came up 1200 x 900
// at 300,225 (measured with a throwaway profile, 2026-10-23). So a size or a
// place is divided by the display's scale on its way to the browser.
// ⓘ Its own file, with no windows.h, so the test binary can reach it;
// open_ui.inl asks the display's scale and starts the browser.
#pragma once

namespace window_open_rule {

// A length or a coordinate in physical pixels, in the browser's units on a
// display of this many dots per inch (96 is 100%). Rounded to the nearest, so
// a place comes back within a pixel; a scale the display does not say is
// taken as 100%.
inline int browser_units(int physical, unsigned dpi)
{
    if (dpi == 0) dpi = 96;
    const double v = static_cast<double>(physical) * 96.0 / static_cast<double>(dpi);
    return static_cast<int>(v < 0 ? v - 0.5 : v + 0.5);
}

}  // namespace window_open_rule
