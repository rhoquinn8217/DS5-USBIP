// What the USB/IP server takes from the wire before it allocates or reads, and
// what an unlink cancels.
//
// ⛔⛔ WHY (code review, 2026-10-05). Three faults on the local USB/IP port:
//   * a CMD_SUBMIT's length went straight into a resize, so a length near 4 GB
//     threw bad_alloc on the read thread and nothing caught it: the process
//     aborted;
//   * an ISO submit of 1024 packets or more had its packet descriptors left
//     unread, so their bytes were read as the next header and the stream fell
//     out of step for good;
//   * a CMD_UNLINK was answered "already done" while the request it named was
//     still queued, and that request was served later all the same.
// ⓘ The port is 127.0.0.1 and the peer is Windows' own USB/IP client, so none
// of this is an attack surface. It is the difference between a client bug or a
// cancelled transfer costing one import and costing the listener.
//
// ⓘ Its own file so the test binary can reach it.

namespace usbip_limits {

// Far above anything this device moves in one transfer (an ISO audio submit is
// at most 1024 packets of 1 KB), far below what would hurt to allocate.
constexpr uint32_t kMaxTransferLength = 8u * 1024u * 1024u;
// USB/IP's own limit on ISO packets per submit (USBIP_MAX_ISO_PACKETS in Linux).
constexpr uint32_t kMaxIsoPackets = 1024u;
// RET_UNLINK's status for a request that was found and cancelled (-ECONNRESET).
// 0 means it had already completed, or was being served, and is not cancelled.
constexpr int32_t kStatusUnlinked = -104;

// Whether a CMD_SUBMIT can be honoured as it stands. A false answer is a
// protocol error: the import is closed rather than read further.
inline bool submit_sizes_ok(uint32_t transferLength, uint32_t packets, uint32_t nonIsoPackets)
{
    if (transferLength > kMaxTransferLength) return false;
    if (packets != nonIsoPackets && packets > kMaxIsoPackets) return false;
    return true;
}

// Removes the queued request an unlink names. True when it was there: it is
// cancelled, and it must get no RET_SUBMIT. False when it is not queued, which
// means it completed or is being served right now.
template <class Queue>
bool cancel_queued(Queue &queue, uint32_t seqnum)
{
    for (auto it = queue.begin(); it != queue.end(); ++it) {
        if (it->seqnum == seqnum) {
            queue.erase(it);
            return true;
        }
    }
    return false;
}

}  // namespace usbip_limits
