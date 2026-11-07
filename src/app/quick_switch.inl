// The quick config switch: two fingers pressing the touchpad open the configs
// window in Quick view as the press comes up (not for a press held to the TV's
// release), for as long as a finger stays on it; each swipe of a finger up or
// down steps the controller to the next config or the one before (one thumb
// resting, the other swiping), and the window closes the moment the last
// finger lifts. For every controller.
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
// Create. With the window in front in Simple or Quick, two fingers swiped up
// or down moved the wheel until rhoquinn8217, 2026-10-26: *"I want to change
// two finger swipe when quick mode or simple mode is up to the same 1 thumb
// touch and 1 thumb swipe to switch to next or previous config."*
// (front_steps(), below).
// ⭐ AND THE PRESS AND THE TOUCHPAD HELD BACK, A SECOND PRESS KEEPING THE
// WINDOW (rhoquinn8217, 2026-10-27: *"if there are two fingers touching the
// touchpad, if the touchpad is pressed, that press should not register in
// windows or the game.  In addition to opening the config window temporarily,
// it should also temporarily restrict touchpad touch, tap and press from
// reaching windows or the game until both fingers are lifted.  Possible
// actions while touching the touchpad. / * 1 finger scroll up or down while
// the other finger stays put / * press again to lift restriction and the
// config window persists after both fingers are lifted / Only these for
// now."*). The pad shortcut, two fingers resting and Options, stays as it is
// (*"Leave it the way it is whether ticked or not."*). And the second press
// puts the window in the layout from before, Simple or Advanced, at once
// (2026-10-28: *"on the second press, it should bring you to simple or
// advance based on what you had open last"*), where at first it stayed Quick
// and the layout came back only once it closed. while_held() and the layout
// key, below; the holding is in mouse_exclusive.inl.
// ⭐ AND THE WINDOW OPENS AS THE PRESS COMES BACK UP, not as it goes in, and
// only for a press shorter than the TV's release gesture (rhoquinn8217,
// 2026-10-28: *"for 2 finger press release and 1 figure hold, don't bring up
// the quick mode until the press is released and only if the press is less
// than 4 seconds which is when a release is attempted."*): two fingers
// pressed and held about four seconds release the pad on the TV, and the
// window came up over it the moment they pressed. Armed and armed_report(),
// below.
//
// ⓘ This file is the setting, the gestures and the keys a step is sent as,
// with no windows.h, so the test binary can reach them.
// quick_switch_session.inl opens and closes the window and keeps the steps;
// rebind.inl feeds it, and touch_mouse.inl stands aside for it.
// ⓘ Kept in window-state.txt as "quick_config_switch = 1" or "= 0", beside
// Open on bridge, under the line "quick_config_switch_default = on". On
// until it is turned off, and with that file gone.
// ⭐⭐ ON UNTIL IT IS TURNED OFF, SINCE 2026-10-30 (rhoquinn8217, on every
// feature being on out of the box: *"flip the default"*). It was off before,
// so two of the four ways of opening the window did nothing on a fresh
// install. ⓘ For an existing install too, once: see loaded() below.
#pragma once

#include <deque>
#include <string>

