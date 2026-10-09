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

    return 0;
}
