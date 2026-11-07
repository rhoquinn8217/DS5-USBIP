// The quick config switch at work: two fingers pressing the touchpad open the
// configs window in Quick view as the press comes up, each swipe of a finger
// up or down steps the controller's config there (one thumb resting, the other
// swiping), and the window closes the moment the last finger lifts -- or,
// pressed again, stays.
// 🔗 quick_switch.inl for what was asked, the gestures and the keys; rebind.inl
// feeds this every report of a pad with a touchpad, in the window or out of
// it, and sends the keys from its config-mode branch; touch_mouse.inl asks it
// whether the touchpad is the switch's.
//
// ⭐ WHILE THE WINDOW IS OPEN THE TOUCHPAD IS THE SWITCH'S: no cursor, scroll,
// taps, swipes or press (rhoquinn8217, 2026-10-23: "Disable all touch pad
// rebinds during this time including touch-pad to mouse"). From the press that
// opens it to the last finger lifting.
// ⭐⭐ AND NONE OF IT REACHES WINDOWS OR THE GAME, the press that opens it
// included (rhoquinn8217, 2026-10-27: "that press should not register in
// windows or the game"): the gate takes the pad only once the window has the
// front, half a second on, so until then the touches and the press went out
// with the report. mouse_exclusive.inl blanks them while holds_touchpad says
// so. ⭐ A second press keeps the window, in the layout it had before, and
// lets the touchpad go (quick_switch::while_held; config_move::keep_quick_view;
// the page's KeyV).
// ⭐ THE WINDOW COMES AS THE PRESS COMES UP, under four seconds (rhoquinn8217,
// 2026-10-28; quick_switch::armed_report): held that long, the press is the
// TV's release, and the window came up over it. From the press to the
// window, or to the last finger lifting after a press held to the release,
// the touchpad is held back as before.
// ⭐ AND WITH THE WINDOW IN FRONT IN SIMPLE OR QUICK THE SAME SWIPE MOVES ITS
// WHEEL, one thumb resting and the other swiping, with no press (rhoquinn8217,
// 2026-10-26: *"the same 1 thumb touch and 1 thumb swipe"*), where two fingers
// swiped together did (2026-10-23: *"When simple and quick mode are in focus,
// two finger swipe up or down is disabled and only control the wheel"*). The
// config's own swipe up and down and its two-finger scroll stand aside there;
// one finger alone still moves the cursor.
// ⭐ AND THE RIGHT STICK, UP OR DOWN, WHILE A FINGER HOLDS THE WINDOW UP
// (rhoquinn8217, 2026-11-03; quick_switch::stick_steps): a step a push, the
// stick mouse standing aside meanwhile (holds_stick).
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
// ⛔ AND THE WINDOW CLOSES ONLY ONCE IT IS THERE: fingers lifted while the
// browser was still starting found no window to close, and it came and stayed
// (rhoquinn8217, 2026-10-27). quick_switch::close_now.
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
    bool launched = false;          // its open has been made: the window is coming, or there
    bool kept = false;              // pressed again: the window is the person's, in its own layout
    unsigned askedAtOpen = 0;       // the pages' layout questions when the window was asked for
    bool ending = false;            // done once the steps are sent, the window then closed
    long long endAt = 0;            // when it began to end
    unsigned id = 0;                // which switch a closer belongs to
    quick_switch::Keys keys;
};

struct PadState {
    quick_switch::Press press;
    quick_switch::Armed armed;      // pressed with two fingers: the window comes as it comes up
    quick_switch::Swipes swipes;    // each finger's swipe while the window is open
    quick_switch::Swipes front;     // the same, with the wheel in front and both fingers down
    quick_switch::Stick stick;      // the right stick's pushes while the window is open
    quick_switch::Mock mock;        // the tour's stand-in, while a step asks (g_mockOn)
    quick_switch::Swipes mockSwipes;   // its Quick's swipes, while it is up (mock_steps)
    quick_switch::Stick mockStick;     // and its right stick's pushes
    quick_switch::Armed closeArmed;    // pressed with two fingers, the window in front: it closes as it comes up
    quick_switch::AppSwitch app;       // the d-pad's app switcher: Alt, and the Tabs on their way
};

