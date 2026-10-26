// The quick config switch at work: two fingers pressing the touchpad open the
// configs window in Quick view, each swipe of a finger up or down steps the
// controller's config there (one thumb resting, the other swiping), and the
// window closes the moment the last finger lifts.
// 🔗 quick_switch.inl for what was asked, the gestures and the keys; rebind.inl
// feeds this every report of a pad with a touchpad, in the window or out of
// it, and sends the keys from its config-mode branch; touch_mouse.inl asks it
// whether the touchpad is the switch's.
//
// ⭐ WHILE THE WINDOW IS OPEN THE TOUCHPAD IS THE SWITCH'S: no cursor, scroll,
// taps, swipes or press (rhoquinn8217, 2026-10-23: "Disable all touch pad
// rebinds during this time including touch-pad to mouse"). From the press that
// opens it to the last finger lifting.
// ⭐ AND WITH THE WINDOW IN FRONT IN SIMPLE OR QUICK THE SAME SWIPE MOVES ITS
// WHEEL, one thumb resting and the other swiping, with no press (rhoquinn8217,
// 2026-10-26: *"the same 1 thumb touch and 1 thumb swipe"*), where two fingers
// swiped together did (2026-10-23: *"When simple and quick mode are in focus,
// two finger swipe up or down is disabled and only control the wheel"*). The
// config's own swipe up and down and its two-finger scroll stand aside there;
// one finger alone still moves the cursor.
// ⓘ A press with the window already in front opens nothing: it is there.
// ⓘ Advanced has no wheel: a swipe there steps nothing.
//
// ⭐ ONE CONTROLLER AT A TIME. Another pad's press or swipes are left alone
// while one is switching, as two people stepping one window would fight over
// it. ⓘ A window open behind a game is not in front: the press opens its own
// in its place, as the shortcut does, and that one closes at the end, the
// layout and the controller the shortcut and the tray open on put back
// (config_move.inl, begin_quick_view).
//
// ⛔ The page hears a step only once it has asked which layout to show, after
// the window was asked for: its scripts are running then, and its keys are
// heard. Not its report that it has the front, which it makes once a second
// and which kept the first step waiting a second more. A step that comes
// before the page has drawn its wheel waits in the page, and a step not yet
// linked when the window closes is linked by the page as it goes.
#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

