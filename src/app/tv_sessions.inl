// Which TV a bridge session belongs to, which port it gets, and what one TV
// may do to another's.
//
// ⭐⭐ TWO TVS AT ONCE (rhoquinn8217, 2026-10-08): "Having multiple streams is
// expected. Use case, having to users steam the host from 2 separate TVs in
// the same house or over the internet and allowing them to bridge their
// controllers is what we want."
//
// ⛔ WHAT STOPPED IT (code review, 2026-10-05). A session was known by its name
// alone, and the TV chose its own port. Every TV named its first DualSense the
// same and chose 48055, so the second TV was answered "OK" with no session --
// its pad then sat in the first TV's -- or it evicted the first TV's session,
// which took the same port.
//
// ➡️ A session records its TV: the fingerprint of the key the TV paired with
// the streaming host, when its pairing proof was PROVEN (paired_tv.inl), or ""
// for a TV that proved nothing -- an older app, or the proof switched off.
// Names and stale ports only count within one TV, a TV stops only its own
// sessions, and the listener hands out the ports, from [global]
// bridge_port_range, to a TV that asks for port 0.
//
// ⚠️ Only a PROVEN fingerprint: in the proof's log mode a failed proof still
// gets in, and its fingerprint is only a claim, which would let one TV stop
// another's pads by naming its fingerprint.
// ⓘ Its own file so the test binary can reach it.

#pragma once

#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

namespace tv_sessions {

constexpr uint16_t kFirstPort = 48055;
constexpr uint16_t kLastPort = 48074;

// A session that exists: its name, its port and its TV.
struct Held {
    std::string busId;
    uint16_t port = 0;
    std::string owner;
};

enum class Start {
    Create,         // a new session on `port`
    AlreadyYours,   // this TV asked again for a session it has: answer OK, make nothing
    ReplaceStale,   // this TV's older session holds the port: stop `stale`, then create
    NameTaken,      // another TV's session has this name: refuse
    PortTaken,      // another TV's session holds this port: refuse, never evict it
    NoFreePort      // the range is full: refuse
};

struct Plan {
    Start what = Start::Create;
    uint16_t port = 0;
    std::string stale;
};

// "48055-48074" into first and last. False, leaving both alone, for anything
// else: a blank, a reversed pair, a port under 1024, or more than 1000 ports.
inline bool parse_range(const std::string &text, uint16_t *first, uint16_t *last)
{
    const size_t dash = text.find('-');
    if (dash == std::string::npos || dash == 0 || dash + 1 >= text.size()) return false;
    for (size_t i = 0; i < text.size(); ++i) {
        if (i != dash && (text[i] < '0' || text[i] > '9')) return false;
    }
    const long a = std::strtol(text.substr(0, dash).c_str(), nullptr, 10);
    const long b = std::strtol(text.substr(dash + 1).c_str(), nullptr, 10);
    if (a < 1024 || b > 65535 || a > b || b - a >= 1000) return false;
    if (first) *first = static_cast<uint16_t>(a);
    if (last) *last = static_cast<uint16_t>(b);
    return true;
}

// The lowest port in [first, last] that no session holds and nothing else of
// the listener's uses. 0 when there is none.
inline uint16_t pick_port(const std::vector<Held> &held, uint16_t first, uint16_t last,
                          const std::vector<uint16_t> &reserved)
{
    for (uint32_t p = first; p <= last; ++p) {
        bool used = false;
        for (uint16_t r : reserved) used = used || r == p;
        for (const Held &h : held) used = used || h.port == p;
        if (!used) return static_cast<uint16_t>(p);
    }
    return 0;
}

// What a BRIDGE_START for `busId` from TV `owner` does. `port` 0 asks the
// listener to choose one.
inline Plan plan_start(const std::vector<Held> &held, const std::string &busId, uint16_t port,
                       const std::string &owner, uint16_t first, uint16_t last,
                       const std::vector<uint16_t> &reserved)
{
    Plan plan;
    for (const Held &h : held) {
        if (h.busId != busId) continue;
        plan.what = h.owner == owner ? Start::AlreadyYours : Start::NameTaken;
        plan.port = h.port;
        return plan;
    }
    if (port == 0) {
        plan.port = pick_port(held, first, last, reserved);
        plan.what = plan.port ? Start::Create : Start::NoFreePort;
        return plan;
    }
    plan.port = port;
    for (const Held &h : held) {
        if (h.port != port) continue;
        // ⓘ The TV's own older session on the port it asked for is one that
        // died without a stop, as before. Another TV's is a pad in use.
        if (h.owner == owner) {
            plan.what = Start::ReplaceStale;
            plan.stale = h.busId;
        } else {
            plan.what = Start::PortTaken;
        }
        return plan;
    }
    plan.what = Start::Create;
    return plan;
}

// May TV `owner` stop, or retire, a session of TV `sessionOwner`? Only its own.
// ⓘ Two TVs that proved nothing are both "" and still match each other, as
// every TV did before.
inline bool same_tv(const std::string &sessionOwner, const std::string &owner)
{
    return sessionOwner == owner;
}

} // namespace tv_sessions