inline std::mutex g_mutex;
inline std::unordered_map<const void *, PadState> g_pads;
inline Session g_session;
inline unsigned g_nextId = 1;
// How many times a page has asked which layout to show (GET ui/view): once as
// each window opens.
inline std::atomic<unsigned> g_pageAsked{0};
// ⭐ THE TOUR'S STAND-IN (2026-11-06; quick_switch::mock_report): on only while
// a tour step asks, by the tutorial-bubble's "pad" flag, and its words kept for
// the page's tutorial-press poll. ⓘ It does nothing else: the real switch goes
// on exactly as it would.
inline std::atomic_bool g_mockOn{false};
inline std::string g_mockEvents;   // under g_mutex: "open,lift,..." since the page last asked

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

// The open this switch asked for has been made: its window is on its way, or
// another open already under way brings one. ⓘ From the thread that made it.
inline void mark_launched(unsigned id)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_session.active && g_session.id == id) g_session.launched = true;
}

// The window the press opened, closed once it is there and its steps have
// gone, and the layout put back (quick_switch::close_now says when). ⓘ Its
// own thread: the report path must not sleep.
// ⓘ The window is coming until its open has been made and the open's claim
// is let go, which happens once the window exists (open_ui.inl,
// raise_when_ready) or the open gave up.
inline void close_when_sent(unsigned id)
{
    std::thread([id]() {
        long long heardAt = -1;
        long long liftedAt = 0;
        bool waitedForWindow = false;
        for (;;) {
            bool done = false;
            const bool inFlight = ctm_open_ui::open_in_flight();
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                Session &s = g_session;
                if (!s.active || s.id != id) return;   // a new switch has the window
                const long long now = now_ms();
                const bool heard = g_pageAsked.load() != s.askedAtOpen;
                if (heard && heardAt < 0) heardAt = now;
                const bool coming = s.openedWindow && (!s.launched || inFlight);
                if (coming) waitedForWindow = true;
                liftedAt = s.endAt;
                if (quick_switch::close_now(now, s.endAt, coming, heard, heardAt,
                                            quick_switch::idle(s.keys))) {
                    s.active = false;
                    done = true;
                }
            }
            if (done) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        const long long closedAfter = now_ms() - liftedAt;
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
            << "quick switch: the last finger lifted -- the window closed "
            << closedAfter << " ms after"
            << (waitedForWindow ? ", once it was there" : "")
            << ", and the layout it had before is back");
    }).detach();
}

