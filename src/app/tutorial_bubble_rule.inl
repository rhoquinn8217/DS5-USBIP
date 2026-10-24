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
struct ButtonSpec {
    std::string id;
    std::string label;   // UTF-8, as the page sent it
    std::string kind;    // "go", "off" or "plain"
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
            std::string kind = item.substr(b + 1);
            if (kind != "go" && kind != "off") kind = "plain";
            out.push_back({ item.substr(0, a), item.substr(a + 1, b - a - 1), kind });
        }
        at = end + 1;
    }
    return out;
}

// What a line may break between: plain words, the spaces between them, and
// each lit name WHOLE, dashes and spaces and all (*"Don't wrap it on it's
// dashes"*).
struct Token {
    std::wstring text;
    bool lit = false;
    bool space = false;
};

inline std::vector<Token> tokens(const std::vector<Piece> &pieces)
{
    std::vector<Token> out;
    for (const Piece &p : pieces) {
        if (p.lit) {
            out.push_back({ p.text, true, false });
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

// The whole words window, in screen pixels: the words, the buttons under
// them, and the arrow on the side facing the Controller Configs window.
// `arrowAt` is where along that side the arrow sits, from the window's own
// left (Above, Below) or top (Left, Right).
struct Placement {
    Side side = Side::Above;
    Box window;
    int arrowAt = 0;
};

inline int clamp_to(int v, int lo, int hi)
{
    if (hi < lo) return lo;
    return v < lo ? lo : (v > hi ? hi : v);
}

// ⭐ ABOVE FIRST, then below, then the side with more room; and when nothing
// fits outside, above or below as room allows, kept on the screen even if
// that means over the window's edge. `stackW` and `stackH` are the words and
// the buttons; `arrow` is how far the arrow stands out, `gap` the space left
// between it and the window, `round` the corner the arrow keeps clear of. The
// anchor is the middle of what is lit, in thousandths of the page's view
// across (ax) and down (ay), so the arrow points along the window at it.
inline Placement place(const Box &win, const Box &work, int stackW, int stackH,
                       int arrow, int gap, int round, int ax, int ay)
{
    const int margin = gap;
    const int anchorX = win.left + win.width() * clamp_to(ax, 0, 1000) / 1000;
    const int anchorY = win.top + win.height() * clamp_to(ay, 0, 1000) / 1000;
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

    Placement p;
    if (fitsAbove || fitsBelow || (!fitsRight && !fitsLeft)) {
        p.side = (fitsAbove || (!fitsBelow && roomAbove >= roomBelow)) ? Side::Above : Side::Below;
        const int w = stackW, h = stackH + arrow;
        const int x = clamp_to(anchorX - w / 2, work.left + margin, work.right - margin - w);
        const int want = p.side == Side::Above ? win.top - gap - h : win.bottom + gap;
        const int y = clamp_to(want, work.top + margin, work.bottom - margin - h);
        p.window = { x, y, x + w, y + h };
        p.arrowAt = clamp_to(anchorX - x, round + arrow, w - round - arrow);
        return p;
    }
    p.side = (fitsRight && (!fitsLeft || roomRight >= roomLeft)) ? Side::Right : Side::Left;
    const int w = stackW + arrow, h = stackH;
    const int x = p.side == Side::Right ? win.right + gap : win.left - gap - w;
    const int y = clamp_to(anchorY - h / 2, work.top + margin, work.bottom - margin - h);
    p.window = { x, y, x + w, y + h };
    p.arrowAt = clamp_to(anchorY - y, round + arrow, h - round - arrow);
    return p;
}

}  // namespace tutorial_bubble_rule
