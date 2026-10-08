// The rebinder's per-pad "was this button held" flags.
//
// ⭐ WHAT THESE PROTECT. A binding that fires once per press -- the on-screen
// keyboard's toggle, a wheel click, the settings window's Square -- fires on
// the edge from released to pressed. These pin that edge per pad, per button
// and per binding, and that a pad going away takes its flags with it: before,
// nothing did, and a later pad handed the same address lost its first press.
//
// WHAT THESE CANNOT DO. The lock is exercised by two threads at once below,
// but a race that never happened proves nothing; that the table is locked is
// read off the code. That a real pad's first press opens the keyboard is
// checked with a pad.

#include "harness.h"

#include <functional>
#include <map>
#include <mutex>
#include <thread>

#include "input/held_edges.inl"

using namespace ctmtest;

int run_held_edges_tests()
{
    int padA = 0, padB = 0, padC = 0;   // only their addresses are used

    section("held edges: a press is fresh once, then held until released");
    {
        CTM_CHECK(!held_edges::exchange(held_edges::kOsk, &padA, 3, true));   // fresh
        CTM_CHECK(held_edges::exchange(held_edges::kOsk, &padA, 3, true));    // held
        CTM_CHECK(held_edges::exchange(held_edges::kOsk, &padA, 3, false));   // was held
        CTM_CHECK(!held_edges::exchange(held_edges::kOsk, &padA, 3, true));   // fresh again
    }

    section("held edges: each pad, button and binding has its own flag");
    {
        CTM_CHECK(!held_edges::exchange(held_edges::kOsk, &padB, 3, true));
        CTM_CHECK(!held_edges::exchange(held_edges::kOsk, &padA, 4, true));
        CTM_CHECK(!held_edges::exchange(held_edges::kWheel, &padA, 3, true));
        CTM_CHECK(!held_edges::exchange(held_edges::kGateSquare, &padA, 3, true));
    }

    section("held edges: a pad that goes away takes its flags, and no one else's");
    {
        CTM_CHECK(held_edges::entries_for(&padA) > 0);
        held_edges::forget(&padA);
        CTM_CHECK_EQ(held_edges::entries_for(&padA), static_cast<size_t>(0));
        // ⛔ The case that was lost: a later pad at the same address, its
        // first press held down. Without the forget this read as already held.
        CTM_CHECK(!held_edges::exchange(held_edges::kOsk, &padA, 3, true));
        CTM_CHECK(held_edges::exchange(held_edges::kOsk, &padB, 3, true));   // B untouched
    }

    section("held edges: two pads pressing at once, and a third going away");
    {
        held_edges::forget(&padA);
        held_edges::forget(&padB);
        auto press = [](const void *pad) {
            for (int n = 0; n < 20000; ++n) {
                (void)held_edges::exchange(held_edges::kWheel, pad, n % 17, (n & 1) != 0);
            }
        };
        std::thread a(press, &padA);
        std::thread b(press, &padB);
        std::thread c([&padC]() {
            for (int n = 0; n < 2000; ++n) {
                (void)held_edges::exchange(held_edges::kOsk, &padC, 1, true);
                held_edges::forget(&padC);
            }
        });
        a.join();
        b.join();
        c.join();
        CTM_CHECK_EQ(held_edges::entries_for(&padA), static_cast<size_t>(17));
        CTM_CHECK_EQ(held_edges::entries_for(&padB), static_cast<size_t>(17));
        CTM_CHECK_EQ(held_edges::entries_for(&padC), static_cast<size_t>(0));
    }

    held_edges::forget(&padA);
    held_edges::forget(&padB);
    return 0;
}
