// The words on the tray icon's menu.
//
// ⭐ WHAT THESE PROTECT. The menu says how many controllers are connected,
// lists each one, and marks which layout the config window is in. Each of
// those is a small rule with an edge: one controller is not "1 controllers",
// a pad that has not reported its battery must not read as an empty one, and
// the padlock belongs on one mode or on none. The hints on the settings page
// and the tool that quits the listener both quote these words, so the words
// themselves are pinned too.
//
// WHAT THESE CANNOT DO. They draw nothing and open no menu. That the title is
// larger, that the lines come in this order and that choosing one does what
// it says is looked at with the listener running.

#include "harness.h"

#include <string>
#include <vector>

#include "app/tray_menu.inl"
#include "app/open_on_bridge.inl"

using namespace ctmtest;

namespace {

std::string narrow(const std::wstring &w)
{
    std::string out;
    for (wchar_t c : w) out += (c < 128) ? static_cast<char>(c) : '?';
    return out;
}

} // namespace

int run_tray_menu_tests()
{
    section("open on bridge: kept in window-state.txt, on unless it says 0");
    {
        CTM_CHECK(open_on_bridge::parse("1"));
        CTM_CHECK(!open_on_bridge::parse("0"));
        CTM_CHECK(!open_on_bridge::parse(""));      // junk reads as off, the default since 2026-10-15
        CTM_CHECK(!open_on_bridge::parse("yes"));
        CTM_CHECK_EQ(open_on_bridge::line(true), std::string("open_on_bridge = 1\n"));
        CTM_CHECK_EQ(open_on_bridge::line(false), std::string("open_on_bridge = 0\n"));
        CTM_CHECK_EQ(open_on_bridge::default_line(), std::string("open_on_bridge_default = off\n"));
        // ⭐ Off by default, and for every install once (2026-10-15).
        CTM_CHECK(!open_on_bridge::kDefault);
        CTM_CHECK(!open_on_bridge::loaded(false, false, false));   // no file, or no line
        CTM_CHECK(!open_on_bridge::loaded(true, true, false));     // "on" from before the change: dropped
        CTM_CHECK(open_on_bridge::loaded(true, true, true));       // turned on since: kept
        CTM_CHECK(!open_on_bridge::loaded(true, false, true));
        CTM_CHECK(!open_on_bridge::loaded(false, false, true));
    }

    section("open on bridge: whether a bridge opens the window");
    {
        using open_on_bridge::opens_window;
        CTM_CHECK(opens_window(true, true, false));     // on, not in front: it opens
        CTM_CHECK(!opens_window(true, false, false));   // switched off: never
        CTM_CHECK(!opens_window(true, true, true));     // already in front: left alone
        CTM_CHECK(!opens_window(false, true, false));   // no --ui: no window at all
    }

    section("open on bridge: a stream's start opens no window, and a bridge still does");
    {
        // ⓘ rhoquinn8217, 2026-10-12: no Controller Configs window at a stream's
        // start; on a bridge, unless "Open on bridge" is off.
        using open_on_bridge::opens_at_start;
        using open_on_bridge::opens_window;
        CTM_CHECK(opens_at_start(true, false, false));     // a double-click: the window
        CTM_CHECK(!opens_at_start(true, true, false));     // Vibepollo's start: none
        CTM_CHECK(!opens_at_start(true, false, true));     // one already brought forward
        CTM_CHECK(!opens_at_start(false, false, false));   // no --ui: no window at all
        CTM_CHECK(opens_window(true, true, false));        // then a bridge, the switch on: it opens
        CTM_CHECK(!opens_window(true, false, false));      // ... and with the switch off: it does not
    }

    section("tray menu: picking a mode in Window Mode");
    {
        using tray_menu::mode_pick;
        using tray_menu::ModePick;
        CTM_CHECK(mode_pick(1, 0, true) == ModePick::ChangeLayout);    // another mode, window open
        CTM_CHECK(mode_pick(1, 0, false) == ModePick::ChangeLayout);   // another mode, none open
        CTM_CHECK(mode_pick(2, 2, true) == ModePick::BringForward);    // the one it is in: forward
        CTM_CHECK(mode_pick(2, 2, false) == ModePick::Nothing);        // ... and none open: as before
        CTM_CHECK(mode_pick(0, -1, true) == ModePick::ChangeLayout);   // no layout known yet
    }

    section("tray menu: the count under the title");
    {
        using tray_menu::count_line;
        CTM_CHECK_EQ(narrow(count_line(0)), std::string("No devices connected"));
        // ⛔ One is not "1 devices".
        CTM_CHECK_EQ(narrow(count_line(1)), std::string("1 device connected"));
        CTM_CHECK_EQ(narrow(count_line(2)), std::string("2 devices connected"));
        CTM_CHECK_EQ(narrow(count_line(11)), std::string("11 devices connected"));
    }

    section("tray menu: the parts under one name are one device");
    {
        using tray_menu::group_by_name;
        const std::vector<std::vector<size_t>> g =
            group_by_name({ "Odin", "Titan", "Odin", "", "", "Odin" });
        CTM_CHECK(g.size() == 4);
        CTM_CHECK(g.size() == 4 && g[0] == (std::vector<size_t>{ 0, 2, 5 }));
        CTM_CHECK(g.size() == 4 && g[1] == (std::vector<size_t>{ 1 }));
        // ⓘ No name is no identity: each nameless part is a device of its own.
        CTM_CHECK(g.size() == 4 && g[2] == (std::vector<size_t>{ 3 }));
        CTM_CHECK(g.size() == 4 && g[3] == (std::vector<size_t>{ 4 }));
        CTM_CHECK(group_by_name({}).empty());
    }

    section("tray menu: a device's line");
    {
        using tray_menu::device_line;
        // ⓘ The device's name first, then the nickname (2026-10-02).
        CTM_CHECK_EQ(device_line("Kestrel", "DualSense (USB)", 85, "discharging"),
                     std::string("DualSense (USB) - Kestrel - 85%"));
        CTM_CHECK_EQ(device_line("Combo", "DualSense (BT)", 40, "charging"),
                     std::string("DualSense (BT) - Combo - 40%, charging"));
        // ⛔ A pad that has not said is not a pad at zero: no battery at all.
        CTM_CHECK_EQ(device_line("Rhino", "Xbox Controller", -1, ""),
                     std::string("Xbox Controller - Rhino"));
        // ⓘ And a pad that really is at zero says so.
        CTM_CHECK_EQ(device_line("Rhino", "DualSense (BT)", 0, "discharging"),
                     std::string("DualSense (BT) - Rhino - 0%"));
        CTM_CHECK_EQ(device_line("Zeus", "DualSense (USB)", 100, "full"),
                     std::string("DualSense (USB) - Zeus - 100%"));
        // A session that is still starting may have no nickname yet.
        CTM_CHECK_EQ(device_line("", "DualSense (USB)", 85, ""),
                     std::string("DualSense (USB) - 85%"));
        CTM_CHECK_EQ(device_line("Kestrel", "", -1, ""), std::string("Kestrel"));
    }

    section("tray menu: what a pad said about its battery");
    {
        using tray_menu::battery_words;
        CTM_CHECK(battery_words(-1, "").empty());
        CTM_CHECK(battery_words(-1, "charging").empty());
        CTM_CHECK_EQ(battery_words(0, ""), std::string("0%"));
        CTM_CHECK_EQ(battery_words(55, "discharging"), std::string("55%"));
        CTM_CHECK_EQ(battery_words(55, "charging"), std::string("55%, charging"));
        // ⓘ "full" is said by the number.
        CTM_CHECK_EQ(battery_words(100, "full"), std::string("100%"));
        // A reading past the top is the top.
        CTM_CHECK_EQ(battery_words(130, ""), std::string("100%"));
    }

    section("tray menu: the keyboard's line says what choosing it will do");
    {
        CTM_CHECK_EQ(narrow(tray_menu::keyboard_line(false)), std::string("Open Virtual Keyboard"));
        CTM_CHECK_EQ(narrow(tray_menu::keyboard_line(true)), std::string("Close Virtual Keyboard"));
    }

    section("tray menu: which mode the window is in");
    {
        using tray_menu::mode_index;
        CTM_CHECK_EQ(mode_index(true, false, false), 0);     // Advanced
        CTM_CHECK_EQ(mode_index(true, true, false), 1);      // Simple
        CTM_CHECK_EQ(mode_index(true, true, true), 2);       // Quick
        // ⓘ "quick" means nothing unless the window is compact.
        CTM_CHECK_EQ(mode_index(true, false, true), 0);
        // ⛔ A listener that has not been told has no mode to mark.
        CTM_CHECK_EQ(mode_index(false, false, false), -1);
        CTM_CHECK_EQ(mode_index(false, true, true), -1);
        // The table says the same thing the other way round.
        for (int i = 0; i < tray_menu::kModeCount; ++i) {
            CTM_CHECK_EQ(mode_index(true, tray_menu::kModes[i].compact, tray_menu::kModes[i].quick), i);
        }
    }

    section("tray menu: the padlock is on the mode the window is in, and on no other");
    {
        using tray_menu::mode_line;
        const std::wstring lock = tray_menu::kPadlock;
        // ⓘ One code point above the first plane: two wchar_t on Windows.
        CTM_CHECK_EQ(static_cast<int>(lock.size()), 2);
        CTM_CHECK(lock[0] == 0xD83D && lock[1] == 0xDD12);

        CTM_CHECK(mode_line(0, 1) == L"Advanced");
        CTM_CHECK(mode_line(1, 1) == std::wstring(L"Simple ") + lock);
        CTM_CHECK(mode_line(2, 1) == L"Quick");
        // Not told: no padlock anywhere.
        for (int i = 0; i < tray_menu::kModeCount; ++i) {
            CTM_CHECK(mode_line(i, -1).find(lock) == std::wstring::npos);
        }
        // Exactly one line carries it, whichever mode it is.
        for (int current = 0; current < tray_menu::kModeCount; ++current) {
            int marked = 0;
            for (int i = 0; i < tray_menu::kModeCount; ++i) {
                if (mode_line(i, current).find(lock) != std::wstring::npos) ++marked;
            }
            CTM_CHECK_EQ(marked, 1);
        }
        CTM_CHECK(mode_line(-1, 0).empty());
        CTM_CHECK(mode_line(3, 0).empty());
    }

    section("tray menu: the words other things quote");
    {
        // ⛔ The settings page's hints name the first of these, and the tool
        // every session quits the listener with finds the last by its words.
        CTM_CHECK_EQ(narrow(tray_menu::kOpenConfig), std::string("Open Controller Configs"));
        // ⓘ The swap tool opens this side menu by its words.
        CTM_CHECK_EQ(narrow(tray_menu::kDevices), std::string("Devices"));
        CTM_CHECK_EQ(narrow(tray_menu::kConfigMode), std::string("Window Mode"));
        // ⓘ The settings page's switch says the same words.
        CTM_CHECK_EQ(narrow(tray_menu::kOpenOnBridge), std::string("Open on bridge"));
        CTM_CHECK_EQ(narrow(tray_menu::kOpenFolder), std::string("Open configs folder"));
        CTM_CHECK_EQ(narrow(tray_menu::kTitle), std::string("DS5-USBIP"));
        CTM_CHECK_EQ(narrow(tray_menu::kQuit), std::string("Quit"));
    }

    return 0;
}
