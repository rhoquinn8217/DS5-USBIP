// The tutorial's words, on the desktop beside the Controller Configs window.
//
// ⭐ OUTSIDE THE WINDOW (rhoquinn8217, 2026-10-14, after going through the
// tutorial on the hardware: *"We need to move the descriptions so that they
// don't cover any of the browser box."*). A page cannot draw outside its own
// window, so the listener draws them, in a small window of its own that never
// takes focus, as the on-screen keyboard does (overlay_window.inl): the
// controller goes on driving the configs window while it is up.
//
// The page sends what a step says, the step count, whether Next can be
// pressed and Previous is there, which of the two the pad's halo is on, and
// where the lit part sits in its view (the /api/v1/ui/tutorial-bubble route).
// A click on Next or Previous here waits in take_press() until the page
// collects it. Where the window goes and how its lines break:
// tutorial_bubble_rule.inl.
//
// ⓘ It follows the configs window ten times a second, and hides whenever that
// window is not the one in front: minimised, or behind a game, there is
// nothing for it to point at.
#pragma once

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
// ⓘ <dwmapi.h> and its library come in with taskbar_back.inl, above this.

namespace tutorial_bubble {

namespace tbr = tutorial_bubble_rule;

// What a step shows, as the page last sent it.
struct Content {
    bool show = false;
    std::wstring count;       // "3 of 10"
    std::wstring words;       // lit names between [[ and ]]
    bool nextOn = false;      // Next can be pressed; greyed out on a step that asks for something
    bool prevShown = false;   // Previous is there at all; not on the first step
    std::string focus;        // the pad's halo: "next", "prev" or ""
    int ax = 500, ay = 500;   // the lit part's middle, thousandths of the page's view
};

inline std::mutex g_mutex;
inline Content g_content;
inline std::string g_press;
inline std::atomic<HWND> g_hwnd{ nullptr };
inline std::atomic_bool g_running{ false };
inline const wchar_t *const kClassName = L"DS5USBIP_TutorialWords";
inline const UINT kMsgUpdate = WM_APP + 41;
inline const UINT_PTR kFollowTimer = 1;

// ⓘ The page's own colours: its tutorial's amber, its buttons, its halo.
inline const COLORREF kFill = RGB(0x1d, 0x20, 0x30);
inline const COLORREF kAmber = RGB(0xf0, 0xb4, 0x29);
inline const COLORREF kText = RGB(0xe8, 0xe8, 0xea);
inline const COLORREF kMuted = RGB(0x9a, 0xa4, 0xb2);
inline const COLORREF kBtn = RGB(0x26, 0x28, 0x30);
inline const COLORREF kBtnEdge = RGB(0x3a, 0x3d, 0x48);
inline const COLORREF kGo = RGB(0x6c, 0x8c, 0xff);
inline const COLORREF kGoText = RGB(0x0f, 0x10, 0x14);
inline const COLORREF kOff = RGB(0x1a, 0x1b, 0x20);
inline const COLORREF kOffEdge = RGB(0x2c, 0x2e, 0x36);
inline const COLORREF kOffText = RGB(0x5d, 0x62, 0x70);
inline const COLORREF kHalo = RGB(0x2a, 0x35, 0x66);
inline const COLORREF kDesk = RGB(0x2b, 0x3a, 0x4f);   // the picture mode's stand-in desktop

inline std::wstring wide(const std::string &utf8)
{
    if (utf8.empty()) return std::wstring();
    const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (n <= 0) return std::wstring();
    std::wstring out(static_cast<size_t>(n), L' ');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), &out[0], n);
    return out;
}

inline int px(int v, UINT dpi) { return MulDiv(v, static_cast<int>(dpi), 96); }

// ⭐ SMALL ON PURPOSE (*"The description boxes seen to take a lot of
// space"*): ordinary desktop sizes, scaled for the display, where the page's
// bubble used the window's own large type.
struct Fonts {
    UINT dpi = 0;
    HFONT text = nullptr, lit = nullptr, count = nullptr, button = nullptr;
};

