// The quick config switch: Create held and two fingers swiped up for the next
// config, down for the one before, for every controller, the configs window
// shown in Quick view while it switches.
//
// ⭐ rhoquinn8217, 2026-10-23: *"while holding create button, swipe up to go to
// next config and down to go to previous config / when the chord/gesture is
// caught, the window will open temporarily to show you that you are switching
// configs.  While the config is up, you can continue to swipe up or down while
// you are holding the create button.  Once you let go of the create button give
// a few second for the use to see what config was landed on then close the
// config window."* Then *"always use quick view for quick config select
// button/gesture"*, and their answers: Create still reaches the game; two
// fingers, *"this is essentially a global setting.  when turned on, the pre
// controller setting will be disabled"*; and the game losing the pad while
// the window shows is fine.
// ⭐ CORRECTED THE SAME DAY: *"Config’s own two-finger swipe up and down should
// still work, they just should no nothing when create button is held
// Create should still resize the window when it is in focus and the touchpad
// should still move the cursor but not while Create is held.  When simple and
// quick mode are in focus, two finger swipe up or down is disabled and only
// control the wheel."*
//
// ⓘ This file is the setting, the swipe and the keys a step is sent as, with
// no windows.h, so the test binary can reach them. quick_switch_session.inl
// opens and closes the window and keeps the steps; rebind.inl feeds it.
// ⓘ Kept in window-state.txt as "quick_config_switch = 1" or "= 0", beside
// Open on bridge. Off until it is turned on, and with that file gone.
#pragma once

#include <string>

namespace quick_switch {

inline const char *const kKey = "quick_config_switch";

// On only when the line says 1: junk in a hand-editable file is off.
inline bool parse(const std::string &value)
{
    return value == "1";
}

inline std::string line(bool on)
{
    return std::string(kKey) + " = " + (on ? "1" : "0") + "\n";
}

// ⭐ How far the fingers' midpoint goes up or down for a step: the two-finger
// swipe's own distances (touch_mouse.inl, kSwipeVerticalUnits and
// kSwipeSidewaysUnits), so the two read the same to a hand. Up or down, about
// a quarter of a DualSense's pad, which is 1080 tall; sideways decides a touch
// at a quarter of its 1920 width, and steps nothing.
constexpr int kVerticalUnits = 300;
constexpr int kSidewaysUnits = 480;

// ⭐ How long the window stays once Create is let go, so the config landed on
// can be read (*"give a few second"*). The page links a step once the steps
// stop (its QUICK_LINK_MS), well inside this.
constexpr long long kLingerMs = 2000;

// One controller's swipe, followed report by report.
struct Pad {
    bool tracking = false;   // two fingers down and armed, measured from x0, y0
    float x0 = 0.0f;
    float y0 = 0.0f;
    bool done = false;       // this touch has decided; again once every finger lifts
};

// The step a report makes: +1 the next config (two fingers up), -1 the one
// before (down), 0 none. Only while armed -- Create held, or the window in
// front in Simple or Quick -- and once per touch, as the two-finger swipe
// acts: the first direction met decides it, so a stroke that goes sideways
// steps nothing however it ends. ⓘ The pad's y grows downward.
inline int step(Pad &p, bool armed, int fingers, float mx, float my)
{
    if (fingers != 2 || !armed) {
        p.tracking = false;
        if (fingers == 0) p.done = false;
        return 0;
    }
    if (!p.tracking) {
        p.tracking = true;
        p.x0 = mx;
        p.y0 = my;
        return 0;
    }
    if (p.done) return 0;
    const float dx = mx - p.x0;
    const float dy = my - p.y0;
    const float ax = dx < 0 ? -dx : dx;
    const float ay = dy < 0 ? -dy : dy;
    if (ay >= ax && ay >= static_cast<float>(kVerticalUnits)) {
        p.done = true;
        return dy < 0 ? 1 : -1;
    }
    if (ax > ay && ax >= static_cast<float>(kSidewaysUnits)) p.done = true;
    return 0;
}

// ⭐ THE PAGE HEARS A STEP AS A KEY: KeyJ for the next config, KeyK for the
// one before, with the gate's three modifiers (rebind.inl), held a moment and
// let go, one at a time. The page's CONFIG_MODE_KEYS turn them into a step of
// Quick's wheel whatever has focus.
constexpr long long kKeyDownMs = 40;
constexpr long long kKeyGapMs = 40;

inline const char *key_code(int step)
{
    return step > 0 ? "KeyJ" : "KeyK";
}

struct Keys {
    int pending = 0;       // steps waiting: more next than before, or fewer
    int current = 0;       // the step being sent: +1, -1, or 0 for none
    bool inGap = false;    // its key is up again, before the next
    long long until = 0;   // when the press, or the gap after it, ends
};

inline void queue(Keys &k, int step)
{
    k.pending += step;
}

// Nothing waiting and nothing on its way.
inline bool idle(const Keys &k)
{
    return k.pending == 0 && k.current == 0;
}

// The key to hold down on this report, or nullptr, asked on every report while
// the page can hear the steps.
inline const char *key_now(Keys &k, long long nowMs)
{
    if (k.current != 0) {
        if (!k.inGap) {
            if (nowMs < k.until) return key_code(k.current);
            k.inGap = true;
            k.until = nowMs + kKeyGapMs;
            return nullptr;
        }
        if (nowMs < k.until) return nullptr;
        k.current = 0;
        k.inGap = false;
    }
    if (k.pending == 0) return nullptr;
    k.current = k.pending > 0 ? 1 : -1;
    k.pending -= k.current;
    k.until = nowMs + kKeyDownMs;
    return key_code(k.current);
}

}  // namespace quick_switch