namespace quick_switch {

inline const char *const kKey = "quick_config_switch";

// The switch's default: on (2026-10-30; off before).
inline constexpr bool kDefault = true;

// ⭐ The line every save writes since the default became on. A file without
// it was written when the default was off, so its "quick_config_switch = 0"
// said only that, and is dropped once (loaded() below).
inline const char *const kDefaultKey = "quick_config_switch_default";

// On only when the line says 1: junk in a hand-editable file is off.
inline bool parse(const std::string &value)
{
    return value == "1";
}

inline std::string line(bool on)
{
    return std::string(kKey) + " = " + (on ? "1" : "0") + "\n";
}

// The line that says the file was written since the default became on.
inline std::string default_line()
{
    return std::string(kDefaultKey) + " = on\n";
}

// The switch a whole window-state.txt gives: its own line, but only in a file
// that also has the default's line. Anything else is the default, on: a file
// written before 2026-10-30 included, which is how an existing install is
// turned on once.
inline bool loaded(bool sawLine, bool lineOn, bool sawDefaultLine)
{
    return sawDefaultLine && sawLine ? lineOn : kDefault;
}

// ⭐ How far a finger goes up or down for a step: the two-finger swipe's own
// distances (touch_mouse.inl, kSwipeVerticalUnits and kSwipeSidewaysUnits), so
// the gestures read the same to a hand. Up or down, about a quarter of a
// DualSense's pad, which is 1080 tall; sideways decides a two-finger touch at
// a quarter of its 1920 width, and steps nothing.
constexpr int kVerticalUnits = 300;
constexpr int kSidewaysUnits = 480;

// ⭐ THE PRESS THAT ARMS IT: the touchpad pressed in while two fingers are on
// it, at the moment it goes in. Once per press; a press held does not arm it
// again. ⓘ The window opens as the press comes up (Armed, below): the TV's
// release gesture is two fingers pressed and HELD about four seconds.
struct Press {
    bool wasPressed = false;
};

// The touchpad going in on this report, with any number of fingers.
inline bool press_edge(Press &p, bool pressed)
{
    const bool edge = pressed && !p.wasPressed;
    p.wasPressed = pressed;
    return edge;
}

inline bool arms(Press &p, int fingers, bool pressed)
{
    return press_edge(p, pressed) && fingers == 2;
}

// ⭐ THE WINDOW OPENS AS THE ARMED PRESS COMES UP, within kReleaseGestureMs,
// with a finger still on the pad to hold it open; it closes as before, when
// the last finger lifts. A press held to the TV's release opens nothing, and
// the touchpad stays the switch's until the last finger lifts, so the
// release's touches reach neither Windows nor the game. A press let go as
// every finger lifts opens nothing either: no finger is left to hold it.
constexpr long long kReleaseGestureMs = 4000;

struct Armed {
    bool on = false;      // two fingers pressed it in, and nothing is open yet
    long long since = 0;  // when it went in
    bool spent = false;   // held to the release: it opens nothing
};

inline void arm(Armed &a, long long nowMs)
{
    a = Armed();
    a.on = true;
    a.since = nowMs;
}

enum class Up { None, Open, TooLong };

// Each report while armed: TooLong once, as the press passes the release's
// time; Open as it comes up before then, a finger still down; None
// otherwise. Armed no longer once it opens, or once the last finger lifts.
inline Up armed_report(Armed &a, bool pressed, int fingers, long long nowMs)
{
    if (!a.on) return Up::None;
    const bool inTime = nowMs - a.since < kReleaseGestureMs;
    if (pressed) {
        if (a.spent || inTime) return Up::None;
        a.spent = true;
        return Up::TooLong;
    }
    const bool open = !a.spent && inTime && fingers > 0;
    if (open || fingers == 0) a = Armed();
    return open ? Up::Open : Up::None;
}

// ⭐ WHAT A REPORT DOES WHILE THE WINDOW A PRESS OPENED HOLDS THE TOUCHPAD:
// only the two things asked for. The last finger lifting closes it; a press
// keeps it, in the layout it had before (rhoquinn8217, 2026-10-28: "double
// touchpad press should bring you to quick mode on the first press but on the
// second press, it should bring you to simple or advance based on what you had
// open last"), and lets the touchpad go; a swipe beside a resting finger steps
// the config (swipe_steps, below); anything else does nothing.
enum class Held { Stay, Step, Keep, Close };

inline Held while_held(bool pressEdge, int fingers, int steps)
{
    if (fingers == 0) return Held::Close;
    if (pressEdge) return Held::Keep;
    if (steps != 0) return Held::Step;
    return Held::Stay;
}

// ⭐ THE TOUR'S STAND-IN (rhoquinn8217, 2026-11-06: the tutorial plays the
// switch in a pretend window, "I only want to mock it during the tutorial",
// then "C": the real press drives it). The same press as the real switch,
// read the same way: armed by two fingers going in, open as the press comes
// up in time with a finger still down, kept by a second press, and the last
// finger lifting ends it, kept or not. Nothing is opened or closed: each is a
// word for the page ("open", "keep", "lift"), which plays it.
struct Mock {
    Armed armed;
    bool open = false;
    bool kept = false;
};

inline const char *mock_report(Mock &m, bool pressEdge, bool pressed, int fingers, long long nowMs)
{
    if (m.open) {
        if (fingers == 0) {
            m = Mock();
            return "lift";
        }
        if (pressEdge && !m.kept) {
            m.kept = true;
            return "keep";
        }
        return nullptr;
    }
    if (pressEdge && fingers == 2) {
        arm(m.armed, nowMs);
        return nullptr;
    }
    if (armed_report(m.armed, pressed, fingers, nowMs) == Up::Open) {
        m.open = true;
        return "open";
    }
    return nullptr;
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

// The step this report makes: +1 when a finger has just gone far enough up
// (the next config), -1 down, 0 none. ⭐ One step for the two fingers' touches:
// a step decides the other finger's touch as well, so two fingers swiped
// together step once, as one swiping beside a resting one does, and not twice.
// ⓘ The pad's y grows downward.
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
                if (steps == 0) steps = dy < 0 ? 1 : -1;
            } else if (ax > ay && ax >= static_cast<float>(kSidewaysUnits)) {
                tr.done = true;
            }
        }
        next[i] = tr;
    }
    if (steps != 0) {
        for (Track &tr : next) {
            if (tr.id >= 0) tr.done = true;
        }
    }
    s.t[0] = next[0];
    s.t[1] = next[1];
    return steps;
}

