// Two TVs at once: which TV a bridge session belongs to, which port it gets,
// and what one TV may do to another's.
//
// ⭐ WHAT THESE PROTECT (rhoquinn8217, 2026-10-08: two users streaming from two
// TVs, each bridging their own controllers, is wanted). Every TV named its
// first DualSense the same and chose port 48055, so a second TV was either
// handed the first's session or evicted it. These pin app/tv_sessions.inl.
//
// WHAT THESE CANNOT DO. They open no socket and start no session: that
// agent.inl asks these, with the fingerprint a proof proved, is checked by
// bridging from two TVs at once.

#include "harness.h"

#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "app/tv_sessions.inl"

using namespace ctmtest;

namespace {

const std::string kTvA = "0123456789abcdef0123456789abcdef";
const std::string kTvB = "fedcba9876543210fedcba9876543210";
const std::vector<uint16_t> kReserved = {48053, 48054, 3240};

std::string read_agent_text()
{
    const char *candidates[] = {
        "src/app/agent.inl",
        "../src/app/agent.inl",
        "../../src/app/agent.inl",
        "../../../src/app/agent.inl",
        "../../../../src/app/agent.inl",
    };
    for (const char *where : candidates) {
        std::ifstream in(where, std::ios::binary);
        if (!in) continue;
        std::stringstream buffer;
        buffer << in.rdbuf();
        return buffer.str();
    }
    return "";
}

} // namespace

