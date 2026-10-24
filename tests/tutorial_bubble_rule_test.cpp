// Where the tutorial's words go beside the Controller Configs window, and how
// they break into lines.
//
// ⭐ WHAT THESE PROTECT. The words must not cover the window (rhoquinn8217,
// 2026-10-14: *"We need to move the descriptions so that they don't cover any
// of the browser box."*). They go above it while there is room, which is where
// Simple's window leaves it; below or beside it when not; always on the
// screen; with the arrow along the window at what is lit. And a lit name never
// breaks at its dashes.
//
// WHAT THESE CANNOT DO. They draw nothing. How the words look, and that they
// follow the window and answer a click, is looked at with the listener running.

#include "harness.h"

#include <string>
#include <vector>

#include "app/tutorial_bubble_rule.inl"

using namespace ctmtest;
namespace tbr = tutorial_bubble_rule;

namespace {

bool overlaps(const tbr::Box &a, const tbr::Box &b)
{
    return a.left < b.right && b.left < a.right && a.top < b.bottom && b.top < a.bottom;
}

bool inside(const tbr::Box &a, const tbr::Box &outer)
{
    return a.left >= outer.left && a.right <= outer.right &&
           a.top >= outer.top && a.bottom <= outer.bottom;
}

// Ten units a character, so widths can be worked out by eye.
std::vector<int> widths_of(const std::vector<tbr::Token> &toks)
{
    std::vector<int> out;
    for (const tbr::Token &t : toks) out.push_back(static_cast<int>(t.text.size()) * 10);
    return out;
}

} // namespace