// ⭐ THE SAME WITH THE WINDOW IN FRONT IN SIMPLE OR QUICK (armed), with no
// press: one thumb resting and the other swiping step the wheel. Counted only
// while both are on the pad, since one finger alone moves the cursor there,
// and measured afresh each time the second comes down. ⓘ Two fingers swiped
// together, the gesture this replaced, still step once, as above.
inline int front_steps(Swipes &s, bool armed, const Touch (&now)[2])
{
    const int fingers = (now[0].down ? 1 : 0) + (now[1].down ? 1 : 0);
    if (!armed || fingers != 2) {
        s = Swipes();
        return 0;
    }
    return swipe_steps(s, now);
}

// ⭐ THE RIGHT STICK STEPS IT TOO, WHILE A FINGER HOLDS THE WINDOW UP
// (rhoquinn8217, 2026-11-03: "whenever the quick mode is being held up by
// resting a finger on the touchpad, I want right stick to up and down to move
// the wheel"). One step a push, as a swipe steps once: past kStickStepAt up or
// down, at least as far as it went sideways, as a swipe is judged, and nothing
// more until the stick has come
// back within kStickRearmAt of the middle. ⓘ Up is the config above, as the
// d-pad's up is on the wheel; a swipe up is the one below, the wheel
// following the finger.
// ⓘ It starts unarmed, so a stick already pushed as the window opens steps
// nothing until it has come back.
constexpr float kStickStepAt = 0.6f;
constexpr float kStickRearmAt = 0.3f;

struct Stick {
    bool armed = false;   // back near the middle since its last step
};

// The step this report makes: -1 for a push up (the config above), +1 down,
// 0 none. ⓘ x and y as stick_axis gives them, from -1 to +1, up negative.
inline int stick_steps(Stick &s, float x, float y)
{
    const float ax = x < 0 ? -x : x;
    const float ay = y < 0 ? -y : y;
    if (!s.armed) {
        if (ax < kStickRearmAt && ay < kStickRearmAt) s.armed = true;
        return 0;
    }
    if (ay >= kStickStepAt && ay >= ax) {
        s.armed = false;
        return y < 0 ? -1 : 1;
    }
    return 0;
}

// ⭐ AND THE TOUR'S QUICK STEPS AS A REAL ONE DOES (rhoquinn8217, 2026-11-06:
// *"swiping up or down with one finger or the R-stick up/down aren't
// working"*): while the stand-in's Quick is up (Mock above: open, not kept),
// each finger's swipe, another resting or not, and the right stick's pushes
// are its steps, as while_held and stick_steps make them for a window the
// press opened. ⓘ From the report after it came up, both starting afresh
// then, as at a real open; until the last finger lifts or a second press
// keeps it. `wasUp`: up on the report before. +1 for the next config, -1 for
// the one before, 0 for none; `byStick` says the stick made it.
inline bool mock_up(const Mock &m)
{
    return m.open && !m.kept;
}

inline int mock_steps(const Mock &m, bool wasUp, Swipes &swipes, Stick &stick,
                      const Touch (&touch)[2], bool haveStick, float rx, float ry, bool *byStick)
{
    if (byStick != nullptr) *byStick = false;
    if (!mock_up(m)) return 0;
    if (!wasUp) {
        swipes = Swipes();
        stick = Stick();
        return 0;
    }
    const int swiped = swipe_steps(swipes, touch);
    const int pushed = haveStick ? stick_steps(stick, rx, ry) : 0;
    if (swiped != 0) return swiped;
    if (byStick != nullptr) *byStick = pushed != 0;
    return pushed;
}

// ⭐ THE PAGE HEARS A STEP AS A KEY: KeyJ for the next config, KeyK for the
// one before, with the gate's three modifiers (rebind.inl), held a moment and
// let go, one at a time. The page's CONFIG_MODE_KEYS turn them into a step of
// the wheel whatever has focus.
// ⭐ AND KeyV, ONCE, FOR A SECOND PRESS: the window takes the layout it had
// before the switch showed it Quick (see while_held), after any steps still on
// their way, so they land in the wheel they were swiped on.
constexpr long long kKeyDownMs = 40;
constexpr long long kKeyGapMs = 40;
constexpr int kLayoutKey = 2;