int run_tv_sessions_tests()
{
    using tv_sessions::Held;
    using tv_sessions::Start;

    section("tv sessions: the range setting");
    {
        uint16_t first = 1, last = 2;
        CTM_CHECK(tv_sessions::parse_range("48055-48074", &first, &last));
        CTM_CHECK_EQ(first, static_cast<uint16_t>(48055));
        CTM_CHECK_EQ(last, static_cast<uint16_t>(48074));
        CTM_CHECK(tv_sessions::parse_range("50000-50000", &first, &last));
        first = 1; last = 2;
        CTM_CHECK(!tv_sessions::parse_range("", &first, &last));
        CTM_CHECK(!tv_sessions::parse_range("48074-48055", &first, &last));   // reversed
        CTM_CHECK(!tv_sessions::parse_range("80-90", &first, &last));         // under 1024
        CTM_CHECK(!tv_sessions::parse_range("48055 - 48074", &first, &last)); // spaces
        CTM_CHECK(!tv_sessions::parse_range("2000-9000", &first, &last));     // over 1000 ports
        CTM_CHECK(!tv_sessions::parse_range("48055-", &first, &last));
        CTM_CHECK_EQ(first, static_cast<uint16_t>(1));                         // left alone
    }

    section("tv sessions: two TVs, each with its first DualSense");
    {
        // ⛔ The fault: the same name and the same port from both TVs.
        std::vector<Held> held = {{"ctm-ds5-1", 48055, kTvA}};
        const auto plan = tv_sessions::plan_start(held, "ctm-ds5-1", 48055, kTvB,
                                                  48055, 48074, kReserved, false);
        CTM_CHECK(plan.what != Start::AlreadyYours);   // never TV A's session for TV B
        CTM_CHECK(plan.what != Start::ReplaceStale);   // and never an eviction
        CTM_CHECK(plan.what == Start::NameTaken);
    }

    section("tv sessions: the listener hands out the port");
    {
        std::vector<Held> held = {{"ctm-ds5-1-aaaaaa", 48055, kTvA},
                                  {"ctm-mouse-1-aaaaaa", 48056, kTvA}};
        auto plan = tv_sessions::plan_start(held, "ctm-ds5-1-bbbbbb", 0, kTvB, 48055, 48074, kReserved, false);
        CTM_CHECK(plan.what == Start::Create);
        CTM_CHECK_EQ(plan.port, static_cast<uint16_t>(48057));   // the next free one, not TV A's
        held.push_back({"ctm-ds5-1-bbbbbb", plan.port, kTvB});
        plan = tv_sessions::plan_start(held, "ctm-ds5-1-bbbbbb", 0, kTvB, 48055, 48074, kReserved, false);
        CTM_CHECK(plan.what == Start::AlreadyYours);              // asked twice: the same session
        CTM_CHECK_EQ(plan.port, static_cast<uint16_t>(48057));
        // A reserved port inside a range is never handed out, from either end.
        plan = tv_sessions::plan_start({}, "x", 0, kTvA, 48053, 48056, kReserved, false);
        CTM_CHECK_EQ(plan.port, static_cast<uint16_t>(48055));
        plan = tv_sessions::plan_start({}, "x", 0, kTvA, 48050, 48054, kReserved, true);
        CTM_CHECK_EQ(plan.port, static_cast<uint16_t>(48052));
        // A full range says so.
        plan = tv_sessions::plan_start({{"a", 49000, kTvA}}, "b", 0, kTvB, 49000, 49000, kReserved, false);
        CTM_CHECK(plan.what == Start::NoFreePort);
    }

    section("tv sessions: home from the top, another site from the bottom");
    {
        // ⭐ A TV at another site keeps the low ports the router already
        // forwards, however many pads a TV at home has bridged first.
        std::vector<Held> held;
        auto plan = tv_sessions::plan_start(held, "home-pad-1", 0, kTvA, 48055, 48074, kReserved, true);
        CTM_CHECK_EQ(plan.port, static_cast<uint16_t>(48074));   // home: the top
        held.push_back({"home-pad-1", plan.port, kTvA});
        plan = tv_sessions::plan_start(held, "home-pad-2", 0, kTvA, 48055, 48074, kReserved, true);
        CTM_CHECK_EQ(plan.port, static_cast<uint16_t>(48073));
        held.push_back({"home-pad-2", plan.port, kTvA});
        plan = tv_sessions::plan_start(held, "remote-pad-1", 0, kTvB, 48055, 48074, kReserved, false);
        CTM_CHECK_EQ(plan.port, static_cast<uint16_t>(48055));   // remote: 48055, as it always was
        held.push_back({"remote-pad-1", plan.port, kTvB});
        plan = tv_sessions::plan_start(held, "remote-pad-2", 0, kTvB, 48055, 48074, kReserved, false);
        CTM_CHECK_EQ(plan.port, static_cast<uint16_t>(48056));
        // They meet in the middle only when the range is full.
        held.clear();
        for (int p = 48056; p <= 48074; ++p) held.push_back({"h" + std::to_string(p), static_cast<uint16_t>(p), kTvA});
        plan = tv_sessions::plan_start(held, "last", 0, kTvB, 48055, 48074, kReserved, true);
        CTM_CHECK_EQ(plan.port, static_cast<uint16_t>(48055));   // home takes the last one left
    }

    section("tv sessions: a TV that chooses its own port");
    {
        // An older TV app still sends a port. Another TV's session on it is a
        // pad in use: refused, never evicted.
        std::vector<Held> held = {{"ctm-ds5-1-aaaaaa", 48055, kTvA}};
        auto plan = tv_sessions::plan_start(held, "ctm-ds5-1", 48055, "", 48055, 48074, kReserved, true);
        CTM_CHECK(plan.what == Start::PortTaken);
        // Its own older session there died without a stop: replaced, as before.
        held = {{"ctm-ds5-1", 48055, ""}};
        plan = tv_sessions::plan_start(held, "ctm-ds5-2", 48055, "", 48055, 48074, kReserved, true);
        CTM_CHECK(plan.what == Start::ReplaceStale);
        CTM_CHECK_EQ(plan.stale, std::string("ctm-ds5-1"));
        // A free port is simply taken.
        plan = tv_sessions::plan_start(held, "ctm-ds5-3", 48060, "", 48055, 48074, kReserved, true);
        CTM_CHECK(plan.what == Start::Create);
        CTM_CHECK_EQ(plan.port, static_cast<uint16_t>(48060));
    }

    section("tv sessions: a TV stops and retires only its own");
    {
        CTM_CHECK(tv_sessions::same_tv(kTvA, kTvA));
        CTM_CHECK(!tv_sessions::same_tv(kTvA, kTvB));
        CTM_CHECK(!tv_sessions::same_tv(kTvA, ""));    // no proof is not TV A
        CTM_CHECK(tv_sessions::same_tv("", ""));       // two older apps, as before
    }

    section("tv sessions: a TV's own stop sent without its proof");
    {
        const uint32_t kHere = 0xC0A80114;    // 192.168.1.20: where the bridge was started from
        const uint32_t kElsewhere = 0xCB007107;   // 203.0.113.7, a documentation address
        // ⛔ The fault: its challenge went unanswered, so its stop went plainly.
        CTM_CHECK(tv_sessions::may_stop(kTvA, kHere, "", kHere));
        CTM_CHECK(!tv_sessions::may_stop(kTvA, kHere, "", kElsewhere));   // not from anywhere else
        CTM_CHECK(!tv_sessions::may_stop(kTvA, kHere, kTvB, kHere));      // nor by another TV's proof
        CTM_CHECK(!tv_sessions::may_stop(kTvA, 0, "", 0));                // nor with no address kept
        CTM_CHECK(tv_sessions::may_stop(kTvA, kHere, kTvA, kElsewhere));  // its proof, from anywhere
        CTM_CHECK(tv_sessions::may_stop("", kHere, "", kElsewhere));      // two older apps, as before
    }

    section("tv sessions: where agent.inl asks");
    {
        const std::string agent = read_agent_text();
        CTM_CHECK(!agent.empty());
        CTM_CHECK(agent.find("tv_sessions::may_stop(session->owner, session->onlyPeer, owner, peer)") !=
                  std::string::npos);
        CTM_CHECK(agent.find("stop_bridge_session_for(busId, owner, peerAddr, &notYours)") != std::string::npos);
        // A session whose TV never connected goes, and frees its port.
        const size_t failed = agent.find("if (!backendPtr->start(");
        const size_t reap = agent.find("request_bridge_session_reap(session->busId);", failed);
        CTM_CHECK(failed != std::string::npos && reap != std::string::npos && reap - failed < 1500);
        // The control port actually listened on is never handed out.
        CTM_CHECK(agent.find("const std::vector<uint16_t> reserved = {g_agent_control_port,") !=
                  std::string::npos);
        CTM_CHECK(agent.find("g_agent_control_port = port;") != std::string::npos);
        // A nickname is adopted only from the same TV's pads.
        const size_t adopt = agent.find("ONE DEVICE, ONE NICKNAME");
        const size_t sameTv = agent.find("if (!tv_sessions::same_tv(other->owner, session->owner)) continue;", adopt);
        const size_t match = agent.find("same_device::matches(", adopt);
        CTM_CHECK(adopt != std::string::npos && sameTv != std::string::npos && match != std::string::npos &&
                  sameTv < match);
    }
    return 0;
}
