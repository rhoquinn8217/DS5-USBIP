// The quick config switch at work: the swipes a controller makes, the configs
// window opened in Quick view to show them, each step sent to the page, and
// the window closed again a moment after Create is let go.
// 🔗 quick_switch.inl for what was asked, the swipe and the keys; rebind.inl
// feeds this every report of a pad with a touchpad, in the window or out of
// it, and sends the keys from its config-mode branch.
//
// ⭐ TWO WAYS A SWIPE STEPS (rhoquinn8217, 2026-10-23):
// - **Out of the window**, Create held and two fingers up or down. The window
//   opens in Quick on that controller, the swipes go on while Create is held,
//   and the window closes kLingerMs after it is let go, the layout and the
//   controller the shortcut and the tray open on put back
//   (config_move.inl, begin_quick_view).
// - **With the window in front in Simple or Quick**, two fingers up or down
//   alone: *"When simple and quick mode are in focus, two finger swipe up or
//   down is disabled and only control the wheel."* The config's own swipe up
//   and down stand aside there (touch_mouse.inl), and the window stays.
// ⓘ Create in the window is the window's own: a press there resizes it, as
// ever (*"Create should still resize the window when it is in focus"*); one
// held since before the window came forward is no press (config_move.inl).
// ⓘ Advanced has no wheel: there a swipe steps nothing.
//
// ⭐ ONE CONTROLLER AT A TIME. Another pad's swipes are left alone while one
// is switching, as two people stepping one window would fight over it.
// ⓘ A window open behind a game is not in front: the switch opens its own in
// its place, as the shortcut does, and that one closes at the end.
//
// ⛔ The page hears a step only once it has asked which layout to show, after
// the window was asked for: its scripts are running then, and its keys are
// heard. Before that a key reaches nothing that can take it, and the steps
// wait. ⓘ Not its report that it has the front, which it makes once a second
// and which kept the first step waiting a second more (rhoquinn8217,
// 2026-10-23: "it take a while before the wheel animates to show you you've
// switch a config.  Can you make it faster."). A step that comes before the
// page has drawn its wheel waits in the page; a window that never comes
// forward is closed after Create comes up all the same.
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
    const void *device = nullptr;   // whose swipes
    bool openedWindow = false;      // so the switch closes it, and puts the layout back
    unsigned askedAtOpen = 0;       // the pages' layout questions when the window was asked for
    long long readyAt = 0;          // when its page could first take a step
    bool createHeld = true;
    bool ending = false;            // its window stays: done once the steps are sent
    bool closing = false;           // a closer is waiting out the linger
    long long closeAt = 0;
    unsigned id = 0;                // which switch a closer belongs to
    quick_switch::Keys keys;
};

inline std::mutex g_mutex;
inline std::unordered_map<const void *, quick_switch::Pad> g_pads;
inline Session g_session;
inline unsigned g_nextId = 1;
// How many times a page has asked which layout to show (GET ui/view): once as
// each window opens.
inline std::atomic<unsigned> g_pageAsked{0};

// ⓘ A window whose page never takes a step is closed this long after Create
// is let go all the same.
constexpr long long kGiveUpMs = 3000;

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

// ⭐ Create on the switching controller, under g_mutex. Let go: the window the
// switch opened stays kLingerMs for the config landed on to be read, and then
// closes; a window that was in front already stays, and the switch ends once
// its steps are sent. Held again first: the window stays while it is held.
// Returns the switch whose closer must start, or 0.
inline unsigned follow_create(const void *deviceKey, bool createHeld)
{
    Session &s = g_session;
    if (!s.active || s.device != deviceKey || createHeld == s.createHeld) return 0;
    s.createHeld = createHeld;
    if (createHeld) {
        if (!s.openedWindow) s.ending = false;
        return 0;
    }
    if (!s.openedWindow) {
        s.ending = true;
        if (quick_switch::idle(s.keys)) s.active = false;
        return 0;
    }
    s.closeAt = now_ms() + quick_switch::kLingerMs;
    if (s.closing) return 0;   // the closer already waiting reads the new time
    s.closing = true;
    return s.id;
}

