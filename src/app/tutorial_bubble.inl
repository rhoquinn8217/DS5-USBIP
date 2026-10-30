// The tutorial's words, on the desktop beside the Controller Configs window.
//
// ⭐ OUTSIDE THE WINDOW (rhoquinn8217, 2026-10-14, after going through the
// tutorial on the hardware: *"We need to move the descriptions so that they
// don't cover any of the browser box."*). A page cannot draw outside its own
// window, so the listener draws them, in a small window of its own that never
// takes focus, as the on-screen keyboard does (overlay_window.inl): the
// controller goes on driving the configs window while it is up.
//
// The page sends what a step says, the step count, the buttons under the
// words and how each looks, which one the pad's halo is on, and where the lit
// part sits in its view (the /api/v1/ui/tutorial-bubble route). The start's
// card and the "End the tutorial?" question come the same way, with a title
// and no arrow, since they point at nothing. A click on a button here waits
// in take_press() until the page collects it. Where the window goes, how its
// lines break and how the buttons are named: tutorial_bubble_rule.inl.
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

// A button under the words, as the page named it.
struct Button {
    std::string id;           // what a click hands back: "next", "prev", "end", ...
    std::wstring label;
    std::string kind;         // "go", "off" or "plain" (tutorial_bubble_rule::parse_buttons)
    std::wstring mark;        // the pad's button for it, "*R2", or a key's name; "" for none
};

// What a step shows, as the page last sent it.
struct Content {
    bool show = false;
    std::wstring title;       // the start's card and the question; empty on a step
    std::wstring count;       // "3 of 11"; empty with a title
    std::wstring words;       // lit names between [[ and ]]
    std::vector<Button> buttons;   // left to right, at the words' right
    std::string focus;        // the pad's halo: a button's id, or ""
    bool arrow = true;        // pointing at the lit part; none when nothing is lit
    int ax = 500, ay = 500;   // the lit part's middle, thousandths of the page's view
    std::vector<std::pair<int, int>> anchors;   // every arrow's, when there are several
    tbr::Prefer prefer = tbr::Prefer::None;     // the side the step asks for
    // ⭐ What the step lights and what is on the window, thousandths of the
    // page's view, for the words to stand beside the one and off the other
    // (tbr::place_near); `clear` is the room the page's arrow takes, in
    // thousandths of the view's height. No target: the rule above places them.
    bool hasTarget = false;
    tbr::Box target;
    std::vector<tbr::Box> obstacles;
    int clear = 0;
    // ⭐ LARGER, for the card that ends a tour (rhoquinn8217, 2026-10-29: "Add
    // Congratulation bubble with larger text with confetti drop animation
    // after last step"); the confetti is the page's.
    bool big = false;
};

inline std::mutex g_mutex;
inline Content g_content;
inline std::string g_press;
// ⭐ WHETHER THE WORDS ARE STILL ON THEIR WAY (rhoquinn8217, 2026-10-29: "when
// the bubble move to it's next spot remove the darkening completely then fade
// in the darkening with the feather highlighted area"): each set of words the
// page sends is counted, and the window's thread marks the count it has
// placed once they stand still there, slid or not. The page fades its dim
// back in when the two agree (moving()).
inline std::atomic<unsigned> g_posted{ 0 };
inline std::atomic<unsigned> g_settled{ 0 };
// ⓘ Whether a tour is up: its words sent to be shown, from the opening card
// to its end (up()).
inline std::atomic_bool g_up{ false };
inline unsigned g_placing = 0;   // the window thread's alone: the count it is placing
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
    HFONT text = nullptr, lit = nullptr, count = nullptr, button = nullptr, title = nullptr;
    // ⓘ The congratulations card's (Content::big).
    HFONT bigText = nullptr, bigLit = nullptr, bigButton = nullptr, bigTitle = nullptr;
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
    for (HFONT h : { f->text, f->lit, f->count, f->button, f->title, f->bigText, f->bigLit, f->bigButton,
                     f->bigTitle }) {
        if (h != nullptr) DeleteObject(h);
    }
    f->dpi = dpi;
    f->text = make_font(16, FW_NORMAL, dpi);
    f->lit = make_font(16, FW_SEMIBOLD, dpi);
    f->count = make_font(12, FW_NORMAL, dpi);
    f->button = make_font(15, FW_SEMIBOLD, dpi);
    f->title = make_font(18, FW_SEMIBOLD, dpi);
    f->bigText = make_font(26, FW_NORMAL, dpi);
    f->bigLit = make_font(26, FW_SEMIBOLD, dpi);
    f->bigButton = make_font(22, FW_SEMIBOLD, dpi);
    f->bigTitle = make_font(52, FW_BOLD, dpi);
}

// ⓘ The words', the title's and the buttons' type, larger on the card that
// ends a tour.
inline HFONT words_font(const Fonts *f, const Content &c, bool lit)
{
    return c.big ? (lit ? f->bigLit : f->bigText) : (lit ? f->lit : f->text);
}
inline HFONT title_font(const Fonts *f, const Content &c) { return c.big ? f->bigTitle : f->title; }
inline HFONT button_font(const Fonts *f, const Content &c) { return c.big ? f->bigButton : f->button; }

// Everything a paint and a click need, in the words window's own pixels.
struct Layout {
    Content content;
    UINT dpi = 96;
    tbr::Placement place;
    RECT box{};
    std::vector<RECT> btns;   // one for each of content.buttons
    struct Tri { POINT p[3]; };
    std::vector<Tri> arrows;  // none when nothing is lit
    bool hasArrow = false;
    int pad = 0, round = 0, titleH = 0, countH = 0, lineH = 0, litPad = 0, markH = 0;
    std::vector<tbr::Token> toks;
    std::vector<int> widths;
    std::vector<std::vector<size_t>> lines;
    // ⓘ The card that ends a tour: its words and buttons in the middle of it,
    // from this height down.
    bool centred = false;
    int contentTop = 0;
};

inline SIZE measure(HDC dc, HFONT font, const std::wstring &text)
{
    SIZE s{};
    SelectObject(dc, font);
    GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &s);
    return s;
}