inline const char *key_code(int step)
{
    if (step == kLayoutKey) return "KeyV";
    return step > 0 ? "KeyJ" : "KeyK";
}

struct Keys {
    int pending = 0;       // steps waiting: more next than before, or fewer
    bool layout = false;   // the layout key waiting, after the steps
    int current = 0;       // the key being sent: +1, -1, kLayoutKey, or 0 for none
    bool inGap = false;    // its key is up again, before the next
    long long until = 0;   // when the press, or the gap after it, ends
};

inline void queue(Keys &k, int step)
{
    k.pending += step;
}

inline void queue_layout(Keys &k)
{
    k.layout = true;
}

// Nothing waiting and nothing on its way.
inline bool idle(const Keys &k)
{
    return k.pending == 0 && !k.layout && k.current == 0;
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
    if (k.pending != 0) {
        k.current = k.pending > 0 ? 1 : -1;
        k.pending -= k.current;
    } else if (k.layout) {
        k.current = kLayoutKey;
        k.layout = false;
    } else {
        return nullptr;
    }
    k.until = nowMs + kKeyDownMs;
    return key_code(k.current);
}

// ⭐ WHEN THE WINDOW A PRESS OPENED MAY GO, once the last finger has lifted
// (or the switch was turned off) at liftedAt. ⓘ The page hears steps only once
// it has asked which layout to show (heard, from heardAt); keysIdle says
// nothing is waiting or on its way.
// ⭐ NOT BEFORE IT IS THERE (rhoquinn8217, 2026-10-27: "If I press the
// touchpad with two fingers then release immediately, the config window
// doesn't go away when it should"): lifted while the browser was still
// starting, the close found no window, and the window then came and stayed.
// So while it is coming nothing closes, and it goes as soon as it is there.
// ⓘ Steps still to send are given the page, and kDrainMs once it hears them;
// nothing waits longer than kGiveUpMs from the lift, a window that never
// came or a page that never asked included.
constexpr long long kDrainMs = 300;
constexpr long long kGiveUpMs = 4000;

inline bool close_now(long long nowMs, long long liftedAt, bool coming, bool heard,
                      long long heardAt, bool keysIdle)
{
    if (nowMs - liftedAt >= kGiveUpMs) return true;
    if (coming) return false;
    if (keysIdle) return true;
    if (!heard) return false;
    const long long from = heardAt > liftedAt ? heardAt : liftedAt;
    return nowMs - from >= kDrainMs;
}

// ⭐ A PRESS WITH THE WINDOW IN FRONT CLOSES IT (rhoquinn8217, 2026-11-06:
// "When the window is up, when pressing the two finger press once, close the
// config window"), whoever opened it. ⭐ As the press comes up, as the press
// that opens Quick does: two fingers pressed and held about four seconds are
// the TV's release, and a window that closed as they went in would close at
// every release. Armed by two fingers pressing in with the window in front;
// true on the report the press comes up in time, a finger still down or not.
inline bool close_up(Armed &a, bool pressed, long long nowMs)
{
    if (!a.on) return false;
    const bool inTime = nowMs - a.since < kReleaseGestureMs;
    if (pressed) {
        if (!inTime) a.spent = true;
        return false;
    }
    const bool close = !a.spent && inTime;
    a = Armed();
    return close;
}

// ⭐⭐ THE D-PAD'S APP SWITCHER (rhoquinn8217, 2026-11-06: "specific function
// for d-pad left and right when holding the two finger press quick switch.
// The first time you press left or right alt is held down also if right is
// pressed, they press tab while alt is still pressed. If left was pressed,
// press shift+tab. Then don't let go of alt until either of two things
// happen. cross/A is pressed or the config window closes(you stop touching
// the touchpad)"). Windows' own window switcher, from the pad: while two
// fingers hold the Quick window up, right holds Alt down and presses Tab,
// left presses Shift+Tab, and Alt stays down until Cross picks the window or
// the last finger lifts. ⭐ After Cross, a finger still down, the Quick window
// comes back in front of the window picked and is as it was: lifting closes
// it, a press keeps it (quick_switch_session.inl).
// ⭐ LEFT, RIGHT AND CROSS ARE ALL IT TAKES (2026-11-06: "left, right and
// cross|A are the only buttons disabled"): left and right while the window is
// held up, Cross while Alt is down and until the press that picked is let go,
// so it reaches neither the page nor the window picked.
// ⓘ A press of the touchpad, a swipe and the right stick do nothing while Alt
// is down: the switcher is over the wheel.
// ⓘ Its own box beside the switch's, on until it is turned off. It acts only
// while the switch is on, and keeps its own tick while the switch is off
// (2026-11-06: "It should come back however it was ticked when quick switch
// was turned off"). Kept in window-state.txt as "quick_switch_app_switcher".
inline const char *const kAppSwitcherKey = "quick_switch_app_switcher";
inline constexpr bool kAppSwitcherDefault = true;

