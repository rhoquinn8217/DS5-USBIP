// Opening the settings page.
//
// WHY THE EXE DOES THIS. The launcher script used to open the page every time
// it started, and the listener stops on every build -- so a few rebuilds left a
// pile of identical browser windows lying around. Doing it here means the check
// for "one is already open" happens in the same place as the launch, which a
// batch file cannot do reliably.
//
// ⭐ REUSE IS BY WINDOW TITLE. Chrome offers no way to say "focus my app window
// if it exists"; --user-data-dir shares a profile but still opens a second
// window. So this walks the top-level windows looking for the page's own title
// and brings that one forward instead.
//
// ⚠️ That makes the page's <title> load-bearing. If it changes, this stops
// matching and the old behaviour returns -- a new window each time. The marker
// below is deliberately a fragment rather than the whole title so a version
// suffix or a trailing app name does not break it.

#pragma once

// ShellExecuteW lives here, not in windows.h. main.cpp does not include it
// because nothing else in the project launches another program.
#include <shellapi.h>
// Whether Windows is locked (session_locked, below).
#include <wtsapi32.h>
#include <shellscalingapi.h>   // the scale of the display the window opens on
#pragma comment(lib, "shcore.lib")
#pragma comment(lib, "wtsapi32.lib")

// The rule that tells our app window from a browser window showing the page in
// a tab. The icon needed it first; every lookup here uses it too.
#include "window_icon_rule.inl"
#include "front_rule.inl"
#include "window_open_rule.inl"   // the browser's units for a size and a place; pure

namespace ctm_open_ui {

inline bool g_open_ui = false;              // set by --ui
inline bool g_ui_already_focused = false;   // main focused one before starting
inline bool g_no_start_window = false;      // set by --no-start-window: none until a bridge (open_on_bridge.inl)

// A distinctive fragment of the page's <title>. See the warning above.
// ⛔ NOT the page title. "Controller config" appears in the <title>, so any
// window showing this page matched -- including an ordinary browser window with
// the page in a TAB, which is how "Open it properly" closed someone entire
// browser.
//
// ⭐ The page appends this marker to the END of its own title only when it was
// opened with ?app on the URL, which is how we open it.
// ⛔⛔ AND THE MARKER MUST END THE WINDOW'S TITLE, not merely be in it
// (window_icon_rule::is_app_window_title). A tab opened on a URL that carries
// ?app -- an address copied out of the window, a closed window reopened as a
// tab -- puts the marker on too, and its browser window then reads
// "... [ctm-app] - Google Chrome". Matched anywhere, that window was found,
// and close_existing() closed the whole browser (code review, 2026-10-05).
// In the window we open, the page's title is the whole title.
inline const wchar_t *kTitleMarker = L"[ctm-app]";

inline const wchar_t *kRelativePage = L"tools\\controller-config-test-client.html";

struct FindState {
    HWND found = nullptr;
};

inline BOOL CALLBACK find_window_proc(HWND hwnd, LPARAM param)
{
    if (!IsWindowVisible(hwnd)) return TRUE;
    wchar_t title[512] = {};
    if (GetWindowTextW(hwnd, title, 511) <= 0) return TRUE;
    if (!window_icon_rule::is_app_window_title(title, kTitleMarker)) return TRUE;
    reinterpret_cast<FindState *>(param)->found = hwnd;
    return FALSE;                                   // stop at the first match
}

// Brings an already-open page forward. Returns false when there is none.
//
// ⓘ The Alt-key dance is not superstition: Windows refuses SetForegroundWindow
// from a process that does not own the foreground, and faking a keypress is the
// documented way around it. Without it the window is raised but stays behind.
inline bool focus_existing()
{
    FindState state;
    EnumWindows(find_window_proc, reinterpret_cast<LPARAM>(&state));
    if (state.found == nullptr) return false;

    if (IsIconic(state.found)) {
        ShowWindow(state.found, SW_RESTORE);
    }
    INPUT alt = {};
    alt.type = INPUT_KEYBOARD;
    alt.ki.wVk = VK_MENU;
    SendInput(1, &alt, sizeof(alt));
    SetForegroundWindow(state.found);
    alt.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &alt, sizeof(alt));
    return true;
}

// ⭐ An http:// URL now, not a file path.
//
// The agent serves the page, so there is nothing on disk to point at -- which
// is the whole reason a released exe works with no files beside it.
// ⭐ WHICH window is the current one.
//
// Two settings windows should never exist, but Ctrl+Shift+T reopens a closed
// one and that is the browser's key, not ours. So rather than trying to prevent
// it, the agent decides which window is real and the others stand down.
//
// ⓘ A fresh token every time one is opened. A page carrying an older one is
// stale and closes itself -- no message to read, no decision to make.
//
// ⚠️ Empty until the first window is opened, and a restart empties it again.
// That means every window from before a restart is stale, which is a convenient
// way to clear leftovers after a rebuild.
inline std::mutex g_tokenMutex;
inline std::wstring g_uiToken;