// ⭐ A BUTTON DRAWN AS THE LEGENDS DRAW IT (rhoquinn8217, 2026-10-23: "Can you
// make the legend button symbols in the tutorial?"): the page marks one as
// [[*glyph]] (tutorial_bubble_rule::tokens), and the on-screen keyboard's own
// drawing does it (ctm_overlay::draw_mark: a PlayStation shape or a letter in a
// circle, a shoulder button in its box). The d-pad, which the keyboard never
// shows, is drawn here as the page draws it: a cross, 20 units square.
inline const wchar_t *const kDpad = L"\u271c";

inline bool gdiplus_up()
{
    static ULONG_PTR token = 0;
    static const bool up = [] {
        Gdiplus::GdiplusStartupInput in;
        return Gdiplus::GdiplusStartup(&token, &in, nullptr) == Gdiplus::Ok;
    }();
    return up;
}

inline int mark_w(const std::wstring &glyph, int h)
{
    return glyph == kDpad ? h : ctm_overlay::mark_width(glyph.c_str(), h);
}

// ⭐ THE BUTTON THAT PRESSES IT, in it, before its name (2026-10-29: "add R2
// symbol in the "Continue" button, L2 to the "Go Back" button and "Circle" to
// the "End Tutorial" button"): the pad's, drawn as the legends draw it, or a
// keyboard's key, its name in a key's outline. Its height, and the width it
// and the gap after it take.
inline int button_mark_h(const Content &c, UINT dpi) { return px(c.big ? 24 : 17, dpi); }
inline int button_mark_w(HDC dc, const Fonts *f, const Button &b, int h, UINT dpi)
{
    if (b.mark.empty()) return 0;
    const int gap = px(7, dpi);
    if (b.mark.size() > 1 && b.mark[0] == L'*') return mark_w(b.mark.substr(1), h) + gap;
    const SIZE s = measure(dc, f->count, b.mark);
    return s.cx + 2 * px(5, dpi) + gap;
}

