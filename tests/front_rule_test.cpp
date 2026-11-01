// The Controller Configs window and a full-screen game.
//
// ⭐ WHAT THESE PROTECT (rhoquinn8217, 2026-10-09): the window takes the front
// back only from a full-screen game -- not from the tray, whose menu it shut
// for seconds, nor the taskbar or an ordinary program -- and gives that game
// the front back as it closes, unless it did not have the front or a new
// window is on its way.
//
// WHAT THESE CANNOT DO. No window is made here. The rules are checked with
// the numbers and names a window would give; that the game really comes back
// is looked at with one running.

#include "harness.h"

#include "app/front_rule.inl"

using namespace ctmtest;

int run_front_rule_tests()
{
    using namespace front_rule;

    section("front rule: a window covering its monitor");
    {
        CTM_CHECK(covers(0, 0, 3840, 2160, 0, 0, 3840, 2160));        // full screen, exactly
        CTM_CHECK(covers(-8, -8, 3848, 2168, 0, 0, 3840, 2160));      // a frame over the edges
        CTM_CHECK(!covers(-8, -8, 3848, 2088, 0, 0, 3840, 2160));     // maximized: stops at the taskbar
        CTM_CHECK(!covers(100, 100, 1920, 1080, 0, 0, 3840, 2160));   // a window, not the screen
        CTM_CHECK(covers(3840, 0, 5760, 1080, 3840, 0, 5760, 1080));  // full screen on a second monitor
        CTM_CHECK(!covers(0, 0, 3840, 2160, 3840, 0, 5760, 1080));    // measured on the other one
    }

    section("front rule: a full-screen game, never the shell or the listener");
    {
        CTM_CHECK(is_full_screen_game(true, false, L"UnrealWindow"));
        CTM_CHECK(!is_full_screen_game(true, true, L"CtmTray"));          // the tray's own: the menu that shut
        CTM_CHECK(!is_full_screen_game(true, false, L"WorkerW"));         // the desktop covers its monitor
        CTM_CHECK(!is_full_screen_game(true, false, L"Progman"));
        CTM_CHECK(!is_full_screen_game(false, false, L"Shell_TrayWnd"));  // the taskbar
        CTM_CHECK(!is_full_screen_game(false, false, L"Chrome_WidgetWin_1")); // an ordinary window
        CTM_CHECK(is_shell_class(L"TopLevelWindowForOverflowXamlIsland"));
        CTM_CHECK(!is_shell_class(L"UnrealWindow"));
        CTM_CHECK(!is_shell_class(nullptr));
    }

    section("front rule: giving the front back as the window closes");
    {
        CTM_CHECK(give_front_back(true, true, true, false));    // over a game, had the front: back to it
        CTM_CHECK(!give_front_back(false, true, true, false));  // it was not raised over a game
        CTM_CHECK(!give_front_back(true, false, true, false));  // the game has gone
        CTM_CHECK(!give_front_back(true, true, false, false));  // closed from elsewhere: the tray, a mode change
        CTM_CHECK(!give_front_back(true, true, true, true));    // a new window is on its way
    }

    section("front rule: what is remembered as the window is raised");
    {
        CTM_CHECK(on_raise(true, true) == Remember::Set);      // over a game
        CTM_CHECK(on_raise(true, false) == Remember::Clear);   // over anything else: someone is at the desktop
        CTM_CHECK(on_raise(false, false) == Remember::Keep);   // our own window had it: mid-reopen
    }

    section("front rule: the last game someone was in, while the window is open");
    {
        CTM_CHECK(on_front_change(true) == Remember::Set);     // into another game: that one now
        CTM_CHECK(on_front_change(false) == Remember::Keep);   // an ordinary program: the game stays
    }

    // A work area of 3840 x 2088 at 0,0, the taskbar under it.
    section("front rule: a notification that took the front from the window");
    {
        // 400 x 120 at the bottom right, kept on top: a notification
        CTM_CHECK(looks_like_notification(3420, 1950, 3820, 2070, 0, 0, 3840, 2088,
                                          true, false, false, false, L"SomeToast"));
        // at the top right, out of the taskbar
        CTM_CHECK(looks_like_notification(3420, 20, 3820, 140, 0, 0, 3840, 2088,
                                          false, true, false, false, L"SomeToast"));
        // a little past the work area's edge, over the taskbar
        CTM_CHECK(looks_like_notification(3460, 1990, 3860, 2110, 0, 0, 3840, 2088,
                                          true, true, false, false, L"SomeToast"));
        // on a second monitor, in its own bottom right
        CTM_CHECK(looks_like_notification(5340, 900, 5740, 1060, 3840, 0, 5760, 1040,
                                          true, false, false, false, L"SomeToast"));
        // under the pointer: clicked, so chosen
        CTM_CHECK(!looks_like_notification(3420, 1950, 3820, 2070, 0, 0, 3840, 2088,
                                           true, false, true, false, L"SomeToast"));
        // an ordinary window someone put in the corner
        CTM_CHECK(!looks_like_notification(3420, 1950, 3820, 2070, 0, 0, 3840, 2088,
                                           false, false, false, false, L"Notepad"));
        // mid-screen: a dialog, not a notification
        CTM_CHECK(!looks_like_notification(1700, 900, 2100, 1020, 0, 0, 3840, 2088,
                                           true, false, false, false, L"#32770"));
        // half the screen in the corner: not small
        CTM_CHECK(!looks_like_notification(1800, 900, 3840, 2088, 0, 0, 3840, 2088,
                                           true, false, false, false, L"SomeApp"));
        // the tray's overflow, the shell's own, in that very corner
        CTM_CHECK(!looks_like_notification(3500, 1900, 3840, 2088, 0, 0, 3840, 2088,
                                           true, true, false, false, L"NotifyIconOverflowWindow"));
        // the listener's own window
        CTM_CHECK(!looks_like_notification(3420, 1950, 3820, 2070, 0, 0, 3840, 2088,
                                           true, true, false, true, L"CtmOverlay"));
        // and three times in ten seconds is the end of it
        CTM_CHECK(may_take_front_back(0));
        CTM_CHECK(may_take_front_back(2));
        CTM_CHECK(!may_take_front_back(3));
    }

    return 0;
}
