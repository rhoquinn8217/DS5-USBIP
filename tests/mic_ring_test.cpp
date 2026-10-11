// The microphone ring: the audio from a pad's microphone, held between the
// TV's sends and Windows' requests.
//
// ⭐ WHAT THESE PROTECT. The rules the ring's own header sets out, each of
// which has a way of going wrong that nobody would hear as a bug:
// - a short reply when it runs dry, never padding: padding claims a rate the
//   pad does not have;
// - the OLDEST dropped when it overflows: keeping it would make the delay
//   permanent;
// - one ring per session: with one shared ring, two pads' voices came out
//   mixed in both Windows microphones (2026-08-03);
// - a fixed table of four slots, so a slot not given back is gone for good:
//   after four sessions that never gave theirs back, every later microphone
//   was silent (code review, 2026-10-05). These pin that a reset gives the
//   slot back, and what a fifth session gets while four are held.
//
// WHAT THESE CANNOT DO. They do not show that a session gives its slot back
// when it ends, by any of the ways one ends: that is the bridge's teardown,
// and the idle rule's path through it is checked with a pad.

#include "harness.h"
#include "units.h"

#include <chrono>
#include <vector>

using namespace ctmtest;

namespace micring {

// ⓘ The ring only ever compares this pointer: any address names a session.
class CtmBackend;

inline uint64_t monotonic_us()
{
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

namespace device_log = units::device_log;

#include "audio/mic_ring.inl"

} // namespace micring

namespace {

// Distinct session identities: the ring never reads through them.
const micring::CtmBackend *session(int n)
{
    static char sessions[8];
    return reinterpret_cast<const micring::CtmBackend *>(&sessions[n]);
}

std::vector<uint8_t> bytes(size_t n, uint8_t start)
{
    std::vector<uint8_t> v(n);
    for (size_t i = 0; i < n; ++i) v[i] = static_cast<uint8_t>(start + i);
    return v;
}

void push(int s, const std::vector<uint8_t> &v)
{
    micring::mic_ring_push(session(s), v.data(), v.size());
}

std::vector<uint8_t> pop(int s, uint32_t want)
{
    std::vector<uint8_t> out;
    micring::mic_ring_pop_fill(session(s), &out, want);
    return out;
}

void reset_all()
{
    for (int i = 0; i < 8; ++i) micring::mic_ring_reset(session(i));
}

} // namespace

int run_mic_ring_tests()
{
    reset_all();

    section("mic ring: what arrives is handed over in order, and only once");
    {
        push(0, bytes(10, 1));
        push(0, bytes(5, 100));
        const std::vector<uint8_t> first = pop(0, 8);
        const std::vector<uint8_t> second = pop(0, 100);
        CTM_CHECK(first == bytes(8, 1));
        std::vector<uint8_t> rest = bytes(2, 9);
        const std::vector<uint8_t> tail = bytes(5, 100);
        rest.insert(rest.end(), tail.begin(), tail.end());
        CTM_CHECK(second == rest);
        CTM_CHECK(pop(0, 100).empty());
        reset_all();
    }

    section("mic ring: running dry is a short reply, never padding");
    {
        push(0, bytes(30, 1));
        const std::vector<uint8_t> got = pop(0, 192);
        CTM_CHECK_EQ(got.size(), static_cast<size_t>(30));
        CTM_CHECK(got == bytes(30, 1));
        reset_all();
    }

    section("mic ring: an overflow drops the OLDEST and keeps the newest 200 ms");
    {
        const size_t cap = micring::kMicRingCapacity;
        CTM_CHECK_EQ(cap, static_cast<size_t>(38400));           // 192000 bytes a second / 5
        std::vector<uint8_t> old(cap, 0x11);
        std::vector<uint8_t> fresh(1000, 0x22);
        push(0, old);
        push(0, fresh);
        const std::vector<uint8_t> got = pop(0, static_cast<uint32_t>(cap + 5000));
        CTM_CHECK_EQ(got.size(), cap);
        // The last thousand bytes are the newest, and only the oldest went.
        bool newestKept = true;
        for (size_t i = cap - 1000; i < cap; ++i) newestKept = newestKept && got[i] == 0x22;
        CTM_CHECK(newestKept);
        CTM_CHECK_EQ(static_cast<int>(got[0]), 0x11);
        reset_all();
    }

    section("mic ring: two sessions never mix");
    {
        push(0, bytes(4, 10));
        push(1, bytes(4, 200));
        CTM_CHECK(pop(1, 100) == bytes(4, 200));
        CTM_CHECK(pop(0, 100) == bytes(4, 10));
        reset_all();
    }

    section("mic ring: asking for a session nothing has arrived for takes no slot");
    {
        CTM_CHECK(pop(5, 100).empty());
        // ⓘ So four others still fit: a pop must not have used one up.
        for (int s = 0; s < 4; ++s) push(s, bytes(2, static_cast<uint8_t>(s)));
        for (int s = 0; s < 4; ++s) CTM_CHECK(pop(s, 10) == bytes(2, static_cast<uint8_t>(s)));
        reset_all();
    }

    section("mic ring: with four slots held a fifth session gets nothing, and a reset frees one");
    {
        for (int s = 0; s < 4; ++s) push(s, bytes(2, 1));
        push(4, bytes(3, 50));
        CTM_CHECK(pop(4, 10).empty());              // dropped, not mixed into another
        CTM_CHECK(pop(0, 10) == bytes(2, 1));       // and nobody else's ring got it
        micring::mic_ring_reset(session(2));
        push(4, bytes(3, 50));
        CTM_CHECK(pop(4, 10) == bytes(3, 50));
        reset_all();
    }

    section("mic ring: a reset throws away what was waiting, so a new session starts clean");
    {
        push(0, bytes(20, 1));
        micring::mic_ring_reset(session(0));
        CTM_CHECK(pop(0, 100).empty());
        push(0, bytes(3, 7));
        CTM_CHECK(pop(0, 100) == bytes(3, 7));
        reset_all();
    }

    section("mic ring: nothing to push, or nowhere to put it, changes nothing");
    {
        micring::mic_ring_push(nullptr, bytes(4, 1).data(), 4);
        micring::mic_ring_push(session(0), nullptr, 4);
        const std::vector<uint8_t> four = bytes(4, 1);
        micring::mic_ring_push(session(0), four.data(), 0);
        CTM_CHECK(pop(0, 100).empty());
        std::vector<uint8_t> out(5, 9);
        micring::mic_ring_pop_fill(session(0), &out, 0);
        CTM_CHECK(out.empty());
        reset_all();
    }

    return 0;
}