// The window the switch opened, closed once the linger is over, and the
// layout put back. ⭐ The linger runs from Create let go or from when the page
// could first show a step, whichever is later: a Create let go before the
// window had come up would otherwise leave the config landed on in sight for
// less than the linger. ⓘ Its own thread: the report path must not sleep.
inline void close_after_linger(unsigned id)
{
    std::thread([id]() {
        for (;;) {
            long long wait = 0;
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                Session &s = g_session;
                if (!s.active || s.id != id || s.createHeld) {
                    if (s.id == id) s.closing = false;
                    return;
                }
                const long long now = now_ms();
                long long until = s.closeAt;
                if (s.readyAt != 0) {
                    if (s.readyAt + quick_switch::kLingerMs > until) until = s.readyAt + quick_switch::kLingerMs;
                } else if (now < s.closeAt - quick_switch::kLingerMs + kGiveUpMs) {
                    until = now + 100;   // its page has not taken a step yet: look again
                }
                wait = until - now;
                if (wait <= 0) {
                    s.active = false;
                    s.closing = false;
                    break;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(wait));
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
            << "quick switch: Create let go " << quick_switch::kLingerMs
            << "ms ago -- the window closed, and the layout it had before is back");
    }).detach();
}

// Every report of a pad with a touchpad: Create and the fingers, read before
// anything changes the report. fingers is 0, 1 or 2; mx, my are their
// midpoint when two are down.
inline void report(const void *deviceKey, bool createHeld, int fingers, float mx, float my)
{
    if (!config_move::quick_switch_on()) {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_pads.erase(deviceKey);
        return;
    }

    // ⓘ Asked only while two fingers are down: Windows is not asked on every
    // report for nothing.
    const bool wheelInFront = fingers == 2 && window_wheel_in_front();
    int step = 0;
    unsigned closer = 0;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        step = quick_switch::step(g_pads[deviceKey], createHeld || wheelInFront, fingers, mx, my);
        closer = follow_create(deviceKey, createHeld);
    }
    if (closer != 0) close_after_linger(closer);
    if (step == 0) return;

    const bool inFront = wheelInFront || ctm_rebind_config_mode_effective();
    bool open = false;
    bool someoneElse = false;
    bool noWheel = false;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        Session &s = g_session;
        if (s.active && s.device != deviceKey && !s.ending) {
            someoneElse = true;
        } else if (s.active && s.device == deviceKey) {
            quick_switch::queue(s.keys, step);
            // ⓘ A step while the window waits to close gives the config landed
            // on its full time again.
            if (s.openedWindow && !s.createHeld) s.closeAt = now_ms() + quick_switch::kLingerMs;
        } else if (inFront && !wheelInFront) {
            noWheel = true;   // Advanced, with no wheel to step
        } else {
            s = Session();
            s.active = true;
            s.device = deviceKey;
            s.createHeld = createHeld;
            s.openedWindow = !inFront;
            s.ending = inFront;   // a window already there just takes the steps
            s.askedAtOpen = g_pageAsked.load();
            s.id = g_nextId++;
            open = s.openedWindow;
            quick_switch::queue(s.keys, step);
        }
    }
    const std::string ordinal = ctm_ordinal_for_device(deviceKey);
    if (someoneElse || noWheel) {
        device_log::input(device_log::msg()
            << "quick switch: " << ordinal << " swiped, but "
            << (someoneElse ? "another controller is switching" : "Advanced has no wheel to step"));
        return;
    }
    if (open) {
        // ⭐ Quick for this window alone, before it asks which layout to show.
        config_move::begin_quick_view(ordinal);
        // ⓘ The gate waits four seconds for the page, as for the shortcut.
        ctm_rebind::g_gateProvisionalUntil.store(now_ms() + 4000);
        std::thread([ordinal]() { ctm_chord_show_ui(ordinal); }).detach();
    }
    device_log::input(device_log::msg()
        << "quick switch: " << ordinal << " -- two fingers "
        << (step > 0 ? "up: the next config" : "down: the config before")
        << (open ? "; the window opening in Quick" :
            wheelInFront ? ", on the wheel in front" : ""));
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
    if (s.readyAt == 0) s.readyAt = now_ms();
    const char *k = quick_switch::key_now(s.keys, now_ms());
    if (k == nullptr && s.ending && quick_switch::idle(s.keys)) s.active = false;
    return k;
}

}  // namespace quick_switch_session

// ⓘ For the declarations in main.cpp: rebind.inl, touch_mouse.inl and
// rest_config.inl reach these, and the first and last are included before
// this file.
void quick_switch_report(const void *deviceKey, bool createHeld, int fingers, float mx, float my)
{
    quick_switch_session::report(deviceKey, createHeld, fingers, mx, my);
}

const char *quick_switch_key(const void *deviceKey)
{
    return quick_switch_session::key(deviceKey);
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
