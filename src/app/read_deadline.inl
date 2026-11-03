// A whole request within one deadline, not one timeout per read.
//
// ⛔ THE FAULT (code review, 2026-11-01). The control port and the REST port
// read a request on the one loop that answers every TV, the REST API and
// discovery, and gave each read its own timeout with no limit overall. A
// caller sending one byte a second kept every read inside its limit and held
// that loop for as long as it liked: the control port's line is up to 2 KiB,
// so half an hour; REST's head and body, longer. Now each read waits at most
// what is left of the request's own time, and none once it is spent.
//
// ⓘ Pure, so tests/read_deadline_test.cpp checks it without a socket.

#pragma once

#include <cstdint>

namespace read_deadline {

// How long the next read may wait, in milliseconds: what is left until `end`,
// at most `cap`; 0 once the time is up, which means do not read again.
inline uint32_t wait_ms(uint64_t now, uint64_t end, uint32_t cap)
{
    if (now >= end) return 0;
    const uint64_t left = end - now;
    return left < cap ? static_cast<uint32_t>(left) : cap;
}

}  // namespace read_deadline