// Every report of a pad with a touchpad, read before anything changes the
// report: whether the pad is pressed in, each of its two fingers -- down or
// not, the id the pad gave its touch, and where it is -- and its right stick,
// when the report holds one (haveStick), up negative.
inline void report(const void *deviceKey, bool pressed, const quick_switch::Touch (&touch)[2],
                   bool haveStick, float rx, float ry)
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
    enum class Did { Nothing, Armed, Opened, TooLong, Stepped, Lifted, Kept, InFront, SomeoneElse,
                     Closing, Closed, CloseTooLong };
    Did did = Did::Nothing;
    int step = 0;
    bool byStick = false;
    bool byMock = false;
    unsigned closer = 0;
    unsigned opened = 0;
    unsigned kept = 0;
    long long heldMs = 0;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        PadState &ps = g_pads[deviceKey];
        Session &s = g_session;
        const bool mine = s.active && s.device == deviceKey;
        const bool pressEdge = quick_switch::press_edge(ps.press, pressed);
        // ⭐ A press of the touchpad while the d-pad's app switcher has Alt down
        // picks, as Cross does, with Quick held up or not
        // (quick_switch::app_switch_press). The press is the switcher's then:
        // it arms, keeps and closes nothing.
        const bool pickedByPress = pressEdge && quick_switch::app_switch_press(ps.app);
        const bool arms = pressEdge && !pickedByPress && fingers == 2;
        // ⓘ The tour's stand-in reads the same edge, beside the switch; while
        // its Quick is up, its swipes and the right stick are its steps, sent
        // as the switch's keys below (quick_switch::mock_steps).
        int mockStep = 0;
        bool mockByStick = false;
        if (g_mockOn.load()) {
            const bool wasUp = quick_switch::mock_up(ps.mock);
            if (const char *word = quick_switch::mock_report(ps.mock, pressEdge, pressed, fingers, now_ms())) {
                if (g_mockEvents.size() < 256) {
                    if (!g_mockEvents.empty()) g_mockEvents += ",";
                    g_mockEvents += word;
                }
            }
            mockStep = quick_switch::mock_steps(ps.mock, wasUp, ps.mockSwipes, ps.mockStick, touch,
                                                haveStick, rx, ry, &mockByStick);
        } else {
            ps.mock = quick_switch::Mock();
        }
        // ⓘ Its swipes are its own while it is up: the in-front swipe stands
        // aside, as it would step a second time.
        const bool mockUp = g_mockOn.load() && quick_switch::mock_up(ps.mock);
        const int swipe = quick_switch::front_steps(ps.front, wheelInFront && !mockUp, touch);
        if (mine && s.pressed && !s.ending) {
            // ⓘ Each finger by itself, not the two together: a thumb swiping
            // beside a resting one moves their midpoint as well, and would
            // step twice.
            const int swiped = quick_switch::swipe_steps(ps.swipes, touch);
            // ⭐ And the right stick's push, a step as a swipe is. Read on
            // every report, so it knows when the stick has come back.
            const int pushed = haveStick ? quick_switch::stick_steps(ps.stick, rx, ry) : 0;
            const int steps = swiped != 0 ? swiped : pushed;
            // ⭐ While the d-pad's app switcher has Alt down, Windows' switcher
            // is over the wheel: a press of the touchpad picks (above), and the
            // last finger lifting lets Alt go (quick_switch::app_switch_end), so
            // Windows switches to the window its switcher shows.
            const bool switching = ps.app.alt;
            switch (quick_switch::while_held(switching ? false : pressEdge, fingers,
                                             switching ? 0 : steps)) {
            case quick_switch::Held::Close:
                s.ending = true;
                s.endAt = now_ms();
                closer = s.id;
                quick_switch::app_switch_end(ps.app);
                did = Did::Lifted;
                break;
            case quick_switch::Held::Keep:
                // ⭐ The window is the person's now: the touchpad goes back to
                // the config, the window stays when the fingers lift, and it
                // takes the layout it had before, the page told by the layout
                // key after any steps still on their way (key()).
                s.ending = true;
                s.kept = true;
                quick_switch::queue_layout(s.keys);
                kept = s.id;
                did = Did::Kept;
                break;
            case quick_switch::Held::Step:
                quick_switch::queue(s.keys, steps);
                step = steps;
                byStick = swiped == 0;
                did = Did::Stepped;
                break;
            case quick_switch::Held::Stay:
                break;
            }
        } else if (arms) {
            if (s.active && s.device != deviceKey && !s.ending) {
                did = Did::SomeoneElse;
            } else if (inFront) {
                // ⭐ A press with the window in front closes it, as the press
                // comes up (quick_switch::close_up). ⛔ Not while the tour's
                // stand-in is on: its steps play the switch with the window in
                // front, and closing it would end the tour.
                if (g_mockOn.load()) {
                    did = Did::InFront;
                } else {
                    quick_switch::arm(ps.closeArmed, now_ms());
                    did = Did::Closing;
                }
            } else {
                quick_switch::arm(ps.armed, now_ms());
                did = Did::Armed;
            }
        } else if (ps.closeArmed.on) {
            if (g_mockOn.load()) {
                ps.closeArmed = quick_switch::Armed();
            } else {
                const bool wasSpent = ps.closeArmed.spent;
                heldMs = now_ms() - ps.closeArmed.since;
                if (quick_switch::close_up(ps.closeArmed, pressed, now_ms())) {
                    did = Did::Closed;
                } else if (!wasSpent && ps.closeArmed.spent) {
                    did = Did::CloseTooLong;
                }
            }
        } else if (ps.armed.on) {
            heldMs = now_ms() - ps.armed.since;
            switch (quick_switch::armed_report(ps.armed, pressed, fingers, now_ms())) {
            case quick_switch::Up::Open:
                if (s.active && s.device != deviceKey && !s.ending) {
                    did = Did::SomeoneElse;
                    break;
                }
                s = Session();
                s.active = true;
                s.device = deviceKey;
                s.pressed = true;
                s.openedWindow = true;
                s.askedAtOpen = g_pageAsked.load();
                s.id = g_nextId++;
                opened = s.id;
                ps.swipes = quick_switch::Swipes();
                ps.stick = quick_switch::Stick();
                did = Did::Opened;
                break;
            case quick_switch::Up::TooLong:
                did = Did::TooLong;
                break;
            case quick_switch::Up::None:
                break;
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
        } else if (mockStep != 0) {
            // ⭐ The tour's Quick, stepped: the page hears it as the switch's
            // keys, as it hears the in-front swipe's, above.
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
                quick_switch::queue(s.keys, mockStep);
                step = mockStep;
                byStick = mockByStick;
                byMock = true;
                did = Did::Stepped;
            }
        }
    }
    if (closer != 0) close_when_sent(closer);
    // ⓘ The layout from before is the listener's again at once, so the page
    // finds it when the layout key asks, or as it opens if it has not yet.
    if (kept != 0) config_move::keep_quick_view();
    if (did == Did::Nothing || did == Did::Lifted) return;

    const std::string ordinal = ctm_ordinal_for_device(deviceKey);
    if (did == Did::Opened) {
        // ⓘ The gate waits four seconds for the page, as for the shortcut.
        ctm_rebind::g_gateProvisionalUntil.store(now_ms() + 4000);
        std::thread([ordinal, opened]() {
            // ⛔ A WINDOW ALREADY THERE GOES FIRST, IN ITS OWN LAYOUT (rhoquinn8217,
            // 2026-10-28: "sometimes when I use the two finger press hold quick
            // switch, the text in quick mode is very large"). A window not in
            // front, a kept one not yet given the front included, is replaced;
            // its place and size are kept as it goes, in the slot of the layout
            // the listener holds then. Quick was set first, so a Simple window's
            // 1728x800 went into Quick's slot, and every Quick after it opened
            // that size, its words sized to the window. So the old one is gone
            // before Quick is set, as close_when_sent puts the layout back only
            // once the window has gone.
            if (ctm_open_ui::window_exists()) {
                const long long from = now_ms();
                ui_close_window();
                for (int i = 0; i < 40 && ctm_open_ui::window_exists(); ++i) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(25));
                }
                device_log::input(device_log::msg()
                    << "quick switch: the window that was there closed first, in its own layout, in "
                    << (now_ms() - from) << " ms");
            }
            // ⭐ Quick for this window alone, before it asks which layout to show.
            config_move::begin_quick_view(ordinal);
            // ⓘ Pressed again while the old window went: kept, as at once.
            bool keptMeanwhile = false;
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                keptMeanwhile = g_session.active && g_session.id == opened && g_session.kept;
            }
            if (keptMeanwhile) config_move::keep_quick_view();
            ctm_chord_show_ui(ordinal);
            mark_launched(opened);
        }).detach();
    }
    // ⭐ The window in front, closed as the press came up: as its own Close
    // does, so it keeps its place and gives the front back as it goes. ⓘ Its
    // own thread: the report path must not wait on a window.
    if (did == Did::Closed) {
        std::thread([]() { ui_close_window(); }).detach();
    }
    device_log::msg line;
    line << "quick switch: " << ordinal << " -- ";
    if (byMock) line << "the tour's Quick, ";
    if (did == Did::Opened || did == Did::Closed) {
        line << "two fingers pressed and let go after " << heldMs << " ms: ";
    }
    device_log::input(line
        << (did == Did::Opened ? "the window opening in Quick" :
            did == Did::Closed ? "the window in front closing" :
            did == Did::Closing ? "two fingers pressed with the window in front: it closes as the "
                                  "press comes up, under four seconds" :
            did == Did::CloseTooLong ? "two fingers held four seconds with the window in front, the "
                                       "TV's release: the window stays" :
            did == Did::Armed ? "two fingers pressed: the window comes as the press comes up, "
                                "under four seconds" :
            did == Did::TooLong ? "two fingers held four seconds, the TV's release: no window, "
                                  "the touchpad held back until the last finger lifts" :
            did == Did::Kept ? "pressed again: the window stays, in the layout it had before, "
                               "and the touchpad is the config's again" :
            did == Did::InFront ? "two fingers pressed, with the window in front already" :
            did == Did::SomeoneElse ? "the touchpad, while another controller is switching" :
            byStick ? (step > 0 ? "right stick down: the next config" : "right stick up: the config before") :
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
    // ⓘ Nothing for the page while the d-pad's app switcher has Alt down: the
    // page's keys ride on Ctrl, Shift and Alt, and would reach Windows'
    // switcher as well.
    const auto pad = g_pads.find(deviceKey);
    if (pad != g_pads.end() && pad->second.app.alt) return nullptr;
    if (s.openedWindow && g_pageAsked.load() == s.askedAtOpen) return nullptr;
    const char *k = quick_switch::key_now(s.keys, now_ms());
    // ⓘ A window someone else opened stays, and so does one a second press
    // kept; this switch is done with it once its steps have gone.
    if (k == nullptr && s.ending && (!s.openedWindow || s.kept) && quick_switch::idle(s.keys)) {
        s.active = false;
    }
    return k;
}

// Whether the touchpad is the switch's right now, kept from the listener's own
// touchpad (touch_mouse.inl) and from Windows and the game (mouse_exclusive.inl):
// any press with two fingers on it; every report from the press that arms
// the switch until the window opens, or, held to the TV's release, until the
// last finger lifts; and every report after the window opens until the last
// finger lifts or a second press keeps it.
inline bool holds_touchpad(const void *deviceKey, int fingers, bool pressed)
{
    if (!config_move::quick_switch_on()) return false;
    if (fingers == 2 && pressed) return true;
    std::lock_guard<std::mutex> lock(g_mutex);
    const auto pad = g_pads.find(deviceKey);
    if (pad != g_pads.end() && pad->second.armed.on) return true;
    // ⭐ And while the tour's Quick is up, as a window the press opened holds
    // it: its swipes are the tour's, and no cursor moves.
    if (pad != g_pads.end() && g_mockOn.load() && quick_switch::mock_up(pad->second.mock)) return true;
    // ⭐ And while the d-pad's app switcher has Alt down: its press picks, and
    // reaches neither Windows nor the game.
    if (pad != g_pads.end() && pad->second.app.alt) return true;
    const Session &s = g_session;
    return s.active && s.device == deviceKey && s.pressed && !s.ending;
}

// Whether the pad's sticks are the switch's right now, kept from the stick
// mouse (stick_mouse.inl): every report from the window opening until the last
// finger lifts or a second press keeps it, while the right stick steps the
// wheel. ⓘ The stick mouse stands aside whole, either stick, as it does while
// a stick steers a window.
inline bool holds_stick(const void *deviceKey)
{
    if (!config_move::quick_switch_on()) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    // ⓘ The tour's Quick too, while it is up (holds_touchpad).
    const auto pad = g_pads.find(deviceKey);
    if (pad != g_pads.end() && g_mockOn.load() && quick_switch::mock_up(pad->second.mock)) return true;
    // ⭐ And while the d-pad's app switcher has Alt down: the right stick moves
    // it, and no cursor.
    if (pad != g_pads.end() && pad->second.app.alt) return true;
    const Session &s = g_session;
    return s.active && s.device == deviceKey && s.pressed && !s.ending;
}

// ⭐ AFTER A PICK, THE QUICK WINDOW BACK IN FRONT of the window picked
// (rhoquinn8217, 2026-11-06: "I want the two finger press quick switch to
// reactivate if the user is still touching the touchpad"): while a finger
// still holds it up, or always for a switcher opened from Quick in front not
// held up (`always`; "the config window always comes back after selecting an
// app"). A moment after Alt went up, so Windows has given the picked window
// the front; it is then the window the front goes back to as the Quick window
// closes (ctm_open_ui::front_after_pick).
constexpr long long kPickSettleMs = 250;

inline void raise_after_pick(unsigned id, bool always)
{
    std::thread([id, always]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(kPickSettleMs));
        if (!always) {
            std::lock_guard<std::mutex> lock(g_mutex);
            const Session &s = g_session;
            if (!s.active || s.id != id || !s.pressed || s.ending) return;
        }
        const std::wstring picked = ctm_open_ui::front_after_pick();
        device_log::input_w() << L"quick switch: picked "
                              << (picked.empty() ? std::wstring(L"a window") : picked)
                              << (always ? L": the Quick window back in front of it"
                                         : L": the Quick window back in front of it, a finger still on the touchpad");
    }).detach();
}

