// Window Select: two rebind values that step through Windows' window picker,
// Alt held between presses, as aurora-tv's Desktop Mode does it with L1 and R1
// (its stream/input/window_select.c).
//
// ⭐ rhoquinn8217, 2026-11-12, Desktop Mode's hand-over: *"The transition
// between aurora-tv managing controllers and DS5-USBIP managing controllers
// needs to be seemless"* and *"we can adjust the default windows config to
// match everything the aurora-tv rebinds do"*. default_windows_config binds L1
// to WindowSelectBack and R1 to WindowSelectNext; any config may.
//
// ⭐ WHAT IT DOES, as the TV's does:
//   - WindowSelectNext: Alt goes down and stays, then Tab; again, Tab again.
//   - WindowSelectBack: the same with Shift+Tab, back through the picker.
//   - While Alt is held, another bound button acts by what it is bound to:
//       Enter        picks the window: Alt goes up, and no Enter is sent;
//       Escape       cancels: Esc with Alt held, then Alt up;
//       Tab, Shift, an arrow key, Window Select again
//                    keeps Alt, and goes out with it (an arrow moves the
//                    picker, Tab steps it);
//       a mouse click
//                    goes out first, Alt held, and Alt goes up after it;
//       anything else
//                    lets Alt go first, then does its own job.
// ⓘ Each key is down kKeyDownMs and then up kKeyGapMs before the next, Alt
// first alone for a gap, and Alt goes up on a report of its own, after what
// is on its way: the quick switch's app switcher's timing (quick_switch.inl).
// ⓘ Its own file, with no windows.h, so the test binary can reach it;
// rebind.inl holds each pad's state and puts its keys into the pad's own.
#pragma once

#include <cctype>
#include <deque>
#include <string>

namespace window_select {

inline const char *const kBackValue = "WindowSelectBack";
inline const char *const kNextValue = "WindowSelectNext";

constexpr long long kKeyDownMs = 40;
constexpr long long kKeyGapMs = 40;
// ⓘ A click goes out first, Alt held; Alt goes up this long after it.
constexpr long long kAfterClickMs = 40;

// The keys a report holds for it.
enum : int { kNone = 0, kTab = 1, kShiftTab = -1, kEscape = 2 };

inline bool same_name(const std::string &code, const char *name)
{
    size_t i = 0;
    for (; i < code.size() && name[i] != '\0'; ++i) {
        if (std::tolower(static_cast<unsigned char>(code[i])) !=
            std::tolower(static_cast<unsigned char>(name[i]))) return false;
    }
    return i == code.size() && name[i] == '\0';
}

// +1 for WindowSelectNext, -1 for WindowSelectBack, 0 for any other value.
// ⓘ Folded: the config reader lowercases values (binding_names.inl).
inline int step_of(const std::string &code)
{
    if (same_name(code, kNextValue)) return 1;
    if (same_name(code, kBackValue)) return -1;
    return 0;
}

// What another bound button does while Alt is held, by what it is bound to.
enum class Other { Keep, Pick, Cancel, Click, LetGo };

inline Other classify(const std::string &code)
{
    if (same_name(code, "Enter")) return Other::Pick;
    if (same_name(code, "Escape")) return Other::Cancel;
    if (same_name(code, "Tab") || same_name(code, "ArrowUp") || same_name(code, "ArrowDown") ||
        same_name(code, "ArrowLeft") || same_name(code, "ArrowRight") ||
        same_name(code, "ShiftLeft") || same_name(code, "ShiftRight") || step_of(code) != 0) {
        return Other::Keep;
    }
    if (same_name(code, "MouseLeft") || same_name(code, "MouseRight") || same_name(code, "MouseMiddle")) {
        return Other::Click;
    }
    return Other::LetGo;
}

struct State {
    bool alt = false;           // Alt is down: Windows' picker is up
    bool letGo = false;         // Alt goes up once what is on its way has gone
    long long letGoAt = 0;      // ...or at this time, after a click (0: not asked)
    std::deque<int> queue;      // keys waiting: kTab, kShiftTab, kEscape
    int current = kNone;        // the key down now
    bool inGap = false;         // Alt alone, between keys or before the first
    long long until = 0;        // when the key, or the gap, ends
};

// Alt held and not on its way up: another button's press acts by classify.
inline bool active(const State &s)
{
    return s.alt && !s.letGo && s.letGoAt == 0;
}

// A press of a Window Select button: Alt down first, alone for a gap, and the
// step queued. ⓘ One pressed while Alt is on its way up takes it back.
inline void step(State &s, int dir, long long nowMs)
{
    if (!s.alt) {
        s.alt = true;
        s.current = kNone;
        s.inGap = true;
        s.until = nowMs + kKeyGapMs;
    }
    s.letGo = false;
    s.letGoAt = 0;
    s.queue.push_back(dir < 0 ? kShiftTab : kTab);
}

// The press of another bound button while active(). True when that button's
// own job is held back for as long as it is held: Enter picks and Escape
// cancels, neither then sending its key. ⓘ A LetGo button's job waits only
// while Alt is still down (rebind.inl holds it back until then).
inline bool other_press(State &s, Other what, long long nowMs)
{
    switch (what) {
    case Other::Keep:
        return false;
    case Other::Pick:
        s.letGo = true;
        return true;
    case Other::Cancel:
        s.queue.push_back(kEscape);
        s.letGo = true;
        return true;
    case Other::Click:
        s.letGoAt = nowMs + kAfterClickMs;
        return false;
    case Other::LetGo:
        s.letGo = true;
        return false;
    }
    return false;
}

struct Keys {
    bool alt = false;    // Alt down on this report
    int key = kNone;     // kTab, kShiftTab or kEscape down on this report
};

// The keys this pad's report holds for Window Select, asked once a report.
inline Keys keys(State &s, long long nowMs)
{
    Keys k;
    if (!s.alt) return k;
    if (s.current != kNone) {
        if (nowMs < s.until) {
            k.alt = true;
            k.key = s.current;
            return k;
        }
        s.current = kNone;
        s.inGap = true;
        s.until = nowMs + kKeyGapMs;
    }
    if (s.inGap) {
        if (nowMs < s.until) {
            k.alt = true;
            return k;
        }
        s.inGap = false;
    }
    if (!s.queue.empty()) {
        s.current = s.queue.front();
        s.queue.pop_front();
        s.until = nowMs + kKeyDownMs;
        k.alt = true;
        k.key = s.current;
        return k;
    }
    if (s.letGoAt != 0 && nowMs >= s.letGoAt) {
        s.letGo = true;
        s.letGoAt = 0;
    }
    if (s.letGo) {
        s = State();
        return k;              // Alt up, on a report of its own
    }
    k.alt = true;
    return k;
}

} // namespace window_select
