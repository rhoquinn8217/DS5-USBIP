// The words on the tray icon's menu.
//
// ⭐ WHAT THESE PROTECT. The menu says how many controllers are connected and
// lists each one. Each of those is a small rule with an edge: one controller is
// not "1 controllers", and a pad that has not reported its battery must not
// read as an empty one. The hints on the settings page and the tool that quits
// the listener both quote these words, so the words themselves are pinned too.
//
// WHAT THESE CANNOT DO. They draw nothing and open no menu. That the title is
// larger, that the lines come in this order and that choosing one does what
// it says is looked at with the listener running.

#include "harness.h"

#include <algorithm>
#include <string>
#include <vector>

#include "app/tray_menu.inl"
#include "app/open_on_bridge.inl"
#include "app/tutorial_state.inl"

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
    section("tutorial: seen only when window-state.txt says 1, and what the page is told");
    {
        CTM_CHECK(tutorial_state::parse_seen("1"));
        CTM_CHECK(!tutorial_state::parse_seen("0"));
        CTM_CHECK(!tutorial_state::parse_seen(""));      // a file from before: shown once more
        CTM_CHECK(!tutorial_state::parse_seen("yes"));   // junk: the same
        CTM_CHECK_EQ(tutorial_state::seen_line(tutorial_state::kSimpleSeenKey, true),
                     std::string("tutorial_simple_seen = 1\n"));
        CTM_CHECK_EQ(tutorial_state::seen_line(tutorial_state::kQuickSeenKey, false),
                     std::string("tutorial_quick_seen = 0\n"));
        CTM_CHECK_EQ(tutorial_state::seen_line(tutorial_state::kAdvancedSeenKey, true),
                     std::string("tutorial_advanced_seen = 1\n"));
        CTM_CHECK_EQ(tutorial_state::json(false, false, false, tutorial_state::None),
                     std::string(R"({"simple_seen":false,"quick_seen":false,"advanced_seen":false,"request":"","part":"","parts_done":""})"));
        CTM_CHECK_EQ(tutorial_state::json(true, false, false, tutorial_state::Simple),
                     std::string(R"({"simple_seen":true,"quick_seen":false,"advanced_seen":false,"request":"simple","part":"","parts_done":""})"));
        CTM_CHECK_EQ(tutorial_state::json(true, true, false, tutorial_state::Quick),
                     std::string(R"({"simple_seen":true,"quick_seen":true,"advanced_seen":false,"request":"quick","part":"","parts_done":""})"));
        CTM_CHECK_EQ(tutorial_state::json(true, true, true, tutorial_state::Advanced),
                     std::string(R"({"simple_seen":true,"quick_seen":true,"advanced_seen":true,"request":"advanced","part":"","parts_done":""})"));
        CTM_CHECK_EQ(tutorial_state::kind_of("simple"), int(tutorial_state::Simple));
        CTM_CHECK_EQ(tutorial_state::kind_of("quick"), int(tutorial_state::Quick));
        CTM_CHECK_EQ(tutorial_state::kind_of("advanced"), int(tutorial_state::Advanced));
        CTM_CHECK_EQ(tutorial_state::kind_of("expert"), int(tutorial_state::None));
        CTM_CHECK_EQ(narrow(tray_menu::kStartTutorial), std::string("Start Tutorial"));
    }

    section("tutorial: the tray's list, each tour going to the layout it is named for");
    {
        CTM_CHECK_EQ(tray_menu::kTutorialCount, 3);
        // ⓘ Under Mode, in kModes' order, by the mode's name (2026-11-13).
        CTM_CHECK_EQ(narrow(tray_menu::kTutorials[0].name), std::string("Advanced"));
        CTM_CHECK_EQ(narrow(tray_menu::kTutorials[1].name), std::string("Simple"));
        CTM_CHECK_EQ(narrow(tray_menu::kTutorials[2].name), std::string("Quick"));
        for (int i = 0; i < tray_menu::kTutorialCount; ++i) {
            const tray_menu::Tutorial &t = tray_menu::kTutorials[i];
            CTM_CHECK(t.mode >= 0 && t.mode < tray_menu::kModeCount);
            CTM_CHECK(narrow(t.name).rfind(narrow(tray_menu::kModes[t.mode].name), 0) == 0);
            CTM_CHECK_EQ(std::string(tutorial_state::kind_name(t.kind)),
                         t.mode == 0 ? std::string("advanced")
                                     : t.mode == 1 ? std::string("simple") : std::string("quick"));
        }
    }

    section("tutorial: the parts, each once, the tours that have them and each tour's order");
    {
        CTM_CHECK_EQ(tutorial_state::kPartCount, 8);
        const char *ids[] = { "modes", "controllers", "making", "managing", "window", "switch",
                              "overview", "customizing" };
        const char *names[] = { "Switching modes", "Your controllers", "Making configs", "Managing configs",
                                "The window", "Quick switch", "The Overview and tabs", "Customizing configs" };
        for (int i = 0; i < tutorial_state::kPartCount; ++i) {
            CTM_CHECK_EQ(std::string(tutorial_state::kParts[i].id), std::string(ids[i]));
            CTM_CHECK_EQ(narrow(tutorial_state::kParts[i].name), std::string(names[i]));
            CTM_CHECK_EQ(tutorial_state::part_index(ids[i]), i);
            // ⓘ No name twice: Topic lists each once ("I said not to have duplicates").
            for (int j = 0; j < i; ++j) CTM_CHECK(narrow(tutorial_state::kParts[j].name) != std::string(names[i]));
        }
        CTM_CHECK_EQ(tutorial_state::part_index("expert"), -1);
        CTM_CHECK(tutorial_state::tour_parts(tutorial_state::Simple) == std::vector<int>({ 0, 1, 2, 3, 4, 5 }));
        CTM_CHECK(tutorial_state::tour_parts(tutorial_state::Quick) == std::vector<int>({ 0, 1, 3, 4, 5 }));
        CTM_CHECK(tutorial_state::tour_parts(tutorial_state::Advanced) == std::vector<int>({ 0, 6, 2, 7, 3, 4 }));
        CTM_CHECK(tutorial_state::tour_parts(tutorial_state::None).empty());
        // ⓘ A tour lists a part exactly when the part says that tour has it.
        for (int kind = tutorial_state::Simple; kind <= tutorial_state::Advanced; ++kind) {
            const std::vector<int> order = tutorial_state::tour_parts(kind);
            for (int i = 0; i < tutorial_state::kPartCount; ++i) {
                const bool listed = std::find(order.begin(), order.end(), i) != order.end();
                CTM_CHECK_EQ(listed, tutorial_state::kParts[i].tours[kind]);
            }
        }
    }

    section("tutorial: the parts done, kept in window-state.txt by their ids, and told to the page");
    {
        CTM_CHECK_EQ(tutorial_state::parse_parts(""), 0u);
        CTM_CHECK_EQ(tutorial_state::parse_parts("modes,switch"), (1u << 0) | (1u << 5));
        CTM_CHECK_EQ(tutorial_state::parse_parts(" switch , modes "), (1u << 0) | (1u << 5));
        CTM_CHECK_EQ(tutorial_state::parse_parts("modes,expert,,window"), (1u << 0) | (1u << 4));   // junk left out
        CTM_CHECK_EQ(tutorial_state::parts_list((1u << 5) | (1u << 0)), std::string("modes,switch"));
        CTM_CHECK_EQ(tutorial_state::parts_list(0), std::string(""));
        CTM_CHECK_EQ(tutorial_state::parts_line((1u << 7) | (1u << 6)),
                     std::string("tutorial_parts_done = overview,customizing\n"));
        CTM_CHECK_EQ(tutorial_state::parts_line(0), std::string("tutorial_parts_done = \n"));
        const uint32_t all = (1u << tutorial_state::kPartCount) - 1;
        CTM_CHECK_EQ(tutorial_state::parse_parts(tutorial_state::parts_list(all)), all);
        CTM_CHECK_EQ(tutorial_state::json(true, false, false, tutorial_state::Simple, 5, (1u << 0) | (1u << 5)),
                     std::string(R"({"simple_seen":true,"quick_seen":false,"advanced_seen":false,"request":"simple","part":"switch","parts_done":"modes,switch"})"));
        // ⓘ A part with no tour asked for is no request.
        CTM_CHECK_EQ(tutorial_state::json(false, false, false, tutorial_state::None, 3, 0),
                     std::string(R"({"simple_seen":false,"quick_seen":false,"advanced_seen":false,"request":"","part":"","parts_done":""})"));
    }

    section("tutorial: Topic's part runs in the mode the window is in when its tour has it");
    {
        using tutorial_state::kind_for_part;
        CTM_CHECK_EQ(kind_for_part(0, tutorial_state::Quick), int(tutorial_state::Quick));        // modes: all three
        CTM_CHECK_EQ(kind_for_part(0, tutorial_state::Advanced), int(tutorial_state::Advanced));
        CTM_CHECK_EQ(kind_for_part(1, tutorial_state::Advanced), int(tutorial_state::Simple));    // controllers: not Advanced
        CTM_CHECK_EQ(kind_for_part(2, tutorial_state::Quick), int(tutorial_state::Simple));       // making: not Quick
        CTM_CHECK_EQ(kind_for_part(5, tutorial_state::Quick), int(tutorial_state::Quick));        // switch
        CTM_CHECK_EQ(kind_for_part(5, tutorial_state::Advanced), int(tutorial_state::Simple));
        CTM_CHECK_EQ(kind_for_part(6, tutorial_state::Simple), int(tutorial_state::Advanced));    // overview: Advanced only
        CTM_CHECK_EQ(kind_for_part(7, tutorial_state::None), int(tutorial_state::Advanced));
        CTM_CHECK_EQ(kind_for_part(-1, tutorial_state::Simple), int(tutorial_state::None));
        CTM_CHECK_EQ(kind_for_part(8, tutorial_state::Simple), int(tutorial_state::None));
    }

    section("tutorial: the tray's ids for Mode's tours and Topic's parts, and back");
    {
        CTM_CHECK_EQ(narrow(tray_menu::kTutorialMode), std::string("Mode"));
        CTM_CHECK_EQ(narrow(tray_menu::kTutorialTopic), std::string("Topic"));
        const unsigned first = 300;
        std::vector<unsigned> seen;
        auto back = [&](int tour, int part) {
            const unsigned id = tray_menu::tutorial_id(first, tour, part);
            CTM_CHECK(std::find(seen.begin(), seen.end(), id) == seen.end());   // no id twice
            seen.push_back(id);
            tray_menu::TutorialChoice c;
            CTM_CHECK(tray_menu::tutorial_choice(first, id, &c));
            CTM_CHECK_EQ(c.tour, tour);
            CTM_CHECK_EQ(c.part, part);
        };
        for (int i = 0; i < tray_menu::kTutorialCount; ++i) back(i, -1);
        for (int part = 0; part < tutorial_state::kPartCount; ++part) back(-1, part);
        CTM_CHECK_EQ(seen.size(), size_t(3 + 8));
        CTM_CHECK(tray_menu::tutorial_id(first, 0, -1) == first);
        CTM_CHECK(!tray_menu::tutorial_choice(first, first - 1, nullptr));
        CTM_CHECK(!tray_menu::tutorial_choice(first, first + 3, nullptr));                        // past the tours
        CTM_CHECK(!tray_menu::tutorial_choice(first, first + tray_menu::kTopicIds - 1, nullptr));  // before the parts
        CTM_CHECK(!tray_menu::tutorial_choice(first, first + tray_menu::kTopicIds + 8, nullptr));  // past the parts
        // ⓘ Clear of the other ids the menu uses: the devices' block ends at 164.
        CTM_CHECK(tray_menu::tutorial_id(first, -1, tutorial_state::kPartCount - 1) < 400u);
    }

    section("tutorial: an install's stamp starts the tours again, and nothing else does");
    {
        CTM_CHECK(tutorial_state::fresh_install("2026-10-26 10:00:00.000", ""));        // the first install
        CTM_CHECK(tutorial_state::fresh_install("2026-10-27 09:00:00.000",
                                                "2026-10-26 10:00:00.000"));           // a reinstall
        CTM_CHECK(!tutorial_state::fresh_install("2026-10-26 10:00:00.000",
                                                 "2026-10-26 10:00:00.000"));          // already seen
        CTM_CHECK(!tutorial_state::fresh_install("", ""));                             // a build's own folder
        CTM_CHECK(!tutorial_state::fresh_install("", "2026-10-26 10:00:00.000"));
        CTM_CHECK_EQ(tutorial_state::stamp_from_file("2026-10-26 10:00:00.000\r\n"),
                     std::string("2026-10-26 10:00:00.000"));
        CTM_CHECK_EQ(tutorial_state::stamp_from_file("  stamp  \nmore"), std::string("stamp"));
        CTM_CHECK_EQ(tutorial_state::stamp_from_file(""), std::string(""));
    }

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

    section("tray menu: asking for a layout");
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
        // ⛔ A listener that has not been told has no mode.
        CTM_CHECK_EQ(mode_index(false, false, false), -1);
        CTM_CHECK_EQ(mode_index(false, true, true), -1);
        // The table says the same thing the other way round.
        for (int i = 0; i < tray_menu::kModeCount; ++i) {
            CTM_CHECK_EQ(mode_index(true, tray_menu::kModes[i].compact, tray_menu::kModes[i].quick), i);
        }
    }

    section("tray menu: the words other things quote");
    {
        // ⛔ The settings page's hints name the first of these, and the tool
        // every session quits the listener with finds the last by its words.
        CTM_CHECK_EQ(narrow(tray_menu::kOpenConfig), std::string("Open Controller Configs"));
        // ⓘ The swap tool opens this side menu by its words.
        CTM_CHECK_EQ(narrow(tray_menu::kDevices), std::string("Devices"));
        CTM_CHECK_EQ(narrow(tray_menu::kOpenFolder), std::string("Open configs folder"));
        CTM_CHECK_EQ(narrow(tray_menu::kTitle), std::string("DS5-USBIP"));
        // ⓘ The version beside it: a v, then the tooltip's own words for it,
        // as the Controller Configs window's title has them.
        CTM_CHECK_EQ(narrow(tray_menu::kVersion), std::string("v") + PRODUCT_VERSION_DISPLAY);
        CTM_CHECK(narrow(tray_menu::kVersion).rfind("v" PRODUCT_VERSION " (", 0) == 0);
        CTM_CHECK_EQ(narrow(tray_menu::kQuit), std::string("Quit"));
    }

    return 0;
}
