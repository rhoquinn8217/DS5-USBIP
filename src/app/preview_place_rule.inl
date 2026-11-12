// Where the config preview stands: beside the configs window, and with it as
// it moves.
//
// ⭐ rhoquinn8217, 2026-11-11: "Make the preview follow the main config and
// move the window above it, below it, to it's right or to it's left base on
// whether there's space. When preview is opened in advance mode, put on the
// side with the most space even if it overlapes the config window."
// ⭐ THE ORDER IS THEIRS (2026-11-12: "preview position preference in order
// by most preferred to least preferred: to the right, above, to the left,
// below").
// ➡️ So: right, above, left, below, the first where the box fits on the
// screen without covering the window; above and below centred on the window,
// right and left level with its middle. In Advanced, and wherever no side
// has room, the side where the box covers the least of the window, the room
// left over, then that order, deciding a tie. It never leaves the screen's
// work area.
// ⓘ The box grows taller for a longer config (the page asks), and stands
// where this rule puts a box of that size.
// ⓘ Its own file, with no windows.h, so the test binary can reach it;
// open_ui.inl reads the windows and the screen.
#pragma once

#include <algorithm>

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

enum class Side { Above, Below, Right, Left };

// The box's top-left, w x h, `gap` from the window on `side`.
inline Place beside(const Rect &window, int w, int h, int gap, Side side)
{
    const int midX = window.left + (window.right - window.left - w) / 2;
    const int midY = window.top + (window.bottom - window.top - h) / 2;
    switch (side) {
    case Side::Above: return Place{ midX, window.top - gap - h };
    case Side::Below: return Place{ midX, window.bottom + gap };
    case Side::Right: return Place{ window.right + gap, midY };
    case Side::Left:  return Place{ window.left - gap - w, midY };
    }
    return Place{ midX, midY };
}

// ...kept on the screen.
inline Place on_screen(Place p, const Rect &work, int w, int h)
{
    if (p.x + w > work.right) p.x = work.right - w;
    if (p.x < work.left) p.x = work.left;
    if (p.y + h > work.bottom) p.y = work.bottom - h;
    if (p.y < work.top) p.y = work.top;
    return p;
}

// Whether a box w x h fits on `side` without covering the window.
inline bool fits(const Rect &window, const Rect &work, int w, int h, int gap, Side side)
{
    const bool wide = w <= work.right - work.left;
    const bool tall = h <= work.bottom - work.top;
    switch (side) {
    case Side::Above: return wide && window.top - gap - h >= work.top;
    case Side::Below: return wide && window.bottom + gap + h <= work.bottom;
    case Side::Right: return tall && window.right + gap + w <= work.right;
    case Side::Left:  return tall && window.left - gap - w >= work.left;
    }
    return false;
}

// How much of the window a box at p covers, in square pixels.
inline long long covered(const Rect &window, Place p, int w, int h)
{
    const int x0 = std::max(window.left, p.x), x1 = std::min(window.right, p.x + w);
    const int y0 = std::max(window.top, p.y), y1 = std::min(window.bottom, p.y + h);
    if (x1 <= x0 || y1 <= y0) return 0;
    return static_cast<long long>(x1 - x0) * static_cast<long long>(y1 - y0);
}

// The room on `side` beyond the box, negative where the box does not fit.
inline int room(const Rect &window, const Rect &work, int w, int h, int gap, Side side)
{
    switch (side) {
    case Side::Above: return window.top - gap - h - work.top;
    case Side::Below: return work.bottom - (window.bottom + gap + h);
    case Side::Right: return work.right - (window.right + gap + w);
    case Side::Left:  return window.left - gap - w - work.left;
    }
    return 0;
}

inline Place place(const Rect &window, const Rect &work, int w, int h, int gap, bool advanced)
{
    const Side order[] = { Side::Right, Side::Above, Side::Left, Side::Below };
    if (!advanced) {
        for (Side s : order) {
            if (fits(window, work, w, h, gap, s)) return on_screen(beside(window, w, h, gap, s), work, w, h);
        }
    }
    Place best = on_screen(beside(window, w, h, gap, order[0]), work, w, h);
    long long bestCovered = covered(window, best, w, h);
    int bestRoom = room(window, work, w, h, gap, order[0]);
    for (Side s : order) {
        const Place p = on_screen(beside(window, w, h, gap, s), work, w, h);
        const long long c = covered(window, p, w, h);
        const int r = room(window, work, w, h, gap, s);
        if (c < bestCovered || (c == bestCovered && r > bestRoom)) {
            best = p;
            bestCovered = c;
            bestRoom = r;
        }
    }
    return best;
}

}  // namespace preview_place_rule
