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

    section("tutorial words: [[*glyph]] is a button's mark, [[name]] stays a lit name");
    {
        const auto toks = tbr::tokens(tbr::parse(L"Press [[*\u25b3]] or [[OK]], then [[*L1]]."));
        // Press, ' ', the mark, ' ', or, ' ', OK, ',', ' ', then, ' ', the mark, '.'
        CTM_CHECK_EQ(toks.size(), size_t(13));
        CTM_CHECK(toks[2].mark && toks[2].lit && toks[2].text == L"\u25b3");
        CTM_CHECK(toks[6].lit && !toks[6].mark && toks[6].text == L"OK");
        CTM_CHECK(toks[11].mark && toks[11].text == L"L1");
        CTM_CHECK(!toks[0].mark && !toks[1].mark);
        // Create's three strokes come through whole, backslash and all.
        const auto create = tbr::tokens(tbr::parse(L"[[*\\|/]]"));
        CTM_CHECK(create.size() == 1 && create[0].mark && create[0].text == L"\\|/");
        // A star alone is no button: a lit name of one character.
        const auto star = tbr::tokens(tbr::parse(L"[[*]]"));
        CTM_CHECK(star.size() == 1 && star[0].lit && !star[0].mark && star[0].text == L"*");
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

    // ⭐ A button carries the pad's button for it (2026-10-29: "add R2 symbol
    // in the "Continue" button, L2 to the "Go Back" button and "Circle" to the
    // "End Tutorial" button"), or a key's name; one with none still reads.
    section("tutorial buttons: the button or key that presses each, when it has one");
    {
        const auto b = tbr::parse_buttons("next:Continue:go:*R2|prev:Go back:plain:PgUp|end:End tutorial:plain");
        CTM_CHECK_EQ(b.size(), size_t(3));
        CTM_CHECK(b[0].id == "next" && b[0].kind == "go" && b[0].mark == "*R2");
        CTM_CHECK(b[1].label == "Go back" && b[1].kind == "plain" && b[1].mark == "PgUp");
        CTM_CHECK(b[2].kind == "plain" && b[2].mark.empty());
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

    // ⭐ A step names its side (rhoquinn8217, 2026-10-23: "Put the bubble on the
    // left side of the config", "Move bubble to right side of window", "Move
    // bubble to bottom side of window").
    section("tutorial words: on the side a step asks for, when it has room");
    {
        const tbr::Box quick{ 540, 820, 1380, 1030 };
        const std::vector<std::pair<int, int>> mid{ { 500, 500 } };
        const auto left = tbr::place_at(quick, work, 380, 130, 12, 10, 10, mid, tbr::Prefer::Left);
        CTM_CHECK(left.side == tbr::Side::Left);
        CTM_CHECK(left.window.right <= quick.left);
        const auto right = tbr::place_at(quick, work, 380, 130, 12, 10, 10, mid, tbr::Prefer::Right);
        CTM_CHECK(right.side == tbr::Side::Right);
        CTM_CHECK(right.window.left >= quick.right);
        CTM_CHECK(inside(left.window, work) && inside(right.window, work));
        CTM_CHECK(tbr::parse_prefer("left") == tbr::Prefer::Left);
        CTM_CHECK(tbr::parse_prefer("below") == tbr::Prefer::Below);
        CTM_CHECK(tbr::parse_prefer("") == tbr::Prefer::None);
        CTM_CHECK(tbr::parse_prefer("sideways") == tbr::Prefer::None);
    }

    // ⭐ The tours' opening card and last step (rhoquinn8217, 2026-10-26: "For
    // the introduction and closing step of the tutorial, put the bubble in the
    // center of the config window").
    section("tutorial words: center is over the window's middle, and on the screen");
    {
        CTM_CHECK(tbr::parse_prefer("center") == tbr::Prefer::Center);
        const tbr::Box simple{ 528, 560, 1392, 1030 };
        const auto p = tbr::place_at(simple, work, 380, 130, 12, 10, 10, { { 500, 500 } }, tbr::Prefer::Center);
        CTM_CHECK(inside(p.window, simple));
        CTM_CHECK_EQ(p.window.width(), 380);
        CTM_CHECK_EQ(p.window.height(), 130);
        CTM_CHECK_EQ(p.window.left + p.window.width() / 2, simple.left + simple.width() / 2);
        CTM_CHECK_EQ(p.window.top + p.window.height() / 2, simple.top + simple.height() / 2);
        // A window mostly off the screen's edge: the words stay on it.
        const tbr::Box off{ 1800, 980, 2400, 1300 };
        const auto q = tbr::place_at(off, work, 380, 130, 12, 10, 10, { { 500, 500 } }, tbr::Prefer::Center);
        CTM_CHECK(inside(q.window, work));
    }

    // ⭐ Beside what is lit, over the window where it covers nothing
    // (rhoquinn8217, 2026-10-26: "I also want the bubble to overlap the window
    // as long as it's not covering any text or elements on the window").
    section("tutorial words: beside what is lit, over the window's empty part, off what is on it");
    {
        // Simple's window: its title bar, the name the step lights, a panel under it.
        const tbr::Box simple{ 528, 560, 1392, 1030 };
        const tbr::Box name{ 650, 600, 1000, 650 };
        const std::vector<tbr::Box> on = { name, { 560, 680, 1360, 1000 }, { 528, 560, 1392, 590 } };
        tbr::Placement p;
        CTM_CHECK(tbr::place_near(name, work, 200, 60, on, tbr::Prefer::Left, 20, 8, 900, 6, &p));
        CTM_CHECK(p.side == tbr::Side::Left);
        CTM_CHECK(p.window.right <= name.left - 20);    // room left for the arrow
        CTM_CHECK(!tbr::overlaps_any(p.window, on));
        CTM_CHECK(inside(p.window, work));
        CTM_CHECK(p.window.right > simple.left);        // over the window's own empty corner
    }

    section("tutorial words: the step's own side first, even when another is nearer");
    {
        const tbr::Box lit{ 600, 700, 1300, 760 };
        // Everything around it on the window is taken; above the window is free.
        const std::vector<tbr::Box> on = { lit, { 528, 560, 1392, 690 }, { 528, 770, 1392, 1030 } };
        tbr::Placement p;
        CTM_CHECK(tbr::place_near(lit, work, 300, 100, on, tbr::Prefer::Above, 10, 8, 900, 6, &p));
        CTM_CHECK(p.side == tbr::Side::Above);
        CTM_CHECK(p.window.bottom <= 560);
        CTM_CHECK(!tbr::overlaps_any(p.window, on));
    }

    section("tutorial words: nowhere to stand leaves them to the rule before");
    {
        const tbr::Box lit{ 900, 500, 1000, 540 };
        tbr::Placement p;
        CTM_CHECK(!tbr::place_near(lit, work, 300, 100, { work }, tbr::Prefer::Left, 10, 8, 900, 6, &p));
    }

    section("tutorial words: boxes as the page sends them, a broken one left out");
    {
        const auto b = tbr::parse_boxes("10,20,30,40|bad|5,5,5,9|0,0,2000,1000|1,2,3");
        CTM_CHECK_EQ(b.size(), size_t(2));
        CTM_CHECK(b[0].left == 10 && b[0].top == 20 && b[0].right == 30 && b[0].bottom == 40);
        CTM_CHECK(b[1].right == 1000);
        CTM_CHECK(tbr::parse_boxes("").empty());
        CTM_CHECK(std::string(tbr::side_name(tbr::Side::Left)) == "left");
    }

    section("tutorial words: below asked with no room below goes above, still clear of the window");
    {
        // Quick opens at the bottom of the screen: nothing fits under it.
        const tbr::Box quick{ 540, 820, 1380, 1030 };
        const auto p = tbr::place_at(quick, work, 380, 130, 12, 10, 10, { { 500, 500 } }, tbr::Prefer::Below);
        CTM_CHECK(p.side == tbr::Side::Above);
        CTM_CHECK(!overlaps(p.window, quick));
        // Moved up the screen, there is room below, and it goes there.
        const tbr::Box raised{ 540, 300, 1380, 510 };
        const auto q = tbr::place_at(raised, work, 380, 130, 12, 10, 10, { { 500, 500 } }, tbr::Prefer::Below);
        CTM_CHECK(q.side == tbr::Side::Below);
        CTM_CHECK(q.window.top >= raised.bottom);
    }

    // ⭐ L1 and R1 in the panel's two corners, an arrow at each (rhoquinn8217,
    // 2026-10-23: "Put and arrow pointing at the R2/RB/E as well").
    section("tutorial words: two arrows far apart, the words widened to stand over both");
    {
        const tbr::Box quick{ 540, 820, 1380, 1030 };
        const auto p = tbr::place_at(quick, work, 380, 130, 12, 10, 10,
                                     { { 40, 60 }, { 960, 60 } }, tbr::Prefer::None);
        CTM_CHECK(p.side == tbr::Side::Above);
        CTM_CHECK_EQ(p.arrowsAt.size(), size_t(2));
        CTM_CHECK(p.window.width() > 380);
        // Each arrow stands over its corner, to the pixel the rule allows.
        const int l1 = quick.left + quick.width() * 40 / 1000;
        const int r1 = quick.left + quick.width() * 960 / 1000;
        CTM_CHECK_EQ(p.window.left + p.arrowsAt[0], l1);
        CTM_CHECK_EQ(p.window.left + p.arrowsAt[1], r1);
        CTM_CHECK(!overlaps(p.window, quick));
        CTM_CHECK(inside(p.window, work));
    }

    section("tutorial words: anchors as the page sends them, a broken one left out");
    {
        const auto a = tbr::parse_anchors("40,60|960,60");
        CTM_CHECK_EQ(a.size(), size_t(2));
        CTM_CHECK(a[0].first == 40 && a[0].second == 60 && a[1].first == 960);
        const auto b = tbr::parse_anchors("1,2,3|x,5|7,|,8|99999999999999,5|500,500");
        CTM_CHECK_EQ(b.size(), size_t(2));
        CTM_CHECK(b[0].first == 1000 && b[0].second == 5);   // past 1000 reads as 1000
        CTM_CHECK(b[1].first == 500 && b[1].second == 500);
        CTM_CHECK(tbr::parse_anchors("").empty());
    }

    section("tutorial words: the slide starts and ends slowly, and is quickest half way");
    {
        CTM_CHECK(tbr::ease_in_out(0.0) == 0.0 && tbr::ease_in_out(1.0) == 1.0);
        CTM_CHECK(tbr::ease_in_out(-1.0) == 0.0 && tbr::ease_in_out(2.0) == 1.0);
        CTM_CHECK(tbr::ease_in_out(0.5) > 0.4999 && tbr::ease_in_out(0.5) < 0.5001);
        // Never back the way it came, and the two halves mirror each other.
        double last = 0.0;
        bool forward = true, mirrored = true;
        for (int i = 1; i <= 100; ++i) {
            const double t = i / 100.0, e = tbr::ease_in_out(t);
            if (e < last) forward = false;
            const double d = e + tbr::ease_in_out(1.0 - t) - 1.0;
            if (d > 1e-9 || d < -1e-9) mirrored = false;
            last = e;
        }
        CTM_CHECK(forward);
        CTM_CHECK(mirrored);
        // Slow off the mark and into the stop, quickest in the middle tenth.
        const double first = tbr::ease_in_out(0.1), middle = tbr::ease_in_out(0.55) - tbr::ease_in_out(0.45);
        CTM_CHECK(first < 0.01);
        CTM_CHECK(middle > 0.14);
        CTM_CHECK(1.0 - tbr::ease_in_out(0.9) < 0.01);
    }

    section("tutorial words: a longer slide takes longer, within a third to a half of a second");
    {
        CTM_CHECK_EQ(tbr::slide_ms(0, 96), 260);
        CTM_CHECK_EQ(tbr::slide_ms(400, 96), 360);
        CTM_CHECK_EQ(tbr::slide_ms(600, 144), 360);   // the same way at 150%
        CTM_CHECK_EQ(tbr::slide_ms(5000, 96), 480);
        CTM_CHECK(tbr::slide_ms(200, 96) < tbr::slide_ms(500, 96));
        CTM_CHECK_EQ(tbr::slide_ms(400, 0), 360);     // no dpi reads as 96
    }
    return 0;
}