inline HFONT make_font(int sizePx, int weight, UINT dpi)
{
    return CreateFontW(-px(sizePx, dpi), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
}

inline void fonts_for(Fonts *f, UINT dpi)
{
    if (f->dpi == dpi && f->text != nullptr) return;
    for (HFONT h : { f->text, f->lit, f->count, f->button }) {
        if (h != nullptr) DeleteObject(h);
    }
    f->dpi = dpi;
    f->text = make_font(16, FW_NORMAL, dpi);
    f->lit = make_font(16, FW_SEMIBOLD, dpi);
    f->count = make_font(12, FW_NORMAL, dpi);
    f->button = make_font(15, FW_SEMIBOLD, dpi);
}

// Everything a paint and a click need, in the words window's own pixels.
struct Layout {
    Content content;
    UINT dpi = 96;
    tbr::Placement place;
    RECT box{}, next{}, prev{};
    POINT arrow[3]{};
    int pad = 0, round = 0, countH = 0, lineH = 0, litPad = 0;
    std::vector<tbr::Token> toks;
    std::vector<int> widths;
    std::vector<std::vector<size_t>> lines;
};

inline SIZE measure(HDC dc, HFONT font, const std::wstring &text)
{
    SIZE s{};
    SelectObject(dc, font);
    GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &s);
    return s;
}

// ⭐ THE LAYOUT, from the words and from where the configs window and the
// screen's working area are. The words box, Next and Previous under it at its
// right, and the arrow on the side facing the window.
inline Layout compute_layout(HDC dc, Fonts *fonts, const Content &c, UINT dpi,
                             const RECT &frame, const RECT &work)
{
    fonts_for(fonts, dpi);
    Layout L;
    L.content = c;
    L.dpi = dpi;
    L.pad = px(12, dpi);
    L.round = px(10, dpi);
    L.litPad = px(5, dpi);
    const int maxWidth = px(380, dpi);
    const int between = px(8, dpi);
    const int btnPadX = px(14, dpi), btnPadY = px(6, dpi);
    const int arrow = px(12, dpi), gap = px(10, dpi);

    L.toks = tbr::tokens(tbr::parse(c.words));
    for (const tbr::Token &t : L.toks) {
        const SIZE s = measure(dc, t.lit ? fonts->lit : fonts->text, t.text);
        L.widths.push_back(s.cx + (t.lit ? 2 * L.litPad : 0));
    }
    L.lines = tbr::lines(L.toks, L.widths, maxWidth);
    TEXTMETRICW tm{};
    SelectObject(dc, fonts->text);
    GetTextMetricsW(dc, &tm);
    L.lineH = tm.tmHeight + px(6, dpi);
    const SIZE countSize = measure(dc, fonts->count, c.count);
    L.countH = countSize.cy;
    int textW = countSize.cx;
    for (const auto &line : L.lines) {
        int w = 0;
        for (const size_t i : line) w += L.widths[i];
        if (w > textW) textW = w;
    }
    const int boxW = textW + 2 * L.pad;
    const int boxH = L.pad + L.countH + px(2, dpi) + static_cast<int>(L.lines.size()) * L.lineH + L.pad;

    const SIZE nextSize = measure(dc, fonts->button, L"Next");
    const SIZE prevSize = measure(dc, fonts->button, L"Previous");
    const int btnH = nextSize.cy + 2 * btnPadY;
    const int nextW = nextSize.cx + 2 * btnPadX;
    const int prevW = prevSize.cx + 2 * btnPadX;
    const int rowW = nextW + (c.prevShown ? between + prevW : 0);
    const int stackW = boxW > rowW ? boxW : rowW;
    const int stackH = boxH + between + btnH;

    const tbr::Box win{ frame.left, frame.top, frame.right, frame.bottom };
    const tbr::Box area{ work.left, work.top, work.right, work.bottom };
    L.place = tbr::place(win, area, stackW, stackH, arrow, gap, L.round, c.ax, c.ay);

    // ⓘ The words and buttons start after the arrow when it is on their top
    // or left side.
    const int offX = L.place.side == tbr::Side::Right ? arrow : 0;
    const int offY = L.place.side == tbr::Side::Below ? arrow : 0;
    L.box = { offX + stackW - boxW, offY, offX + stackW, offY + boxH };
    const int rowTop = offY + boxH + between;
    // [Next] [Previous], in rhoquinn8217's order, at the box's right.
    if (c.prevShown) {
        L.prev = { offX + stackW - prevW, rowTop, offX + stackW, rowTop + btnH };
        L.next = { L.prev.left - between - nextW, rowTop, L.prev.left - between, rowTop + btnH };
    } else {
        L.next = { offX + stackW - nextW, rowTop, offX + stackW, rowTop + btnH };
        L.prev = { 0, 0, 0, 0 };
    }
    const int a = L.place.arrowAt;
    switch (L.place.side) {
    case tbr::Side::Above:
        L.arrow[0] = { a - arrow, stackH };
        L.arrow[1] = { a + arrow, stackH };
        L.arrow[2] = { a, stackH + arrow };
        break;
    case tbr::Side::Below:
        L.arrow[0] = { a - arrow, arrow };
        L.arrow[1] = { a + arrow, arrow };
        L.arrow[2] = { a, 0 };
        break;
    case tbr::Side::Right:
        L.arrow[0] = { arrow, a - arrow };
        L.arrow[1] = { arrow, a + arrow };
        L.arrow[2] = { 0, a };
        break;
    case tbr::Side::Left:
        L.arrow[0] = { stackW, a - arrow };
        L.arrow[1] = { stackW, a + arrow };
        L.arrow[2] = { stackW + arrow, a };
        break;
    }
    return L;
}