namespace quick_switch_session {

struct Session {
    bool active = false;
    const void *device = nullptr;   // whose fingers
    bool pressed = false;           // a press opened it: it ends when the last finger lifts
    bool openedWindow = false;      // so it closes the window, and puts the layout back
    unsigned askedAtOpen = 0;       // the pages' layout questions when the window was asked for
    bool ending = false;            // done once the steps are sent, the window then closed
    long long endAt = 0;            // when it began to end
    unsigned id = 0;                // which switch a closer belongs to
    quick_switch::Keys keys;
};

struct PadState {
    quick_switch::Press press;
    quick_switch::Swipes swipes;    // each finger's swipe while the window is open
    quick_switch::Swipes front;     // the same, with the wheel in front and both fingers down
};

inline std::mutex g_mutex;
inline std::unordered_map<const void *, PadState> g_pads;
inline Session g_session;
inline unsigned g_nextId = 1;
// How many times a page has asked which layout to show (GET ui/view): once as
// each window opens.
inline std::atomic<unsigned> g_pageAsked{0};

// ⓘ The last finger lifted, the window waits this long at most for steps
// still on their way to the page. A page that has not yet asked for its layout
// had nothing to show, and closes at once.
constexpr long long kDrainMs = 300;

inline long long now_ms()
{
    return ctm_rebind::chord_now_ms();
}

// The window in front in Simple or Quick, where two fingers up and down move
// the wheel. ⓘ Windows is asked only while the gate is on, which it is not in
// a game, so this costs a game nothing.
inline bool window_wheel_in_front()
{
    bool compact = false;
    config_move::view_get(&compact, nullptr, nullptr);
    return compact && ctm_rebind_config_mode_effective();
}

// The window the press opened, closed once its steps have gone, and the
// layout put back. ⓘ Its own thread: the report path must not sleep.
inline void close_when_sent(unsigned id)
{
    std::thread([id]() {
        for (;;) {
            bool done = false;
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                Session &s = g_session;
                if (!s.active || s.id != id) return;   // a new switch has the window
                const bool heard = g_pageAsked.load() != s.askedAtOpen;
                if (!heard || quick_switch::idle(s.keys) || now_ms() - s.endAt >= kDrainMs) {
                    s.active = false;
                    done = true;
                }
            }
            if (done) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        ui_close_window();
        // ⛔ GONE BEFORE THE LAYOUT GOES BACK. The page's own goodbye
        // remembers the window's place, in the slot of the layout the listener
        // holds as it arrives; a Quick window still there then would be put in
        // the other layout's slot.
        for (int i = 0; i < 40 && ctm_open_ui::window_exists(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
        config_move::end_quick_view();
        device_log::input(device_log::msg()
            << "quick switch: the last finger lifted -- the window closed, and the layout it had before is back");
    }).detach();
}

// Every report of a pad with a touchpad, read before anything changes the
// report: whether the pad is pressed in, and each of its two fingers -- down
// or not, the id the pad gave its touch, and where it is.
inline void report(const void *deviceKey, bool pressed, const quick_switch::Touch (&touch)[2])
{
    const int fingers = (touch[0].down ? 1 : 0) + (touch[1].down ? 1 : 0);
    if (!config_move::quick_switch_on()) {
        unsigned closer = 0;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_pads.erase(deviceKey);
            Session &s = g_session;
            // ⓘ Turned off with its window open: the window goes as the fingers would take it.
            if (s.active && s.device == deviceKey && s.openedWindow && !s.ending) {
                s.ending = true;
                s.endAt = now_ms();
                closer = s.id;
            }
        }
        if (closer != 0) close_when_sent(closer);
        return;
    }

    // ⓘ Windows is asked only when two fingers are down: not on every report
    // for nothing.
    const bool wheelInFront = fingers == 2 && window_wheel_in_front();
    const bool inFront = pressed && fingers == 2 && (wheelInFront || ctm_rebind_config_mode_effective());
    enum class Did { Nothing, Opened, Stepped, Lifted, InFront, SomeoneElse };
    Did did = Did::Nothing;
    int step = 0;
    unsigned closer = 0;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        PadState &ps = g_pads[deviceKey];
        Session &s = g_session;
        const bool mine = s.active && s.device == deviceKey;
        const bool opens = quick_switch::opens(ps.press, fingers, pressed);
        const int swipe = quick_switch::front_steps(ps.front, wheelInFront, touch);
        if (mine && s.pressed && !s.ending) {
            if (fingers == 0) {
                s.ending = true;
                s.endAt = now_ms();
                closer = s.id;
                did = Did::Lifted;
            } else {
                // ⓘ Each finger by itself, not the two together: a thumb
                // swiping beside a resting one moves their midpoint as well,
                // and would step twice.
                step = quick_switch::swipe_steps(ps.swipes, touch);
                if (step != 0) {
                    quick_switch::queue(s.keys, step);
                    did = Did::Stepped;
                }
            }
        } else if (opens) {
            if (s.active && s.device != deviceKey && !s.ending) {
                did = Did::SomeoneElse;
            } else if (inFront) {
                did = Did::InFront;
            } else {
                s = Session();
                s.active = true;
                s.device = deviceKey;
                s.pressed = true;
                s.openedWindow = true;
                s.askedAtOpen = g_pageAsked.load();
                s.id = g_nextId++;
                ps.swipes = quick_switch::Swipes();
                did = Did::Opened;
            }
        } else if (swipe != 0) {
            // ⓘ The wheel in front, no press: the window is someone's own and
            // stays; done once the steps are sent.
            if (s.active && s.device != deviceKey && !s.ending) {
                did = Did::SomeoneElse;
            } else {
                if (!mine) {
                    s = Session();
                    s.active = true;
                    s.device = deviceKey;
                    s.id = g_nextId++;
                }
                s.ending = true;
                quick_switch::queue(s.keys, swipe);
                step = swipe;
                did = Did::Stepped;
            }
        }
    }
    if (closer != 0) close_when_sent(closer);
    if (did == Did::Nothing || did == Did::Lifted) return;

    const std::string ordinal = ctm_ordinal_for_device(deviceKey);
    if (did == Did::Opened) {
        // ⭐ Quick for this window alone, before it asks which layout to show.
        config_move::begin_quick_view(ordinal);
        // ⓘ The gate waits four seconds for the page, as for the shortcut.
        ctm_rebind::g_gateProvisionalUntil.store(now_ms() + 4000);
        std::thread([ordinal]() { ctm_chord_show_ui(ordinal); }).detach();
    }
    device_log::input(device_log::msg()
        << "quick switch: " << ordinal << " -- "
        << (did == Did::Opened ? "two fingers pressed: the window opening in Quick" :
            did == Did::InFront ? "two fingers pressed, with the window in front already" :
            did == Did::SomeoneElse ? "the touchpad, while another controller is switching" :
            step > 0 ? "up: the next config" : "down: the config before"));
}

// The key this pad's report carries to the page, or nullptr. Asked from the
// config-mode branch of rebind.inl, so only while the gate has the pad and
// our window has the front.
inline const char *key(const void *deviceKey)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    Session &s = g_session;
    if (!s.active || s.device != deviceKey) return nullptr;
    if (s.openedWindow && g_pageAsked.load() == s.askedAtOpen) return nullptr;
    const char *k = quick_switch::key_now(s.keys, now_ms());
    // ⓘ A window someone else opened stays; this switch is done with it.
    if (k == nullptr && s.ending && !s.openedWindow && quick_switch::idle(s.keys)) s.active = false;
    return k;
}