// ⭐ THE RUN ID -- one value for the whole life of this listener, unlike the
// window token above which is minted per window.
//
// ⓘ Added 2026-09-01 for remembering where you were. The place a person left
// off has to survive the settings window being CLOSED and reopened -- the
// window token cannot carry that, because the new window gets a new one. A
// listener restart is the right boundary to forget at: it is exactly when
// "somewhere you were an hour ago" stops being meaningful.
inline std::string agent_run_id()
{
    static const std::string id = std::to_string(GetTickCount64());
    return id;
}

inline std::string current_ui_token()
{
    std::lock_guard<std::mutex> lock(g_tokenMutex);
    std::string out;
    out.reserve(g_uiToken.size());
    for (wchar_t c : g_uiToken) out.push_back(static_cast<char>(c));
    return out;
}

inline std::wstring new_ui_token()
{
    std::lock_guard<std::mutex> lock(g_tokenMutex);
    g_uiToken = std::to_wstring(GetTickCount64());
    return g_uiToken;
}

// ⛔ A window opened by a listener that will NOT be serving the API must not
// carry a token.
//
// Measured 2026-08-29: starting the exe while another listener was running
// opened a window and minted a token here -- but the OTHER listener serves
// /api/v1/devices and knows nothing of it, so the page saw a mismatch and
// closed itself immediately.
//
// ⓘ A page with no token never closes itself, which is exactly right: nothing
// is claiming to be a newer window.
// ⭐ Which controller the NEXT window should open on. Set just before opening,
// cleared as it is used -- so a target can never leak into an unrelated open.
// ⚠️ Both this and the claim below are under g_open_mutex: bridges set the
// target from their own threads, and raise_when_ready clears it from another.
inline std::mutex g_open_mutex;
inline std::wstring g_open_on_tab;

// ⛔⛔ ONE OPEN AT A TIME (rhoquinn8217, 2026-09-01: two bridged controllers
// rapid-fired every button; bisected to this half).
//
// ⚠️ Opening the window CLOSES any existing one first, matching by title. Two
// controllers bridging meant two of these running at once, each closing what
// the other had just opened -- and every one of those transitions flips config
// mode, which gates the pads and re-arms the swallow-until-released mask. The
// oscillation reached the game as every button repeating.
//
// ⓘ The loser does not queue: it has already set the target, and the open in
// flight reads the target when it builds its URL -- so the LAST controller to
// bridge before then is the one the window comes up on, which is the one you
// just picked up.
// ⭐ After the URL is built the claim is still held, until the window exists
// (raise_when_ready), and a target left in that time is forgotten: the window
// keeps the device it was launched for, and the others are tabs in it.
inline bool g_open_in_flight = false;

// ⛔ The target and the claim in ONE step. Apart, the open in flight could
// clear the target between the two, and this caller then took the freed claim
// with no target and opened on the wrong tab.
inline bool claim_open_on(const std::string &ordinal)
{
    std::lock_guard<std::mutex> lock(g_open_mutex);
    if (!ordinal.empty()) g_open_on_tab.assign(ordinal.begin(), ordinal.end());
    if (g_open_in_flight) return false;
    g_open_in_flight = true;
    return true;
}

// ⓘ And the target goes with it. A launched window carries its own, so one a
// loser left since belongs to nothing, and an open that never launched has no
// window to give it to.
inline void release_open()
{
    std::lock_guard<std::mutex> lock(g_open_mutex);
    g_open_on_tab.clear();
    g_open_in_flight = false;
}

// ⓘ Whether an open is under way: from its claim until its window exists or
// it gave up. The quick config switch waits on this to close a window it
// asked for that is not there yet (quick_switch_session.inl).
inline bool open_in_flight()
{
    std::lock_guard<std::mutex> lock(g_open_mutex);
    return g_open_in_flight;
}

inline std::wstring page_url_untokened(uint16_t restPort)
{
    return L"http://127.0.0.1:" + std::to_wstring(restPort) + L"/?app";
}

inline std::wstring page_url(uint16_t restPort)
{
    // ⭐ ?app marks a window WE opened. The page cannot tell an app window from
    // an ordinary tab by asking the browser -- resizeTo is asynchronous, so a
    // resize probe reads the old size and reports a false answer, and chrome
    // height is a guess that blanked the page when it was wrong.
    //
    // ⓘ Someone can of course type the parameter by hand. That is fine: it means
    // "I know what I am doing", not "this is definitely an app window".
    std::wstring url = L"http://127.0.0.1:" + std::to_wstring(restPort) +
                       L"/?app=" + new_ui_token();
    // ⭐ THE TAB TO LAND ON (rhoquinn8217, 2026-09-01). A window opened because
    // a controller bridged, or because someone ran the chord ON a controller,
    // should come up on THAT controller -- not on Overview, leaving them to
    // find it.
    //
    // ⓘ Empty for an ordinary open, and the page then does what it always did.
    std::lock_guard<std::mutex> lock(g_open_mutex);
    if (!g_open_on_tab.empty()) {
        url += L"&tab=" + g_open_on_tab;
        g_open_on_tab.clear();      // one open, one target
    }
    return url;
}

