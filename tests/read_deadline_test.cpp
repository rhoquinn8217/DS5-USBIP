// A request's reads share one deadline (src/app/read_deadline.inl).
//
// ⭐ WHAT THESE PROTECT. With a timeout per read and none overall, a caller
// sending a byte a second held the loop that answers every TV for half an
// hour. The arithmetic is small, and a slip in it brings that back without a
// sound: a wait that never reaches 0 is the same fault.

#include "harness.h"

#include "app/read_deadline.inl"

using namespace ctmtest;

int run_read_deadline_tests()
{
    section("read deadline: each read waits at most what is left of the request's time");
    {
        const uint64_t start = 5000;
        const uint64_t end = start + 1000;
        // At the start, the whole cap.
        CTM_CHECK_EQ(read_deadline::wait_ms(start, end, 1000), 1000u);
        // Part way, only what is left: a slow caller cannot buy a new second
        // with every byte.
        CTM_CHECK_EQ(read_deadline::wait_ms(start + 900, end, 1000), 100u);
        CTM_CHECK_EQ(read_deadline::wait_ms(end - 1, end, 1000), 1u);
        // ⭐ Spent: no more reads at all.
        CTM_CHECK_EQ(read_deadline::wait_ms(end, end, 1000), 0u);
        CTM_CHECK_EQ(read_deadline::wait_ms(end + 30000, end, 1000), 0u);
        // A cap below what is left still caps.
        CTM_CHECK_EQ(read_deadline::wait_ms(start, start + 5000, 2000), 2000u);
    }

    section("read deadline: a byte a second ends within the request's time");
    {
        // The caller the review found: one byte every 900 ms, each read
        // inside its own second. Counted the way the loops count.
        const uint64_t start = 0;
        const uint64_t end = start + 1000;
        uint64_t now = start;
        int reads = 0;
        while (read_deadline::wait_ms(now, end, 1000) != 0 && reads < 10000) {
            ++reads;
            now += 900;
        }
        CTM_CHECK_EQ(reads, 2);
        CTM_CHECK(now <= end + 900);
    }

    return 0;
}