inline std::string app_switcher_line(bool on)
{
    return std::string(kAppSwitcherKey) + " = " + (on ? "1" : "0") + "\n";
}

// A file without the line is the default, on: every file written before it.
inline bool app_switcher_loaded(bool sawLine, bool lineOn)
{
    return sawLine ? lineOn : kAppSwitcherDefault;
}

struct AppSwitch {
    bool alt = false;           // Alt is down: Windows' switcher is up
    bool letGo = false;         // Alt goes up once the Tabs on their way have gone
    bool byCross = false;       // ...because Cross picked, not because the window went
    std::deque<int> tabs;       // presses waiting: +1 Tab, -1 Shift+Tab
    int current = 0;            // the Tab down now: +1, -1, or 0
    bool inGap = false;         // Alt alone, after a Tab or before the first
    long long until = 0;        // when the Tab, or the gap, ends
    bool left = false;          // the buttons on the report before, for their edges
    bool right = false;
    bool cross = false;
    bool crossTaken = false;    // the press that picked, held back until it is let go
};

// What the switcher takes from a report.
struct AppTakes {
    bool leftRight = false;
    bool cross = false;
};

// Every report of a pad with a touchpad: its d-pad left and right and Cross.
// `holding`: the switch holds the window up for this pad, the app switcher on.
inline AppTakes app_switch_buttons(AppSwitch &a, bool holding, bool left, bool right, bool cross,
                                   long long nowMs)
{
    const bool leftEdge = left && !a.left;
    const bool rightEdge = right && !a.right;
    const bool crossEdge = cross && !a.cross;
    a.left = left;
    a.right = right;
    a.cross = cross;
    if (holding && !a.letGo && (leftEdge || rightEdge)) {
        if (!a.alt) {
            // ⓘ Alt goes down alone, and the first Tab a moment after.
            a.alt = true;
            a.current = 0;
            a.inGap = true;
            a.until = nowMs + kKeyGapMs;
        }
        if (rightEdge) a.tabs.push_back(1);
        if (leftEdge) a.tabs.push_back(-1);
    }
    if (crossEdge && a.alt && !a.letGo) {
        a.letGo = true;
        a.byCross = true;
        a.crossTaken = true;
    }
    if (!cross) a.crossTaken = false;
    AppTakes t;
    t.leftRight = holding || a.alt;
    t.cross = a.alt || a.crossTaken;
    return t;
}

// The window went (the last finger lifted, or the switch was turned off): Alt
// goes up once the Tabs on their way have gone, and Windows switches to the
// window its switcher shows.
inline void app_switch_end(AppSwitch &a)
{
    if (a.alt && !a.letGo) {
        a.letGo = true;
        a.byCross = false;
    }
}

struct AppKeys {
    bool alt = false;       // Alt down on this report
    int tab = 0;            // +1 Tab, -1 Shift+Tab, 0 none
    bool wentUp = false;    // Alt has just gone up
    bool byCross = false;   // ...because Cross picked
};

// The keys this pad's report holds for the switcher, asked once a report. Each
// Tab is down kKeyDownMs and up kKeyGapMs before the next, Alt down throughout;
// Alt goes up on a report of its own, after the last Tab's gap.
inline AppKeys app_switch_keys(AppSwitch &a, long long nowMs)
{
    AppKeys k;
    if (!a.alt) return k;
    if (a.current != 0) {
        if (nowMs < a.until) {
            k.alt = true;
            k.tab = a.current;
            return k;
        }
        a.current = 0;
        a.inGap = true;
        a.until = nowMs + kKeyGapMs;
    }
    if (a.inGap) {
        if (nowMs < a.until) {
            k.alt = true;
            return k;
        }
        a.inGap = false;
    }
    if (!a.tabs.empty()) {
        a.current = a.tabs.front();
        a.tabs.pop_front();
        a.until = nowMs + kKeyDownMs;
        k.alt = true;
        k.tab = a.current;
        return k;
    }
    if (a.letGo) {
        k.wentUp = true;
        k.byCross = a.byCross;
        a.alt = false;
        a.letGo = false;
        a.byCross = false;
        return k;
    }
    k.alt = true;
    return k;
}

}  // namespace quick_switch