inline void draw_glyph_mark(HDC dc, int x, int y, int h, const std::wstring &glyph, COLORREF colour)
{
    if (glyph != kDpad) {
        ctm_overlay::draw_mark(dc, x, y, h, glyph.c_str(), colour);
        return;
    }
    if (!gdiplus_up()) return;
    const float s = h / 20.0f;
    Gdiplus::Graphics g(dc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    Gdiplus::Pen pen(Gdiplus::Color(255, GetRValue(colour), GetGValue(colour), GetBValue(colour)),
                     s * 1.25f < 1.0f ? 1.0f : s * 1.25f);
    pen.SetLineJoin(Gdiplus::LineJoinRound);
    const float u[12][2] = { { 7.6f, 3.2f }, { 12.4f, 3.2f }, { 12.4f, 7.6f }, { 16.8f, 7.6f },
                             { 16.8f, 12.4f }, { 12.4f, 12.4f }, { 12.4f, 16.8f }, { 7.6f, 16.8f },
                             { 7.6f, 12.4f }, { 3.2f, 12.4f }, { 3.2f, 7.6f }, { 7.6f, 7.6f } };
    Gdiplus::PointF p[12];
    for (int i = 0; i < 12; ++i) p[i] = Gdiplus::PointF(x + u[i][0] * s, y + u[i][1] * s);
    g.DrawPolygon(&pen, p, 12);
}

// ⓘ Where the words stand now, and the side of what is lit they were placed
// on there.
struct Standing {
    POINT at;
    tbr::Side side;
};

// ⭐ THE LAYOUT, from the words and from where the configs window and the
// screen's working area are. The words box, its buttons under it at its
// right, and the arrow on the side facing the window, when there is one.
// `client` is the window's page, which the page's thousandths are of; `keep`,
// when given, is where the words stand now, kept while it covers nothing.
inline Layout compute_layout(HDC dc, Fonts *fonts, const Content &c, UINT dpi,
                             const RECT &frame, const RECT &work, const RECT &client,
                             const Standing *keep = nullptr)
{
    fonts_for(fonts, dpi);
    Layout L;
    L.content = c;
    L.dpi = dpi;
    L.pad = px(c.big ? 28 : 12, dpi);
    L.round = px(10, dpi);
    L.litPad = px(5, dpi);
    L.hasArrow = c.arrow;
    const int maxWidth = px(c.big ? 560 : 380, dpi);
    const int between = px(8, dpi);
    const int btnPadX = px(14, dpi), btnPadY = px(6, dpi);
    const int arrow = c.arrow ? px(12, dpi) : 0, gap = px(10, dpi);

    L.toks = tbr::tokens(tbr::parse(c.words));
    // ⓘ A button's mark at the page's 1.1 of the words' size, with a little
    // room either side.
    L.markH = px(c.big ? 24 : 18, dpi);
    for (const tbr::Token &t : L.toks) {
        if (t.mark) {
            L.widths.push_back(mark_w(t.text, L.markH) + 2 * px(2, dpi));
            continue;
        }
        const SIZE s = measure(dc, words_font(fonts, c, t.lit), t.text);
        L.widths.push_back(s.cx + (t.lit ? 2 * L.litPad : 0));
    }
    L.lines = tbr::lines(L.toks, L.widths, maxWidth);
    TEXTMETRICW tm{};
    SelectObject(dc, words_font(fonts, c, false));
    GetTextMetricsW(dc, &tm);
    L.lineH = tm.tmHeight + px(6, dpi);
    int textW = 0;
    if (!c.title.empty()) {
        const SIZE s = measure(dc, title_font(fonts, c), c.title);
        L.titleH = s.cy + px(4, dpi);
        textW = s.cx;
    }
    if (!c.count.empty()) {
        const SIZE s = measure(dc, fonts->count, c.count);
        L.countH = s.cy + px(2, dpi);
        if (s.cx > textW) textW = s.cx;
    }
    for (const auto &line : L.lines) {
        int w = 0;
        for (const size_t i : line) w += L.widths[i];
        if (w > textW) textW = w;
    }
    int boxW = textW + 2 * L.pad;
    int boxH = L.pad + L.titleH + L.countH + static_cast<int>(L.lines.size()) * L.lineH + L.pad;
    const int textH = L.titleH + L.countH + static_cast<int>(L.lines.size()) * L.lineH;

    // ⓘ The buttons in the page's order, as wide as their names, one height.
    std::vector<int> btnW;
    int rowW = 0, btnH = 0;
    const int btnMarkH = button_mark_h(c, dpi);
    for (const Button &b : c.buttons) {
        const SIZE s = measure(dc, button_font(fonts, c), b.label);
        btnW.push_back(button_mark_w(dc, fonts, b, btnMarkH, dpi) + s.cx + 2 * btnPadX);
        if (rowW > 0) rowW += between;
        rowW += btnW.back();
        if (s.cy + 2 * btnPadY > btnH) btnH = s.cy + 2 * btnPadY;
    }
    // ⭐ THE CARD THAT ENDS A TOUR, LARGER AND CENTRED (rhoquinn8217,
    // 2026-10-29: "Make the Congraduation step box bigger, horizontallly an
    // vertically centered."): its buttons inside it, under its words, the
    // lot in the middle of a card at least 680 by 320, and the card over the
    // window's middle (tbr::place_at, Center).
    const int innerGap = px(22, dpi);
    const int innerH = textH + (c.buttons.empty() ? 0 : innerGap + btnH);
    if (c.big) {
        L.centred = true;
        const int wide = (textW > rowW ? textW : rowW) + 2 * L.pad;
        boxW = wide > px(680, dpi) ? wide : px(680, dpi);
        boxH = innerH + 2 * L.pad > px(320, dpi) ? innerH + 2 * L.pad : px(320, dpi);
    }
    int stackW = c.big ? boxW : (boxW > rowW ? boxW : rowW);
    const int stackH = c.big ? boxH : boxH + (c.buttons.empty() ? 0 : between + btnH);

    const tbr::Box win{ frame.left, frame.top, frame.right, frame.bottom };
    const tbr::Box area{ work.left, work.top, work.right, work.bottom };
    std::vector<std::pair<int, int>> anchors = c.anchors;
    if (anchors.empty()) anchors.push_back({ c.ax, c.ay });
    L.place = tbr::place_at(win, area, stackW, stackH, arrow, gap, L.round, anchors, c.prefer);
    // ⭐ BESIDE WHAT IS LIT, OVER THE WINDOW WHERE IT COVERS NOTHING
    // (tbr::place_near): the page's thousandths made the page's pixels, its
    // title bar one more thing to keep off. Where the words stand now stays,
    // while it still covers nothing, so a tip changing or the wheel turning
    // under them moves nothing.
    if (c.hasTarget && c.prefer != tbr::Prefer::Center) {
        const int cw = client.right - client.left, ch = client.bottom - client.top;
        const auto onPage = [&](const tbr::Box &b) {
            return tbr::Box{ client.left + cw * b.left / 1000, client.top + ch * b.top / 1000,
                             client.left + cw * b.right / 1000, client.top + ch * b.bottom / 1000 };
        };
        std::vector<tbr::Box> obstacles;
        for (const tbr::Box &b : c.obstacles) obstacles.push_back(onPage(b));
        if (client.top > frame.top) obstacles.push_back({ frame.left, frame.top, frame.right, client.top });
        const int margin = px(8, dpi);
        const tbr::Box target = onPage(c.target);
        tbr::Placement beside;
        bool kept = false;
        if (keep != nullptr) {
            const tbr::Box here{ keep->at.x, keep->at.y, keep->at.x + stackW, keep->at.y + stackH };
            const bool onScreen = here.left >= area.left + margin && here.top >= area.top + margin &&
                                  here.right <= area.right - margin && here.bottom <= area.bottom - margin;
            if (onScreen && !tbr::overlaps_any(here, obstacles) && !tbr::overlaps_any(here, { target })) {
                beside.window = here;
                // ⓘ The side they were placed on, as it was said to the page,
                // so its arrow does not turn while the words stay put.
                beside.side = keep->side;
                beside.arrowsAt.assign(1, 0);
                kept = true;
            }
        }
        if (kept || tbr::place_near(target, area, stackW, stackH, obstacles, c.prefer,
                                    ch * c.clear / 1000, margin, px(900, dpi), px(6, dpi), &beside)) {
            L.place = beside;
        }
    }
    // ⓘ Widened to stand over two arrows far apart: the words box spans it.
    int wordsW = boxW;
    const bool across = L.place.side == tbr::Side::Above || L.place.side == tbr::Side::Below;
    if (across && L.place.window.width() > stackW) {
        stackW = L.place.window.width();
        wordsW = stackW;
    }

    // ⓘ The words and buttons start after the arrow when it is on their top
    // or left side.
    const int offX = L.place.side == tbr::Side::Right ? arrow : 0;
    const int offY = L.place.side == tbr::Side::Below ? arrow : 0;
    L.box = { offX + stackW - wordsW, offY, offX + stackW, offY + boxH };
    if (L.centred) {
        L.contentTop = L.box.top + (boxH - innerH) / 2;
        const int inTop = L.contentTop + textH + innerGap;
        int bx = L.box.left + (boxW - rowW) / 2;
        for (size_t i = 0; i < c.buttons.size(); ++i) {
            L.btns.push_back({ bx, inTop, bx + btnW[i], inTop + btnH });
            bx += btnW[i] + between;
        }
        return L;
    }
    const int rowTop = offY + boxH + between;
    // [Continue] [Go back] [End tutorial], in rhoquinn8217's order, at the
    // box's right.
    int x = offX + stackW - rowW;
    for (size_t i = 0; i < c.buttons.size(); ++i) {
        L.btns.push_back({ x, rowTop, x + btnW[i], rowTop + btnH });
        x += btnW[i] + between;
    }
    // ⓘ None over the window's middle: there is nothing beside it to point at.
    if (!c.arrow || c.prefer == tbr::Prefer::Center) return L;
    for (const int a : L.place.arrowsAt) {
        Layout::Tri t{};
        switch (L.place.side) {
        case tbr::Side::Above:
            t.p[0] = { a - arrow, stackH };
            t.p[1] = { a + arrow, stackH };
            t.p[2] = { a, stackH + arrow };
            break;
        case tbr::Side::Below:
            t.p[0] = { a - arrow, arrow };
            t.p[1] = { a + arrow, arrow };
            t.p[2] = { a, 0 };
            break;
        case tbr::Side::Right:
            t.p[0] = { arrow, a - arrow };
            t.p[1] = { arrow, a + arrow };
            t.p[2] = { 0, a };
            break;
        case tbr::Side::Left:
            t.p[0] = { stackW, a - arrow };
            t.p[1] = { stackW, a + arrow };
            t.p[2] = { stackW + arrow, a };
            break;
        }
        L.arrows.push_back(t);
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
    for (const RECT &b : L.btns) {
        add(CreateRoundRectRgn(b.left, b.top, b.right + 1, b.bottom + 1, br, br));
    }
    for (const Layout::Tri &t : L.arrows) add(CreatePolygonRgn(t.p, 3, WINDING));
    return rgn;
}

inline void paint_button(HDC dc, const Fonts *fonts, const Content &c, const Button &b, const RECT &r,
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
    HFONT font = button_font(fonts, c);
    SetTextColor(dc, ink);
    if (b.mark.empty()) {
        SelectObject(dc, font);
        RECT t = r;
        DrawTextW(dc, b.label.c_str(), -1, &t, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }
    // ⓘ The mark, then the name, as one in the middle of the button.
    const int h = button_mark_h(c, dpi);
    const int mw = button_mark_w(dc, fonts, b, h, dpi);
    const SIZE ls = measure(dc, font, b.label);
    int x = r.left + (r.right - r.left - (mw + ls.cx)) / 2;
    const int mid = (r.top + r.bottom) / 2;
    if (b.mark.size() > 1 && b.mark[0] == L'*') {
        draw_glyph_mark(dc, x, mid - h / 2, h, b.mark.substr(1), ink);
    } else {
        const SIZE ks = measure(dc, fonts->count, b.mark);
        const int kw = ks.cx + 2 * px(5, dpi);
        HPEN keyPen = CreatePen(PS_SOLID, px(1, dpi) < 1 ? 1 : px(1, dpi), ink);
        HGDIOBJ oldKeyPen = SelectObject(dc, keyPen);
        HGDIOBJ oldKeyBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
        RoundRect(dc, x, mid - h / 2, x + kw, mid + h / 2, px(5, dpi), px(5, dpi));
        SelectObject(dc, oldKeyPen);
        SelectObject(dc, oldKeyBrush);
        DeleteObject(keyPen);
        SelectObject(dc, fonts->count);
        TextOutW(dc, x + px(5, dpi), mid - ks.cy / 2, b.mark.c_str(), static_cast<int>(b.mark.size()));
    }
    SelectObject(dc, font);
    TextOutW(dc, x + mw, mid - ls.cy / 2, b.label.c_str(), static_cast<int>(b.label.size()));
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

    // The title or the count, then the lines, a lit name amber in an amber
    // outline.
    int y = L.centred ? L.contentTop : L.box.top + L.pad;
    const int boxWidth = L.box.right - L.box.left;
    if (!L.content.title.empty()) {
        SelectObject(dc, title_font(fonts, L.content));
        SetTextColor(dc, L.content.big ? kAmber : kText);
        const int tx = L.centred ? L.box.left + (boxWidth - measure(dc, title_font(fonts, L.content), L.content.title).cx) / 2
                                 : L.box.left + L.pad;
        TextOutW(dc, tx, y, L.content.title.c_str(), static_cast<int>(L.content.title.size()));
        y += L.titleH;
    }
    if (!L.content.count.empty()) {
        SelectObject(dc, fonts->count);
        SetTextColor(dc, kMuted);
        TextOutW(dc, L.box.left + L.pad, y, L.content.count.c_str(),
                 static_cast<int>(L.content.count.size()));
        y += L.countH;
    }
    HGDIOBJ hollow = GetStockObject(HOLLOW_BRUSH);
    for (const auto &line : L.lines) {
        int x = L.box.left + L.pad;
        if (L.centred) {
            int lineW = 0;
            for (const size_t i : line) lineW += L.widths[i];
            x = L.box.left + (boxWidth - lineW) / 2;
        }
        for (const size_t i : line) {
            const tbr::Token &t = L.toks[i];
            SelectObject(dc, words_font(fonts, L.content, t.lit));
            TEXTMETRICW tm{};
            GetTextMetricsW(dc, &tm);
            const int ty = y + (L.lineH - tm.tmHeight) / 2;
            if (t.mark) {
                draw_glyph_mark(dc, x + px(2, dpi), y + (L.lineH - L.markH) / 2, L.markH, t.text, kAmber);
            } else if (t.lit) {
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
    for (const Layout::Tri &t : L.arrows) Polygon(dc, t.p, 3);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(amberPen);
    DeleteObject(fill);
    DeleteObject(amber);

    // ⭐ The buttons as the page named them: "go" lit, the way on; "off"
    // greyed out, as Continue is on a step that asks for something; the rest
    // plain. The pad's halo on whichever it is on, never on a greyed one.
    for (size_t i = 0; i < L.btns.size() && i < L.content.buttons.size(); ++i) {
        const Button &b = L.content.buttons[i];
        const bool halo = L.content.focus == b.id;
        if (b.kind == "go") {
            paint_button(dc, fonts, L.content, b, L.btns[i], kGo, kGo, kGoText, halo, dpi);
        } else if (b.kind == "off") {
            paint_button(dc, fonts, L.content, b, L.btns[i], kOff, kOffEdge, kOffText, false, dpi);
        } else {
            paint_button(dc, fonts, L.content, b, L.btns[i], kBtn, kBtnEdge, kText, halo, dpi);
        }
    }
}

inline Layout g_layout;   // the window thread's alone
inline Fonts g_fonts;     // likewise
inline std::wstring g_shown;
inline std::string g_side;          // the side the words stand on, for the page's arrow (g_mutex)

// ⓘ A step is its words; the rest the page says about it (the halo, the
// buttons' looks, what is lit and what is on the window) can change within
// it.
inline std::wstring step_of(const Content &c)
{
    return c.title + L"|" + c.count + L"|" + c.words;
}

// ⓘ What the words were last placed for: the step, the configs window's place
// with the screen's, and what the step lit; and the side of it they stand on.
struct Placed {
    std::wstring step;
    std::wstring place;
    bool hasTarget = false;
    tbr::Box target;
    tbr::Side side = tbr::Side::Above;
};
inline Placed g_placed;   // the window thread's alone

// ⭐ THE WORDS STAY WHERE THEY STAND ONLY BESIDE WHAT THEY WERE PLACED FOR
// (rhoquinn8217, 2026-10-27: "Sometimes window is positioned in a different
// spot then it was origianlly back to the right place when you go back"):
// the same step, on the configs window where it was, lighting the same part.
// They stayed for the same step alone, so words placed beside the part the
// last step lit, before the page said what this one lights, stayed there:
// beside the step before's going forward, the step after's going back. Now a
// step reached, gone back to, or its lit part appearing or moving, places
// them afresh beside it (compute_layout, tbr::place_near), so a step's words
// stand in one place however it was reached. Within it they still stay while
// they cover nothing, as a tip changes or the wheel turns under them.
inline bool still_for(const Placed &was, const Content &c, const std::wstring &place)
{
    const tbr::Box &a = was.target, &b = c.target;
    return step_of(c) == was.step && place == was.place && c.hasTarget == was.hasTarget &&
           (!c.hasTarget || (a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom));
}
inline Placed placed_for(const Content &c, const std::wstring &place, tbr::Side side)
{
    return Placed{ step_of(c), place, c.hasTarget, c.target, side };
}

// ⭐ ONE WORDS BOX, SLIDING TO THE NEXT STEP (rhoquinn8217, 2026-10-26: "I
// want a persistent tutorial bubble that move to where the next step is"):
// from where it stands to its new place, starting slowly, quickest half way
// and slowing as it arrives (tbr::ease_in_out), a third to a half of a
// second by how far it goes (tbr::slide_ms).
// ⭐ A STEP EVERY FRAME (2026-10-27, "Make animation for fluid"): the
// window's thread moves it once each time the screen refreshes (frame_wait,
// in thread_main's loop), where it was a 15 ms timer, which Windows runs
// at 64 a second at most, against the stream's 120; and the time gone is the
// steady clock's, where GetTickCount64 moved in steps of 15.6 ms.
inline POINT g_slideFrom{}, g_slideTo{};
inline std::chrono::steady_clock::time_point g_slideAt;
inline int g_slideMs = 0;
inline bool g_sliding = false;

inline void slide_stop(HWND)
{
    g_sliding = false;
}

inline void slide_step(HWND hwnd)
{
    const double gone = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - g_slideAt).count();
    const double t = g_slideMs <= 0 || gone >= g_slideMs ? 1.0 : gone / g_slideMs;
    const double e = tbr::ease_in_out(t);
    const int x = g_slideFrom.x + static_cast<int>((g_slideTo.x - g_slideFrom.x) * e + (g_slideTo.x >= g_slideFrom.x ? 0.5 : -0.5));
    const int y = g_slideFrom.y + static_cast<int>((g_slideTo.y - g_slideFrom.y) * e + (g_slideTo.y >= g_slideFrom.y ? 0.5 : -0.5));
    SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    if (t >= 1.0) slide_stop(hwnd);
}

inline void slide_to(HWND hwnd, POINT from, POINT to)
{
    g_slideFrom = from;
    g_slideTo = to;
    g_slideAt = std::chrono::steady_clock::now();
    const double dx = to.x - from.x, dy = to.y - from.y;
    g_slideMs = tbr::slide_ms(std::sqrt(dx * dx + dy * dy), GetDpiForWindow(hwnd));
    g_sliding = true;
}

// ⓘ Until the screen's next refresh: DwmFlush waits for the desktop's next
// composition, so each step of a slide is a frame of its own. One that did
// not wait, with nothing being composed, gives way to a short sleep.
inline void frame_wait()
{
    const auto before = std::chrono::steady_clock::now();
    if (FAILED(DwmFlush()) || std::chrono::steady_clock::now() - before < std::chrono::milliseconds(1)) Sleep(4);
}

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
    slide_stop(hwnd);
    if (IsWindowVisible(hwnd)) ShowWindow(hwnd, SW_HIDE);
    g_shown.clear();
    g_placed = Placed();
    std::lock_guard<std::mutex> lock(g_mutex);
    g_side.clear();
}

inline std::wstring box_text(const tbr::Box &b)
{
    return std::to_wstring(b.left) + L"," + std::to_wstring(b.top) + L"," + std::to_wstring(b.right) + L"," +
           std::to_wstring(b.bottom) + L";";
}

// ⭐ PLACE AND SHOW, or hide: on every update from the page and on the timer.
// Nothing is moved or drawn again unless something it depends on changed.
// ⭐ A new place on the same screen is SLID to; the configs window moved, or
// the words shown for the first time, they are put there at once.
inline void refresh(HWND hwnd)
{
    Content c;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        c = g_content;
        g_placing = g_posted.load();
    }
    const HWND win = c.show ? configs_window() : nullptr;
    if (win == nullptr || IsIconic(win) || GetForegroundWindow() != win) {
        hide_now(hwnd);
        return;
    }
    const UINT dpi = GetDpiForWindow(win);
    const RECT frame = frame_of(win);
    // ⓘ The page: the window's client area, on the screen.
    RECT client{};
    GetClientRect(win, &client);
    MapWindowPoints(win, nullptr, reinterpret_cast<POINT *>(&client), 2);
    MONITORINFO mi{};
    mi.cbSize = sizeof mi;
    GetMonitorInfoW(MonitorFromWindow(win, MONITOR_DEFAULTTONEAREST), &mi);
    std::wstring key = step_of(c) + L"|" + (c.arrow ? L"1" : L"0") + wide(c.focus) + L"|";
    for (const Button &b : c.buttons) key += wide(b.id) + L":" + b.label + L":" + wide(b.kind) + L";";
    key += L"|" + std::to_wstring(static_cast<int>(c.prefer)) + L"|";
    for (const auto &a : c.anchors) key += std::to_wstring(a.first) + L"," + std::to_wstring(a.second) + L";";
    key += L"|" + std::to_wstring(c.ax) + L"," + std::to_wstring(c.ay) + L"|";
    if (c.hasTarget) key += box_text(c.target);
    key += L"|";
    for (const tbr::Box &b : c.obstacles) key += box_text(b);
    key += L"|" + std::to_wstring(c.clear);
    const std::wstring place = std::to_wstring(frame.left) + L"," + std::to_wstring(frame.top) + L"," +
                               std::to_wstring(frame.right) + L"," + std::to_wstring(frame.bottom) + L"|" +
                               std::to_wstring(client.left) + L"," + std::to_wstring(client.top) + L"," +
                               std::to_wstring(client.right) + L"," + std::to_wstring(client.bottom) + L"|" +
                               std::to_wstring(mi.rcWork.left) + L"," + std::to_wstring(mi.rcWork.top) + L"," +
                               std::to_wstring(mi.rcWork.right) + L"," + std::to_wstring(mi.rcWork.bottom) + L"|" +
                               std::to_wstring(dpi);
    key += L"#" + place;
    if (key == g_shown && IsWindowVisible(hwnd)) return;
    const bool visible = IsWindowVisible(hwnd) != FALSE;
    RECT now{};
    GetWindowRect(hwnd, &now);
    // ⓘ Where the words stand, or are on their way to.
    const POINT here = g_sliding ? g_slideTo : POINT{ now.left, now.top };
    const bool keep = visible && still_for(g_placed, c, place);
    HDC dc = GetDC(hwnd);
    const Standing standing{ here, g_placed.side };
    g_layout = compute_layout(dc, &g_fonts, c, dpi, frame, mi.rcWork, client, keep ? &standing : nullptr);
    ReleaseDC(hwnd, dc);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_side = c.prefer == tbr::Prefer::Center ? "center" : tbr::side_name(g_layout.place.side);
    }
    SetWindowRgn(hwnd, make_region(g_layout), FALSE);   // ⓘ Windows owns the region now
    const tbr::Box &w = g_layout.place.window;
    const POINT to{ w.left, w.top };
    const UINT show = SWP_NOACTIVATE | SWP_SHOWWINDOW;
    if (visible && place == g_placed.place && g_sliding && to.x == g_slideTo.x && to.y == g_slideTo.y) {
        // ⓘ On its way there already: only the size, and the slide carries on.
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, w.width(), w.height(), show | SWP_NOMOVE);
    } else if (visible && place == g_placed.place && (now.left != to.x || now.top != to.y)) {
        SetWindowPos(hwnd, HWND_TOPMOST, now.left, now.top, w.width(), w.height(), show);
        slide_to(hwnd, POINT{ now.left, now.top }, to);
    } else {
        slide_stop(hwnd);
        SetWindowPos(hwnd, HWND_TOPMOST, to.x, to.y, w.width(), w.height(), show);
    }
    InvalidateRect(hwnd, nullptr, FALSE);
    g_shown = key;
    g_placed = placed_for(c, place, g_layout.place.side);
}

// The id of the button under a point, or "": a greyed one answers nothing.
inline std::string button_at(const Layout &L, POINT pt)
{
    for (size_t i = 0; i < L.btns.size() && i < L.content.buttons.size(); ++i) {
        if (L.content.buttons[i].kind != "off" && PtInRect(&L.btns[i], pt)) return L.content.buttons[i].id;
    }
    return std::string();
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
        if (!button_at(g_layout, pt).empty()) {
            SetCursor(LoadCursor(nullptr, IDC_HAND));
            return TRUE;
        }
        break;
    }
    case WM_LBUTTONUP: {
        const POINT pt{ static_cast<short>(LOWORD(lp)), static_cast<short>(HIWORD(lp)) };
        const std::string pressed = button_at(g_layout, pt);
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
    // ⓘ While the words slide: what is waiting, then a frame, then a step;
    // otherwise asleep until a message comes.
    MSG m;
    bool quit = false;
    while (!quit) {
        if (!g_sliding) {
            // ⓘ At rest: the words last placed stand where they go, or are
            // hidden, which is as still.
            g_settled.store(g_placing);
            if (GetMessageW(&m, nullptr, 0, 0) <= 0) break;
            TranslateMessage(&m);
            DispatchMessageW(&m);
            continue;
        }
        while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) {
            if (m.message == WM_QUIT) { quit = true; break; }
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
        if (quit || !g_sliding) continue;
        frame_wait();
        if (g_sliding) slide_step(hwnd);
    }
    g_hwnd.store(nullptr);
    g_running.store(false);
}

// ---- what the rest of the listener calls --------------------------------------

// The words as the page sends them (the /api/v1/ui/tutorial-bubble route),
// made into what the window shows.
inline Content content_of(const std::string &title, const std::string &count, const std::string &words,
                          const std::string &buttons, const std::string &focus, bool arrow, int ax, int ay,
                          const std::string &side, const std::string &anchors, const std::string &target,
                          const std::string &obstacles, int clear, bool big = false)
{
    Content c;
    c.show = true;
    c.big = big;
    c.title = wide(title);
    c.count = wide(count);
    c.words = wide(words);
    for (const tbr::ButtonSpec &b : tbr::parse_buttons(buttons)) {
        c.buttons.push_back({ b.id, wide(b.label), b.kind, wide(b.mark) });
    }
    c.focus = focus;
    c.arrow = arrow;
    c.ax = ax;
    c.ay = ay;
    c.prefer = tbr::parse_prefer(side);
    c.anchors = tbr::parse_anchors(anchors);
    const std::vector<tbr::Box> lit = tbr::parse_boxes(target);
    c.hasTarget = !lit.empty();
    if (c.hasTarget) c.target = lit.front();
    c.obstacles = tbr::parse_boxes(obstacles);
    c.clear = clear < 0 ? 0 : (clear > 1000 ? 1000 : clear);
    return c;
}

inline void update(const Content &c)
{
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_content = c;
        g_up.store(c.show);
        if (c.show) g_posted.fetch_add(1);
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

// The side the words stand on now, for the arrow the page draws: "left",
// "right", "above", "below" or "center"; "" while they are not shown.
inline std::string side()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_side;
}

// Whether a tour is up: the page's words are to be shown, whether or not the
// configs window is in front for them now.
inline bool up()
{
    return g_up.load();
}

// Whether the words the page sent last are not yet standing where they go:
// not placed yet, or sliding there.
inline bool moving()
{
    return g_settled.load() != g_posted.load();
}

// A click on a button, handed over once: its id, or "".
inline std::string take_press()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    std::string p = g_press;
    g_press.clear();
    return p;
}