// Whether the touchpad is the switch's right now: the press that opens the
// window, and every report after it until the last finger lifts.
inline bool holds_touchpad(const void *deviceKey, int fingers, bool pressed)
{
    if (!config_move::quick_switch_on()) return false;
    if (fingers == 2 && pressed) return true;
    std::lock_guard<std::mutex> lock(g_mutex);
    const Session &s = g_session;
    return s.active && s.device == deviceKey && s.pressed && !s.ending;
}

}  // namespace quick_switch_session

// ⓘ For the declarations in main.cpp: rebind.inl, touch_mouse.inl and
// rest_config.inl reach these, and the first and last are included before
// this file.
void quick_switch_report(const void *deviceKey, bool pressed,
                         bool down0, int id0, float x0, float y0,
                         bool down1, int id1, float x1, float y1)
{
    const quick_switch::Touch touch[2] = { { down0, id0, x0, y0 }, { down1, id1, x1, y1 } };
    quick_switch_session::report(deviceKey, pressed, touch);
}

const char *quick_switch_key(const void *deviceKey)
{
    return quick_switch_session::key(deviceKey);
}

bool quick_switch_holds_touchpad(const void *deviceKey, int fingers, bool pressed)
{
    return quick_switch_session::holds_touchpad(deviceKey, fingers, pressed);
}

// Whether two fingers up and down are the switch's right now, and so not the
// config's own swipes or its two-finger scroll: the switch is on and the
// window is in front in Simple or Quick.
bool quick_switch_has_vertical_swipes()
{
    return config_move::quick_switch_on() && quick_switch_session::window_wheel_in_front();
}

void quick_switch_page_asked_view()
{
    quick_switch_session::g_pageAsked.fetch_add(1);
}
