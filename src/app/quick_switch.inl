// The quick config switch: two fingers pressing the touchpad open the configs
// window in Quick view for as long as a finger stays on it, each swipe of a
// finger up or down steps the controller to the next config or the one before
// (one thumb resting, the other swiping), and the window closes the moment the
// last finger lifts. For every controller.
//
// ⭐ rhoquinn8217, 2026-10-23, the gesture as it stands: *"change create swipe
// up/down gesture to, two finger touch and press to temporarily open the quick
// mode config window.  Keep the window open as long as the a least 1 finger is
// touching.  Allow 1 finger swipe up or down to change the config.  Close
// immediately when there are no fingers touching the touchpad.  Disable all
// touch pad rebinds during this time including touch-pad to mouse."*
// ⓘ It began the same day as Create held and two fingers swiped (*"while
// holding create button, swipe up to go to next config and down to go to
// previous config"*), with *"always use quick view for quick config select
// button/gesture"* and *"this is essentially a global setting"*, and was
// corrected once (*"Config’s own two-finger swipe up and down should still
// work ... When simple and quick mode are in focus, two finger swipe up or
// down is disabled and only control the wheel."*) before the press replaced
// Create. That last rule stands: with the window in front in Simple or Quick,
// two fingers up or down move the wheel (step(), below).
//
// ⓘ This file is the setting, the gestures and the keys a step is sent as,
// with no windows.h, so the test binary can reach them.
// quick_switch_session.inl opens and closes the window and keeps the steps;
// rebind.inl feeds it, and touch_mouse.inl stands aside for it.
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

// ⭐ How far a finger goes up or down for a step: the two-finger swipe's own
// distances (touch_mouse.inl, kSwipeVerticalUnits and kSwipeSidewaysUnits), so
// the gestures read the same to a hand. Up or down, about a quarter of a
// DualSense's pad, which is 1080 tall; sideways decides a two-finger touch at
// a quarter of its 1920 width, and steps nothing.
constexpr int kVerticalUnits = 300;
constexpr int kSidewaysUnits = 480;

// ⭐ THE PRESS THAT OPENS IT: the touchpad pressed in while two fingers are on
// it, at the moment it goes in. Once per press; a press held does not open it
// again. ⓘ The TV's release gesture is two fingers pressed and HELD about four
// seconds, so a quick press is the one to make.
struct Press {
    bool wasPressed = false;
};

inline bool opens(Press &p, int fingers, bool pressed)
{
    const bool edge = pressed && !p.wasPressed;
    p.wasPressed = pressed;
    return edge && fingers == 2;
}

// ⭐ A FINGER SWIPED UP OR DOWN, WHILE THE WINDOW IS OPEN, ANOTHER RESTING OR
// NOT (rhoquinn8217, 2026-10-23: "after pressing the touchpad with two finders,
// 1 finger will not move and the other will swipe up and down.  For example
// both thumbs touch and press and release the touchpad but both thumbs don't
// left from the touchpad.  Then the only the right finger is lifted.  Swipe up
// and swipe down with only the right thumb should switch the configs next or
// previous config."). Each finger on the pad is followed by itself, from where
// it landed: one that has gone kVerticalUnits up or down, more than it went
// sideways, steps once for that touch; one that goes kSidewaysUnits sideways
// first steps nothing. So the resting thumb, which does not move, never
// steps, and the swiping one steps once a swipe, lifting between swipes.
// ⓘ A finger is known by the id the pad gives each touch, kept while it stays
// down.
struct Touch {
    bool down = false;
    int id = -1;
    float x = 0.0f;
    float y = 0.0f;
};

struct Track {
    int id = -1;        // -1: no finger
    float x0 = 0.0f;
    float y0 = 0.0f;
    bool done = false;  // this touch has decided
};

struct Swipes {
    Track t[2];
};

// The steps the two fingers of this report make together: +1 for each that has
// just gone far enough up (the next config), -1 for each down, 0 none. ⓘ The
// pad's y grows downward.
inline int swipe_steps(Swipes &s, const Touch (&now)[2])
{
    int steps = 0;
    Track next[2];
    for (int i = 0; i < 2; ++i) {
        if (!now[i].down) continue;
        Track tr;
        tr.id = now[i].id;
        tr.x0 = now[i].x;
        tr.y0 = now[i].y;
        for (int j = 0; j < 2; ++j) {
            if (s.t[j].id == now[i].id && s.t[j].id >= 0) tr = s.t[j];
        }
        if (!tr.done) {
            const float dx = now[i].x - tr.x0;
            const float dy = now[i].y - tr.y0;
            const float ax = dx < 0 ? -dx : dx;
            const float ay = dy < 0 ? -dy : dy;
            if (ay >= ax && ay >= static_cast<float>(kVerticalUnits)) {
                tr.done = true;
                steps += dy < 0 ? 1 : -1;
            } else if (ax > ay && ax >= static_cast<float>(kSidewaysUnits)) {
                tr.done = true;
            }
        }
        next[i] = tr;
    }
    s.t[0] = next[0];
    s.t[1] = next[1];
    return steps;
}

// One controller's two-finger swipe in the window, followed report by report.
struct Pad {
    bool tracking = false;   // two fingers down and armed, measured from x0, y0
    float x0 = 0.0f;
    float y0 = 0.0f;
    bool done = false;       // this touch has decided; again once every finger lifts
};

// ⭐ TWO FINGERS UP OR DOWN WITH THE WINDOW IN FRONT IN SIMPLE OR QUICK: +1 the
// next config (up), -1 the one before (down), 0 none. Only while armed, and
// once per touch, as the two-finger swipe acts: the first direction met
// decides it, so a stroke that goes sideways steps nothing however it ends.
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
// the wheel whatever has focus.
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
