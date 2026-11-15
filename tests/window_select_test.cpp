// Window Select: Alt held through Windows' window picker, as Desktop Mode's L1
// and R1 do it on the TV.
//
// ⭐ WHAT THESE PROTECT (rhoquinn8217, 2026-11-12, Desktop Mode's hand-over:
// "The transition between aurora-tv managing controllers and DS5-USBIP
// managing controllers needs to be seemless"): Alt down first and held, a Tab
// (or Shift+Tab) a press; Enter picks, Escape cancels with Esc, a click goes
// out before Alt goes up, the arrows, Tab and another step keep Alt, and
// anything else lets Alt go first.
//
// WHAT THESE CANNOT DO. No keyboard here: the keys reaching Windows' picker,
// and the picker switching, are looked at on the PC.

#include "harness.h"

#include "input/window_select.inl"

using namespace ctmtest;

namespace {

// Asks for the keys every 4 ms, as reports at 250 Hz do, from `from` to `to`;
// counts the reports Alt was down and each key's down reports.
struct Run {
    int altReports = 0;
    int tabReports = 0;
    int shiftTabReports = 0;
    int escReports = 0;
    bool altAtEnd = false;
};

Run run(window_select::State &s, long long from, long long to)
{
    Run r;
    for (long long t = from; t < to; t += 4) {
        const window_select::Keys k = window_select::keys(s, t);
        if (k.alt) ++r.altReports;
        if (k.key == window_select::kTab) ++r.tabReports;
        if (k.key == window_select::kShiftTab) ++r.shiftTabReports;
        if (k.key == window_select::kEscape) ++r.escReports;
        r.altAtEnd = k.alt;
    }
    return r;
}

} // namespace

int run_window_select_tests()
{
    using namespace window_select;

    section("window select: the two values, folded as the config reader lowercases them");
    {
        CTM_CHECK_EQ(step_of("WindowSelectNext"), 1);
        CTM_CHECK_EQ(step_of("windowselectnext"), 1);
        CTM_CHECK_EQ(step_of("WindowSelectBack"), -1);
        CTM_CHECK_EQ(step_of("windowselectback"), -1);
        CTM_CHECK_EQ(step_of("Tab"), 0);
        CTM_CHECK_EQ(step_of(""), 0);
        CTM_CHECK_EQ(step_of("WindowSelect"), 0);
    }

    section("window select: what another button does while Alt is held, by what it is bound to");
    {
        CTM_CHECK(classify("Enter") == Other::Pick);
        CTM_CHECK(classify("enter") == Other::Pick);
        CTM_CHECK(classify("Escape") == Other::Cancel);
        CTM_CHECK(classify("Tab") == Other::Keep);
        CTM_CHECK(classify("ArrowLeft") == Other::Keep);
        CTM_CHECK(classify("arrowdown") == Other::Keep);
        CTM_CHECK(classify("ShiftLeft") == Other::Keep);
        CTM_CHECK(classify("WindowSelectBack") == Other::Keep);
        CTM_CHECK(classify("MouseLeft") == Other::Click);
        CTM_CHECK(classify("mouseright") == Other::Click);
        CTM_CHECK(classify("Space") == Other::LetGo);
        CTM_CHECK(classify("KeyboardDS5_USBIP") == Other::LetGo);
        CTM_CHECK(classify("button_cross") == Other::LetGo);
        CTM_CHECK(classify("MouseWheelUp") == Other::LetGo);
    }

    section("window select: a press holds Alt, alone first, then one Tab; Alt stays down");
    {
        State s;
        CTM_CHECK(!active(s));
        step(s, 1, 0);
        CTM_CHECK(active(s));
        const Run r = run(s, 0, 1000);
        CTM_CHECK(r.tabReports > 0 && r.shiftTabReports == 0 && r.escReports == 0);
        CTM_CHECK(r.altAtEnd);                            // held between presses
        // ⓘ Alt alone before the Tab: the first report has no key.
        State t;
        step(t, 1, 0);
        CTM_CHECK(keys(t, 0).alt && keys(t, 0).key == kNone);
    }

    section("window select: back is Shift+Tab; two presses are two keys, one after the other");
    {
        State s;
        step(s, -1, 0);
        step(s, -1, 1);
        Run r = run(s, 0, 1000);
        CTM_CHECK(r.shiftTabReports > 0 && r.tabReports == 0);
        // Two separate keys: down, up, down.
        State t;
        step(t, 1, 0);
        step(t, 1, 0);
        int downs = 0;
        bool was = false;
        for (long long ms = 0; ms < 1000; ms += 4) {
            const bool down = keys(t, ms).key == kTab;
            if (down && !was) ++downs;
            was = down;
        }
        CTM_CHECK_EQ(downs, 2);
    }

    section("window select: Enter picks, Alt up, its key held back");
    {
        State s;
        step(s, 1, 0);
        run(s, 0, 200);
        CTM_CHECK(other_press(s, Other::Pick, 200));      // held back while held
        CTM_CHECK(!active(s));
        const Run r = run(s, 200, 400);
        CTM_CHECK(!r.altAtEnd && r.escReports == 0);
        CTM_CHECK(!s.alt);
    }

    section("window select: Escape cancels: Esc with Alt held, then Alt up");
    {
        State s;
        step(s, 1, 0);
        run(s, 0, 200);
        CTM_CHECK(other_press(s, Other::Cancel, 200));
        const Run r = run(s, 200, 600);
        CTM_CHECK(r.escReports > 0);
        CTM_CHECK(!r.altAtEnd);
    }

    section("window select: a click goes out first, Alt up after it");
    {
        State s;
        step(s, 1, 0);
        run(s, 0, 200);
        CTM_CHECK(!other_press(s, Other::Click, 200));    // the click itself goes out
        CTM_CHECK(keys(s, 200).alt);                      // Alt still down with it
        const Run r = run(s, 204, 400);
        CTM_CHECK(!r.altAtEnd);
    }

    section("window select: an arrow or Tab keeps Alt; anything else lets it go");
    {
        State s;
        step(s, 1, 0);
        run(s, 0, 200);
        CTM_CHECK(!other_press(s, Other::Keep, 200));
        CTM_CHECK(active(s) && run(s, 200, 600).altAtEnd);
        CTM_CHECK(!other_press(s, Other::LetGo, 600));    // its job waits for Alt (rebind.inl)
        CTM_CHECK(!active(s));
        CTM_CHECK(!run(s, 600, 800).altAtEnd);
    }

    section("window select: a step while Alt is going up takes it back, and a Tab still in flight finishes first");
    {
        State s;
        step(s, 1, 0);
        other_press(s, Other::Pick, 0);                   // picked before the first Tab went
        const Run r = run(s, 0, 400);
        CTM_CHECK(r.tabReports > 0);                      // the Tab on its way went
        CTM_CHECK(!r.altAtEnd);
        State t;
        step(t, 1, 0);
        run(t, 0, 200);
        other_press(t, Other::Click, 200);
        step(t, 1, 204);                                  // stepped again before Alt went up
        CTM_CHECK(active(t));
        CTM_CHECK(run(t, 204, 800).altAtEnd);
    }

    return 0;
}