// The configs window in front in Quick, held up by the switch or not: there
// the d-pad's left and right, and the right stick's, open the app switcher
// too (2026-11-06). ⓘ It asks Windows, so it is asked only when a press or a
// push could open the switcher, never on every report.
inline bool quick_in_front()
{
    bool compact = false, quick = false;
    config_move::view_get(&compact, &quick, nullptr);
    return compact && quick && ctm_rebind_config_mode_effective();
}

// ⭐ THE D-PAD'S APP SWITCHER (quick_switch::AppSwitch), every report of a pad
// with a touchpad: its d-pad left and right, Cross, and its right stick, read
// before anything changes the report. Returns the buttons it takes, as a mask
// of the pad's button indexes, for rebind.inl to blank before the gate and
// the bindings read them, so neither the page nor the game sees them.
inline uint32_t app_buttons(const void *deviceKey, bool left, bool right, bool cross,
                            bool haveStick, float rx, float ry)
{
    // ⓘ Not while the tour's stand-in is on: its Quick takes the d-pad's up
    // and down and the switch's steps, and Alt+Tab would take the front from
    // the tour.
    const bool on = config_move::app_switcher_on() && !g_mockOn.load();
    bool held = false;
    bool alt = false;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        const auto pad = g_pads.find(deviceKey);
        if (pad == g_pads.end()) return 0;   // the switch is off: report() keeps no pad
        const Session &s = g_session;
        held = on && s.active && s.device == deviceKey && s.pressed && !s.ending;
        alt = pad->second.app.alt;
    }
    // ⭐ Quick in front, not held up: asked of Windows only for a press or a
    // push that could open the switcher (quick_in_front).
    bool quickFront = false;
    if (on && !held && !alt) {
        const float ax = rx < 0 ? -rx : rx;
        if (left || right || (haveStick && ax >= quick_switch::kStickStepAt)) quickFront = quick_in_front();
    }
    const bool canOpen = held || quickFront;
    const bool fromQuick = !held && quickFront;
    bool altDown = false;
    uint32_t mask = 0;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        const auto pad = g_pads.find(deviceKey);
        if (pad == g_pads.end()) return 0;
        quick_switch::AppSwitch &a = pad->second.app;
        const bool wasAlt = a.alt;
        const long long now = now_ms();
        const quick_switch::AppTakes t =
            quick_switch::app_switch_buttons(a, canOpen, fromQuick, left, right, cross, now);
        if (haveStick) quick_switch::app_switch_stick(a, canOpen, fromQuick, rx, ry, now);
        altDown = !wasAlt && a.alt;
        if (t.leftRight) mask |= (1u << ctm_rebind::kBtnDpadLeft) | (1u << ctm_rebind::kBtnDpadRight);
        if (t.cross) mask |= 1u << ctm_rebind::kBtnFaceDown;
    }
    if (altDown) {
        device_log::input(device_log::msg()
            << "quick switch: " << ctm_ordinal_for_device(deviceKey) << " -- "
            << (left || right ? (right ? "d-pad right" : "d-pad left") : "right stick")
            << (fromQuick ? " in Quick" : "") << ": Alt held, Windows' window switcher up, until "
            << (fromQuick ? "Cross or a touchpad press picks" : "Cross, a touchpad press, or the last finger lifts"));
    }
    return mask;
}

