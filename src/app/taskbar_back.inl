// Keeps the taskbar behind a full-screen game while the Controller Configs
// window is in front of it: taskbar_rule.inl says when, and why.
//
// ⭐ HOW. The taskbar has a switch made for this, ITaskbarList2's
// MarkFullscreenWindow: a window carrying the mark counts as full-screen, and
// while it is the active window the taskbar goes to the bottom, behind the
// game. Twenty times a second this decides whether the configs window should
// carry it:
// - while the configs window is in front, by the window just behind it on its
//   screen: one that fills the screen puts the mark on, anything else takes
//   it off, so over the ordinary desktop the taskbar behaves as it always has;
// - while another window is in front, by that window: a full-screen one on
//   the configs window's screen is what the configs window will come up over
//   (open_ui.inl's raise_now), so the mark goes on AHEAD of it, and the
//   taskbar does not show for a moment first.
// ⓘ The mark matters only while the window is active, so one left on while
// another window is in front does nothing.
//
// ⚠️ The window is the browser's, not ours. MarkFullscreenWindow takes any
// window, and nothing in its documentation keeps it to the caller's own; a
// hands-on run with a borderless game is what shows the taskbar honours it.
// ⚠️ And a window the browser has only just opened cannot be marked before
// it first comes up, so a fresh window may still show the taskbar for a
// moment on its first bring-up.

#pragma once

#include <shobjidl.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

namespace taskbar_back {

inline const int kEveryMs = 50;
inline std::atomic_bool g_running{false};

inline bool is_configs_window(HWND hwnd)
{
    wchar_t title[512] = {};
    if (GetWindowTextW(hwnd, title, 511) <= 0) return false;
    return taskbar_rule::is_configs_title(title, ctm_open_ui::kTitleMarker);
}

inline std::wstring class_of(HWND hwnd)
{
    wchar_t cls[128] = {};
    GetClassNameW(hwnd, cls, 127);
    return cls;
}

// A window a person sees and could be playing in: shown, not minimised, not
// hidden by Windows (cloaked: on another desktop, or an app frame put away),
// and not one drawn over everything for another program -- a tool window, or
// an overlay that lets clicks through or never takes the keyboard, as the
// graphics drivers' overlays are.
inline bool counts(HWND hwnd)
{
    if (!IsWindowVisible(hwnd) || IsIconic(hwnd)) return false;
    BOOL cloaked = FALSE;
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof cloaked)) && cloaked) return false;
    const LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if ((ex & (WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE)) != 0) return false;
    RECT r = {};
    return GetWindowRect(hwnd, &r) && r.right > r.left && r.bottom > r.top;
}

// Whether a window is a full-screen one on `screen`: it fills it, and it is
// not the desktop or a taskbar.
inline bool full_screen_on(HWND hwnd, HMONITOR screen)
{
    if (taskbar_rule::is_shell(class_of(hwnd).c_str())) return false;
    MONITORINFO info = {};
    info.cbSize = sizeof info;
    RECT r = {};
    if (!GetMonitorInfoW(screen, &info) || !GetWindowRect(hwnd, &r)) return false;
    return taskbar_rule::fills({ r.left, r.top, r.right, r.bottom },
                               { info.rcMonitor.left, info.rcMonitor.top, info.rcMonitor.right, info.rcMonitor.bottom });
}

// The window just behind `above` on `screen` that counts. The desktop and the
// taskbars are passed over: neither is a game, and a taskbar that is above
// every ordinary window says nothing about what is behind this one.
inline HWND window_behind(HWND above, HMONITOR screen)
{
    for (HWND w = GetWindow(above, GW_HWNDNEXT); w != nullptr; w = GetWindow(w, GW_HWNDNEXT)) {
        if (!counts(w) || MonitorFromWindow(w, MONITOR_DEFAULTTONULL) != screen) continue;
        if (taskbar_rule::is_shell(class_of(w).c_str())) continue;
        return w;
    }
    return nullptr;
}

inline void watch()
{
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    ITaskbarList2 *taskbar = nullptr;
    if (FAILED(CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&taskbar))) ||
        FAILED(taskbar->HrInit())) {
        device_log::session_w() << L"configs window: the taskbar could not be reached, so it comes up over a game as before";
        if (taskbar != nullptr) taskbar->Release();
        if (SUCCEEDED(com)) CoUninitialize();
        return;
    }
    HWND configs = nullptr;  // the configs window, once it has been in front
    bool told = false;       // whether the taskbar has been told about it yet
    bool marked = false;     // ...and whether it carries the mark
    while (g_running.load()) {
        const HWND front = GetForegroundWindow();
        HWND game = nullptr;   // the full-screen window it is over, or will come up over
        bool decided = false;
        if (front != nullptr && is_configs_window(front)) {
            if (front != configs) {
                configs = front;
                told = false;
            }
            const HMONITOR screen = MonitorFromWindow(front, MONITOR_DEFAULTTONEAREST);
            const HWND behind = window_behind(front, screen);
            if (behind != nullptr && full_screen_on(behind, screen)) game = behind;
            decided = true;
        } else if (configs != nullptr && !IsWindow(configs)) {
            configs = nullptr;   // closed
        } else if (configs != nullptr && front != nullptr) {
            const HMONITOR screen = MonitorFromWindow(configs, MONITOR_DEFAULTTONEAREST);
            if (counts(front) && MonitorFromWindow(front, MONITOR_DEFAULTTONULL) == screen &&
                full_screen_on(front, screen)) {
                game = front;
            }
            decided = true;
        }
        const bool mark = game != nullptr;
        if (decided && (!told || mark != marked)) {
            taskbar->MarkFullscreenWindow(configs, mark ? TRUE : FALSE);
            if (mark && game == front) {
                device_log::session_w() << L"configs window: the taskbar will stay behind it when it comes up over the full-screen "
                                        << class_of(game) << L" window in front";
            } else if (mark) {
                device_log::session_w() << L"configs window: the taskbar stays behind it, over the full-screen "
                                        << class_of(game) << L" window behind it";
            } else if (told) {
                device_log::session_w() << L"configs window: the taskbar comes up with it as usual";
            }
            told = true;
            marked = mark;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(kEveryMs));
    }
    taskbar->Release();
    if (SUCCEEDED(com)) CoUninitialize();
}

inline void start()
{
    if (g_running.exchange(true)) return;
    std::thread(watch).detach();
}

inline void stop()
{
    g_running.store(false);
}

}  // namespace taskbar_back
