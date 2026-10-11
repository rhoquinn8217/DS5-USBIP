// Who may send which command to the control port (48054).
//
// ⛔⛔ THE PORT IS REACHABLE FROM THE INTERNET (code review, 2026-10-05). A TV
// at another site bridges through the home's public address, so this port is
// forwarded, and the control port answered every command from anyone. Two of
// them were never the TV's to send:
// - USBIP_ATTACH makes this PC import a USB device from the CALLER's own
//   USB/IP server: anyone who reached the port could plug any USB device they
//   liked into this PC.
// - RESTART drops every bridge, so anyone could pull every pad out of a game.
// ➡️ The TV sends STATUS, BRIDGE_START and BRIDGE_STOP and nothing else (read
// off the bridge core, 2026-10-07), so those two are this PC's alone.
//
// ⓘ What this does NOT close: BRIDGE_START itself, which is how a caller makes
// Windows see a keyboard. That needs to know which callers are this person's
// TVs, a change on both sides.
//
// ⓘ THIS PC MEANS THE LOOPBACK ADDRESS. A local tool connects to 127.0.0.1; a
// caller that arrives on one of this PC's network addresses is treated as
// anyone else, since a forwarded port arrives that way too.

#pragma once

#include <cstdint>
#include <string>

namespace control_caller {

// An IPv4 address in HOST byte order (ntohl of sin_addr): anything in
// 127.0.0.0/8 is this PC.
inline bool is_this_pc(uint32_t addrHostOrder)
{
    return (addrHostOrder >> 24) == 127u;
}

// The commands only this PC may send.
inline bool needs_this_pc(const std::string &command)
{
    return command == "USBIP_ATTACH" || command == "RESTART";
}

// Whether a command from this caller is answered at all.
inline bool allowed(const std::string &command, uint32_t addrHostOrder)
{
    return !needs_this_pc(command) || is_this_pc(addrHostOrder);
}

} // namespace control_caller