// The switcher's keys on this pad's report, asked once a report from
// whichever branch of rebind.inl publishes the pad's keys: whether Alt is
// down, and the Tab down (+1 Tab, -1 Shift+Tab, 0 none).
inline void app_keys(const void *deviceKey, bool *alt, int *tab)
{
    *alt = false;
    *tab = 0;
    bool wentUp = false;
    bool picked = false;
    bool byPress = false;
    bool fromQuick = false;
    bool reraise = false;
    unsigned id = 0;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        const auto pad = g_pads.find(deviceKey);
        if (pad != g_pads.end()) {
            const quick_switch::AppKeys k = quick_switch::app_switch_keys(pad->second.app, now_ms());
            *alt = k.alt;
            *tab = k.tab;
            wentUp = k.wentUp;
            picked = k.picked;
            byPress = k.byPress;
            fromQuick = k.fromQuick;
            const Session &s = g_session;
            const bool heldUp = s.active && s.device == deviceKey && s.pressed && !s.ending;
            if (k.wentUp && k.picked && (k.fromQuick || heldUp)) {
                reraise = true;
                id = s.id;
            }
        }
        // ⛔ The listener's own raise presses Alt to be let take the front
        // (open_ui.inl, focus_existing): never while a pad holds it down here,
        // or Windows' switcher would take that Alt's let-go as the pick.
        bool any = false;
        for (const auto &p : g_pads) {
            if (p.second.app.alt) any = true;
        }
        ctm_open_ui::g_appSwitcherAlt.store(any);
    }
    if (wentUp) {
        device_log::input(device_log::msg()
            << "quick switch: " << ctm_ordinal_for_device(deviceKey) << " -- Alt let go "
            << (!picked ? "as the window went" : byPress ? "at the touchpad's press" : "at Cross")
            << ": Windows switches to the window it shows"
            << (reraise ? ", and the Quick window comes back in front of it" : ""));
    }
    if (reraise) raise_after_pick(id, fromQuick);
}