inline std::wstring find_browser()
{
    const wchar_t *candidates[] = {
        L"C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe",
        L"C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe",
        L"C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe",
        L"C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe",
    };
    for (const wchar_t *c : candidates) {
        if (GetFileAttributesW(c) != INVALID_FILE_ATTRIBUTES) return c;
    }
    return std::wstring();
}

// ⭐ Closes the settings page if one is open. Returns true when it did.
//
// Same lookup as focus_existing -- by window title -- with a close message
// instead of a focus one.
//
// ⓘ WM_CLOSE rather than terminating anything: the browser owns the window, and
// asking it to close lets it tear the page down properly, which is what fires
// the page's own "I am going away" beacon.
//
// ⚠️ Best effort by design. A window that will not close is untidy; a gate that
// will not release is what strands someone. So callers release the gate FIRST
// and treat this as cleanup.
// Is a settings window up right now? Same lookup, no side effects.
// ⭐ Does OUR window have the keyboard right now? Asked of Windows, not of the
// page.
//
// ⛔ The page's own document.hasFocus() cannot be trusted here: after the window
// is raised it is VISIBLE and reports focus, while the game still owns the
// keyboard -- so the gate stayed on and the keystrokes went to the game.
//
// ⓘ This is the only question that actually matters: keystrokes follow the
// foreground window, so gate exactly when that window is ours.
inline bool window_has_foreground()
{
    const HWND fg = GetForegroundWindow();
    if (fg == nullptr) return false;
    wchar_t title[512] = {};
    if (GetWindowTextW(fg, title, 511) <= 0) return false;
    return window_icon_rule::is_app_window_title(title, kTitleMarker);
}

inline bool window_exists()
{
    FindState state;
    EnumWindows(find_window_proc, reinterpret_cast<LPARAM>(&state));
    return state.found != nullptr;
}

// ⛔ PARKING IS GONE (rhoquinn8217, 2026-09-09, reversing that morning's
// decision): "closing closes it and the only ways to get it back is through
// the task bar icon or the chord". The window minimised itself instead, and
// close_existing() below is what does the closing now -- there is nothing
// here a park needed that it does not already do.

// ⭐ Bring our window ABOVE a borderless game.
//
// ⛔ Measured 2026-08-29: the window opened, took focus and HELD it for nine
// seconds -- while being completely invisible behind a borderless game. Focus
// and drawing order are different things, which is why an earlier attempt at
// SetForegroundWindow changed nothing: it was already foreground.
//
// ⓘ TOPMOST then back to NOTOPMOST. Setting it topmost lifts it above a
// full-screen game; dropping it again immediately means it does not then sit
// over everything else forever, which would be its own annoyance.
inline void raise_now(HWND hwnd)
{
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE);
    // ⛔ And take the KEYBOARD too. Raising changes drawing order only,
    // so without this the window sat in front while the game still
    // owned input -- and the gate's keystrokes went to the game.
    focus_existing();
}

// The program a window belongs to, by its exe's name, for the log. "?" when
// Windows will not say -- a game run elevated, or by an anti-cheat, refuses.
inline std::wstring exe_of(HWND hwnd)
{
    DWORD pid = 0;
    if (hwnd == nullptr || GetWindowThreadProcessId(hwnd, &pid) == 0) return L"?";
    std::wstring name = L"?";
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (process != nullptr) {
        wchar_t path[MAX_PATH] = {};
        DWORD size = MAX_PATH;
        if (QueryFullProcessImageNameW(process, 0, path, &size)) {
            const wchar_t *base = wcsrchr(path, L'\\');
            name = base != nullptr ? base + 1 : path;
        }
        CloseHandle(process);
    }
    return name;
}

// ⭐ IS THE WINDOW THAT HAD THE FRONT A FULL-SCREEN GAME? front_rule.inl holds
// the rule (rhoquinn8217, 2026-10-09); this reads the window it is asked about.
inline bool is_full_screen_game(HWND hwnd)
{
    if (hwnd == nullptr || !IsWindow(hwnd)) return false;
    RECT wr = {};
    if (!GetWindowRect(hwnd, &wr)) return false;
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi)) return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    wchar_t cls[128] = {};
    GetClassNameW(hwnd, cls, 127);
    return front_rule::is_full_screen_game(
        front_rule::covers(wr.left, wr.top, wr.right, wr.bottom,
                           mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right, mi.rcMonitor.bottom),
        pid == GetCurrentProcessId(), cls);
}

// ⭐ THE GAME THE WINDOW WAS RAISED OVER, to give the front back to as it
// closes (rhoquinn8217, 2026-10-09). front_rule::on_raise says when it changes.
inline std::mutex g_gameMutex;
inline HWND g_raisedOverGame = nullptr;

