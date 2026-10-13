// When the taskbar should stay behind the Controller Configs window: a window
// that fills its screen is behind it, as a game in borderless window mode
// does.
//
// ⭐ WHY (rhoquinn8217, 2026-10-12: "when a game is in borderless window
// mode, I want to bring up the controller config window but I don't want the
// task bar to come up. Is it possible to keep the task bar behind the game
// while the controller config is in front of the game?"). Windows keeps the
// taskbar behind the ACTIVE window only while that window fills its screen.
// A borderless game does; the configs window, brought up over it, does not,
// so the taskbar came up with it. taskbar_back.inl tells the taskbar to
// treat the configs window as full-screen while it is over such a window.
//
// ⓘ Pure, so the tests can reach it: taskbar_back.inl hands it what it reads
// off the windows.

#pragma once

#include <cwchar>

#include "window_icon_rule.inl"

namespace taskbar_rule {

struct Box {
    long left;
    long top;
    long right;
    long bottom;
};

// A window fills its screen when it covers it edge to edge. A borderless game
// may reach past an edge; that still fills it.
inline bool fills(const Box &window, const Box &screen)
{
    return window.left <= screen.left && window.top <= screen.top &&
           window.right >= screen.right && window.bottom >= screen.bottom;
}

// Windows that fill a screen and are no game: the desktop behind every window
// (Progman, or WorkerW with a wallpaper slideshow) and the taskbars.
inline bool is_shell(const wchar_t *windowClass)
{
    const wchar_t *const shell[] = { L"Progman", L"WorkerW", L"Shell_TrayWnd", L"Shell_SecondaryTrayWnd" };
    for (const wchar_t *name : shell) {
        if (std::wcscmp(windowClass, name) == 0) return true;
    }
    return false;
}

// The configs window by its title: the page's marker once its script has run
// (window_icon_rule.inl), and the page's own <title> for the moment before
// that, so a window just opened is known at once. A browser tab showing the
// page has more after it (" - Google Chrome") and is neither.
// ⚠️ The <title> is the page's, in tools/controller-config-test-client.html.
inline const wchar_t *const kPageTitle = L"DS5-USBIP Controller Configs";

inline bool is_configs_title(const wchar_t *title, const wchar_t *marker)
{
    if (title == nullptr) return false;
    return window_icon_rule::is_app_window_title(title, marker) || std::wcscmp(title, kPageTitle) == 0;
}

}  // namespace taskbar_rule
