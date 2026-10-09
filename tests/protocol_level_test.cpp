// Whether this listener and a TV app can work together: the protocol levels.
//
// ⭐ WHAT THESE PROTECT (rhoquinn8217, 2026-10-08, "1a and 2a"). Each side has
// a level that goes up only when the way the two talk changes. A TV from
// before the levels says none and must still bridge; a TV below kMinTv is
// turned away in the words the TV looks for, so its panel can say "Update the
// TV app". These pin app/protocol_level.inl, and read agent.inl for where it
// answers with them.
//
// WHAT THESE CANNOT DO. They open no socket: that the TV reads the answers is
// the bridge core's own test, and the whole is checked by bridging from a TV.

#include "harness.h"

#include <fstream>
#include <sstream>
#include <string>

#include "app/protocol_level.inl"

using namespace ctmtest;

namespace {

std::string read_agent_source()
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

// The text from `head` to the first `end` after it, or "".
std::string between(const std::string &text, const std::string &head, const std::string &end)
{
    const size_t a = text.find(head);
    if (a == std::string::npos) return "";
    const size_t b = text.find(end, a + head.size());
    if (b == std::string::npos) return "";
    return text.substr(a, b - a);
}

} // namespace

int run_protocol_level_tests()
{
    using protocol_level::kLevel;
    using protocol_level::kMinTv;

    section("protocol level: what STATUS says");
    {
        const std::string reply = protocol_level::status_reply();
        CTM_CHECK(reply.rfind("OK CTM_AGENT_V1 ", 0) == 0);   // the TV's probe wants the OK first
        CTM_CHECK_EQ(protocol_level::word_value(reply, "level=", 0), kLevel);
        CTM_CHECK_EQ(protocol_level::word_value(reply, "min_tv=", 0), kMinTv);
        CTM_CHECK(!reply.empty() && reply.back() == '\n');
    }

    section("protocol level: a TV's level, from its BRIDGE_START");
    {
        CTM_CHECK_EQ(protocol_level::tv_level("BRIDGE_START ds5 0 ctm-ds5-1 level=3"), 3);
        CTM_CHECK_EQ(protocol_level::tv_level("BRIDGE_START ds5 0 ctm-ds5-1 level=2 PROOF=n:f:s"), 2);
        CTM_CHECK_EQ(protocol_level::tv_level("BRIDGE_START ds5 48055 ctm-ds5-1"), 1);   // a TV from before
        CTM_CHECK_EQ(protocol_level::tv_level("BRIDGE_START ds5 0 ctm-ds5-1 level= PROOF=n:f:s"), 1);
        // ⓘ Inside another word, as a signature could hold it: not the TV's level.
        CTM_CHECK_EQ(protocol_level::tv_level("BRIDGE_START ds5 0 ctm-ds5-1 PROOF=n:f:alevel=9"), 1);
    }

    section("protocol level: who is turned away");
    {
        CTM_CHECK(protocol_level::refusal_for("BRIDGE_START ds5 48055 ctm-ds5-1").empty());
        CTM_CHECK(protocol_level::refusal_for("BRIDGE_START ds5 0 ctm-ds5-1 level=" +
                                              std::to_string(kMinTv)).empty());
        const std::string refused = protocol_level::refusal_for(
            "BRIDGE_START ds5 0 ctm-ds5-1 level=" + std::to_string(kMinTv - 1));
        CTM_CHECK(refused.rfind("ERR update the TV app", 0) == 0);   // the words the TV reads
        CTM_CHECK(!refused.empty() && refused.back() == '\n');
    }

    section("protocol level: where agent.inl answers with it");
    {
        const std::string agent = read_agent_source();
        CTM_CHECK(!agent.empty());
        const std::string status = between(agent, "if (command == \"STATUS\") {", "return;");
        CTM_CHECK(status.find("send_text(client, protocol_level::status_reply());") != std::string::npos);
        const std::string start = between(agent, "if (command == \"BRIDGE_START\") {", "start_bridge_session_for(");
        CTM_CHECK(start.find("protocol_level::refusal_for(") != std::string::npos);
    }
    return 0;
}