inline void remember_front(HWND before, bool beforeIsGame)
{
    std::lock_guard<std::mutex> lock(g_gameMutex);
    switch (front_rule::on_raise(before != nullptr, beforeIsGame)) {
    case front_rule::Remember::Set:   g_raisedOverGame = before;  break;
    case front_rule::Remember::Clear: g_raisedOverGame = nullptr; break;
    case front_rule::Remember::Keep:  break;
    }
}

// ⭐ THE WINDOW HAS CLOSED: give the full-screen game it was raised over the
// front back (rhoquinn8217, 2026-10-09: *"restore focus on the full screen mode
// game when the configs window closes"*). `hadFront` is the page's word that it
// had the front as it went. ⓘ On its own thread, once the window is gone: the
// page says it is closing while its window still stands, and a window still
// there after a second was reloaded, not closed.
// ⚠️ An exclusive full-screen game minimises when it loses the front, so it is
// restored first. The Alt key is focus_existing's, for the same reason.
inline void give_front_back(bool hadFront)
{
    std::thread([hadFront]() {
        for (int waited = 0; waited < 1000 && window_exists(); waited += 50) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        if (window_exists()) {
            device_log::input_w() << L"ui: the page went but its window is still there (a reload): the front stays";
            return;
        }
        HWND game = nullptr;
        {
            std::lock_guard<std::mutex> lock(g_gameMutex);
            game = g_raisedOverGame;
        }
        bool reopening = false;
        {
            std::lock_guard<std::mutex> lock(g_open_mutex);
            reopening = g_open_in_flight;
        }
        const bool there = game != nullptr && IsWindow(game);
        if (!front_rule::give_front_back(game != nullptr, there, hadFront, reopening)) {
            if (game != nullptr) {
                device_log::input_w() << L"ui: the window closed; the front stays where it is ("
                                      << (!there ? L"the game has gone" : !hadFront ? L"it did not have the front"
                                                                       : L"a new window is on its way")
                                      << L")";
            }
            return;
        }
        {
            std::lock_guard<std::mutex> lock(g_gameMutex);
            if (g_raisedOverGame == game) g_raisedOverGame = nullptr;
        }
        if (IsIconic(game)) ShowWindow(game, SW_RESTORE);
        INPUT alt = {};
        alt.type = INPUT_KEYBOARD;
        alt.ki.wVk = VK_MENU;
        SendInput(1, &alt, sizeof(alt));
        SetForegroundWindow(game);
        alt.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &alt, sizeof(alt));
        device_log::input_w() << L"ui: the window closed; gave the front back to " << exe_of(game);
    }).detach();
}

// ⭐ FOLLOW THE LAST GAME (rhoquinn8217, 2026-10-09: *"follow the last game I
// was in"*). While a window stands, the front is looked at four times a
// second, and a full-screen game coming to it becomes the one to give the
// front back to (front_rule::on_front_change). ⓘ One follower at a time, for
// as long as a window stands: a reopen hands over to the new window without a
// gap, and two seconds with none ends it. Our own window is never the game.
inline std::atomic_bool g_followingFront{false};

// ⭐⭐ WHO TOOK THE FRONT FROM THE WINDOW, AND A NOTIFICATION GIVES IT BACK
// (rhoquinn8217, 2026-10-22: "sometimes when there is a bottom right
// notification like samsung magic driver manager notification, it steal focus
// from the config window even if it's on top", then "yes take it back, with
// log"). The follower below sees the front change while a window stands: when
// the window loses it, the log names the program that took it, and one that
// looks like a notification (front_rule.inl) is made to give it back, three
// times in ten seconds at most.
// ⓘ Never for the window's own browser -- a list it opens is a window of its
// own -- nor for this listener's windows.
struct FrontTaker {
    std::wstring exe;
    wchar_t cls[128] = {};
    RECT rect = {};
    bool topmost = false;
    bool tool = false;
    bool underPointer = false;
    bool notification = false;
};

inline FrontTaker read_front_taker(HWND hwnd)
{
    FrontTaker t;
    t.exe = exe_of(hwnd);
    GetClassNameW(hwnd, t.cls, 127);
    GetWindowRect(hwnd, &t.rect);
    const LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    t.topmost = (ex & WS_EX_TOPMOST) != 0;
    t.tool = (ex & WS_EX_TOOLWINDOW) != 0;
    POINT pt = {};
    t.underPointer = GetCursorPos(&pt) && PtInRect(&t.rect, pt);
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi)) {
        t.notification = front_rule::looks_like_notification(
            t.rect.left, t.rect.top, t.rect.right, t.rect.bottom,
            mi.rcWork.left, mi.rcWork.top, mi.rcWork.right, mi.rcWork.bottom,
            t.topmost, t.tool, t.underPointer, pid == GetCurrentProcessId(), t.cls);
    }
    return t;
}

