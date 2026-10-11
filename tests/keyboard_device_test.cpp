// The virtual keyboard: what Windows is told is held, when one of the things
// holding keys lets go.
//
// ⭐ WHAT THESE PROTECT. Three things hold keys on the one virtual keyboard:
// each pad's rebinder, the trigger gesture and the touchpad. Windows sees what
// all of them hold together. release_all, which runs when the settings window
// closes and when the rebinds go off, cleared the rebinders' keys and then
// wrote "nothing held" over the report, so a key the trigger or the touchpad
// still held went up, and down again at that pad's next report: a release and
// a press nobody made (code review, 2026-10-05).
//
// WHAT THESE CANNOT DO. No keyboard is made here. The device the keyboard
// sends its reports to is a stand-in, and what is checked is the state those
// reports are made from, as the keyboard's own thread would send it.
//
// ⓘ HOW IT IS REACHED. keyboard_device.inl is included in this file's unnamed
// namespace, as main.cpp includes it in its own, with stand-ins for the
// USB/IP device and server it would start. ⛔ All of it in the unnamed
// namespace: the stand-ins wear real names (rebind_test.cpp says why).

#include "harness.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <windows.h>

#include "ctm/protocol.h"

namespace {

bool ctm_verbose_logs() { return false; }

namespace device_log {
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
class wstream {
public:
    template <typename T>
    wstream &operator<<(const T &) { return *this; }
};
inline void input(const std::string &) {}
inline wstream input_w() { return wstream(); }
}  // namespace device_log

std::wstring find_relative_asset(const wchar_t *path) { return path; }
std::wstring widen_ascii(const char *text, size_t len) { return std::wstring(text, text + len); }
constexpr uint16_t kDefaultUsbipPort = 3240;
bool run_usbip_attach(const std::wstring &, uint16_t) { return true; }

// What the keyboard would start: never started here.
class CtmUsbipDevice {
public:
    bool load(const std::wstring &, const std::wstring &, uint8_t, bool, uint8_t, std::wstring *) { return true; }
    void inject_synthetic_input(const CTM_INPUT_REPORT &) {}
};
struct NoServer {
    bool add_device(std::shared_ptr<CtmUsbipDevice>, const std::string &, std::wstring *) { return false; }
};
std::unique_ptr<NoServer> g_agent_usbip_server;

#include "../src/input/keyboard_device.inl"

using namespace ctmtest;
namespace kb = ctm_keyboard_device;

int g_padA = 0;
int g_padB = 0;

// What the next report would carry: the keys held, in any order.
std::vector<uint8_t> held_keys()
{
    std::lock_guard<std::mutex> lock(kb::g_stateMutex);
    std::vector<uint8_t> out;
    for (uint8_t k : kb::g_keys) {
        if (k != 0) out.push_back(k);
    }
    std::sort(out.begin(), out.end());
    return out;
}

uint8_t held_modifiers()
{
    std::lock_guard<std::mutex> lock(kb::g_stateMutex);
    return kb::g_modifiers;
}

void nothing_held()
{
    std::lock_guard<std::mutex> lock(kb::g_stateMutex);
    kb::g_perDevice.clear();
    kb::g_triggerHeld.clear();
    kb::g_touchHeld.clear();
    kb::set_state_locked_from_devices();
}

}  // namespace

int run_keyboard_device_tests()
{
    const uint8_t keyA = 0x04;
    const uint8_t keyB = 0x05;
    const uint8_t keyC = 0x06;

    section("virtual keyboard: letting go of everything the rebinders hold leaves a trigger's key held");
    {
        nothing_held();
        kb::set_trigger_keys_for(&g_padA, 0x02, &keyA, 1);   // the trigger holds Shift+A
        kb::set_state_for(&g_padB, 0, &keyB, 1);              // a rebind holds B
        CTM_CHECK(held_keys() == std::vector<uint8_t>({keyA, keyB}));
        kb::release_all();
        CTM_CHECK(held_keys() == std::vector<uint8_t>({keyA}));
        CTM_CHECK_EQ(static_cast<int>(held_modifiers()), 0x02);
    }

    section("virtual keyboard: ... and a key the touchpad is pressing in");
    {
        nothing_held();
        kb::set_touch_keys_for(&g_padA, 0, &keyC, 1);
        kb::set_state_for(&g_padA, 0x01, &keyB, 1);
        kb::release_all();
        CTM_CHECK(held_keys() == std::vector<uint8_t>({keyC}));
        CTM_CHECK_EQ(static_cast<int>(held_modifiers()), 0);
    }

    section("virtual keyboard: with only the rebinders holding, everything goes up");
    {
        nothing_held();
        kb::set_state_for(&g_padA, 0x01, &keyA, 1);
        kb::set_state_for(&g_padB, 0, &keyB, 1);
        kb::release_all();
        CTM_CHECK(held_keys().empty());
        CTM_CHECK_EQ(static_cast<int>(held_modifiers()), 0);
    }

    section("virtual keyboard: one pad's keys going does not take another pad's trigger key with them");
    {
        nothing_held();
        kb::set_trigger_keys_for(&g_padB, 0, &keyA, 1);
        kb::set_state_for(&g_padA, 0, &keyB, 1);
        kb::forget_device(&g_padA);
        CTM_CHECK(held_keys() == std::vector<uint8_t>({keyA}));
    }

    nothing_held();
    return 0;
}