int run_tutorial_bubble_rule_tests()
{
    section("tutorial words: a lit name is one piece, the rest plain");
    {
        const auto p = tbr::parse(L"Click [[New]] to make a new controller config.");
        CTM_CHECK_EQ(p.size(), size_t(3));
        CTM_CHECK(p[0].text == L"Click " && !p[0].lit);
        CTM_CHECK(p[1].text == L"New" && p[1].lit);
        CTM_CHECK(p[2].text == L" to make a new controller config." && !p[2].lit);
        const auto plain = tbr::parse(L"no names here");
        CTM_CHECK(plain.size() == 1 && plain[0].text == L"no names here" && !plain[0].lit);
        const auto open = tbr::parse(L"half [[open");   // unclosed: plain text
        CTM_CHECK(open.size() == 1 && open[0].text == L"half [[open" && !open[0].lit);
    }

    section("tutorial words: a lit name never breaks at its dashes");
    {
        const auto toks = tbr::tokens(tbr::parse(L"Select [[stick-to-mouse]] to see what it does."));
        CTM_CHECK(toks.size() >= 3);
        CTM_CHECK(toks[2].text == L"stick-to-mouse" && toks[2].lit);
        // 150 wide: the name (140) takes a line of its own, whole.
        const auto lines = tbr::lines(toks, widths_of(toks), 150);
        bool seen = false;
        for (const auto &line : lines) {
            CTM_CHECK(!toks[line.front()].space);   // no line starts with a space
            CTM_CHECK(!toks[line.back()].space);    // nor ends with one
            for (const size_t i : line) {
                if (!toks[i].lit) continue;
                seen = true;
                CTM_CHECK_EQ(line.size(), size_t(1));
            }
        }
        CTM_CHECK(seen);
    }

    section("tutorial words: lines fit the width, and a name too wide stays whole");
    {
        const auto toks = tbr::tokens(tbr::parse(L"one two three four"));
        // "one two" is 70; "three four" would be 100.
        const auto lines = tbr::lines(toks, widths_of(toks), 80);
        CTM_CHECK_EQ(lines.size(), size_t(3));
        const auto wide = tbr::tokens(tbr::parse(L"[[stick-to-mouse]]"));
        CTM_CHECK_EQ(tbr::lines(wide, widths_of(wide), 50).size(), size_t(1));
    }

    // A 1920 by 1080 screen with its taskbar, and Simple's window at the
    // bottom centre, where it opens.
    const tbr::Box work{ 0, 0, 1920, 1040 };
    const tbr::Box simple{ 528, 560, 1392, 1030 };

    section("tutorial words: above Simple's window, clear of it and on the screen");
    {
        const auto p = tbr::place(simple, work, 380, 130, 12, 10, 10, 500, 400);
        CTM_CHECK(p.side == tbr::Side::Above);
        CTM_CHECK(!overlaps(p.window, simple));
        CTM_CHECK(inside(p.window, work));
        CTM_CHECK(p.window.bottom <= simple.top);
        // The arrow points down the lit part's column: the middle, here.
        CTM_CHECK_EQ(p.window.left + p.arrowAt, (simple.left + simple.right) / 2);
    }

    section("tutorial words: the arrow follows what is lit along the window");
    {
        const auto left = tbr::place(simple, work, 380, 130, 12, 10, 10, 100, 400);
        const auto right = tbr::place(simple, work, 380, 130, 12, 10, 10, 900, 400);
        CTM_CHECK(left.window.left + left.arrowAt < right.window.left + right.arrowAt);
        // ...and never into the corners' curve.
        CTM_CHECK(left.arrowAt >= 10 + 12 && left.arrowAt <= 380 - 10 - 12);
    }

    section("tutorial words: below a window at the top of the screen");
    {
        const tbr::Box high{ 528, 10, 1392, 480 };
        const auto p = tbr::place(high, work, 380, 130, 12, 10, 10, 500, 500);
        CTM_CHECK(p.side == tbr::Side::Below);
        CTM_CHECK(!overlaps(p.window, high));
        CTM_CHECK(p.window.top >= high.bottom);
    }

    section("tutorial words: beside a window as tall as the screen, on the side with room");
    {
        const tbr::Box tall{ 100, 20, 1300, 1030 };
        const auto p = tbr::place(tall, work, 380, 130, 12, 10, 10, 500, 300);
        CTM_CHECK(p.side == tbr::Side::Right);
        CTM_CHECK(!overlaps(p.window, tall));
        CTM_CHECK(inside(p.window, work));
        const tbr::Box tallRight{ 600, 20, 1900, 1030 };
        const auto q = tbr::place(tallRight, work, 380, 130, 12, 10, 10, 500, 300);
        CTM_CHECK(q.side == tbr::Side::Left);
        CTM_CHECK(!overlaps(q.window, tallRight));
    }

    section("tutorial words: on the screen even when nothing fits outside the window");
    {
        const auto p = tbr::place(work, work, 380, 130, 12, 10, 10, 500, 500);
        CTM_CHECK(inside(p.window, work));
    }

    section("tutorial words: with no arrow, nothing stands out past the words");
    {
        // The start's card and the question point at nothing: no arrow, and
        // the window is the words and their buttons alone.
        const tbr::Box quick{ 540, 820, 1380, 1030 };
        const auto p = tbr::place(quick, work, 380, 130, 0, 10, 10, 500, 500);
        CTM_CHECK(p.side == tbr::Side::Above);
        CTM_CHECK_EQ(p.window.height(), 130);
        CTM_CHECK(!overlaps(p.window, quick));
    }

    // ⭐ The buttons are named by the page (rhoquinn8217, 2026-10-23: Next
    // became "Continue", Previous "Go back", and End tutorial moved out beside
    // them), so a renamed button is a change to the page alone.
    section("tutorial buttons: named by the page, in its order");
    {
        const auto b = tbr::parse_buttons("next:Continue:off|prev:Go back:plain|end:End tutorial:go");
        CTM_CHECK_EQ(b.size(), size_t(3));
        CTM_CHECK(b[0].id == "next" && b[0].label == "Continue" && b[0].kind == "off");
        CTM_CHECK(b[1].id == "prev" && b[1].label == "Go back" && b[1].kind == "plain");
        CTM_CHECK(b[2].id == "end" && b[2].label == "End tutorial" && b[2].kind == "go");
    }

    section("tutorial buttons: an unknown look is plain, and a broken item is left out");
    {
        const auto b = tbr::parse_buttons("skip:Skip:shiny|:No id:go|noname::go|half:Half|begin:Begin tutorial:go");
        CTM_CHECK_EQ(b.size(), size_t(2));
        CTM_CHECK(b[0].id == "skip" && b[0].kind == "plain");
        CTM_CHECK(b[1].id == "begin" && b[1].label == "Begin tutorial");
        CTM_CHECK(tbr::parse_buttons("").empty());
        CTM_CHECK(tbr::parse_buttons("|").empty());
    }
    return 0;
}
