// The words on the tray icon's menu, and the order they come in.
//
// ⭐ WHAT THE MENU IS (rhoquinn8217, 2026-10-02, in their order, with what has
// come and gone since):
//
//     DS5-USBIP v0.2.0 (N)           a title, larger and bold, the running
//                                    build after it; choosing it
//     2 devices connected            closes the menu. A count under it
//     ----------------------------
//     Devices                    >   one line per device; choosing one opens
//     ----------------------------   the Controller Configs window on it
//     Open Controller Configs
//     Open Virtual Keyboard
//     Open configs folder
//     ----------------------------
//     Start Tutorial             >   Simple, Quick and Advanced mode's tours
//     Remove old controllers...      (2026-10-13, below)
//     ----------------------------
//     Quit
//
// ⓘ Quit was not in the order they gave. It is the only way the listener is
// closed, so it stays, last, under a break of its own.
//
// ⭐ THE WINDOW IS "CONTROLLER CONFIGS" (rhoquinn8217, 2026-10-03). "Config"
// meant the window, the saved set a pad is linked to and the folder at once;
// a config stays a config, and the window that holds them is named for them.
// ⓘ In code and in the log its layout is called the VIEW or the mode of the
// window.
//
// ⭐ NO WINDOW MODE AND NO OPEN ON BRIDGE (rhoquinn8217, 2026-10-30: "remove
// the "window mode" and "open on bridge option" from the tray icon menu").
// Both stay in the window: its Mode picker, and Bridging Options on Advanced's
// Overview.
//
// ⭐ DEVICES, NOT CONTROLLERS, AND NOT THEIR PARTS (rhoquinn8217, 2026-10-02:
// *"Multiple devices with the same name should count as 1 device in the tray
// menu and there should only be 1 controller entry for that device"* and *"in
// the tray menu change controllers to devices."*). A keyboard-and-mouse
// receiver bridged as three parts was three lines and three in the count.
//
// ⭐ Pure on purpose -- no Windows call -- so the test binary includes it as it
// is. tray_icon.inl builds the menu from these and does the drawing.

#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "ctm/product.h"