// The window's shape: the words box, the buttons and the arrow, and nothing
// between them, so the desktop shows through the gaps.
inline HRGN make_region(const Layout &L)
{
    const int r = 2 * L.round;
    HRGN rgn = CreateRoundRectRgn(L.box.left, L.box.top, L.box.right + 1, L.box.bottom + 1, r, r);
    auto add = [&](HRGN part) {
        CombineRgn(rgn, rgn, part, RGN_OR);
        DeleteObject(part);
    };
    const int br = px(12, L.dpi);
    add(CreateRoundRectRgn(L.next.left, L.next.top, L.next.right + 1, L.next.bottom + 1, br, br));
    if (L.content.prevShown) {
        add(CreateRoundRectRgn(L.prev.left, L.prev.top, L.prev.right + 1, L.prev.bottom + 1, br, br));
    }
    add(CreatePolygonRgn(L.arrow, 3, WINDING));
    return rgn;
}

inline void paint_button(HDC dc, HFONT font, const RECT &r, const wchar_t *label,
                         COLORREF fill, COLORREF edge, COLORREF ink, bool halo, UINT dpi)
{
    if (halo) {
        fill = kHalo;
        edge = kGo;
        ink = kText;
    }
    HPEN pen = CreatePen(PS_SOLID, halo ? px(2, dpi) : px(1, dpi), edge);
    HBRUSH brush = CreateSolidBrush(fill);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    const int br = px(12, dpi);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, br, br);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
    SelectObject(dc, font);
    SetTextColor(dc, ink);
    RECT t = r;
    DrawTextW(dc, label, -1, &t, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

inline void paint(HDC dc, Fonts *fonts, const Layout &L, int width, int height, COLORREF back)
{
    HBRUSH backBrush = CreateSolidBrush(back);
    RECT all{ 0, 0, width, height };
    FillRect(dc, &all, backBrush);
    DeleteObject(backBrush);
    SetBkMode(dc, TRANSPARENT);
    const UINT dpi = L.dpi;

    // The words box: the page's bubble, amber edged.
    HPEN amberPen = CreatePen(PS_SOLID, px(1, dpi) < 1 ? 1 : px(1, dpi), kAmber);
    HBRUSH fill = CreateSolidBrush(kFill);
    HGDIOBJ oldPen = SelectObject(dc, amberPen);
    HGDIOBJ oldBrush = SelectObject(dc, fill);
    RoundRect(dc, L.box.left, L.box.top, L.box.right, L.box.bottom, 2 * L.round, 2 * L.round);

    // The count, then the lines, a lit name amber in an amber outline.
    SelectObject(dc, fonts->count);
    SetTextColor(dc, kMuted);
    TextOutW(dc, L.box.left + L.pad, L.box.top + L.pad, L.content.count.c_str(),
             static_cast<int>(L.content.count.size()));
    int y = L.box.top + L.pad + L.countH + px(2, dpi);
    HGDIOBJ hollow = GetStockObject(HOLLOW_BRUSH);
    for (const auto &line : L.lines) {
        int x = L.box.left + L.pad;
        for (const size_t i : line) {
            const tbr::Token &t = L.toks[i];
            SelectObject(dc, t.lit ? fonts->lit : fonts->text);
            TEXTMETRICW tm{};
            GetTextMetricsW(dc, &tm);
            const int ty = y + (L.lineH - tm.tmHeight) / 2;
            if (t.lit) {
                SelectObject(dc, hollow);
                RoundRect(dc, x, y + px(1, dpi), x + L.widths[i], y + L.lineH - px(1, dpi),
                          px(8, dpi), px(8, dpi));
                SetTextColor(dc, kAmber);
                TextOutW(dc, x + L.litPad, ty, t.text.c_str(), static_cast<int>(t.text.size()));
            } else {
                SetTextColor(dc, kText);
                TextOutW(dc, x, ty, t.text.c_str(), static_cast<int>(t.text.size()));
            }
            x += L.widths[i];
        }
        y += L.lineH;
    }

    // The arrow, solid amber, pointing at the lit part.
    HBRUSH amber = CreateSolidBrush(kAmber);
    SelectObject(dc, amber);
    Polygon(dc, L.arrow, 3);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(amberPen);
    DeleteObject(fill);
    DeleteObject(amber);

    // Next: lit while it can be pressed, greyed out on a step that asks for
    // something. Previous: plain. The pad's halo on whichever it is on.
    if (L.content.nextOn) {
        paint_button(dc, fonts->button, L.next, L"Next", kGo, kGo, kGoText, L.content.focus == "next", dpi);
    } else {
        paint_button(dc, fonts->button, L.next, L"Next", kOff, kOffEdge, kOffText, false, dpi);
    }
    if (L.content.prevShown) {
        paint_button(dc, fonts->button, L.prev, L"Previous", kBtn, kBtnEdge, kText,
                     L.content.focus == "prev", dpi);
    }
}

inline Layout g_layout;   // the window thread's alone
inline Fonts g_fonts;     // likewise
inline std::wstring g_shown;

inline HWND configs_window()
{
    ctm_open_ui::FindState state;
    EnumWindows(ctm_open_ui::find_window_proc, reinterpret_cast<LPARAM>(&state));
    return state.found;
}

// ⓘ The frame as it is seen, without the invisible resize border Windows 10
// and 11 put around it, which GetWindowRect counts.
inline RECT frame_of(HWND w)
{
    RECT r{};
    if (FAILED(DwmGetWindowAttribute(w, DWMWA_EXTENDED_FRAME_BOUNDS, &r, sizeof r))) GetWindowRect(w, &r);
    return r;
}

inline void hide_now(HWND hwnd)
{
    if (IsWindowVisible(hwnd)) ShowWindow(hwnd, SW_HIDE);
    g_shown.clear();
}

// ⭐ PLACE AND SHOW, or hide: on every update from the page and on the timer.
// Nothing is moved or drawn again unless something it depends on changed.
inline void refresh(HWND hwnd)
{
    Content c;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        c = g_content;
    }
    const HWND win = c.show ? configs_window() : nullptr;
    if (win == nullptr || IsIconic(win) || GetForegroundWindow() != win) {
        hide_now(hwnd);
        return;
    }
    const UINT dpi = GetDpiForWindow(win);
    const RECT frame = frame_of(win);
    MONITORINFO mi{};
    mi.cbSize = sizeof mi;
    GetMonitorInfoW(MonitorFromWindow(win, MONITOR_DEFAULTTONEAREST), &mi);
    std::wstring key = c.count + L"|" + c.words + L"|" + (c.nextOn ? L"1" : L"0") +
                       (c.prevShown ? L"1" : L"0") + wide(c.focus) + L"|" +
                       std::to_wstring(c.ax) + L"," + std::to_wstring(c.ay) + L"|" +
                       std::to_wstring(frame.left) + L"," + std::to_wstring(frame.top) + L"," +
                       std::to_wstring(frame.right) + L"," + std::to_wstring(frame.bottom) + L"|" +
                       std::to_wstring(mi.rcWork.left) + L"," + std::to_wstring(mi.rcWork.top) + L"," +
                       std::to_wstring(mi.rcWork.right) + L"," + std::to_wstring(mi.rcWork.bottom) + L"|" +
                       std::to_wstring(dpi);
    if (key == g_shown && IsWindowVisible(hwnd)) return;
    HDC dc = GetDC(hwnd);
    g_layout = compute_layout(dc, &g_fonts, c, dpi, frame, mi.rcWork);
    ReleaseDC(hwnd, dc);
    SetWindowRgn(hwnd, make_region(g_layout), FALSE);   // ⓘ Windows owns the region now
    const tbr::Box &w = g_layout.place.window;
    SetWindowPos(hwnd, HWND_TOPMOST, w.left, w.top, w.width(), w.height(),
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(hwnd, nullptr, FALSE);
    g_shown = key;
}

inline LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT cr{};
        GetClientRect(hwnd, &cr);
        // ⓘ Drawn off screen and copied in one go, so a change never flickers.
        HDC mem = CreateCompatibleDC(dc);
        HBITMAP bmp = CreateCompatibleBitmap(dc, cr.right, cr.bottom);
        HGDIOBJ old = SelectObject(mem, bmp);
        paint(mem, &g_fonts, g_layout, cr.right, cr.bottom, kFill);
        BitBlt(dc, 0, 0, cr.right, cr.bottom, mem, 0, 0, SRCCOPY);
        SelectObject(mem, old);
        DeleteObject(bmp);
        DeleteDC(mem);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    // ⛔ NEVER THE FOCUS, as the keyboard window: the configs window keeps it,
    // and with it the controller's input.
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_SETCURSOR: {
        POINT pt{};
        GetCursorPos(&pt);
        ScreenToClient(hwnd, &pt);
        const bool onNext = g_layout.content.nextOn && PtInRect(&g_layout.next, pt);
        const bool onPrev = g_layout.content.prevShown && PtInRect(&g_layout.prev, pt);
        if (onNext || onPrev) {
            SetCursor(LoadCursor(nullptr, IDC_HAND));
            return TRUE;
        }
        break;
    }
    case WM_LBUTTONUP: {
        const POINT pt{ static_cast<short>(LOWORD(lp)), static_cast<short>(HIWORD(lp)) };
        std::string pressed;
        if (g_layout.content.nextOn && PtInRect(&g_layout.next, pt)) pressed = "next";
        else if (g_layout.content.prevShown && PtInRect(&g_layout.prev, pt)) pressed = "prev";
        if (!pressed.empty()) {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_press = pressed;
        }
        return 0;
    }
    case WM_TIMER:
        if (wp == kFollowTimer) refresh(hwnd);
        return 0;
    default:
        if (msg == kMsgUpdate) {
            refresh(hwnd);
            return 0;
        }
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ⛔ ITS OWN THREAD, as the keyboard's: a message loop owns the thread it runs
// on, and none of the listener's other threads can spare one.
inline void thread_main()
{
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);
    // ⭐ THE STYLES, as the keyboard's: never focused, above other windows,
    // and in neither the taskbar nor Alt+Tab.
    const HWND hwnd = CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
                                      kClassName, L"DS5-USBIP tutorial", WS_POPUP,
                                      0, 0, 1, 1, nullptr, nullptr, wc.hInstance, nullptr);
    if (hwnd == nullptr) {
        device_log::session_w() << L"tutorial: the words window could not be made";
        g_running.store(false);
        return;
    }
    g_hwnd.store(hwnd);
    SetTimer(hwnd, kFollowTimer, 100, nullptr);
    refresh(hwnd);
    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    g_hwnd.store(nullptr);
    g_running.store(false);
}

