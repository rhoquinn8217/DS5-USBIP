// Which of the devices Windows keeps from past bridges are ours to remove.
//
// ⭐ WHAT THESE PROTECT. A removal is for good, so the rule must take every
// old record of a bridge and nothing else: not a device that is connected,
// not the hub or the host controller, and not another program's virtual
// devices. rhoquinn8217's PC has Nefarius ViGEm and VirtualHere beside
// usbip-win2, and old DS4 records with Sony's own ids belonged to ViGEm.

#include "harness.h"

#include <algorithm>
#include <string>
#include <vector>

#include "app/old_devices_rule.inl"

using namespace ctmtest;
namespace odr = old_devices_rule;

namespace {

odr::Record rec(const wchar_t *id, const wchar_t *parent, bool present, bool controller = false)
{
    odr::Record r;
    r.id = id;
    r.parent = parent;
    r.present = present;
    r.usbipController = controller;
    return r;
}

bool has(const std::vector<odr::Removal> &removals, const wchar_t *id)
{
    return std::any_of(removals.begin(), removals.end(), [id](const odr::Removal &r) { return r.id == id; });
}

size_t position(const std::vector<odr::Removal> &removals, const wchar_t *id)
{
    for (size_t i = 0; i < removals.size(); ++i) if (removals[i].id == id) return i;
    return removals.size();
}

// A PC like rhoquinn8217's: usbip-win2's controller and hub, an old DualSense
// with its parts, the synthetic mouse connected, ViGEm with an old DS4, a
// Bluetooth pad paired to the PC, and two records that lead nowhere.
std::vector<odr::Record> a_pc()
{
    return {
        rec(L"ROOT\\USB\\0001", L"HTREE\\ROOT\\0", true, true),                  // usbip-win2
        rec(L"USB\\ROOT_HUB30\\1&2728EF4&4&0", L"ROOT\\USB\\0001", true),           // its hub
        // An old DualSense, its interfaces, its HID part and its speaker.
        rec(L"USB\\VID_054C&PID_0CE6\\A0FA9CEF9B30", L"usb\\root_hub30\\1&2728ef4&4&0", false),
        rec(L"USB\\VID_054C&PID_0CE6&MI_00\\8&1", L"USB\\VID_054C&PID_0CE6\\A0FA9CEF9B30", false),
        rec(L"USB\\VID_054C&PID_0CE6&MI_03\\8&2", L"USB\\VID_054C&PID_0CE6\\A0FA9CEF9B30", false),
        rec(L"HID\\VID_054C&PID_0CE6&MI_03\\9&3", L"USB\\VID_054C&PID_0CE6&MI_03\\8&2", false),
        rec(L"SWD\\MMDEVAPI\\{0.0.0.00000000}.{AB}", L"USB\\VID_054C&PID_0CE6&MI_00\\8&1", false),
        // The synthetic mouse, connected, with an old part of its own.
        rec(L"USB\\VID_1D6B&PID_0002\\CTM-GYRO-01", L"USB\\ROOT_HUB30\\1&2728EF4&4&0", true),
        rec(L"HID\\VID_1D6B&PID_0002\\OLD", L"USB\\VID_1D6B&PID_0002\\CTM-GYRO-01", false),
        // A bridge that is up now.
        rec(L"USB\\VID_054C&PID_0DF2\\C0FFEE000001", L"USB\\ROOT_HUB30\\1&2728EF4&4&0", true),
        // Nefarius ViGEm, NOT usbip-win2, with an old DS4 by Sony's own ids.
        rec(L"ROOT\\USB\\0000", L"HTREE\\ROOT\\0", true),
        rec(L"USB\\ROOT_HUB30\\1&2B53A856&1&0", L"ROOT\\USB\\0000", true),
        rec(L"USB\\VID_054C&PID_05C4&REV_0100\\1", L"USB\\ROOT_HUB30\\1&2B53A856&1&0", false),
        // A DualSense once paired to the PC over Bluetooth.
        rec(L"BTHENUM\\DEV_A0FA9CEF9B30\\7&1", L"BTH\\MS_BTHBRB\\6&1", false),
        // A record whose parent Windows no longer has, and two that loop.
        rec(L"USB\\VID_1532&PID_0094\\ORPHAN", L"USB\\ROOT_HUB30\\GONE", false),
        rec(L"X\\LOOP\\A", L"X\\LOOP\\B", false),
        rec(L"X\\LOOP\\B", L"X\\LOOP\\A", false),
    };
}

}  // namespace