namespace tray_menu {

inline const wchar_t *const kTitle = L"DS5-USBIP";
// ⭐ WHICH BUILD IS RUNNING, BESIDE THE TITLE (rhoquinn8217, 2026-10-15: "I
// want the version number next to the DS5-USBIP title in the menu tray";
// with the v, as the Controller Configs window's title has it, and smaller
// or lighter than the name). From product.h, as the tooltip's is, so the
// two can never disagree.
inline const wchar_t *const kVersion = L"v" PRODUCT_VERSION_DISPLAY;
inline const wchar_t *const kDevices = L"Devices";
inline const wchar_t *const kOpenConfig = L"Open Controller Configs";
// ⭐ Where a person's configs are (rhoquinn8217, 2026-10-12): their own folder
// now, not the program's, so the menu goes there.
inline const wchar_t *const kOpenFolder = L"Open configs folder";
inline const wchar_t *const kQuit = L"Quit";
// ⭐ REMOVE OLD CONTROLLERS (rhoquinn8217, 2026-10-13: place B, a group of its
// own above Quit). Windows keeps a record of every controller a bridge ever
// plugged in, and this removes the ones not connected (old_devices.inl). The
// three dots are the Windows convention for a line that asks something before
// it acts: here Windows' own administrator prompt. ⓘ By code point, so no
// editor and no compiler setting can turn it into something else on the way to
// the screen.
inline const wchar_t *const kRemoveOld = L"Remove old controllers\u2026";
// ⭐ START TUTORIAL (rhoquinn8217, 2026-10-14: *"put it in the tray menu"*;
// 2026-10-30: "change "Show tutorial" to "Start Tutorial""), beside Remove old
// controllers: the Controller Configs window's tutorials, listed under it
// (kTutorials), each of which starts by itself only once.
inline const wchar_t *const kStartTutorial = L"Start Tutorial";

// The line under the title. ⓘ It counts the lines the Devices list will show,
// so the two cannot disagree.
inline std::wstring count_line(size_t devices)
{
    if (devices == 0) return L"No devices connected";
    if (devices == 1) return L"1 device connected";
    return std::to_wstring(devices) + L" devices connected";
}

// ⭐ ONE DEVICE, HOWEVER MANY PARTS. The listener gives every part of one
// physical device one nickname (same_device.inl), and the settings page puts
// the parts under one name on one tab; the menu groups by the same thing, so
// the two always agree on what a device is. A part with no name is a device
// of its own: no name is no identity.
// Returns the devices in the order each first appears, each as the indices of
// its parts in `nicknames`.
inline std::vector<std::vector<size_t>> group_by_name(const std::vector<std::string> &nicknames)
{
    std::vector<std::vector<size_t>> groups;
    for (size_t i = 0; i < nicknames.size(); ++i) {
        bool joined = false;
        if (!nicknames[i].empty()) {
            for (std::vector<size_t> &g : groups) {
                if (nicknames[g.front()] == nicknames[i]) {
                    g.push_back(i);
                    joined = true;
                    break;
                }
            }
        }
        if (!joined) groups.push_back(std::vector<size_t>{ i });
    }
    return groups;
}

// The keyboard's line says what choosing it will DO, so it changes with the
// keyboard: there is one line, not two.
inline const wchar_t *keyboard_line(bool keyboardIsOpen)
{
    return keyboardIsOpen ? L"Close Virtual Keyboard" : L"Open Virtual Keyboard";
}

// What a pad last said about its charge, or "" when it has said nothing.
// ⛔ A percent below zero is "the pad did not say", which is not an empty
// battery: the line then leaves the battery out altogether.
inline std::string battery_words(int percent, const std::string &state)
{
    if (percent < 0) return std::string();
    if (percent > 100) percent = 100;
    std::string words = std::to_string(percent) + "%";
    if (state == "charging") words += ", charging";
    return words;
}

// One device's line: what it is and how it is connected, then its nickname,
// and its battery when it has said. "DualSense (USB) - Kestrel - 85%"
// (rhoquinn8217, 2026-10-02: *"put Device name first then then the
// nickname"*; it had led with the nickname).
// ⓘ `label` is device_names::label(), which already carries USB or BT.
inline std::string device_line(const std::string &nickname, const std::string &label,
                               int batteryPercent, const std::string &batteryState)
{
    std::string line = label;
    if (!nickname.empty()) line += (line.empty() ? "" : " - ") + nickname;
    const std::string battery = battery_words(batteryPercent, batteryState);
    if (!battery.empty()) line += (line.empty() ? "" : " - ") + battery;
    return line;
}

// The three layouts of the config window, in the order the page lists them.
struct Mode {
    const wchar_t *name;
    bool compact;
    bool quick;
};
inline constexpr Mode kModes[3] = {
    { L"Advanced", false, false },
    { L"Simple",   true,  false },
    { L"Quick",    true,  true  },
};
inline constexpr int kModeCount = 3;

// ⭐ THE LIST UNDER START TUTORIAL (rhoquinn8217, 2026-10-26: *"put a list of
// tutorials in the tray for now"*): each tours one layout, which the window
// goes to as it starts. `mode` is its place in kModes, `kind` the page's name
// for it (tutorial_state::Kind: 1 Simple, 2 Quick, 3 Advanced). ⓘ Advanced's
// since 2026-10-29, last, as the tours were made.
struct Tutorial {
    const wchar_t *name;
    int mode;
    int kind;
};
inline constexpr Tutorial kTutorials[3] = {
    { L"Simple mode",   1, 1 },
    { L"Quick mode",    2, 2 },
    { L"Advanced mode", 0, 3 },
};
inline constexpr int kTutorialCount = 3;

// Which of the three the window is in, or -1 when the listener has not been
// told: it has just started for the first time and no window has opened yet.
inline int mode_index(bool known, bool compact, bool quick)
{
    if (!known) return -1;
    if (!compact) return 0;
    return quick ? 2 : 1;
}

// ⭐ WHAT ASKING FOR A LAYOUT FROM THE MENU DOES (rhoquinn8217, 2026-10-09: "if
// you select a window mode in the task bar that is already set, the window
// should come in focus", of the Window Mode list the menu had until
// 2026-10-30; Start Tutorial's list asks the same way). Another mode changes
// the layout. The mode the window is already in brings an open window forward
// -- it did nothing at all -- and with no window open still does nothing (their
// word, the same day: "leave it as it is").
enum class ModePick { ChangeLayout, BringForward, Nothing };
inline ModePick mode_pick(int picked, int current, bool windowOpen)
{
    if (picked != current) return ModePick::ChangeLayout;
    return windowOpen ? ModePick::BringForward : ModePick::Nothing;
}

} // namespace tray_menu