// ⓘ `DS5-USBIP tutorial-words-picture <file.bmp> [dpi] [start|corners|left|beside|marks|keys|congrats]`:
// the words as Quick's step that asks for a press shows them (or the card the
// tour opens with; or the step with two arrows, at L1 and R1 in the panel's
// corners; or a step that asks for the window's left side; or every button's
// mark, as the legends draw them; or a step standing beside what is lit,
// off what is on the window, its place printed), beside a stand-in
// for Quick's window, drawn into a picture rather than onto the screen, so
// the drawing can be looked at with no window and no page.
inline int picture_mode(int argc, wchar_t **argv)
{
    if (argc < 3) {
        std::wcout << L"usage: DS5-USBIP tutorial-words-picture <file.bmp> [dpi] [start|corners|left|beside|marks|keys|congrats]"
                   << std::endl;
        return 2;
    }
    const UINT dpi = argc > 3 ? static_cast<UINT>(_wtoi(argv[3])) : 96;
    const std::wstring sample = argc > 4 ? std::wstring(argv[4]) : std::wstring();
    const bool start = sample == L"start";
    Content c;
    c.show = true;
    if (sample == L"corners") {
        c.count = L"2 of 13";
        c.words = L"[[*L1]] and [[*R1]] switch this window to your other controllers. Try it: switch "
                  L"between the three practice controllers, then choose [[Continue]].";
        c.buttons = { { "next", L"Continue", "go" }, { "prev", L"Go back", "plain" },
                      { "end", L"End tutorial", "plain" } };
        c.focus = "next";
        c.anchors = { { 40, 60 }, { 960, 60 } };
    } else if (sample == L"left") {
        c.count = L"1 of 13";
        c.words = L"This is Dual, a practice controller for this tour: nothing you do here changes "
                  L"your real ones.";
        c.buttons = { { "next", L"Continue", "go" }, { "end", L"End tutorial", "plain" } };
        c.focus = "next";
        c.ax = 150;
        c.ay = 400;
        c.prefer = tbr::Prefer::Left;
    } else if (sample == L"beside") {
        // ⓘ The window's left half lit, its right half taken: the words
        // stand at its left, off what is on it, with the auto link mark lit
        // as Simple's step draws it.
        c.count = L"11 of 17";
        c.words = L"[[\U0001F5D8]] beside [[stick_to_mouse]] marks the config Dual auto links to. "
                  L"A controller auto links to one config at a time.";
        c.buttons = { { "next", L"Continue", "off" }, { "prev", L"Go back", "plain" },
                      { "end", L"End tutorial", "plain" } };
        c.focus = "prev";
        c.arrow = false;
        c.prefer = tbr::Prefer::Left;
        c.hasTarget = true;
        c.target = { 300, 400, 450, 500 };
        c.obstacles = { { 300, 400, 450, 500 }, { 500, 0, 1000, 1000 } };
        c.clear = 100;
    } else if (sample == L"marks") {
        c.count = L"every mark";
        c.words = L"PlayStation [[*\u2715]] [[*\u25cb]] [[*\u25a1]] [[*\u25b3]] [[*\u2630]] [[*\\|/]] "
                  L"[[*R3]] [[*L1]] [[*R1]] Xbox [[*A]] [[*B]] [[*X]] [[*Y]] [[*\u2630]] [[*\u29c9]] "
                  L"[[*RS]] [[*LB]] [[*RB]] d-pad [[*\u271c]] keys [[Q]] [[Esc]]";
        c.buttons = { { "next", L"Continue", "go" }, { "end", L"End tutorial", "plain" } };
        c.focus = "next";
        c.ax = 500;
        c.ay = 420;
    } else if (sample == L"keys") {
        // ⓘ The step below as a keyboard has it: each button's key in it.
        c.count = L"4 of 13";
        c.words = L"Press [[Y]] to auto link the config in the middle: Dual will pick it by itself every "
                  L"time it is bridged.";
        c.buttons = { { "next", L"Continue", "go", L"PgDn" }, { "prev", L"Go back", "plain", L"PgUp" },
                      { "end", L"End tutorial", "plain", L"Esc" } };
        c.ax = 500;
        c.ay = 420;
    } else if (sample == L"congrats") {
        // ⓘ The card that ends a tour, larger, over the window's middle.
        c.title = L"Congratulations!";
        c.words = L"You've finished the Advanced mode tutorial.";
        c.buttons = { { "done", L"Done", "go", L"*R2" } };
        c.arrow = false;
        c.big = true;
        c.prefer = tbr::Prefer::Center;
    } else if (start) {
        c.title = L"Quick mode tutorial";
        c.words = L"A short tour with practice controllers: nothing in it changes your real ones.";
        c.buttons = { { "skip", L"Skip", "plain" }, { "begin", L"Begin tutorial", "go" } };
        c.focus = "begin";
        c.arrow = false;
        c.prefer = tbr::Prefer::Center;      // ⓘ over the window's middle, as the tours open
    } else {
        c.count = L"4 of 13";
        c.words = L"Press [[*\u25b3]] to auto link the config in the middle, then choose [[OK]]: Dual "
                  L"will pick it by itself every time it is bridged.";
        c.buttons = { { "next", L"Continue", "off", L"*R2" }, { "prev", L"Go back", "plain", L"*L2" },
                      { "end", L"End tutorial", "plain", L"*\u25cb" } };
        c.ax = 500;
        c.ay = 420;
    }
    const RECT work{ 0, 0, px(1920, dpi), px(1040, dpi) };
    const RECT frame{ px(540, dpi), px(820, dpi), px(1380, dpi), px(1030, dpi) };
    HDC screen = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(screen);
    Fonts fonts;
    const RECT page{ frame.left, frame.top + px(30, dpi), frame.right, frame.bottom };
    const Layout L = compute_layout(mem, &fonts, c, dpi, frame, work, page);
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
    for (HFONT f : { fonts.text, fonts.lit, fonts.count, fonts.button, fonts.title }) {
        if (f != nullptr) DeleteObject(f);
    }
    std::wcout << (ok ? L"wrote " : L"could not write ") << argv[2] << L" (" << w << L"x" << h
               << L", " << tbr::side_name(L.place.side) << L" at " << L.place.window.left << L","
               << L.place.window.top << L")" << std::endl;
    return ok ? 0 : 1;
}

