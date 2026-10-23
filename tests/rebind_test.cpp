// The rebinder: what a pad's report becomes on its way to Windows, and what
// the virtual keyboard and mouse are told, report by report.
//
// ⭐ WHAT THESE PROTECT. rebind.inl is the path every bridged pad's every
// report takes, and the faults it has had were all of one shape: a key or a
// click left down, a press that reached the game it was taken from, or one
// pad's report undoing another's. So these drive the real entry point,
// ctm_rebind_apply, with whole reports and read back three things: the report
// the game would get, the keys each pad holds on the virtual keyboard, and the
// buttons each pad holds on the virtual mouse.
//
// WHAT THESE CANNOT DO. Nothing here presses a key in Windows or opens a
// window. The keyboard, the mouse, the settings window and the settings are
// stand-ins that record what the rebinder asked of them; that Windows got it
// is checked with a pad. Turbo is timed with the real clock, so its checks
// sleep, and assert only what any amount of sleep past the period gives.
//
// ⓘ HOW IT IS REACHED. main.cpp includes the rebinder inside an unnamed
// namespace, after the modules it calls. This file does the same, with
// stand-ins for those modules and the real layouts, binding names and
// trigger effects. ⛔ ALL OF IT IN THE UNNAMED NAMESPACE, rebind.inl included:
// the stand-ins wear the real modules' names, and anything with external
// linkage would let the linker swap one file's copy for another's
// (trigger_click_test.cpp says how that once happened). The standard headers
// come first, outside it, as units.h explains.

#include "harness.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ios>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

#include "../src/input/button_layout.inl"

// ---- The settings ---------------------------------------------------------
// "section.key" -> value. The real ones read the settings files.
std::map<std::string, std::string> g_strings;
std::map<std::string, int> g_ints;
std::map<std::string, bool> g_bools;

std::string device_config_str(const char *section, const char *key)
{
    const auto it = g_strings.find(std::string(section) + "." + key);
    return it == g_strings.end() ? std::string() : it->second;
}

int device_config_int(const char *section, const char *key, int fallback)
{
    const auto it = g_ints.find(std::string(section) + "." + key);
    return it == g_ints.end() ? fallback : it->second;
}

bool device_config_bool(const char *section, const char *key, bool fallback)
{
    const auto it = g_bools.find(std::string(section) + "." + key);
    return it == g_bools.end() ? fallback : it->second;
}

// ---- The log ---------------------------------------------------------------
// Kept, so a check can ask what was said.
namespace device_log {
std::vector<std::string> g_lines;
class msg {
public:
    template <typename T>
    msg &operator<<(const T &value)
    {
        stream_ << value;
        return *this;
    }
    operator std::string() const { return stream_.str(); }

private:
    std::ostringstream stream_;
};
inline void input(const std::string &message) { g_lines.push_back(message); }
}  // namespace device_log

bool ctm_verbose_logs() { return false; }

// ---- Which pad it is -------------------------------------------------------
// ⓘ As trigger_click_test.cpp reads one: a blank descriptor is a DualSense,
// and a DS4 and an Xbox pad are named by their real ids.
struct InputPad {
    const char *kind = nullptr;
    const ctm_rebind::Layout *layout = nullptr;
};

InputPad device_input_pad_for(const std::vector<unsigned char> &d)
{
    uint16_t vendor = 0;
    uint16_t product = 0;
    if (d.size() >= 12) {
        vendor = static_cast<uint16_t>(d[8] | (d[9] << 8));
        product = static_cast<uint16_t>(d[10] | (d[11] << 8));
    }
    const char *kind = "ds5";
    if (vendor == 0x054c && (product == 0x09cc || product == 0x05c4)) kind = "ds4";
    else if (vendor == 0x045e && product == 0x0b12) kind = "xbox";
    InputPad pad;
    pad.kind = kind;
    pad.layout = ctm_rebind::layout_for(kind);
    return pad;
}

