// The Controller Configs window and a full-screen game: when the window takes
// the front back from one, and when it gives the front back as it closes.
//
// ⭐ rhoquinn8217, 2026-10-09: *"I still want the config window to pull focus
// on full screen mode however I want to restore focus on the full screen mode
// game when the configs window closes."*
// ⛔ THE FAULT IT ENDS: after the window opened, the listener raised it again
// whenever the window that had had the front took it back -- written for a
// full-screen game (2026-10-03, The Witcher 3) -- and opened from the tray,
// that window was the tray's own, so opening the tray's menu again raised the
// window over it and the menu snapped shut, for seconds. A click on any other
// program in those seconds was overridden the same way.
// ➡️ So only a full-screen game is watched, and when the window closes with
// the front, the game it was raised over gets the front back.
//
// ⓘ Its own file, with no windows.h, so the test binary can reach the rules;
// open_ui.inl reads the windows they are asked about.
#pragma once

#include <cwchar>

namespace front_rule {

// A window's rectangle covers its monitor's. A full-screen game does, exclusive
// or borderless; a maximized ordinary window stops at the taskbar, and a window
// on another monitor is measured against that monitor.
inline bool covers(long wl, long wt, long wr, long wb, long ml, long mt, long mr, long mb)
{
    return wl <= ml && wt <= mt && wr >= mr && wb >= mb;
}

// The shell's own windows. ⛔ The desktop covers its monitor, and is no game.
inline bool is_shell_class(const wchar_t *cls)
{
    static const wchar_t *const kShell[] = {
        L"Progman", L"WorkerW",                                // the desktop
        L"Shell_TrayWnd", L"Shell_SecondaryTrayWnd",           // the taskbars
        L"NotifyIconOverflowWindow",                           // the tray's overflow
        L"TopLevelWindowForOverflowXamlIsland",                // ... on Windows 11
        L"Windows.UI.Core.CoreWindow",                         // Start, search
    };
    if (cls == nullptr) return false;
    for (const wchar_t *shell : kShell) {
        if (std::wcscmp(cls, shell) == 0) return true;
    }
    return false;
}

// The window that had the front is a full-screen game: it covers its monitor,
// it is not the shell's, and it is not the listener's own -- the tray's window
// above all, whose menu this used to shut.
inline bool is_full_screen_game(bool coversMonitor, bool listenersOwn, const wchar_t *cls)
{
    return coversMonitor && !listenersOwn && !is_shell_class(cls);
}

// Give the game the front back as the window closes: it was raised over one,
// the game is still there, the window had the front as it went (a Close, a
// Circle, its X) and no new window is on its way -- a bridge or the tray
// rebuilding it raises over the game again, and handing the front over in
// between would only flicker.
inline bool give_front_back(bool raisedOverGame, bool gameStillThere, bool hadFront, bool reopening)
{
    return raisedOverGame && gameStillThere && hadFront && !reopening;
}

// What the remembered game becomes as the window is raised again: a full-screen
// game replaces it; any other window clears it, since someone is at the
// desktop; not knowing what had the front -- our own window, mid-reopen --
// keeps it.
enum class Remember { Set, Clear, Keep };
inline Remember on_raise(bool frontKnown, bool frontIsGame)
{
    if (!frontKnown) return Remember::Keep;
    return frontIsGame ? Remember::Set : Remember::Clear;
}

// ⭐ AND WHILE THE WINDOW IS OPEN, THE LAST GAME SOMEONE WAS IN (rhoquinn8217,
// 2026-10-09: *"follow the last game I was in"*). The front moving to a
// full-screen game makes that game the one to give the front back to: open
// the window over one game, Alt+Tab into another, come back and close it, and
// the front goes to the second. Any other window leaves it be -- a click on
// an ordinary program is not a game someone was in.
inline Remember on_front_change(bool frontIsGame)
{
    return frontIsGame ? Remember::Set : Remember::Keep;
}

// ⭐⭐ A NOTIFICATION THAT TAKES THE FRONT FROM THE WINDOW GIVES IT BACK
// (rhoquinn8217, 2026-10-22: *"sometimes when there is a bottom right
// notification like samsung magic driver manager notification, it steal focus
// from the config window even if it's on top"*, then *"yes take it back, with
// log"*). The window stays drawn on top while the notification has the
// keyboard, so the pad went to the game until the window was clicked.
// ➡️ A notification by its shape: small beside the work area of its monitor
// (two fifths of it at most, either way), in one of its corners, kept above
// other windows or out of the taskbar -- and not under the pointer, since a
// click on it is someone choosing it. Never the shell's, whose tray overflow
// sits in that same corner, and never the listener's own.
inline bool looks_like_notification(long wl, long wt, long wr, long wb,
                                    long al, long at, long ar, long ab,
                                    bool topmost, bool toolWindow, bool underPointer,
                                    bool listenersOwn, const wchar_t *cls)
{
    if (underPointer || listenersOwn || is_shell_class(cls)) return false;
    if (!topmost && !toolWindow) return false;
    const long aw = ar - al, ah = ab - at, w = wr - wl, h = wb - wt;
    if (aw <= 0 || ah <= 0 || w <= 0 || h <= 0) return false;
    if (w * 5 > aw * 2 || h * 5 > ah * 2) return false;       // small
    const long nearX = aw / 8, nearY = ah / 5;                 // how near a corner counts
    const bool side = ar - wr <= nearX || wl - al <= nearX;
    const bool end = ab - wb <= nearY || wt - at <= nearY;
    return side && end;
}

// ⓘ And only so often: a notification that takes the front again at once is
// fighting for it, and three times in ten seconds ends the taking back.
inline bool may_take_front_back(int takenBackInTenSeconds)
{
    return takenBackInTenSeconds < 3;
}

}  // namespace front_rule