// ---- what the rest of the listener calls --------------------------------------

inline void update(const Content &c)
{
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_content = c;
        // ⓘ A press left over from words no longer shown must not move the next tutorial.
        if (!c.show) g_press.clear();
    }
    if (c.show) {
        bool expected = false;
        if (g_running.compare_exchange_strong(expected, true)) std::thread(thread_main).detach();
    }
    const HWND h = g_hwnd.load();
    if (h != nullptr) PostMessageW(h, kMsgUpdate, 0, 0);
}

inline void hide()
{
    update(Content());
}

// A click on Next or Previous, handed over once: "next", "prev" or "".
inline std::string take_press()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    std::string p = g_press;
    g_press.clear();
    return p;
}

// ⓘ `DS5-USBIP tutorial-words-picture <file.bmp> [dpi]`: the words as one
// step shows them, above a stand-in for Simple's window, drawn into a picture
// rather than onto the screen, so the drawing can be looked at with no window
// and no page.
inline int picture_mode(int argc, wchar_t **argv)
{
    if (argc < 3) {
        std::wcout << L"usage: DS5-USBIP tutorial-words-picture <file.bmp> [dpi]" << std::endl;
        return 2;
    }
    const UINT dpi = argc > 3 ? static_cast<UINT>(_wtoi(argv[3])) : 96;
    Content c;
    c.show = true;
    c.count = L"4 of 10";
    c.words = L"Select [[stick-to-mouse]] to see what it does. It works with every controller.";
    c.nextOn = false;
    c.prevShown = true;
    c.focus = "prev";
    c.ax = 300;
    c.ay = 250;
    const RECT work{ 0, 0, px(1920, dpi), px(1040, dpi) };
    const RECT frame{ px(528, dpi), px(560, dpi), px(1392, dpi), px(1030, dpi) };
    HDC screen = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(screen);
    Fonts fonts;
    const Layout L = compute_layout(mem, &fonts, c, dpi, frame, work);
    const int w = L.place.window.width(), h = L.place.window.height();
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = h;   // bottom-up, as a .bmp file keeps it
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void *bits = nullptr;
    HBITMAP bmp = CreateDIBSection(mem, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ old = SelectObject(mem, bmp);
    // ⓘ The desktop shows where the window's shape leaves gaps.
    HBRUSH desk = CreateSolidBrush(kDesk);
    RECT all{ 0, 0, w, h };
    FillRect(mem, &all, desk);
    DeleteObject(desk);
    HRGN rgn = make_region(L);
    SelectClipRgn(mem, rgn);
    paint(mem, &fonts, L, w, h, kFill);
    SelectClipRgn(mem, nullptr);
    DeleteObject(rgn);
    GdiFlush();
    BITMAPFILEHEADER fh{};
    const DWORD bytes = static_cast<DWORD>(w) * static_cast<DWORD>(h) * 4;
    fh.bfType = 0x4D42;
    fh.bfOffBits = sizeof fh + sizeof bi.bmiHeader;
    fh.bfSize = fh.bfOffBits + bytes;
    std::ofstream out(argv[2], std::ios::binary);
    out.write(reinterpret_cast<const char *>(&fh), sizeof fh);
    out.write(reinterpret_cast<const char *>(&bi.bmiHeader), sizeof bi.bmiHeader);
    out.write(static_cast<const char *>(bits), bytes);
    const bool ok = static_cast<bool>(out);
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
    for (HFONT f : { fonts.text, fonts.lit, fonts.count, fonts.button }) {
        if (f != nullptr) DeleteObject(f);
    }
    std::wcout << (ok ? L"wrote " : L"could not write ") << argv[2] << L" (" << w << L"x" << h
               << L", " << (L.place.side == tbr::Side::Above ? L"above" : L"not above") << L")" << std::endl;
    return ok ? 0 : 1;
}

}  // namespace tutorial_bubble