// ⓘ `DS5-USBIP tutorial-words-replay <file> [dpi] [frame] [client] [work]`:
// what the page sent for the words, one body a line as the route reads it (a
// line starting # is printed as it is), placed one after another as refresh()
// places them, on a configs window that stays where it is: where each would
// stand, on which side, and whether the words stayed where they were. So a
// step's place can be compared, reached and gone back to, with no window and
// no page. `frame`, `client` and `work` are "l,t,r,b" on the screen; Simple's
// window at home on a 2560 x 1392 working area at 96 dpi, as the page's
// pictures have it, when they are not given.
inline int replay_mode(int argc, wchar_t **argv)
{
    if (argc < 3) {
        std::wcout << L"usage: DS5-USBIP tutorial-words-replay <file> [dpi] [frame] [client] [work]" << std::endl;
        return 2;
    }
    const UINT dpi = argc > 3 ? static_cast<UINT>(_wtoi(argv[3])) : 96;
    const auto box_arg = [&](int i, RECT dflt) {
        RECT r{};
        if (argc <= i || swscanf_s(argv[i], L"%ld,%ld,%ld,%ld", &r.left, &r.top, &r.right, &r.bottom) != 4) return dflt;
        return r;
    };
    const RECT frame = box_arg(4, RECT{ 704, 836, 1856, 1376 });
    const RECT client = box_arg(5, RECT{ 704, 866, 1856, 1376 });
    const RECT work = box_arg(6, RECT{ 0, 0, 2560, 1392 });
    std::ifstream in(argv[2], std::ios::binary);
    if (!in) {
        std::wcout << L"could not read " << argv[2] << std::endl;
        return 1;
    }
    HDC screen = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(screen);
    Fonts fonts;
    const std::wstring place = L"the window, where it stays";
    Placed placed;
    POINT here{};
    bool shown = false;
    std::string line;
    int n = 0;
    while (std::getline(in, line)) {
        ++n;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        if (line[0] == '#') {
            // ⓘ As plain text: a character the console cannot show stops it
            // showing anything after.
            for (char &ch : line) {
                if (static_cast<unsigned char>(ch) > 0x7e) ch = '?';
            }
            std::wcout << wide(line) << L"\n";
            continue;
        }
        RestJson json;
        std::string error;
        if (!rest_parse_flat_json(line, &json, &error)) {
            std::wcout << n << L" not read: " << wide(error) << L"\n";
            continue;
        }
        const auto text = [&](const char *key) {
            auto it = json.strings.find(key);
            return it == json.strings.end() ? std::string() : it->second;
        };
        const auto number = [&](const char *key, int fallback) {
            auto it = json.numbers.find(key);
            return it == json.numbers.end() ? fallback : static_cast<int>(it->second);
        };
        const auto flag = [&](const char *key) {
            auto it = json.bools.find(key);
            return it != json.bools.end() && it->second;
        };
        if (!flag("show")) {
            shown = false;
            placed = Placed();
            std::wcout << n << L" hidden\n";
            continue;
        }
        const Content c = content_of(text("title"), text("count"), text("words"), text("buttons"), text("focus"),
                                     flag("arrow"), number("ax", 500), number("ay", 500), text("side"),
                                     text("anchors"), text("target"), text("obstacles"), number("clear", 0));
        const bool keep = shown && still_for(placed, c, place);
        const Standing standing{ here, placed.side };
        const Layout L = compute_layout(mem, &fonts, c, dpi, frame, work, client, keep ? &standing : nullptr);
        const tbr::Box &w = L.place.window;
        const bool stayed = keep && w.left == here.x && w.top == here.y;
        std::wcout << n << L" " << (c.title.empty() ? c.count : c.title) << L" "
                   << (c.prefer == tbr::Prefer::Center ? L"center" : wide(tbr::side_name(L.place.side))) << L" "
                   << w.left << L"," << w.top << L" " << w.width() << L"x" << w.height()
                   << (stayed ? L" stayed" : L" placed") << (c.hasTarget ? L"" : L" (no target)") << L"\n";
        here = POINT{ w.left, w.top };
        placed = placed_for(c, place, L.place.side);
        shown = true;
    }
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
    for (HFONT f : { fonts.text, fonts.lit, fonts.count, fonts.button, fonts.title }) {
        if (f != nullptr) DeleteObject(f);
    }
    std::wcout << std::flush;
    return 0;
}

}  // namespace tutorial_bubble
