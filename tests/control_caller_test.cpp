// Who may send which command to the control port.
//
// ⭐ WHAT THESE PROTECT. The control port is forwarded to the internet for
// remote play, and two of its commands are dangerous in a stranger's hands:
// USBIP_ATTACH plugs a USB device from the caller's machine into this PC, and
// RESTART drops every bridge. These pin both to this PC, and pin the three
// commands the TV does send -- STATUS, BRIDGE_START, BRIDGE_STOP -- as open to
// it from anywhere, since a TV at another site arrives from a public address.
//
// WHAT THESE CANNOT DO. They open no socket: that agent.inl asks this before
// answering, and with the caller's real address, is checked by sending the
// commands to a running listener from this PC and from a TV.

#include "harness.h"

#include <cstdint>
#include <string>

#include "app/control_caller.inl"

using namespace ctmtest;

namespace {

uint32_t ip(unsigned a, unsigned b, unsigned c, unsigned d)
{
    return (a << 24) | (b << 16) | (c << 8) | d;
}

} // namespace

int run_control_caller_tests()
{
    section("control caller: the loopback range is this PC, and nothing else is");
    {
        CTM_CHECK(control_caller::is_this_pc(ip(127, 0, 0, 1)));
        CTM_CHECK(control_caller::is_this_pc(ip(127, 1, 2, 3)));
        CTM_CHECK(!control_caller::is_this_pc(ip(192, 168, 1, 129)));
        CTM_CHECK(!control_caller::is_this_pc(ip(10, 0, 0, 1)));
        CTM_CHECK(!control_caller::is_this_pc(ip(8, 8, 8, 8)));
        CTM_CHECK(!control_caller::is_this_pc(ip(128, 0, 0, 1)));
        CTM_CHECK(!control_caller::is_this_pc(ip(126, 255, 255, 255)));
        CTM_CHECK(!control_caller::is_this_pc(0));
    }

    section("control caller: USBIP_ATTACH and RESTART are this PC's alone");
    {
        CTM_CHECK(control_caller::allowed("USBIP_ATTACH", ip(127, 0, 0, 1)));
        CTM_CHECK(control_caller::allowed("RESTART", ip(127, 0, 0, 1)));
        CTM_CHECK(!control_caller::allowed("USBIP_ATTACH", ip(192, 168, 1, 129)));
        CTM_CHECK(!control_caller::allowed("RESTART", ip(192, 168, 1, 129)));
        CTM_CHECK(!control_caller::allowed("USBIP_ATTACH", ip(8, 8, 8, 8)));
        CTM_CHECK(!control_caller::allowed("RESTART", ip(8, 8, 8, 8)));
    }

    section("control caller: what the TV sends is answered from anywhere");
    {
        // ⛔ A TV at another site reaches the port from a public address, so
        // these must not be narrowed to the home network.
        for (const char *cmd : { "STATUS", "BRIDGE_START", "BRIDGE_STOP" }) {
            CTM_CHECK(control_caller::allowed(cmd, ip(127, 0, 0, 1)));
            CTM_CHECK(control_caller::allowed(cmd, ip(192, 168, 1, 129)));
            CTM_CHECK(control_caller::allowed(cmd, ip(203, 0, 113, 7)));
        }
        // An unknown word is not this rule's to refuse: the port answers it
        // with its own error.
        CTM_CHECK(control_caller::allowed("BOGUS", ip(203, 0, 113, 7)));
        // ⓘ Exact words: the port reads the command as one token.
        CTM_CHECK(control_caller::allowed("restart", ip(203, 0, 113, 7)));
    }

    return 0;
}