const char *device_button_section_for(const std::vector<unsigned char> &d)
{
    return device_input_pad_for(d).kind;
}

std::string device_settings_section(const char *kind, const std::string &linked)
{
    return linked.empty() ? std::string(kind) : "cfg:" + linked;
}

// ---- The virtual keyboard: each pad's held set, kept as the real one keeps it.
namespace ctm_keyboard_device {
std::map<const void *, std::pair<uint8_t, std::vector<uint8_t>>> g_perDevice;
inline void set_state_for(const void *deviceKey, uint8_t modifiers, const uint8_t *keys, size_t count)
{
    auto &slot = g_perDevice[deviceKey];
    slot.first = modifiers;
    slot.second.assign(keys, keys + (keys ? count : 0));
}
inline bool holds_for(const void *deviceKey)
{
    const auto it = g_perDevice.find(deviceKey);
    if (it == g_perDevice.end()) return false;
    if (it->second.first != 0) return true;
    for (uint8_t k : it->second.second) {
        if (k != 0) return true;
    }
    return false;
}
inline void release_all() { g_perDevice.clear(); }
inline void forget_device(const void *deviceKey) { g_perDevice.erase(deviceKey); }
}  // namespace ctm_keyboard_device

// ---- The virtual mouse: each pad's buttons, and every publish counted.
namespace ctm_mouse_device {
std::map<const void *, uint8_t> g_buttonsFor;
std::map<const void *, int> g_publishes;
int g_wheel = 0;
inline void set_buttons_for(const void *deviceKey, uint8_t mask)
{
    ++g_publishes[deviceKey];
    if (mask != 0) g_buttonsFor[deviceKey] = mask;
    else g_buttonsFor.erase(deviceKey);
}
inline void add_wheel(int clicks) { g_wheel += clicks; }
}  // namespace ctm_mouse_device

int g_mouseStarts = 0;
void ctm_gyro_mouse_ensure_mouse_started() { ++g_mouseStarts; }

// ---- The on-screen keyboard, the settings window, the gestures ------------
int g_oskToggles = 0;
void ctm_osk_toggle(const std::string &, int, int) { ++g_oskToggles; }

bool g_windowInFront = false;
bool ctm_ui_has_foreground() { return g_windowInFront; }

// ⓘ Counted from the thread the chord starts, so atomic. A check can make the
// window slow to open, as a browser starting cold is.
std::atomic<int> g_windowsAsked{0};
std::atomic<int> g_windowOpensMs{0};
std::string ctm_ordinal_for_device(const void *) { return "pad"; }
void ctm_chord_show_ui(const std::string &)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(g_windowOpensMs.load()));
    g_windowsAsked.fetch_add(1);
}

uint32_t g_triggerPresses = 0;
uint32_t trigger_click_pad_buttons(const void *) { return g_triggerPresses; }

// The overlay takes a report only while it is up, and here it never is. The
// window mover takes one while a check says Options is steering the window.
namespace ctm_overlay {
inline bool handle_report(const void *, const ctm_rebind::Layout &, uint8_t *, size_t) { return false; }
inline void blank_report(const ctm_rebind::Layout &, uint8_t *, size_t) {}
inline void forget_device(const void *) {}
}  // namespace ctm_overlay
bool g_steering = false;
// ⓘ The layout the page last named: the gate gives the page R3 in the
// compact views, Simple and Quick, and not in Advanced.
bool g_compactView = false;
bool g_quickView = false;
namespace config_move {
inline bool handle_report(const void *, const ctm_rebind::Layout &, uint8_t *, size_t) { return g_steering; }
inline bool view_get(bool *compact, bool *quick, std::string *)
{
    if (compact) *compact = g_compactView;
    if (quick) *quick = g_quickView;
    return true;
}
}  // namespace config_move
bool ctm_overlay_opened_for_page() { return false; }
void ctm_overlay_hide() {}