inline void follow_front()
{
    bool expected = false;
    if (!g_followingFront.compare_exchange_strong(expected, true)) return;
    std::thread([]() {
        HWND last = nullptr;
        bool lastWasOurs = false;
        std::vector<ULONGLONG> tookBack;    // when a notification gave the front back
        int goneMs = 0;
        while (goneMs < 2000) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            FindState ours;
            EnumWindows(find_window_proc, reinterpret_cast<LPARAM>(&ours));
            if (ours.found == nullptr) {
                goneMs += 250;
                continue;
            }
            goneMs = 0;
            const HWND fg = GetForegroundWindow();
            if (fg == nullptr || fg == last) continue;
            const bool fromOurs = lastWasOurs;
            last = fg;
            wchar_t title[512] = {};
            lastWasOurs = GetWindowTextW(fg, title, 511) > 0 &&
                          window_icon_rule::is_app_window_title(title, kTitleMarker);
            if (lastWasOurs) continue;
            DWORD fgPid = 0, ourPid = 0;
            GetWindowThreadProcessId(fg, &fgPid);
            GetWindowThreadProcessId(ours.found, &ourPid);
            if (fromOurs && fgPid != ourPid && fgPid != GetCurrentProcessId()) {
                const FrontTaker t = read_front_taker(fg);
                device_log::input_w() << L"ui: " << t.exe << L" took the front from the window ("
                                      << t.cls << L", " << (t.rect.right - t.rect.left) << L"x"
                                      << (t.rect.bottom - t.rect.top) << L" at " << t.rect.left
                                      << L"," << t.rect.top << (t.topmost ? L", on top" : L"")
                                      << (t.tool ? L", not in the taskbar" : L"")
                                      << (t.underPointer ? L", under the pointer" : L"") << L")";
                if (t.notification) {
                    const ULONGLONG now = GetTickCount64();
                    tookBack.erase(std::remove_if(tookBack.begin(), tookBack.end(),
                                                  [now](ULONGLONG at) { return now - at > 10000; }),
                                   tookBack.end());
                    if (!front_rule::may_take_front_back(static_cast<int>(tookBack.size()))) {
                        device_log::input_w() << L"ui: it looks like a notification, and it took the front "
                                              << tookBack.size() << L" times in ten seconds: leaving it";
                    } else {
                        tookBack.push_back(now);
                        raise_now(ours.found);
                        std::this_thread::sleep_for(std::chrono::milliseconds(150));
                        const bool back = window_has_foreground();
                        device_log::input_w() << L"ui: it looks like a notification (small, in a corner, "
                                              << L"not clicked): "
                                              << (back ? L"took the front back"
                                                       : L"tried to take the front back, and could not");
                        if (back) {
                            last = GetForegroundWindow();
                            lastWasOurs = true;
                        }
                        continue;
                    }
                }
            }
            if (front_rule::on_front_change(is_full_screen_game(fg)) != front_rule::Remember::Set) continue;
            std::lock_guard<std::mutex> lock(g_gameMutex);
            if (g_raisedOverGame != fg) {
                g_raisedOverGame = fg;
                device_log::input_w() << L"ui: " << exe_of(fg)
                                      << L" is the game now: the front goes back to it when the window closes";
            }
        }
        g_followingFront.store(false);
    }).detach();
}

// ⭐⭐ A FULL-SCREEN GAME TAKES THE FRONT BACK (rhoquinn8217, 2026-10-03, in The
// Witcher 3: "chord take 2 or 3 times to push through full screen on this
// game"). The log showed it the same way every time: the window raised and in
// front, and under a second later the game owned the keyboard again -- until a
// second or third chord stuck.
// ➡️ So after raising, watch the front for a moment, and raise again when it
// goes back to the window it was taken from: what they were doing by hand.
// ⓘ Bounded: three more tries, each one watched for two seconds, and only while
// the front is that same window. Anything else taking it -- a click somewhere,
// Alt+Tab to another program -- is left alone.
// ⓘ Its own thread, so the one-at-a-time claim above still ends when the window
// first appears.
inline void keep_front_from(HWND taken_from)
{
    if (taken_from == nullptr) return;
    std::thread([taken_from]() {
        int tries = 0;
        auto watched_since = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - watched_since < std::chrono::seconds(2)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (GetForegroundWindow() != taken_from) continue;
            FindState state;
            EnumWindows(find_window_proc, reinterpret_cast<LPARAM>(&state));
            if (state.found == nullptr) return;     // closed in the meantime
            if (tries == 3) {
                device_log::input_w() << L"ui: " << exe_of(taken_from)
                                      << L" took the front back again -- leaving it after 3 tries";
                return;
            }
            ++tries;
            device_log::input_w() << L"ui: " << exe_of(taken_from)
                                  << L" took the front back -- raising again (" << tries << L" of 3)";
            raise_now(state.found);
            watched_since = std::chrono::steady_clock::now();
        }
    }).detach();
}

