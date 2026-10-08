// The Options hold that steers a window: when it says the hold is over.
//
// ⭐ WHAT THESE PROTECT. Steering the settings window used to write
// window-state.txt on almost every report of the hold, up to about 250 times a
// second (code review, 2026-10-05). It now moves the window in memory and
// writes the place ONCE, on the report where the step says `released`. So that
// flag carries the whole of the saving: missing it loses the place a person
// steered to, and raising it on every report brings the writes back. These pin
// it to exactly one report per hold -- the one where Options comes up -- for a
// hold that steered and for one that did not.
//
// WHAT THESE CANNOT DO. They move no window and write no file: that the place
// lands in window-state.txt when the hold ends is checked with a pad, by
// steering the window and reading the file's time.
//
// ⓘ A hold that was steered by the STICK is used where the tap matters, since
// the mover also follows the real mouse: a test that moved the mouse by hand
// while it ran would turn an unsteered hold into a steered one.

#include "harness.h"

#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

#include "app/window_move.inl"

using namespace ctmtest;

namespace {

// A DualSense report at rest: sticks centred, nothing pressed.
std::vector<uint8_t> rest_report()
{
    std::vector<uint8_t> r(64, 0);
    r[1] = r[2] = r[3] = r[4] = 0x80;
    r[8] = 0x08;
    return r;
}

} // namespace

int run_window_move_tests()
{
    const ctm_rebind::Layout *ds5 = ctm_rebind::layout_for("ds5");
    CTM_CHECK(ds5 != nullptr);
    if (ds5 == nullptr) return 0;

    section("window move: a hold says released once, on the report where Options comes up");
    {
        window_move::Mover m;
        const std::vector<uint8_t> r = rest_report();
        const window_move::Step a = m.step(true, *ds5, r.data(), r.size());
        const window_move::Step b = m.step(true, *ds5, r.data(), r.size());
        const window_move::Step c = m.step(false, *ds5, r.data(), r.size());
        const window_move::Step d = m.step(false, *ds5, r.data(), r.size());
        CTM_CHECK(a.holding && !a.released);
        CTM_CHECK(b.holding && !b.released);
        CTM_CHECK(!c.holding && c.released);
        // ⛔ Only once: a report after the release is no hold at all.
        CTM_CHECK(!d.holding && !d.released);
    }

    section("window move: with Options never held there is nothing to release");
    {
        window_move::Mover m;
        const std::vector<uint8_t> r = rest_report();
        int released = 0;
        for (int i = 0; i < 20; ++i) {
            if (m.step(false, *ds5, r.data(), r.size()).released) ++released;
        }
        CTM_CHECK_EQ(released, 0);
    }

    section("window move: a hold the stick steered is released too, and is no tap");
    {
        window_move::Mover m;
        std::vector<uint8_t> r = rest_report();
        r[1] = 0xFF;                                   // left stick hard right
        int moved = 0;
        bool releasedEarly = false;
        m.step(true, *ds5, r.data(), r.size());
        // ⓘ The stick moves the window by the clock, so the hold has to last
        // a little: no time between reports would be no distance.
        for (int i = 0; i < 6; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            const window_move::Step s = m.step(true, *ds5, r.data(), r.size());
            moved += s.dx;
            releasedEarly = releasedEarly || s.released;
        }
        const window_move::Step up = m.step(false, *ds5, r.data(), r.size());
        CTM_CHECK(moved > 0);
        CTM_CHECK(!releasedEarly);
        CTM_CHECK(up.released);
        // The steered hold must not also snap the window away from where it
        // was just put.
        CTM_CHECK(!up.tapped);
    }

    section("window move: a hold dropped because the window lost the front is not released later");
    {
        // ⓘ The caller writes the place itself when it drops a hold, so a
        // release seen after that would be a second write for nothing -- and,
        // as a tap, would snap a window nobody was steering.
        window_move::Mover m;
        const std::vector<uint8_t> r = rest_report();
        m.step(true, *ds5, r.data(), r.size());
        m.abandon();
        const window_move::Step up = m.step(false, *ds5, r.data(), r.size());
        CTM_CHECK(!up.released);
        CTM_CHECK(!up.tapped);
    }

    section("window move: each pad's hold is released by that pad alone");
    {
        // ⓘ The per-pad movers (two pads, one holding Options): the other
        // pad's reports, Options up as always, end nothing.
        // ⛔ STATIC: a Movers puts itself on a process-wide list it never
        // leaves, so one on the stack would leave that list pointing at
        // nothing once this block ends.
        static window_move::Movers movers;
        int padA = 0, padB = 0;
        const std::vector<uint8_t> r = rest_report();
        movers.for_key(&padA).step(true, *ds5, r.data(), r.size());
        const window_move::Step other = movers.for_key(&padB).step(false, *ds5, r.data(), r.size());
        CTM_CHECK(!other.released);
        CTM_CHECK(movers.holding(&padA));
        const window_move::Step mine = movers.for_key(&padA).step(false, *ds5, r.data(), r.size());
        CTM_CHECK(mine.released);
        CTM_CHECK(!movers.holding(&padA));
    }

    return 0;
}
