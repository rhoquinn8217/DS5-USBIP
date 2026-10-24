// The quick config switch at work: the swipes a controller makes while Create
// is held, the configs window opened in Quick view to show them, each step
// sent to the page, and the window closed again a moment after Create is let
// go. 🔗 quick_switch.inl for what was asked, the swipe and the keys;
// rebind.inl feeds this every report of a pad with a touchpad, in the window
// or out of it, and sends the keys from its config-mode branch.
//
// ⭐ ONE CONTROLLER AT A TIME. Another pad's swipes are left alone while one
// is switching, as two people stepping one window would fight over it.
//
// ⭐ A WINDOW ALREADY IN FRONT IS SOMEONE'S OWN. Its wheel steps (Simple's or
// Quick's; Advanced has none), and it stays when Create comes up. Only a
// window the switch opened is closed by it, and only then is the layout the
// shortcut and the tray open in put back (config_move.inl, begin_quick_view).
// ⓘ A window open behind a game is not in front: the switch opens its own in
// its place, as the shortcut does, and that one closes at the end.
//
// ⛔ The page hears a step only once it has said it has the front, after the
// window was asked for. Before that a key reaches nothing that can take it,
// and the steps wait; a window that never comes forward is closed after
// Create comes up all the same.
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
    unsigned focusAtOpen = 0;       // the page's focus count when the window was asked for
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
// How many times the page has said it has the front (ui/focus).
inline std::atomic<unsigned> g_pageFocus{0};

inline long long now_ms()
{
    return ctm_rebind::chord_now_ms();
}

// ⭐ Create on the switching controller, under g_mutex. Let go: the window it
// opened stays kLingerMs for the config landed on to be read, and then
// closes; a window that was in front already stays, and the switch ends once
// its steps are sent. Held again first: the window stays and the swipes go on.
// Returns the switch whose closer must start, or 0.
inline unsigned follow_create(const void *deviceKey, bool createHeld)
{
    Session &s = g_session;
    if (!s.active || s.device != deviceKey || createHeld == s.createHeld) return 0;
    s.createHeld = createHeld;
    if (createHeld) {
        s.ending = false;
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
// layout put back. ⓘ Its own thread: the report path must not sleep.
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
                wait = s.closeAt - now_ms();
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

    int step = 0;
    unsigned closer = 0;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        step = quick_switch::step(g_pads[deviceKey], createHeld, fingers, mx, my);
        closer = follow_create(deviceKey, createHeld);
    }
    if (closer != 0) close_after_linger(closer);
    if (step == 0) return;

    // ⓘ Asked of Windows only when a swipe has been made, which is rare.
    const bool inFront = ctm_rebind_config_mode_effective();
    bool open = false;
    bool someoneElse = false;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        Session &s = g_session;
        if (s.active && s.device != deviceKey && !s.ending) {
            someoneElse = true;
        } else {
            if (!s.active || s.device != deviceKey) {
                s = Session();
                s.active = true;
                s.device = deviceKey;
                s.openedWindow = !inFront;
                s.focusAtOpen = g_pageFocus.load();
                s.id = g_nextId++;
                open = s.openedWindow;
            }
            quick_switch::queue(s.keys, step);
        }
    }
    const std::string ordinal = ctm_ordinal_for_device(deviceKey);
    if (someoneElse) {
        device_log::input(device_log::msg()
            << "quick switch: " << ordinal << " swiped, but another controller is switching");
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
        << "quick switch: " << ordinal << " -- Create and two fingers "
        << (step > 0 ? "up: the next config" : "down: the config before")
        << (open ? "; the window opening in Quick" : ""));
}

// The key this pad's report carries to the page, or nullptr. Asked from the
// config-mode branch of rebind.inl, so only while the gate has the pad and
// our window has the front.
inline const char *key(const void *deviceKey)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    Session &s = g_session;
    if (!s.active || s.device != deviceKey) return nullptr;
    if (s.openedWindow && g_pageFocus.load() == s.focusAtOpen) return nullptr;
    const char *k = quick_switch::key_now(s.keys, now_ms());
    if (k == nullptr && s.ending && quick_switch::idle(s.keys)) s.active = false;
    return k;
}

// ⭐ While this pad is switching, Create is the switch's: it does not resize
// the window (config_move.inl), however it reaches the window held.
inline bool owns_create(const void *deviceKey)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_session.active && g_session.device == deviceKey;
}

}  // namespace quick_switch_session

// ⓘ For the declarations in main.cpp: rebind.inl, config_move.inl and
// rest_config.inl are all included before this file.
void quick_switch_report(const void *deviceKey, bool createHeld, int fingers, float mx, float my)
{
    quick_switch_session::report(deviceKey, createHeld, fingers, mx, my);
}

const char *quick_switch_key(const void *deviceKey)
{
    return quick_switch_session::key(deviceKey);
}

bool quick_switch_owns_create(const void *deviceKey)
{
    return quick_switch_session::owns_create(deviceKey);
}

void quick_switch_page_focused()
{
    quick_switch_session::g_pageFocus.fetch_add(1);
}
