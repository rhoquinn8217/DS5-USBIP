// Where the config preview opens: beside the configs window.
//
// ⭐ rhoquinn8217, 2026-11-11: the preview is "a separate browser box" beside
// the configs window, and asked "The box opens to the right of the configs
// window, or to the left if there's no room", *"yes"*. Its bottom is level
// with the window's, so in Simple and Quick, which sit at the bottom of the
// screen, the two stand side by side, and a box that grows (the page asks
// for that itself) grows upward.
// ⭐ ABOVE THE WINDOW when neither side has room, centred on it: the box's
// text was made half as large again the same day ("text in the preview needs
// to be large"), and at that size it no longer fits beside Simple's window at
// the bottom of a screen at 150%, where above it there is room.
// ⓘ No room there either (Advanced's window, most of the screen): the box goes
// to the screen's right edge, over the window. It never leaves the screen's
// work area.
// ⓘ Its own file, with no windows.h, so the test binary can reach it;
// open_ui.inl reads the window and the screen and starts the browser.
#pragma once

namespace preview_place_rule {

struct Rect {
    int left;
    int top;
    int right;
    int bottom;
};

struct Place {
    int x;
    int y;
};

// The box's top-left, w x h, beside `window`, `gap` from it, on the screen
// whose work area is `work`. All in the same pixels.
inline Place place(const Rect &window, const Rect &work, int w, int h, int gap)
{
    Place p{};
    if (window.right + gap + w <= work.right) {
        p.x = window.right + gap;
        p.y = window.bottom - h;
    } else if (window.left - gap - w >= work.left) {
        p.x = window.left - gap - w;
        p.y = window.bottom - h;
    } else if (window.top - gap - h >= work.top) {
        p.x = window.left + (window.right - window.left - w) / 2;
        p.y = window.top - gap - h;
    } else {
        p.x = work.right - w;
        p.y = window.bottom - h;
    }
    if (p.x + w > work.right) p.x = work.right - w;
    if (p.x < work.left) p.x = work.left;
    if (p.y + h > work.bottom) p.y = work.bottom - h;
    if (p.y < work.top) p.y = work.top;
    return p;
}

}  // namespace preview_place_rule