// Defined further down rebind.inl than they are called, as main.cpp declares
// them ahead of it.
bool ctm_rebind_editing_field();
void ctm_ui_notify(const std::string &message);

#include "../src/input/trigger_effect.inl"
#include "../src/input/rebind.inl"

// ---- The checks' own helpers -----------------------------------------------
using namespace ctmtest;
using ctm_rebind::kBtnFaceDown;
using ctm_rebind::kBtnFaceRight;
using ctm_rebind::kBtnFaceLeft;
using ctm_rebind::kBtnFaceUp;
using ctm_rebind::kBtnL1;
using ctm_rebind::kBtnR2;
using ctm_rebind::kBtnStart;
using ctm_rebind::kBtnDpadUp;
using ctm_rebind::kBtnR3;

int g_padA = 0;
int g_padB = 0;
const void *const kPadA = &g_padA;
const void *const kPadB = &g_padB;
const std::vector<unsigned char> kDualSense;   // blank: a DualSense here

const ctm_rebind::Layout &ds5() { return *ctm_rebind::layout_for("ds5"); }

// A DualSense report at rest: sticks centred, the d-pad centred, no fingers
// on the touchpad (bit 0x80 SET is no finger), nothing pressed.
std::vector<uint8_t> at_rest()
{
    std::vector<uint8_t> r(64, 0);
    r[0] = 0x01;
    r[1] = r[2] = r[3] = r[4] = 0x80;
    r[8] = 0x08;
    r[33] = r[37] = 0x80;
    return r;
}

std::vector<uint8_t> pressing(std::initializer_list<int> buttons)
{
    std::vector<uint8_t> r = at_rest();
    for (int b : buttons) ctm_rebind::set_button(ds5(), r.data(), r.size(), b);
    return r;
}

// One report through the real entry point; the report as the game gets it.
std::vector<uint8_t> send(const void *pad, std::vector<uint8_t> report)
{
    ctm_rebind_apply(pad, kDualSense, std::string(), report.data(), report.size());
    return report;
}

bool game_sees(const std::vector<uint8_t> &report, int button)
{
    return ctm_rebind::is_pressed(ds5(), report.data(), report.size(), button);
}

std::vector<uint8_t> keys_of(const void *pad)
{
    const auto it = ctm_keyboard_device::g_perDevice.find(pad);
    std::vector<uint8_t> out;
    if (it != ctm_keyboard_device::g_perDevice.end()) {
        for (uint8_t k : it->second.second) {
            if (k != 0) out.push_back(k);
        }
    }
    return out;
}

uint8_t mods_of(const void *pad)
{
    const auto it = ctm_keyboard_device::g_perDevice.find(pad);
    return it == ctm_keyboard_device::g_perDevice.end() ? 0 : it->second.first;
}

uint8_t mouse_of(const void *pad)
{
    const auto it = ctm_mouse_device::g_buttonsFor.find(pad);
    return it == ctm_mouse_device::g_buttonsFor.end() ? 0 : it->second;
}

bool keys_are(const void *pad, std::initializer_list<uint8_t> want)
{
    return keys_of(pad) == std::vector<uint8_t>(want);
}

// Everything back to the start: no settings, nothing held, the gate off, and
// each pad as if just bridged.
// ⛔ FORGET FIRST, THEN A REPORT AT REST. Once the gate has let go even once,
// a pad the rebinder has not seen yet takes every button held in its first
// report as held over, and swallows it until released: a check whose first
// report pressed something would see it vanish. The report at rest settles
// that, as a real pad's first reports do.
void reset()
{
    g_strings.clear();
    g_ints.clear();
    g_bools.clear();
    ctm_rebind_set_config_mode(false);
    ctm_rebind_clear_provisional();
    g_windowInFront = false;
    g_triggerPresses = 0;
    g_steering = false;
    rebind_forget_pad(kPadA);
    rebind_forget_pad(kPadB);
    {
        std::lock_guard<std::mutex> lock(ctm_rebind::g_turboMutex);
        ctm_rebind::g_turbo.clear();
    }
    send(kPadA, at_rest());
    send(kPadB, at_rest());
    ctm_keyboard_device::g_perDevice.clear();
    ctm_mouse_device::g_buttonsFor.clear();
    ctm_mouse_device::g_publishes.clear();
    ctm_mouse_device::g_wheel = 0;
    g_oskToggles = 0;
    device_log::g_lines.clear();
}