// ⭐⭐ BEHIND THE LOCK SCREEN THE WINDOW WAITS FOR THE UNLOCK (rhoquinn8217,
// 2026-10-22: "when DS5-USBIP start in the background behind the lockscreen,
// the config window can't be brought above it"). Windows draws the lock screen
// on a desktop of its own, and no program's window can come over it: a stream
// started at the lock screen starts this listener behind it, and a bridge then
// opened the window where nobody could see it, its raise refused and never
// tried again.
// ➡️ So a window asked for while Windows is locked comes to the front once it
// is unlocked, as the bridge would have brought it.

// Is Windows locked? Asked of the session itself, so a listener started while
// it already was knows as well as one that watched it lock.
// ⚠️ Windows 7 and Server 2008 R2 report the two states the wrong way round;
// this runs on 10 and 11. Not known counts as not locked: the raise is then
// tried at once, which is what always happened before.
inline bool session_locked()
{
    LPWSTR buffer = nullptr;
    DWORD bytes = 0;
    if (!WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION,
                                     WTSSessionInfoEx, &buffer, &bytes) || buffer == nullptr) {
        return false;
    }
    const auto *info = reinterpret_cast<const WTSINFOEXW *>(buffer);
    const bool locked = info->Level == 1 &&
                        info->Data.WTSInfoExLevel1.SessionFlags == WTS_SESSIONSTATE_LOCK;
    WTSFreeMemory(buffer);
    return locked;
}

// ⓘ One watcher at a time. It finds the window by its title when the unlock
// comes, so a window opened again while locked is the one it raises.
inline std::atomic_bool g_raiseOnUnlock{false};

inline void raise_on_unlock()
{
    bool expected = false;
    if (!g_raiseOnUnlock.compare_exchange_strong(expected, true)) return;
    device_log::input_w() << L"ui: Windows is locked: the window comes to the front when it is unlocked";
    std::thread([]() {
        while (session_locked()) std::this_thread::sleep_for(std::chrono::milliseconds(250));
        g_raiseOnUnlock.store(false);
        // ⓘ The desktop is back a moment after the session says it is
        // unlocked, and Windows then hands the front to whatever had it before
        // the lock: so a few tries, each one checked.
        for (int tries = 1; tries <= 6; ++tries) {
            std::this_thread::sleep_for(std::chrono::milliseconds(400));
            FindState state;
            EnumWindows(find_window_proc, reinterpret_cast<LPARAM>(&state));
            if (state.found == nullptr) {
                device_log::input_w() << L"ui: Windows was unlocked; the window asked for while it was locked has gone";
                return;
            }
            const HWND before = GetForegroundWindow();
            const bool beforeIsGame = before != state.found && is_full_screen_game(before);
            raise_now(state.found);
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            if (!window_has_foreground()) continue;
            device_log::input_w() << L"ui: Windows was unlocked: brought the window to the front (try "
                                  << tries << L", the front was "
                                  << (before != nullptr ? exe_of(before) : std::wstring(L"nothing")) << L")";
            // ⓘ What raise_when_ready does once its window is up.
            remember_front(before != state.found ? before : nullptr, beforeIsGame);
            follow_front();
            if (beforeIsGame) keep_front_from(before);
            return;
        }
        device_log::input_w() << L"ui: Windows was unlocked, and the window would not come to the front";
    }).detach();
}

// ⚠️ Runs on a thread because the window does not exist yet when the browser is
// launched -- it has to be waited for.
// ⭐⭐ `release_claim`: THE ONE-AT-A-TIME CLAIM ENDS WHEN THE WINDOW EXISTS, not
// when it was launched (rhoquinn8217, 2026-09-15). A keyboard of three parts
// bridged three times in 200 ms and opened a window each, one of them at the
// wrong size, and several controllers bridging together do the same: the claim
// was released right after each launch, so the next bridge took it, found no
// window yet -- a browser takes a moment to start -- and launched another.
// ➡️ The caller hands the claim to this thread, which lets it go once the
// window is found and raised, or after its three seconds, and forgets any tab
// target a loser left in the meantime: the launched window already carries its
// own. ⓘ Here rather than in the caller, which may be a controller's report
// thread that must not wait.
inline void raise_when_ready(bool release_claim = false)
{
    // ⓘ Who has the front BEFORE the window exists: the game, when there is
    // one. Read here, while the browser is still starting, so it cannot be the
    // new window itself; and never our own window, which is not a game.
    HWND before = GetForegroundWindow();
    wchar_t title[512] = {};
    if (before != nullptr && GetWindowTextW(before, title, 511) > 0 &&
        window_icon_rule::is_app_window_title(title, kTitleMarker)) {
        before = nullptr;
    }
    const bool beforeIsGame = is_full_screen_game(before);
    std::thread([release_claim, before, beforeIsGame]() {
        struct Done {
            bool release;
            ~Done() { if (release) release_open(); }
        } done{release_claim};
        for (int i = 0; i < 60; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            FindState state;
            EnumWindows(find_window_proc, reinterpret_cast<LPARAM>(&state));
            if (state.found == nullptr) continue;

            // ⭐ Not over the lock screen, which nothing can be: at the unlock.
            if (session_locked()) {
                raise_on_unlock();
                return;
            }
            raise_now(state.found);
            device_log::input_w() << L"ui: raised the window above the game (the front was "
                                  << (before != nullptr ? exe_of(before) : std::wstring(L"nothing")) << L")";
            remember_front(before, beforeIsGame);
            follow_front();
            // ⭐ ONLY A FULL-SCREEN GAME IS WATCHED (rhoquinn8217, 2026-10-09).
            // ⛔ Opened from the tray, the window that had the front was the
            // tray's own, and opening its menu again raised this window over
            // the menu, which shut. front_rule.inl.
            if (beforeIsGame) {
                keep_front_from(before);
            } else if (before != nullptr) {
                device_log::input_w() << L"ui: " << exe_of(before)
                                      << L" had the front and is not a full-screen game: not watching it";
            }
            return;
        }
        // ⓘ A browser started behind the lock screen can be slow to show its
        // window; the unlock raises it all the same.
        if (session_locked()) {
            raise_on_unlock();
            return;
        }
        device_log::input_w() << L"ui: no window appeared to raise";
    }).detach();
}