// ⭐ A PAD GOING AWAY TAKES ITS SWITCH STATE WITH IT, its app switcher's Alt
// above all: rebind_forget_pad (rebind.inl) at its unbridge. The keyboard lets
// its keys go there too; this keeps the listener's own raise from waiting on
// an Alt no pad holds any more (g_appSwitcherAlt).
inline void forget_pad(const void *deviceKey)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_pads.erase(deviceKey);
    bool any = false;
    for (const auto &p : g_pads) {
        if (p.second.app.alt) any = true;
    }
    ctm_open_ui::g_appSwitcherAlt.store(any);
}

// The tour's stand-in, on and off, and its words since the page last asked.
inline void mock_on(bool on)
{
    g_mockOn.store(on);
    if (!on) {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_mockEvents.clear();
    }
}
inline std::string take_mock_events()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    std::string out;
    out.swap(g_mockEvents);
    return out;
}

}  // namespace quick_switch_session

// ⓘ For the declarations in main.cpp: rebind.inl, touch_mouse.inl and
// rest_config.inl reach these, and the first and last are included before
// this file.
void quick_switch_report(const void *deviceKey, bool pressed,
                         bool down0, int id0, float x0, float y0,
                         bool down1, int id1, float x1, float y1,
                         bool haveStick, float rx, float ry)
{
    const quick_switch::Touch touch[2] = { { down0, id0, x0, y0 }, { down1, id1, x1, y1 } };
    quick_switch_session::report(deviceKey, pressed, touch, haveStick, rx, ry);
}

uint32_t quick_switch_app_buttons(const void *deviceKey, bool left, bool right, bool cross,
                                  bool haveStick, float rx, float ry)
{
    return quick_switch_session::app_buttons(deviceKey, left, right, cross, haveStick, rx, ry);
}

void quick_switch_forget_pad(const void *deviceKey)
{
    quick_switch_session::forget_pad(deviceKey);
}

void quick_switch_app_keys(const void *deviceKey, bool *alt, int *tab)
{
    quick_switch_session::app_keys(deviceKey, alt, tab);
}

bool quick_switch_holds_stick(const void *deviceKey)
{
    return quick_switch_session::holds_stick(deviceKey);
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