int run_old_devices_rule_tests()
{
    const std::vector<odr::Record> pc = a_pc();
    const std::vector<odr::Removal> removals = odr::to_remove(pc);

    section("old devices: every old record of a bridge goes");
    {
        CTM_CHECK(has(removals, L"USB\\VID_054C&PID_0CE6\\A0FA9CEF9B30"));
        CTM_CHECK(has(removals, L"USB\\VID_054C&PID_0CE6&MI_00\\8&1"));
        CTM_CHECK(has(removals, L"USB\\VID_054C&PID_0CE6&MI_03\\8&2"));
        CTM_CHECK(has(removals, L"HID\\VID_054C&PID_0CE6&MI_03\\9&3"));
        CTM_CHECK(has(removals, L"SWD\\MMDEVAPI\\{0.0.0.00000000}.{AB}"));
        CTM_CHECK(has(removals, L"HID\\VID_1D6B&PID_0002\\OLD"));    // an old part of a connected device
        CTM_CHECK_EQ(removals.size(), static_cast<size_t>(6));
    }

    section("old devices: nothing connected, and not the hub or the controller");
    {
        CTM_CHECK(!has(removals, L"ROOT\\USB\\0001"));
        CTM_CHECK(!has(removals, L"USB\\ROOT_HUB30\\1&2728EF4&4&0"));
        CTM_CHECK(!has(removals, L"USB\\VID_1D6B&PID_0002\\CTM-GYRO-01"));
        CTM_CHECK(!has(removals, L"USB\\VID_054C&PID_0DF2\\C0FFEE000001"));
    }

    section("old devices: never another program's, nor a record that leads nowhere");
    {
        CTM_CHECK(!has(removals, L"USB\\VID_054C&PID_05C4&REV_0100\\1"));   // ViGEm's, Sony's ids
        CTM_CHECK(!has(removals, L"BTHENUM\\DEV_A0FA9CEF9B30\\7&1"));        // the PC's own Bluetooth
        CTM_CHECK(!has(removals, L"USB\\VID_1532&PID_0094\\ORPHAN"));
        CTM_CHECK(!has(removals, L"X\\LOOP\\A"));                            // and the loop ends
    }

    section("old devices: a part goes before the device it is part of");
    {
        const size_t device = position(removals, L"USB\\VID_054C&PID_0CE6\\A0FA9CEF9B30");
        CTM_CHECK(position(removals, L"USB\\VID_054C&PID_0CE6&MI_03\\8&2") < device);
        CTM_CHECK(position(removals, L"HID\\VID_054C&PID_0CE6&MI_03\\9&3") <
                  position(removals, L"USB\\VID_054C&PID_0CE6&MI_03\\8&2"));
        CTM_CHECK(position(removals, L"SWD\\MMDEVAPI\\{0.0.0.00000000}.{AB}") <
                  position(removals, L"USB\\VID_054C&PID_0CE6&MI_00\\8&1"));
    }

    section("old devices: counted as a person would, devices not parts");
    {
        CTM_CHECK_EQ(odr::devices_among(removals), static_cast<size_t>(1));   // the DualSense
    }

    section("old devices: without usbip-win2 nothing is ours");
    {
        std::vector<odr::Record> noUsbip = pc;
        for (odr::Record &r : noUsbip) r.usbipController = false;
        CTM_CHECK(odr::to_remove(noUsbip).empty());
    }
    return 0;
}