inline bool close_existing()
{
    FindState state;
    EnumWindows(find_window_proc, reinterpret_cast<LPARAM>(&state));
    if (state.found == nullptr) {
        device_log::input_w() << L"ui: close_existing found no window";
        return false;
    }
    device_log::input_w() << L"ui: close_existing closing a window";
    PostMessageW(state.found, WM_CLOSE, 0, 0);
    return true;
}

// Opens the settings page, or focuses the one already open.
//
// !! Never fatal. A missing page or browser is logged and the agent carries on
// !! -- the listener is the point, and the page is a convenience on top of it.
// Brings an existing page forward. Returns true when it did.
//
// ⭐ Called EARLY, before any socket is bound: the commonest failure is another
// listener already holding the port, and in that case surfacing the page you
// already have is exactly right.
inline bool focus_only()
{
    if (!focus_existing()) return false;
    device_log::session_w() << L"settings page already open, brought to the front";
    return true;
}

// Opens a new page.
//
// ⛔ Called LATE, only once the agent is actually up. Opening one on a failed
// start would leave a browser window reporting an unreachable agent -- a
// confusing thing to hand someone whose real problem is that the exe did not
// start.
// The scale of the display a point is on, in dots per inch (96 is 100%).
inline UINT dpi_at(POINT pt)
{
    const HMONITOR monitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    UINT dx = 96, dy = 96;
    if (FAILED(GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dx, &dy)) || dx == 0) return 96;
    return dx;
}