bool said(const char *text)
{
    for (const std::string &line : device_log::g_lines) {
        if (line.find(text) != std::string::npos) return true;
    }
    return false;
}

}  // namespace

int run_rebind_tests()
{
    section("rebinder: a pad bound to nothing reaches the game untouched, and no key is published");
    {
        reset();
        const std::vector<uint8_t> in = pressing({kBtnFaceDown, kBtnL1});
        const std::vector<uint8_t> out = send(kPadA, in);
        CTM_CHECK(out == in);
        CTM_CHECK(ctm_keyboard_device::g_perDevice.count(kPadA) == 0);
    }

    section("rebinder: a button bound to a key holds the key while held, and the game never sees it");
    {
        reset();
        g_strings["ds5.rebind_0"] = "KeyA";
        const std::vector<uint8_t> down = send(kPadA, pressing({kBtnFaceDown}));
        CTM_CHECK(keys_are(kPadA, {0x04}));
        CTM_CHECK(!game_sees(down, kBtnFaceDown));
        send(kPadA, at_rest());
        CTM_CHECK(keys_are(kPadA, {}));
    }

    section("rebinder: a binding taken away while its button is held lets the key go at once");
    {
        // ⓘ The code review's "removing the last binding mid-press leaves it
        // held": with nothing bound the rebinder used to publish nothing, so
        // the key it had down stayed down.
        reset();
        g_strings["ds5.rebind_0"] = "KeyA";
        send(kPadA, pressing({kBtnFaceDown}));
        CTM_CHECK(keys_are(kPadA, {0x04}));
        g_strings.erase("ds5.rebind_0");
        const std::vector<uint8_t> after = send(kPadA, pressing({kBtnFaceDown}));
        CTM_CHECK(keys_are(kPadA, {}));
        // With no binding, the button is the game's again.
        CTM_CHECK(game_sees(after, kBtnFaceDown));
    }

    section("rebinder: two buttons held together arrive as one state, a modifier in its own byte");
    {
        reset();
        g_strings["ds5.rebind_0"] = "KeyA";
        g_strings["ds5.rebind_1"] = "ShiftLeft";
        send(kPadA, pressing({kBtnFaceDown, kBtnFaceRight}));
        CTM_CHECK(keys_are(kPadA, {0x04}));
        CTM_CHECK_EQ(static_cast<int>(mods_of(kPadA)), 0x02);
    }

    section("rebinder: a button bound to Win+Tab holds the Win key and Tab together");
    {
        // ⓘ The one name with both halves; either alone would be half of it.
        reset();
        g_strings["ds5.rebind_0"] = "win+tab";
        send(kPadA, pressing({kBtnFaceDown}));
        CTM_CHECK(keys_are(kPadA, {0x2B}));
        CTM_CHECK_EQ(static_cast<int>(mods_of(kPadA)), 0x08);
        send(kPadA, at_rest());
        CTM_CHECK(keys_are(kPadA, {}));
        CTM_CHECK_EQ(static_cast<int>(mods_of(kPadA)), 0);
    }

    section("rebinder: Cross to Circle and Circle to Cross both work, whichever the loop meets first");
    {
        reset();
        g_strings["ds5.rebind_0"] = "button_circle";
        g_strings["ds5.rebind_1"] = "button_cross";
        const std::vector<uint8_t> cross = send(kPadA, pressing({kBtnFaceDown}));
        CTM_CHECK(game_sees(cross, kBtnFaceRight));
        CTM_CHECK(!game_sees(cross, kBtnFaceDown));
        const std::vector<uint8_t> circle = send(kPadA, pressing({kBtnFaceRight}));
        CTM_CHECK(game_sees(circle, kBtnFaceDown));
        CTM_CHECK(!game_sees(circle, kBtnFaceRight));
    }

    section("rebinder: one pad at rest does not let go of another pad's key");
    {
        reset();
        g_strings["ds5.rebind_0"] = "KeyA";
        send(kPadA, pressing({kBtnFaceDown}));
        send(kPadB, at_rest());
        CTM_CHECK(keys_are(kPadA, {0x04}));
        CTM_CHECK(keys_are(kPadB, {}));
    }

    section("rebinder: a mouse button follows its pad button, and a binding taken away mid-press lets it go");
    {
        reset();
        g_strings["ds5.rebind_3"] = "MouseLeft";
        const std::vector<uint8_t> down = send(kPadA, pressing({kBtnFaceUp}));
        CTM_CHECK_EQ(static_cast<int>(mouse_of(kPadA)), 0x01);
        CTM_CHECK(!game_sees(down, kBtnFaceUp));
        CTM_CHECK(g_mouseStarts > 0);
        g_strings.erase("ds5.rebind_3");
        send(kPadA, pressing({kBtnFaceUp}));
        CTM_CHECK_EQ(static_cast<int>(mouse_of(kPadA)), 0);
    }

    section("rebinder: the mouse is told on every report, bound or not");
    {
        // ⓘ Publishing only while something was bound latched a button whose
        // binding was taken away (code review, 2026-10-05).
        reset();
        for (int i = 0; i < 3; ++i) send(kPadB, at_rest());
        CTM_CHECK_EQ(ctm_mouse_device::g_publishes[kPadB], 3);
    }

    section("rebinder: a wheel binding scrolls once per press, not once per report");
    {
        reset();
        g_strings["ds5.rebind_12"] = "MouseWheelUp";
        for (int i = 0; i < 5; ++i) send(kPadA, pressing({kBtnDpadUp}));
        CTM_CHECK_EQ(ctm_mouse_device::g_wheel, 1);
        send(kPadA, at_rest());
        send(kPadA, pressing({kBtnDpadUp}));
        CTM_CHECK_EQ(ctm_mouse_device::g_wheel, 2);
    }

    section("rebinder: a trigger given to the steady gesture is not fired here, and the mouse is still told");
    {
        // ⓘ rhoquinn8217, 2026-09-11: "click with R2 is still sticking and
        // won't unstick" -- the trigger was skipped, and so was the publish.
        reset();
        g_strings["ds5.rebind_7"] = "MouseLeft";
        send(kPadA, pressing({kBtnR2}));
        CTM_CHECK_EQ(static_cast<int>(mouse_of(kPadA)), 0x01);
        g_strings["ds5.right_trigger_steady_cursor_pull"] = "immediate";
        send(kPadA, pressing({kBtnR2}));
        CTM_CHECK_EQ(static_cast<int>(mouse_of(kPadA)), 0);
        // ⓘ "off" is a value the page saves; it leaves the trigger an
        // ordinary button, bound here.
        g_strings["ds5.right_trigger_steady_cursor_pull"] = "off";
        send(kPadA, pressing({kBtnR2}));
        CTM_CHECK_EQ(static_cast<int>(mouse_of(kPadA)), 0x01);
    }

    section("rebinder: the buttons a steady trigger presses are added to the report");
    {
        reset();
        g_triggerPresses = 1u << kBtnFaceLeft;
        const std::vector<uint8_t> out = send(kPadA, at_rest());
        CTM_CHECK(game_sees(out, kBtnFaceLeft));
    }

    section("rebinder: turbo with no binding lets the button through on alternate periods");
    {
        reset();
        g_ints["ds5.turbo_0"] = 40;
        const std::vector<uint8_t> first = send(kPadA, pressing({kBtnFaceDown}));
        std::this_thread::sleep_for(std::chrono::milliseconds(60));
        const std::vector<uint8_t> second = send(kPadA, pressing({kBtnFaceDown}));
        std::this_thread::sleep_for(std::chrono::milliseconds(60));
        const std::vector<uint8_t> third = send(kPadA, pressing({kBtnFaceDown}));
        CTM_CHECK(game_sees(first, kBtnFaceDown));
        CTM_CHECK(!game_sees(second, kBtnFaceDown));
        CTM_CHECK(game_sees(third, kBtnFaceDown));
    }

    section("rebinder: the pad that pressed last is known even when its press was bound away");
    {
        reset();
        g_strings["ds5.rebind_0"] = "KeyA";
        send(kPadA, pressing({kBtnFaceDown}));
        CTM_CHECK(rebind_last_press_device() == kPadA);
        send(kPadB, pressing({kBtnL1}));
        CTM_CHECK(rebind_last_press_device() == kPadB);
    }

    section("config mode: with the window in front, the d-pad types Ctrl+Shift+Alt+W and nothing reaches the game");
    {
        reset();
        g_strings["ds5.rebind_0"] = "KeyF";
        ctm_rebind_set_config_mode(true);
        g_windowInFront = true;
        std::vector<uint8_t> in = pressing({kBtnDpadUp, kBtnL1});
        in[1] = 0xff;                                  // left stick hard right
        const std::vector<uint8_t> out = send(kPadA, in);
        CTM_CHECK(keys_are(kPadA, {0x1a, 0x14}));     // W for up, Q for L1
        CTM_CHECK_EQ(static_cast<int>(mods_of(kPadA)), 0x07);
        CTM_CHECK(out == at_rest());
        // ⓘ The user's own binding does not run while the page has the pad:
        // Cross selects, it does not type F.
        send(kPadA, pressing({kBtnFaceDown}));
        CTM_CHECK(keys_are(kPadA, {0x28}));            // Enter
    }

    section("config mode: Triangle types Ctrl+Shift+Alt+Y and Square Ctrl+Shift+Alt+X, for the config selector");
    {
        // ⓘ Triangle reached nothing here until the config selector gave it
        // auto link; Square's X is the toggle it always was.
        reset();
        ctm_rebind_set_config_mode(true);
        g_windowInFront = true;
        const std::vector<uint8_t> out = send(kPadA, pressing({kBtnFaceUp}));
        CTM_CHECK(keys_are(kPadA, {0x1c}));            // Y
        CTM_CHECK_EQ(static_cast<int>(mods_of(kPadA)), 0x07);
        CTM_CHECK(out == at_rest());
        send(kPadA, pressing({kBtnFaceLeft}));
        CTM_CHECK(keys_are(kPadA, {0x1b}));            // X
    }

    section("config mode: while a name is being typed, Triangle and Square type nothing for the page");
    {
        // ⓘ They are space and backspace on the virtual keyboard then, and
        // a page key as well would do both.
        reset();
        ctm_rebind_set_config_mode(true);
        g_windowInFront = true;
        ctm_rebind_set_editing_field(true);
        send(kPadA, pressing({kBtnFaceUp}));
        CTM_CHECK(keys_are(kPadA, {}));
        send(kPadA, pressing({kBtnFaceLeft}));
        CTM_CHECK(keys_are(kPadA, {}));
        ctm_rebind_set_editing_field(false);
    }

    section("config mode: R3 types Ctrl+Shift+Alt+L in Simple and Quick, for their polling limit");
    {
        // ⓘ In Advanced it does what the config binds it to, and while a
        // name is being typed it is the virtual keyboard's size.
        reset();
        ctm_rebind_set_config_mode(true);
        g_windowInFront = true;
        g_compactView = false;                          // Advanced
        g_quickView = false;
        send(kPadA, pressing({kBtnR3}));
        CTM_CHECK(keys_are(kPadA, {}));
        g_compactView = true;                           // Simple
        const std::vector<uint8_t> simple = send(kPadA, pressing({kBtnR3}));
        CTM_CHECK(keys_are(kPadA, {0x0f}));            // L
        CTM_CHECK_EQ(static_cast<int>(mods_of(kPadA)), 0x07);
        CTM_CHECK(simple == at_rest());
        g_quickView = true;                             // Quick
        const std::vector<uint8_t> out = send(kPadA, pressing({kBtnR3}));
        CTM_CHECK(keys_are(kPadA, {0x0f}));            // L
        CTM_CHECK_EQ(static_cast<int>(mods_of(kPadA)), 0x07);
        CTM_CHECK(out == at_rest());
        ctm_rebind_set_editing_field(true);
        send(kPadA, pressing({kBtnR3}));
        CTM_CHECK(keys_are(kPadA, {}));
        ctm_rebind_set_editing_field(false);
        g_compactView = false;
        g_quickView = false;
    }

    section("config mode: with the window behind, the pad works as it does in a game");
    {
        // ⓘ This returned before the bindings, so the game behind got the raw
        // pad with none of them (code review, 2026-10-05).
        reset();
        g_strings["ds5.rebind_0"] = "KeyF";
        ctm_rebind_set_config_mode(true);
        g_windowInFront = false;
        const std::vector<uint8_t> out = send(kPadA, pressing({kBtnFaceDown, kBtnDpadUp}));
        CTM_CHECK(keys_are(kPadA, {0x09}));            // F
        CTM_CHECK(!game_sees(out, kBtnFaceDown));
        CTM_CHECK(game_sees(out, kBtnDpadUp));
        CTM_CHECK(said("our window is not in front"));
    }

    section("config mode: a button held as the gate lets go reaches the game only once released");
    {
        reset();
        ctm_rebind_set_config_mode(true);
        g_windowInFront = true;
        send(kPadA, pressing({kBtnFaceDown}));
        CTM_CHECK(keys_are(kPadA, {0x28}));
        ctm_rebind_set_config_mode(false);             // the window closes
        CTM_CHECK(keys_are(kPadA, {}));                 // and its keys go with it
        const std::vector<uint8_t> held = send(kPadA, pressing({kBtnFaceDown}));
        CTM_CHECK(!game_sees(held, kBtnFaceDown));
        send(kPadA, at_rest());
        const std::vector<uint8_t> again = send(kPadA, pressing({kBtnFaceDown}));
        CTM_CHECK(game_sees(again, kBtnFaceDown));
    }

    section("config mode: each pad swallows its own held buttons, whatever another pad reports between");
    {
        // ⓘ One mask for all pads let pad B's report, 4 ms later, clear the
        // Cross pad A still held (code review, 2026-10-05).
        reset();
        ctm_rebind_set_config_mode(true);
        g_windowInFront = true;
        send(kPadA, pressing({kBtnFaceDown}));
        ctm_rebind_set_config_mode(false);
        send(kPadB, at_rest());
        const std::vector<uint8_t> held = send(kPadA, pressing({kBtnFaceDown}));
        CTM_CHECK(!game_sees(held, kBtnFaceDown));
    }

    section("the chord: two fingers resting and Options pressed ask for the window once, and Options goes through");
    {
        reset();
        std::vector<uint8_t> chord = pressing({kBtnStart});
        chord[33] = 0x00;                              // finger 1 down
        chord[37] = 0x00;                              // finger 2 down
        const int before = g_windowsAsked.load();
        auto asked_since = [&](int want) {
            for (int i = 0; i < 200 && g_windowsAsked.load() - before < want; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            return g_windowsAsked.load() - before;
        };
        const std::vector<uint8_t> out = send(kPadA, chord);
        CTM_CHECK_EQ(asked_since(1), 1);
        CTM_CHECK(game_sees(out, kBtnStart));
        CTM_CHECK(said("chord: two fingers + Options"));
        // ⓘ Still held is no second press. The window on its way is cleared
        // first, or its own refusal (below) would hide whether the press was
        // read on its edge.
        ctm_rebind_clear_provisional();
        send(kPadA, chord);
        CTM_CHECK_EQ(asked_since(1), 1);
        // A fresh press asks again...
        send(kPadA, at_rest());
        send(kPadA, chord);
        CTM_CHECK_EQ(asked_since(2), 2);
        // ...but not while the window it asked for is still on its way: that
        // would close the opening window and unpause the game.
        send(kPadA, at_rest());
        send(kPadA, chord);
        CTM_CHECK_EQ(asked_since(2), 2);
        ctm_rebind_clear_provisional();
    }

    section("the chord: Options alone, or one finger, asks for nothing");
    {
        reset();
        const int before = g_windowsAsked.load();
        send(kPadA, pressing({kBtnStart}));
        send(kPadA, at_rest());
        std::vector<uint8_t> oneFinger = pressing({kBtnStart});
        oneFinger[33] = 0x00;
        send(kPadA, oneFinger);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        CTM_CHECK_EQ(g_windowsAsked.load() - before, 0);
    }

    section("the chord: with the settings window behind the game, it still brings it forward");
    {
        // ⓘ L12's third part: the chord was refused while config mode was
        // on, and that is when the window is behind the game, where the
        // chord is the way back to it (code review, 2026-10-05).
        reset();
        ctm_rebind_set_config_mode(true);
        g_windowInFront = false;
        std::vector<uint8_t> chord = pressing({kBtnStart});
        chord[33] = 0x00;
        chord[37] = 0x00;
        const int before = g_windowsAsked.load();
        send(kPadA, chord);
        for (int i = 0; i < 200 && g_windowsAsked.load() == before; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        CTM_CHECK_EQ(g_windowsAsked.load() - before, 1);
        ctm_rebind_clear_provisional();
    }

    section("the chord: the pad's report is answered at once, however long the window takes to open");
    {
        // ⓘ L6: the window was opened on the pad's own report thread, and a
        // browser starting cold held that pad's presses for up to a second.
        reset();
        g_windowOpensMs.store(800);
        std::vector<uint8_t> chord = pressing({kBtnStart});
        chord[33] = 0x00;
        chord[37] = 0x00;
        const int before = g_windowsAsked.load();
        const auto t0 = std::chrono::steady_clock::now();
        send(kPadA, chord);
        const auto took = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t0).count();
        CTM_CHECK(took < 200);
        // ⓘ And the window is still asked for, once it has had its time.
        for (int i = 0; i < 400 && g_windowsAsked.load() == before; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        CTM_CHECK_EQ(g_windowsAsked.load() - before, 1);
        g_windowOpensMs.store(0);
        ctm_rebind_clear_provisional();
    }

    section("rebinder: steering the settings window lets go of the keys and clicks it had down");
    {
        // ⓘ L11: the rebinder is skipped while Options steers the window, so
        // what it had down stayed down, and Windows repeated the key.
        reset();
        g_strings["ds5.rebind_0"] = "KeyA";
        g_strings["ds5.rebind_3"] = "MouseLeft";
        send(kPadA, pressing({kBtnFaceDown, kBtnFaceUp}));
        CTM_CHECK(keys_are(kPadA, {0x04}));
        CTM_CHECK_EQ(static_cast<int>(mouse_of(kPadA)), 0x01);
        g_steering = true;
        send(kPadA, pressing({kBtnFaceDown, kBtnFaceUp, kBtnStart}));
        CTM_CHECK(keys_are(kPadA, {}));
        CTM_CHECK_EQ(static_cast<int>(mouse_of(kPadA)), 0);
    }

    reset();
    return 0;
}
