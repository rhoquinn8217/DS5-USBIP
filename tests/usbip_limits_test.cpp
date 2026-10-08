// What the USB/IP server takes from the wire, and what an unlink cancels.
//
// ⭐ WHAT THESE PROTECT. A submit's length used to go straight into a resize (a
// near-4 GB one aborted the process), 1024 ISO packets or more left their
// descriptors unread (the stream fell out of step), and an unlink cancelled
// nothing. These pin the limits and the cancel.
//
// WHAT THESE CANNOT DO. They open no socket. That the server closes the import
// on a refused submit, and answers an unlink with these statuses, is read off
// server.inl; that a real pad still works through it is checked with a pad.

#include "harness.h"

#include <cstdint>
#include <deque>

#include "usbip/submit_limits.inl"

using namespace ctmtest;

namespace {
constexpr uint32_t kNonIso = 0xffffffffu;   // common.inl's kNonIsoPackets
struct Pending { uint32_t seqnum; };
}

int run_usbip_limits_tests()
{
    section("usbip: ordinary submits are taken");
    {
        CTM_CHECK(usbip_limits::submit_sizes_ok(64, kNonIso, kNonIso));            // a HID report
        CTM_CHECK(usbip_limits::submit_sizes_ok(0, kNonIso, kNonIso));             // a bare control
        CTM_CHECK(usbip_limits::submit_sizes_ok(1024 * 1024, 1024, kNonIso));      // the largest ISO
        CTM_CHECK(usbip_limits::submit_sizes_ok(usbip_limits::kMaxTransferLength, 0, kNonIso));
    }

    section("usbip: a length past the cap is refused, not allocated");
    {
        CTM_CHECK(!usbip_limits::submit_sizes_ok(usbip_limits::kMaxTransferLength + 1, kNonIso, kNonIso));
        CTM_CHECK(!usbip_limits::submit_sizes_ok(0xfffffff0u, kNonIso, kNonIso));   // the abort
    }

    section("usbip: more than 1024 ISO packets is refused, not left unread");
    {
        CTM_CHECK(usbip_limits::submit_sizes_ok(4096, 1024, kNonIso));   // 1024 is allowed and read
        CTM_CHECK(!usbip_limits::submit_sizes_ok(4096, 1025, kNonIso));
        CTM_CHECK(!usbip_limits::submit_sizes_ok(4096, 0x7fffffffu, kNonIso));
    }

    section("usbip: an unlink removes the queued request it names, and only that one");
    {
        std::deque<Pending> queue = {{10}, {11}, {12}};
        CTM_CHECK(usbip_limits::cancel_queued(queue, 11));
        CTM_CHECK_EQ(queue.size(), static_cast<size_t>(2));
        CTM_CHECK_EQ(queue[0].seqnum, 10u);
        CTM_CHECK_EQ(queue[1].seqnum, 12u);
    }

    section("usbip: an unlink for a request not queued cancels nothing");
    {
        std::deque<Pending> queue = {{10}, {12}};
        CTM_CHECK(!usbip_limits::cancel_queued(queue, 11));   // done, or being served
        CTM_CHECK_EQ(queue.size(), static_cast<size_t>(2));
        std::deque<Pending> empty;
        CTM_CHECK(!usbip_limits::cancel_queued(empty, 11));
    }

    section("usbip: the statuses an unlink is answered with");
    {
        CTM_CHECK_EQ(usbip_limits::kStatusUnlinked, -104);   // -ECONNRESET: cancelled
    }

    return 0;
}
