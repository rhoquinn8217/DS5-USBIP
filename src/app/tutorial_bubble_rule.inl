// Where the tutorial's words go beside the Controller Configs window, and how
// they break into lines: the parts of tutorial_bubble.inl that need no window,
// so the test binary can reach them.
//
// ⭐ OUTSIDE THE WINDOW (rhoquinn8217, 2026-10-14, after going through the
// tutorial on the hardware: *"The description boxes seen to take a lot of
// space and it's hard to tell what is going. We need to move the descriptions
// so that they don't cover any of the browser box."*). Simple's window is
// small, and the pre-set list and the pre-set's page fill it, so there was no
// free place inside it. The words sit on the desktop beside it instead, ABOVE
// it first, because Simple opens at the bottom centre of the screen.
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace tutorial_bubble_rule {

// A piece of the words: plain, or a name lit in amber.
struct Piece {
    std::wstring text;
    bool lit = false;
};

// The page marks a lit name as [[name]]. ⓘ An unclosed [[ stays plain text.
inline std::vector<Piece> parse(const std::wstring &words)
{
    std::vector<Piece> out;
    size_t at = 0;
    while (at < words.size()) {
        const size_t open = words.find(L"[[", at);
        if (open == std::wstring::npos) break;
        const size_t close = words.find(L"]]", open + 2);
        if (close == std::wstring::npos) break;
        if (open > at) out.push_back({ words.substr(at, open - at), false });
        out.push_back({ words.substr(open + 2, close - open - 2), true });
        at = close + 2;
    }
    if (at < words.size()) out.push_back({ words.substr(at), false });
    return out;
}

// ⭐ THE BUTTONS UNDER THE WORDS, named by the page (rhoquinn8217, 2026-10-23:
// Next became "Continue" and Previous "Go back", and End tutorial moved out
// beside them, "outside", as did the start's and the end's questions). One
// string, since the listener's flat JSON carries no lists:
// "id:Label:kind|id:Label:kind", in the order they stand, left to right.
// `kind` is how a button looks and whether a click counts: "go" lit blue,
// "off" greyed out and deaf, anything else plain. ⓘ An item without an id or
// a label is left out.
// ⭐ AND THE PAD'S BUTTON FOR IT, drawn in it (rhoquinn8217, 2026-10-29: "add
// R2 symbol in the "Continue" button, L2 to the "Go Back" button and "Circle"
// to the "End Tutorial" button"): a fourth field, "id:Label:kind:mark", the
// mark the legends draw as `*glyph` (R2, L2, a circle; RT, LT, B), or a
// keyboard's key as its name (PgDn, PgUp, Esc).
struct ButtonSpec {
    std::string id;
    std::string label;   // UTF-8, as the page sent it
    std::string kind;    // "go", "off" or "plain"
    std::string mark;    // "*R2", "PgDn", or "" for none
};

inline std::vector<ButtonSpec> parse_buttons(const std::string &spec)
{
    std::vector<ButtonSpec> out;
    size_t at = 0;
    while (at < spec.size()) {
        size_t end = spec.find('|', at);
        if (end == std::string::npos) end = spec.size();
        const std::string item = spec.substr(at, end - at);
        const size_t a = item.find(':');
        const size_t b = a == std::string::npos ? std::string::npos : item.find(':', a + 1);
        if (a != std::string::npos && b != std::string::npos && a > 0 && b > a + 1) {
            const size_t c = item.find(':', b + 1);
            std::string kind = c == std::string::npos ? item.substr(b + 1) : item.substr(b + 1, c - b - 1);
            if (kind != "go" && kind != "off") kind = "plain";
            out.push_back({ item.substr(0, a), item.substr(a + 1, b - a - 1), kind,
                            c == std::string::npos ? std::string() : item.substr(c + 1) });
        }
        at = end + 1;
    }
    return out;
}

// What a line may break between: plain words, the spaces between them, and
// each lit name WHOLE, dashes and spaces and all (*"Don't wrap it on it's
// dashes"*). ⭐ A lit piece that starts with * is a controller's button, drawn
// as the legends draw it rather than written (rhoquinn8217, 2026-10-23: "Can
// you make the legend button symbols in the tutorial?"): `mark`, with the
// legend's glyph for it in `text` (the page's PAD_LEGEND_PS and _XBOX: a
// shape, a letter, L1 or R1, ✜ for the d-pad).
struct Token {
    std::wstring text;
    bool lit = false;
    bool space = false;
    bool mark = false;
};

inline std::vector<Token> tokens(const std::vector<Piece> &pieces)
{
    std::vector<Token> out;
    for (const Piece &p : pieces) {
        if (p.lit) {
            if (p.text.size() > 1 && p.text[0] == L'*') {
                out.push_back({ p.text.substr(1), true, false, true });
            } else {
                out.push_back({ p.text, true, false });
            }
            continue;
        }
        std::wstring word;
        for (const wchar_t c : p.text) {
            if (c != L' ') {
                word += c;
                continue;
            }
            if (!word.empty()) {
                out.push_back({ word, false, false });
                word.clear();
            }
            out.push_back({ L" ", false, true });
        }
        if (!word.empty()) out.push_back({ word, false, false });
    }
    return out;
}

// The words as lines of token indices, each no wider than maxWidth unless a
// single token is. A space where a line breaks belongs to neither line.
inline std::vector<std::vector<size_t>> lines(const std::vector<Token> &toks,
                                              const std::vector<int> &widths, int maxWidth)
{
    std::vector<std::vector<size_t>> out;
    std::vector<size_t> line;
    int used = 0;
    auto drop_trailing_spaces = [&]() {
        while (!line.empty() && toks[line.back()].space) {
            used -= widths[line.back()];
            line.pop_back();
        }
    };
    for (size_t i = 0; i < toks.size() && i < widths.size(); ++i) {
        if (toks[i].space) {
            if (line.empty()) continue;
            line.push_back(i);
            used += widths[i];
            continue;
        }
        if (!line.empty() && used + widths[i] > maxWidth) {
            drop_trailing_spaces();
            out.push_back(line);
            line.clear();
            used = 0;
        }
        line.push_back(i);
        used += widths[i];
    }
    drop_trailing_spaces();
    if (!line.empty()) out.push_back(line);
    return out;
}

struct Box {
    int left = 0, top = 0, right = 0, bottom = 0;
    int width() const { return right - left; }
    int height() const { return bottom - top; }
};

enum class Side { Above, Below, Right, Left };

// ⭐ THE SIDE A STEP ASKS FOR (rhoquinn8217, 2026-10-23, going through Quick's
// tour: "Put the bubble on the left side of the config", "Move bubble to right
// side of window", "Move bubble to bottom side of window"). None leaves it to
// the rule below. Center is over the window's middle, not beside it
// (2026-10-26: "For the introduction and closing step of the tutorial, put
// the bubble in the center of the config window").
enum class Prefer { None, Above, Below, Left, Right, Center };

// The page names it "above", "below", "left", "right" or "center"; anything
// else is None.
inline Prefer parse_prefer(const std::string &side)
{
    if (side == "above") return Prefer::Above;
    if (side == "below") return Prefer::Below;
    if (side == "left") return Prefer::Left;
    if (side == "right") return Prefer::Right;
    if (side == "center") return Prefer::Center;
    return Prefer::None;
}

// What the arrows point at, as the page sends them: "ax,ay|ax,ay", each in
// thousandths of its view. ⓘ An item that is not two numbers is left out,
// and a number past 1000 reads as 1000: the rule clamps to the view anyway.
inline std::vector<std::pair<int, int>> parse_anchors(const std::string &text)
{
    std::vector<std::pair<int, int>> out;
    auto number = [](const std::string &digits, int *value) {
        if (digits.empty()) return false;
        int v = 0;
        for (const char c : digits) {
            if (c < '0' || c > '9') return false;
            v = v >= 1000 ? 1000 : v * 10 + (c - '0');
        }
        *value = v > 1000 ? 1000 : v;
        return true;
    };
    size_t at = 0;
    while (at < text.size()) {
        size_t end = text.find('|', at);
        if (end == std::string::npos) end = text.size();
        const std::string item = text.substr(at, end - at);
        const size_t comma = item.find(',');
        int x = 0, y = 0;
        if (comma != std::string::npos && number(item.substr(0, comma), &x) &&
            number(item.substr(comma + 1), &y)) {
            out.push_back({ x, y });
        }
        at = end + 1;
    }
    return out;
}

// The whole words window, in screen pixels: the words, the buttons under
// them, and the arrows on the side facing the Controller Configs window.
// `arrowsAt` is where along that side each arrow sits, from the window's own
// left (Above, Below) or top (Left, Right); `arrowAt` is the first of them.
struct Placement {
    Side side = Side::Above;
    Box window;
    int arrowAt = 0;
    std::vector<int> arrowsAt;
};

inline int clamp_to(int v, int lo, int hi)
{
    if (hi < lo) return lo;
    return v < lo ? lo : (v > hi ? hi : v);
}

// ⭐ THE SIDE ASKED FOR, when it has room; else the side opposite it; else
// ABOVE FIRST, then below, then the side with more room; and when nothing fits
// outside, above or below as room allows, kept on the screen even if that
// means over the window's edge. `stackW` and `stackH` are the words and the
// buttons; `arrow` is how far an arrow stands out, `gap` the space left
// between it and the window, `round` the corner an arrow keeps clear of. Each
// anchor is the middle of a lit part, in thousandths of the page's view
// across and down, so its arrow points along the window at it.
// ⭐ TWO ARROWS FAR APART (rhoquinn8217, 2026-10-23: "Put and arrow pointing
// at the R2/RB/E as well", L1's and R1's marks at the panel's two corners):
// above or below the window, the words widen until every arrow stands under
// them; beside it, each arrow goes as far along as the words reach.
inline Placement place_at(const Box &win, const Box &work, int stackW, int stackH,
                          int arrow, int gap, int round,
                          const std::vector<std::pair<int, int>> &anchors, Prefer prefer)
{
    const int margin = gap;
    // ⭐ CENTER: the words and buttons over the middle of the window itself,
    // kept on the screen, with no arrow to point anywhere.
    if (prefer == Prefer::Center) {
        Placement p;
        const int x = clamp_to(win.left + (win.width() - stackW) / 2, work.left + margin,
                               work.right - margin - stackW);
        const int y = clamp_to(win.top + (win.height() - stackH) / 2, work.top + margin,
                               work.bottom - margin - stackH);
        p.window = { x, y, x + stackW, y + stackH };
        p.arrowsAt.push_back(0);
        return p;
    }
    std::vector<int> xs, ys;
    for (const auto &a : anchors) {
        xs.push_back(win.left + win.width() * clamp_to(a.first, 0, 1000) / 1000);
        ys.push_back(win.top + win.height() * clamp_to(a.second, 0, 1000) / 1000);
    }
    if (xs.empty()) {
        xs.push_back(win.left + win.width() / 2);
        ys.push_back(win.top + win.height() / 2);
    }
    int minX = xs[0], maxX = xs[0], minY = ys[0], maxY = ys[0];
    for (size_t i = 1; i < xs.size(); ++i) {
        minX = xs[i] < minX ? xs[i] : minX;
        maxX = xs[i] > maxX ? xs[i] : maxX;
        minY = ys[i] < minY ? ys[i] : minY;
        maxY = ys[i] > maxY ? ys[i] : maxY;
    }
    const int keep = round + arrow;
    const int wideW = stackW > maxX - minX + 2 * keep ? stackW : maxX - minX + 2 * keep;
    const int needV = stackH + arrow + gap + margin;
    const int needH = stackW + arrow + gap + margin;
    const int roomAbove = win.top - work.top;
    const int roomBelow = work.bottom - win.bottom;
    const int roomRight = work.right - win.right;
    const int roomLeft = win.left - work.left;
    const bool fitsAbove = roomAbove >= needV;
    const bool fitsBelow = roomBelow >= needV;
    const bool fitsRight = roomRight >= needH;
    const bool fitsLeft = roomLeft >= needH;
    auto fits = [&](Prefer s) {
        return (s == Prefer::Above && fitsAbove) || (s == Prefer::Below && fitsBelow) ||
               (s == Prefer::Left && fitsLeft) || (s == Prefer::Right && fitsRight);
    };
    auto opposite = [](Prefer s) {
        return s == Prefer::Above ? Prefer::Below : s == Prefer::Below ? Prefer::Above
             : s == Prefer::Left ? Prefer::Right : s == Prefer::Right ? Prefer::Left : Prefer::None;
    };
    auto side_of = [](Prefer s) {
        return s == Prefer::Below ? Side::Below : s == Prefer::Left ? Side::Left
             : s == Prefer::Right ? Side::Right : Side::Above;
    };

    Placement p;
    if (prefer != Prefer::None && fits(prefer)) {
        p.side = side_of(prefer);
    } else if (prefer != Prefer::None && fits(opposite(prefer))) {
        p.side = side_of(opposite(prefer));
    } else if (fitsAbove || fitsBelow || (!fitsRight && !fitsLeft)) {
        p.side = (fitsAbove || (!fitsBelow && roomAbove >= roomBelow)) ? Side::Above : Side::Below;
    } else {
        p.side = (fitsRight && (!fitsLeft || roomRight >= roomLeft)) ? Side::Right : Side::Left;
    }
    if (p.side == Side::Above || p.side == Side::Below) {
        const int w = wideW, h = stackH + arrow;
        const int x = clamp_to((minX + maxX) / 2 - w / 2, work.left + margin, work.right - margin - w);
        const int want = p.side == Side::Above ? win.top - gap - h : win.bottom + gap;
        const int y = clamp_to(want, work.top + margin, work.bottom - margin - h);
        p.window = { x, y, x + w, y + h };
        for (const int ax : xs) p.arrowsAt.push_back(clamp_to(ax - x, keep, w - keep));
    } else {
        const int w = stackW + arrow, h = stackH;
        const int x = p.side == Side::Right ? win.right + gap : win.left - gap - w;
        const int y = clamp_to((minY + maxY) / 2 - h / 2, work.top + margin, work.bottom - margin - h);
        p.window = { x, y, x + w, y + h };
        for (const int ay : ys) p.arrowsAt.push_back(clamp_to(ay - y, keep, h - keep));
    }
    p.arrowAt = p.arrowsAt[0];
    return p;
}

// One anchor, no side asked for: the rule as it stood before steps could ask.
inline Placement place(const Box &win, const Box &work, int stackW, int stackH,
                       int arrow, int gap, int round, int ax, int ay)
{
    return place_at(win, work, stackW, stackH, arrow, gap, round, { { ax, ay } }, Prefer::None);
}

// Boxes as the page sends them: "l,t,r,b|l,t,r,b", each in thousandths of its
// view. ⓘ An item that is not four numbers is left out, a number past 1000
// reads as 1000, and an empty box is left out.
inline std::vector<Box> parse_boxes(const std::string &text)
{
    std::vector<Box> out;
    size_t at = 0;
    while (at < text.size()) {
        size_t end = text.find('|', at);
        if (end == std::string::npos) end = text.size();
        const std::string item = text.substr(at, end - at);
        int v[4] = { 0, 0, 0, 0 };
        int got = 0;
        bool ok = true;
        size_t from = 0;
        while (ok && got < 4 && from <= item.size()) {
            size_t comma = item.find(',', from);
            if (comma == std::string::npos) comma = item.size();
            const std::string digits = item.substr(from, comma - from);
            if (digits.empty()) { ok = false; break; }
            int n = 0;
            for (const char c : digits) {
                if (c < '0' || c > '9') { ok = false; break; }
                n = n >= 1000 ? 1000 : n * 10 + (c - '0');
            }
            v[got++] = n > 1000 ? 1000 : n;
            from = comma + 1;
        }
        if (ok && got == 4 && from > item.size() && v[2] > v[0] && v[3] > v[1]) {
            out.push_back({ v[0], v[1], v[2], v[3] });
        }
        at = end + 1;
    }
    return out;
}

inline bool overlaps_any(const Box &r, const std::vector<Box> &boxes)
{
    for (const Box &b : boxes) {
        if (r.left < b.right && r.right > b.left && r.top < b.bottom && r.bottom > b.top) return true;
    }
    return false;
}

// ⭐ BESIDE WHAT IS LIT, OVER THE WINDOW WHERE IT COVERS NOTHING (rhoquinn8217,
// 2026-10-26: "I also want the bubble to overlap the window as long as it's not
// covering any text or elements on the window"): the words and their buttons,
// `w` by `h`, as near to `target` as they can stand, on the side the step asks
// for first (its opposite, then across, after it), sliding along that side
// to find room, never over one of `obstacles` (what is on the window, and its
// title bar) and on the work area, `margin` in from its edges. `clear` keeps
// room between them and the target for the page's arrow; `reach` is how far
// out they may go, `step` how finely the places are tried. False when nothing
// fits within reach, for the rule above to place them.
inline bool place_near(const Box &target, const Box &work, int w, int h,
                       const std::vector<Box> &obstacles, Prefer prefer,
                       int clear, int margin, int reach, int step, Placement *out)
{
    if (step < 1) step = 1;
    Prefer order[4] = { Prefer::Above, Prefer::Below, Prefer::Left, Prefer::Right };
    if (prefer == Prefer::Below) {
        order[0] = Prefer::Below; order[1] = Prefer::Above;
    } else if (prefer == Prefer::Left) {
        order[0] = Prefer::Left; order[1] = Prefer::Right; order[2] = Prefer::Above; order[3] = Prefer::Below;
    } else if (prefer == Prefer::Right) {
        order[0] = Prefer::Right; order[1] = Prefer::Left; order[2] = Prefer::Above; order[3] = Prefer::Below;
    }
    const int cx = (target.left + target.right) / 2, cy = (target.top + target.bottom) / 2;
    for (const Prefer s : order) {
        const bool beside = s == Prefer::Left || s == Prefer::Right;
        const int span = beside ? h : w;
        for (int d = clear; d <= reach; d += step) {
            for (int off = 0; off <= span; off += 2 * step) {
                for (int sign = 1; sign >= -1; sign -= 2) {
                    if (off == 0 && sign < 0) continue;
                    const int shift = off * sign;
                    int x = 0, y = 0;
                    if (s == Prefer::Left) { x = target.left - d - w; y = cy - h / 2 + shift; }
                    else if (s == Prefer::Right) { x = target.right + d; y = cy - h / 2 + shift; }
                    else if (s == Prefer::Above) { x = cx - w / 2 + shift; y = target.top - d - h; }
                    else { x = cx - w / 2 + shift; y = target.bottom + d; }
                    const Box r{ x, y, x + w, y + h };
                    if (r.left < work.left + margin || r.top < work.top + margin ||
                        r.right > work.right - margin || r.bottom > work.bottom - margin) continue;
                    if (overlaps_any(r, obstacles)) continue;
                    out->side = s == Prefer::Below ? Side::Below : s == Prefer::Left ? Side::Left
                              : s == Prefer::Right ? Side::Right : Side::Above;
                    out->window = r;
                    out->arrowsAt.assign(1, 0);
                    out->arrowAt = 0;
                    return true;
                }
            }
        }
    }
    return false;
}

// The side the words stand on, as the page names it: for the arrow it draws.
inline const char *side_name(Side s)
{
    return s == Side::Below ? "below" : s == Side::Left ? "left" : s == Side::Right ? "right" : "above";
}

// ⭐ HOW THE WORDS SLIDE (rhoquinn8217, 2026-10-27: "Make animation for
// fluid. When moving the tutorial bubble lets add acceleration", choosing
// between three curves): the one that starts slowly, speeds up and slows
// down again as it arrives, cubic ease-in-out. How far along the way the
// words are at `t`, the share of the slide's time gone: 0 at 0, 1 at 1,
// half way at half time.
inline double ease_in_out(double t)
{
    if (t <= 0.0) return 0.0;
    if (t >= 1.0) return 1.0;
    if (t < 0.5) return 4.0 * t * t * t;
    const double u = 2.0 - 2.0 * t;
    return 1.0 - u * u * u / 2.0;
}

// How long a slide of `distance` pixels takes at `dpi`, in milliseconds:
// longer for a longer way, from 260 for a hop to 480 across a screen.
inline int slide_ms(double distance, unsigned dpi)
{
    const double dips = distance * 96.0 / (dpi > 0 ? dpi : 96);
    const double ms = 260.0 + dips * 0.25;
    return ms <= 260.0 ? 260 : ms >= 480.0 ? 480 : static_cast<int>(ms + 0.5);
}

}  // namespace tutorial_bubble_rule