inline void open_new(uint16_t restPort, bool withToken = true)
{
    const std::wstring url = withToken ? page_url(restPort)
                                       : page_url_untokened(restPort);

    const std::wstring browser = find_browser();
    if (browser.empty()) {
        // No Chrome or Edge: hand it to whatever is registered. Loses the app
        // window, which is cosmetic -- and losing the page entirely is not.
        device_log::session_w() << L"no Chrome or Edge found, opening in the default browser";
        ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        return;
    }

    // --app strips the address bar, tabs and bookmarks: this is a control
    // surface, not a page being browsed, and on a TV that furniture is a row of
    // things to hit by accident.
    // ⛔ --user-data-dir IS WHAT MAKES --app WORK.
    //
    // Measured 2026-08-29: with Chrome already running, our process handed the
    // URL to the existing instance and exited -- and that instance opened it as
    // an ORDINARY TAB, dropping --app entirely. The page reported toolbar=true,
    // menubar=true and 119px of browser furniture.
    //
    // ⚠️ That was the root of a day of window problems: it could not reliably be
    // raised, focused, closed or told apart from a tab, because it WAS a tab.
    //
    // ⭐ A private profile forces a separate instance, which honours --app. It
    // also means killing our window can never touch the person's own browsing --
    // no shared cookies, no shared session, nothing of theirs in it.
    wchar_t profile[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, profile);
    const std::wstring dataDir = std::wstring(profile) + L"ctm-usbip-ui";

    // ⭐ The same size the page's Fit window computes -- four fifths of the
    // screen in both directions (rhoquinn8217, 2026-09-08, measured at 250%
    // scaling, where the page is the main event and not a side panel) -- so
    // the window opens right rather than being fitted after it appears.
    // ⚠️ These fractions are duplicated in the page's wantSize(); the two
    // must move together. The work area, so the taskbar is not counted.
    RECT wa = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    const int waW = wa.right - wa.left, waH = wa.bottom - wa.top;
    // ⓘ Advanced's small since 2026-10-29 (kSizes in config_move.inl, where
    // the reason is): 0.68 x 0.73, the large until then, with 0.90 x 0.94 its
    // large.
    const int winW = (int)(waW * 0.68);   // Advanced's small, its opening size
    const int winH = (int)(waH * 0.73);
    // ⭐⭐ AT ITS LAYOUT'S SIZE AND PLACE, IN THE BROWSER'S UNITS (rhoquinn8217,
    // 2026-10-23: "when the window opens in simple or quick mode, a larger
    // window blips open and close then the simple or quick mode window opens
    // after"). The browser was asked for Advanced's size alone, in physical
    // pixels, which it reads as scaled units: at 150% it came up 2496 x 1947
    // at 15,15, most of the screen, and the restore then shrank it to Quick
    // at its place. Now it is asked for the size and the place the restore
    // would give (config_move.inl, opening_rect), divided by the display's
    // scale (window_open_rule.inl), and the restore moves nothing. ⓘ With no
    // layout known yet, Advanced's opening size in the middle.
    int openX = wa.left + (waW - winW) / 2, openY = wa.top + (waH - winH) / 2;
    int openW = winW, openH = winH;
    const bool known = ui_opening_rect(&openX, &openY, &openW, &openH);
    const UINT dpi = dpi_at(POINT{ openX + openW / 2, openY + openH / 2 });
    const std::wstring geometry =
        L" --window-position=" + std::to_wstring(window_open_rule::browser_units(openX, dpi)) +
        L"," + std::to_wstring(window_open_rule::browser_units(openY, dpi)) +
        L" --window-size=" + std::to_wstring(window_open_rule::browser_units(openW, dpi)) +
        L"," + std::to_wstring(window_open_rule::browser_units(openH, dpi));
    // ⭐⭐ DRAWN BY THE PROCESSOR, NOT THE GRAPHICS CARD (rhoquinn8217,
    // 2026-10-22: "while playing final fantasy VII rebirth, when the config
    // window showed above the game, it was very slow and laggy, however the
    // mouse cursor was not", then, asked always or only over a game:
    // "always"). The log of it showed the pad's reports every 4 ms and every
    // pick linked at once: only the window's drawing lagged, its frames
    // queued on a card the game and the stream's encoder kept busy, while
    // the pointer, which the card draws apart, stayed smooth. The page is
    // light, and the processor has room for it. ⓘ This copy of the browser
    // only: its own profile, never the person's.
    const std::wstring args = L"--app=" + url +
                              L" --user-data-dir=\"" + dataDir + L"\"" +
                              L" --no-first-run --no-default-browser-check --disable-gpu" +
                              geometry;
    // ⛔ CHECK THE RESULT. This logged "settings page opened" unconditionally,
    // so a launch that never happened looked identical to one that did --
    // measured 2026-08-29, when no Chrome process and no profile folder
    // appeared but the log said it had opened.
    //
    // ⓘ ShellExecuteW returns a value above 32 on success; anything at or below
    // is an error code.
    const HINSTANCE rc = ShellExecuteW(nullptr, L"open", browser.c_str(),
                                       args.c_str(), nullptr, SW_SHOWNORMAL);
    const auto code = reinterpret_cast<INT_PTR>(rc);
    if (code <= 32) {
        device_log::session_w() << L"settings page FAILED to open, code=" << code
                                << L" args=" << args;
        return;
    }
    device_log::session_w() << L"settings page opened at " << openX << L"," << openY
                            << L" " << openW << L"x" << openH
                            << (known ? L", its layout's place" : L", no layout known yet")
                            << L" (" << dpi << L" dpi)";
}

// Is a listener already running on the control port?
//
// ⭐ Asked BEFORE starting anything, so one exe with one set of flags covers
// every combination of "listener up or not" and "page open or not". Without
// this the second launch dies on a bind error and never reaches the page --
// the case where you most wanted it.
//
// ⛔ CONNECTS rather than binds. A bind probe was tried first and reported the
// port FREE while the agent held it: the agent binds INADDR_ANY, the probe
// bound loopback, and Windows permits that combination. Connecting asks the
// question directly -- is something accepting on this port -- and has no such
// ambiguity.
//
// ⚠️ Loopback only. A listener on another machine is not this one, and probing
// beyond the local host would be both wrong and slow.
inline bool agent_already_running(uint16_t port)
{
    WSADATA data = {};
    const bool startedWsa = WSAStartup(MAKEWORD(2, 2), &data) == 0;
    SOCKET probe = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (probe == INVALID_SOCKET) {
        if (startedWsa) WSACleanup();
        return false;
    }

    // Non-blocking, so a firewall that black-holes the connection cannot stall
    // startup: nothing answering within the timeout counts as nothing there.
    u_long nonBlocking = 1;
    ioctlsocket(probe, FIONBIO, &nonBlocking);

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);

    bool connected = connect(probe, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == 0;
    if (!connected && WSAGetLastError() == WSAEWOULDBLOCK) {
        fd_set writable;
        FD_ZERO(&writable);
        FD_SET(probe, &writable);
        timeval timeout = {};
        timeout.tv_usec = 300000;                  // 300 ms is generous on loopback
        connected = select(0, nullptr, &writable, nullptr, &timeout) > 0;
    }

    closesocket(probe);
    if (startedWsa) WSACleanup();
    return connected;
}

} // namespace ctm_open_ui
