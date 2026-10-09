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

}  // namespace front_rule
