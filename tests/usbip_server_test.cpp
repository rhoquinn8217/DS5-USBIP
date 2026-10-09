// The USB/IP server: what Windows' USB/IP client is told, request by request,
// over a real loopback socket.
//
// ⭐ WHAT THESE PROTECT. Every bridged pad, the virtual mouse and the virtual
// keyboard reach Windows through this server, and its faults were of the kind
// no other check sees: an answer that puts the stream out of step, a request
// for one endpoint holding up another, an unlinked request answered anyway,
// and a stopped device freed under a connection still serving it (code
// review, 2026-10-05). So these play Windows' side of the protocol byte for
// byte against a device of their own, and read back what the server sends.
//
// WHAT THESE CANNOT DO. The device here is a stand-in: it answers what each
// check needs and nothing a real pad does. That Windows enumerates a pad and
// its reports arrive is checked with a pad. ISO audio is not driven here.
//
// ⓘ HOW IT IS REACHED. server.inl is a template over the device it serves,
// so it is included here, with the wire format and the types it reads, in a
// namespace of this file's own, and served a FakeDevice. The program's own
// CtmUsbipDevice is the only other device it is ever given.

// ⓘ Winsock 2 before anything brings in windows.h, as main.cpp has it.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")

#include "harness.h"
#include "units.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <climits>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <deque>
#include <functional>
#include <iomanip>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "ctm/profile.h"

using namespace ctmtest;

namespace usbiptest {

// ⓘ What the server takes from common.inl, as the handshake test takes it:
// common.inl itself needs the USB profiles, which nothing here uses.
std::atomic_bool g_stop(false);

static bool ctm_verbose_logs() { return false; }

static std::wstring wsa_error_message(const wchar_t *prefix)
{
    return std::wstring(prefix) + L": winsock error " + std::to_wstring(WSAGetLastError());
}

static std::string hex_span(const uint8_t *, size_t) { return std::string(); }

namespace device_log = units::device_log;

#include "usbip/wire.inl"
#include "usb/descriptors.inl"
#include "usbip/submit_info.inl"
#include "audio/iso_in_pacing.inl"
// ⓘ Named only, for the program's alias at the end of server.inl; nothing here
// makes a server of it.
class CtmUsbipDevice;
#include "usbip/server.inl"

// ---- The device served ------------------------------------------------------
// Endpoint 0x84 answers a report when the check puts one in; 0x85 has none and
// waits until the session ends; 0x86 is "busy": it waits for the check to let
// it go, ignoring the session, as a device call can be inside a pad's backend.
class FakeDevice {
public:
    FakeDevice()
    {
        info_.vid = 0x054c;
        info_.pid = 0x0ce6;
        info_.bcdDevice = 0x0100;
        info_.numInterfaces = 1;
        InterfaceInfo hid;
        hid.number = 0;
        hid.cls = 0x03;
        info_.interfaces.push_back(hid);
        for (uint8_t ep : {static_cast<uint8_t>(0x84), static_cast<uint8_t>(0x85), static_cast<uint8_t>(0x86),
                           static_cast<uint8_t>(0x03)}) {
            EndpointInfo e;
            e.address = ep;
            e.attributes = 0x03;          // interrupt
            e.maxPacket = 64;
            e.interval = 4;
            info_.endpoints.push_back(e);
        }
    }

    const UsbDeviceInfo &info() const { return info_; }
    bool is_iso_endpoint(uint8_t) const { return false; }
    bool is_interrupt_endpoint(uint8_t address) const
    {
        return address == 0x84 || address == 0x85 || address == 0x86;
    }
    uint32_t iso_ack_min_fill_ms() { return 0; }
    uint32_t audio_reservoir_fill_ms() { return 0; }
    uint32_t iso_out_audio_duration_us(uint8_t, uint32_t) const { return 0; }
    uint32_t iso_out_audio_frames(uint8_t, uint32_t) const { return 0; }
    uint32_t iso_out_expected_rate_hz(uint8_t) const { return 0; }
    CtmAudioStats audio_stats() const { return CtmAudioStats(); }

    // Control and OUT requests. A control IN is answered with the 18 bytes of
    // a device descriptor; an OUT's bytes are kept for the check to read.
    int handle_submit(uint32_t direction, uint32_t ep, const uint8_t setup[8],
                      const std::vector<uint8_t> &outData, uint32_t, std::vector<uint8_t> *inData,
                      CtmSubmitInfo *)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (direction == kUsbipDirIn && (ep & 0x0f) == 0) {
            lastSetup_.assign(setup, setup + 8);
            inData->assign(kDescriptor, kDescriptor + sizeof(kDescriptor));
            return kStatusOk;
        }
        if (direction == kUsbipDirOut) {
            outs_.push_back(outData);
            return kStatusOk;
        }
        return kStatusStall;
    }

    int handle_interrupt_in(uint8_t address, uint32_t, std::vector<uint8_t> *inData, CtmSubmitInfo *,
                            const std::atomic_bool *sessionActive)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (address == 0x86) {
            ++busyCalls_;
            cv_.notify_all();
            cv_.wait(lock, [&]() { return busyReleased_; });
            return kStatusConnReset;
        }
        cv_.wait(lock, [&]() {
            return !sessionActive->load() || (address == 0x84 && !reports_.empty());
        });
        if (address != 0x84 || reports_.empty()) return kStatusConnReset;
        *inData = reports_.front();
        reports_.pop_front();
        return kStatusOk;
    }

    void wake_input_waiters() { cv_.notify_all(); }

    void put_report(std::vector<uint8_t> report)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            reports_.push_back(std::move(report));
        }
        cv_.notify_all();
    }

    bool wait_busy_call(int calls, std::chrono::milliseconds limit)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, limit, [&]() { return busyCalls_ >= calls; });
    }

    void release_busy()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            busyReleased_ = true;
        }
        cv_.notify_all();
    }

    std::vector<std::vector<uint8_t>> outs()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return outs_;
    }

    std::vector<uint8_t> last_setup()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return lastSetup_;
    }

    static constexpr uint8_t kDescriptor[18] = {
        0x12, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40,
        0x4c, 0x05, 0xe6, 0x0c, 0x00, 0x01, 0x01, 0x02, 0x00, 0x01,
    };

private:
    UsbDeviceInfo info_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<std::vector<uint8_t>> reports_;
    std::vector<std::vector<uint8_t>> outs_;
    std::vector<uint8_t> lastSetup_;
    int busyCalls_ = 0;
    bool busyReleased_ = false;
};

using Server = UsbipServerFor<FakeDevice>;

// ---- Windows' side of the protocol ------------------------------------------

// ⓘ A port the system says is free, as the handshake test picks one: ranges a
// guess lands in can be reserved by Hyper-V or Docker (winsock 10013).
uint16_t free_port()
{
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in a = {};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = 0;
    bind(s, reinterpret_cast<sockaddr *>(&a), sizeof(a));
    int len = sizeof(a);
    getsockname(s, reinterpret_cast<sockaddr *>(&a), &len);
    const uint16_t port = ntohs(a.sin_port);
    closesocket(s);
    return port;
}

class Client {
public:
    explicit Client(uint16_t port)
    {
        s_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        sockaddr_in a = {};
        a.sin_family = AF_INET;
        a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        a.sin_port = htons(port);
        connected_ = connect(s_, reinterpret_cast<sockaddr *>(&a), sizeof(a)) == 0;
        DWORD timeoutMs = 3000;
        setsockopt(s_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&timeoutMs), sizeof(timeoutMs));
    }
    ~Client() { close(); }
    Client(const Client &) = delete;
    Client &operator=(const Client &) = delete;

    bool connected() const { return connected_; }
    void close()
    {
        if (s_ != INVALID_SOCKET) {
            closesocket(s_);
            s_ = INVALID_SOCKET;
        }
    }

    bool send_bytes(const std::vector<uint8_t> &bytes) { return send_all(s_, bytes.data(), bytes.size()); }
    bool recv_bytes(std::vector<uint8_t> *out, size_t n)
    {
        out->assign(n, 0);
        return recv_all(s_, out->data(), n);
    }

    // True when the server has closed this connection: a read ends with
    // nothing, rather than waiting out the timeout.
    bool closed_by_server()
    {
        uint8_t b = 0;
        const int rc = recv(s_, reinterpret_cast<char *>(&b), 1, 0);
        return rc == 0 || (rc < 0 && WSAGetLastError() != WSAETIMEDOUT);
    }

    // Nothing arrives within `ms`.
    bool quiet_for(int ms)
    {
        DWORD shortMs = static_cast<DWORD>(ms);
        setsockopt(s_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&shortMs), sizeof(shortMs));
        uint8_t b = 0;
        const int rc = recv(s_, reinterpret_cast<char *>(&b), 1, 0);
        const bool quiet = rc < 0 && WSAGetLastError() == WSAETIMEDOUT;
        DWORD timeoutMs = 3000;
        setsockopt(s_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&timeoutMs), sizeof(timeoutMs));
        return quiet;
    }

private:
    SOCKET s_ = INVALID_SOCKET;
    bool connected_ = false;
};

std::vector<uint8_t> op_request(uint16_t code)
{
    std::vector<uint8_t> out;
    append_be16(&out, kUsbipVersion);
    append_be16(&out, code);
    append_be32(&out, 0);
    return out;
}

std::vector<uint8_t> import_request(const char *busId)
{
    std::vector<uint8_t> out = op_request(kOpReqImport);
    append_fixed_string(&out, busId, 32);
    return out;
}

std::vector<uint8_t> submit(uint32_t seq, uint32_t direction, uint32_t ep, uint32_t length,
                            const std::vector<uint8_t> &setup = std::vector<uint8_t>(8, 0))
{
    std::vector<uint8_t> out;
    append_be32(&out, kCmdSubmit);
    append_be32(&out, seq);
    append_be32(&out, 0x00010002);           // devid
    append_be32(&out, direction);
    append_be32(&out, ep);
    append_be32(&out, 0);                    // transfer flags
    append_be32(&out, length);
    append_be32(&out, 0);                    // start frame
    append_be32(&out, kNonIsoPackets);
    append_be32(&out, 0);                    // interval
    out.insert(out.end(), setup.begin(), setup.end());
    return out;
}

std::vector<uint8_t> unlink(uint32_t seq, uint32_t victim)
{
    std::vector<uint8_t> out;
    append_be32(&out, kCmdUnlink);
    append_be32(&out, seq);
    append_be32(&out, 0x00010002);
    append_be32(&out, 0);
    append_be32(&out, 0);
    append_be32(&out, victim);
    out.resize(48, 0);
    return out;
}

struct Reply {
    uint32_t command = 0;
    uint32_t seq = 0;
    int32_t status = 0;
    uint32_t actual = 0;
    std::vector<uint8_t> payload;
    bool ok = false;
};

// One RET_SUBMIT or RET_UNLINK; a submit's payload is read when it is an IN
// reply, which the caller says, since the header does not.
Reply read_reply(Client &c, bool inPayload)
{
    Reply r;
    std::vector<uint8_t> h;
    if (!c.recv_bytes(&h, 48)) return r;
    r.command = read_be32(h.data());
    r.seq = read_be32(h.data() + 4);
    r.status = static_cast<int32_t>(read_be32(h.data() + 20));
    r.actual = read_be32(h.data() + 24);
    if (r.command == kRetSubmit && inPayload && r.actual != 0) {
        if (!c.recv_bytes(&r.payload, r.actual)) return r;
    }
    r.ok = true;
    return r;
}

// A connection that has imported the device, ready for submits.
bool import(Client &c, const char *busId)
{
    if (!c.send_bytes(import_request(busId))) return false;
    std::vector<uint8_t> head;
    if (!c.recv_bytes(&head, 8)) return false;
    if (read_be16(head.data() + 2) != kOpRepImport || read_be32(head.data() + 4) != 0) return false;
    std::vector<uint8_t> record;
    return c.recv_bytes(&record, 312);
}

}  // namespace usbiptest

using namespace usbiptest;

int run_usbip_server_tests()
{
    WSADATA wsa = {};
    WSAStartup(MAKEWORD(2, 2), &wsa);

    section("usbip server: the device list names the device, its ids and its interfaces");
    {
        FakeDevice device;
        Server server;
        std::wstring error;
        CTM_CHECK(server.add_device(&device, "ctm-test-1", &error));
        const uint16_t port = free_port();
        CTM_CHECK(server.start(port, &error));
        Client c(port);
        CTM_CHECK(c.connected());
        CTM_CHECK(c.send_bytes(op_request(kOpReqDevlist)));
        std::vector<uint8_t> head;
        CTM_CHECK(c.recv_bytes(&head, 12));
        CTM_CHECK_EQ(static_cast<int>(read_be16(head.data())), static_cast<int>(kUsbipVersion));
        CTM_CHECK_EQ(static_cast<int>(read_be16(head.data() + 2)), static_cast<int>(kOpRepDevlist));
        CTM_CHECK_EQ(read_be32(head.data() + 8), 1u);
        std::vector<uint8_t> rec;
        CTM_CHECK(c.recv_bytes(&rec, 312 + 4));
        CTM_CHECK(std::string(reinterpret_cast<const char *>(rec.data() + 256)) == "ctm-test-1");
        CTM_CHECK_EQ(read_be32(rec.data() + 296), kUsbSpeedHigh);
        CTM_CHECK_EQ(static_cast<int>(read_be16(rec.data() + 300)), 0x054c);
        CTM_CHECK_EQ(static_cast<int>(read_be16(rec.data() + 302)), 0x0ce6);
        CTM_CHECK_EQ(static_cast<int>(rec[311]), 1);           // interfaces
        CTM_CHECK_EQ(static_cast<int>(rec[312]), 0x03);        // HID
        CTM_CHECK(c.closed_by_server());
        server.stop();
    }

    section("usbip server: importing a bus id it does not have is refused, and the connection closed");
    {
        FakeDevice device;
        Server server;
        std::wstring error;
        server.add_device(&device, "ctm-test-1", &error);
        const uint16_t port = free_port();
        server.start(port, &error);
        Client c(port);
        CTM_CHECK(c.send_bytes(import_request("ctm-nobody")));
        std::vector<uint8_t> head;
        CTM_CHECK(c.recv_bytes(&head, 8));
        CTM_CHECK_EQ(read_be32(head.data() + 4), 1u);
        CTM_CHECK(c.closed_by_server());
        server.stop();
    }

    section("usbip server: a control request is answered with the device's bytes, cut to what was asked");
    {
        FakeDevice device;
        Server server;
        std::wstring error;
        server.add_device(&device, "ctm-test-1", &error);
        const uint16_t port = free_port();
        server.start(port, &error);
        Client c(port);
        CTM_CHECK(import(c, "ctm-test-1"));
        const std::vector<uint8_t> getDevice = {0x80, 0x06, 0x00, 0x01, 0x00, 0x00, 0x40, 0x00};
        CTM_CHECK(c.send_bytes(submit(7, kUsbipDirIn, 0, 64, getDevice)));
        const Reply full = read_reply(c, true);
        CTM_CHECK(full.ok && full.command == kRetSubmit && full.seq == 7 && full.status == 0);
        CTM_CHECK_EQ(full.actual, 18u);
        CTM_CHECK(full.payload == std::vector<uint8_t>(FakeDevice::kDescriptor, FakeDevice::kDescriptor + 18));
        CTM_CHECK(device.last_setup() == getDevice);
        // ⓘ Windows asks for the first 8 bytes before the rest.
        CTM_CHECK(c.send_bytes(submit(8, kUsbipDirIn, 0, 8, getDevice)));
        const Reply first8 = read_reply(c, true);
        CTM_CHECK(first8.ok && first8.seq == 8);
        CTM_CHECK_EQ(first8.actual, 8u);
        CTM_CHECK_EQ(first8.payload.size(), static_cast<size_t>(8));
        c.close();
        server.stop();
    }

    section("usbip server: an OUT request's bytes reach the device, and its reply carries none of them");
    {
        // ⓘ An echoed OUT payload puts every later reply out of step, so the
        // request after it is the real check.
        FakeDevice device;
        Server server;
        std::wstring error;
        server.add_device(&device, "ctm-test-1", &error);
        const uint16_t port = free_port();
        server.start(port, &error);
        Client c(port);
        CTM_CHECK(import(c, "ctm-test-1"));
        std::vector<uint8_t> out = submit(20, kUsbipDirOut, 3, 5);
        const std::vector<uint8_t> body = {0x02, 0xff, 0x01, 0x00, 0x7f};
        out.insert(out.end(), body.begin(), body.end());
        CTM_CHECK(c.send_bytes(out));
        const Reply outReply = read_reply(c, false);
        CTM_CHECK(outReply.ok && outReply.seq == 20 && outReply.status == 0);
        CTM_CHECK_EQ(outReply.actual, 5u);
        CTM_CHECK(c.send_bytes(submit(21, kUsbipDirIn, 0, 64)));
        const Reply next = read_reply(c, true);
        CTM_CHECK(next.ok && next.command == kRetSubmit && next.seq == 21);
        CTM_CHECK_EQ(next.actual, 18u);
        const std::vector<std::vector<uint8_t>> outs = device.outs();
        CTM_CHECK(outs.size() == 1 && outs[0] == body);
        c.close();
        server.stop();
    }

    section("usbip server: a request waiting on an idle endpoint does not hold up another endpoint");
    {
        // ⓘ One shared worker blocked on an unused endpoint and starved the
        // live one: input never reached Windows (server.inl, EndpointInWorker).
        FakeDevice device;
        Server server;
        std::wstring error;
        server.add_device(&device, "ctm-test-1", &error);
        const uint16_t port = free_port();
        server.start(port, &error);
        Client c(port);
        CTM_CHECK(import(c, "ctm-test-1"));
        CTM_CHECK(c.send_bytes(submit(30, kUsbipDirIn, 5, 64)));   // 0x85: never answered
        CTM_CHECK(c.send_bytes(submit(31, kUsbipDirIn, 4, 64)));   // 0x84
        device.put_report({0x01, 0x80, 0x80, 0x80, 0x80});
        const Reply r = read_reply(c, true);
        CTM_CHECK(r.ok && r.seq == 31 && r.status == 0);
        CTM_CHECK(r.payload == std::vector<uint8_t>({0x01, 0x80, 0x80, 0x80, 0x80}));
        c.close();
        server.stop();
    }

    section("usbip server: an unlinked request still queued is cancelled and never answered");
    {
        // ⓘ The unlink answered "already done" every time, and the request
        // stayed queued and was served later, eating an input report (code
        // review, 2026-10-05).
        FakeDevice device;
        Server server;
        std::wstring error;
        server.add_device(&device, "ctm-test-1", &error);
        const uint16_t port = free_port();
        server.start(port, &error);
        Client c(port);
        CTM_CHECK(import(c, "ctm-test-1"));
        CTM_CHECK(c.send_bytes(submit(40, kUsbipDirIn, 4, 64)));
        CTM_CHECK(c.send_bytes(submit(41, kUsbipDirIn, 4, 64)));
        CTM_CHECK(c.send_bytes(unlink(42, 41)));
        const Reply unlinked = read_reply(c, false);
        CTM_CHECK(unlinked.ok && unlinked.command == kRetUnlink && unlinked.seq == 42);
        CTM_CHECK_EQ(unlinked.status, usbip_limits::kStatusUnlinked);
        device.put_report({0xaa});
        const Reply served = read_reply(c, true);
        CTM_CHECK(served.ok && served.seq == 40);
        device.put_report({0xbb});
        CTM_CHECK(c.quiet_for(300));
        // ⓘ One already answered completes as before: its unlink says so.
        CTM_CHECK(c.send_bytes(unlink(43, 40)));
        const Reply late = read_reply(c, false);
        CTM_CHECK(late.ok && late.command == kRetUnlink && late.seq == 43);
        CTM_CHECK_EQ(late.status, kStatusOk);
        c.close();
        server.stop();
    }

    section("usbip server: a request asking for more than the limit closes the import");
    {
        FakeDevice device;
        Server server;
        std::wstring error;
        server.add_device(&device, "ctm-test-1", &error);
        const uint16_t port = free_port();
        server.start(port, &error);
        Client c(port);
        CTM_CHECK(import(c, "ctm-test-1"));
        CTM_CHECK(c.send_bytes(submit(50, kUsbipDirOut, 3, 0xfffffff0u)));
        CTM_CHECK(c.closed_by_server());
        server.stop();
    }

    section("usbip server: removing a device ends the connection importing it, though Windows keeps its side open");
    {
        // ⓘ Its thread is watched through what it holds: once the thread has
        // ended, nothing holds the backend handed in with the device. A
        // shutdown alone left that thread waiting until Windows closed its
        // side, and this client never closes it.
        auto device = std::make_shared<FakeDevice>();
        std::shared_ptr<int> backend = std::make_shared<int>(1);
        const std::weak_ptr<int> watch = backend;
        Server server;
        std::wstring error;
        server.add_device(device, "ctm-test-1", &error, backend);
        backend.reset();
        const uint16_t port = free_port();
        server.start(port, &error);
        Client c(port);
        CTM_CHECK(import(c, "ctm-test-1"));
        // ⓘ One request answered first, so the connection is surely waiting
        // for the next when the device goes: a removal landing before that wait
        // starts ends it whatever the stop does.
        CTM_CHECK(c.send_bytes(submit(55, kUsbipDirIn, 0, 64)));
        CTM_CHECK(read_reply(c, true).ok);
        server.remove_device("ctm-test-1");
        bool gone = false;
        for (int i = 0; i < 100 && !gone; ++i) {
            gone = watch.expired();
            if (!gone) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        CTM_CHECK(gone);
        CTM_CHECK(c.closed_by_server());
        c.close();
        server.stop();
    }

    section("usbip server: stopping ends an idle connection at once, though Windows keeps its side open");
    {
        FakeDevice device;
        Server server;
        std::wstring error;
        server.add_device(&device, "ctm-test-1", &error);
        const uint16_t port = free_port();
        server.start(port, &error);
        // ⛔ NEVER CLOSED, on purpose. Should the stop ever stop cancelling
        // the wait, this check then fails, and the connection's thread waits on
        // this socket for good, instead of waking later into a server that is
        // gone and taking the whole test binary down with it.
        Client &c = *new Client(port);
        CTM_CHECK(import(c, "ctm-test-1"));
        // ⓘ One request answered first, so the connection is surely waiting
        // for the next when the stop comes (as in the removal check above).
        CTM_CHECK(c.send_bytes(submit(65, kUsbipDirIn, 0, 64)));
        CTM_CHECK(read_reply(c, true).ok);
        const auto t0 = std::chrono::steady_clock::now();
        server.stop();
        const auto waited = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t0).count();
        // ⓘ The stop allows a connection two seconds before giving up on it;
        // an idle one has nothing to finish.
        CTM_CHECK(waited < 1000);
    }

    section("usbip server: what a device calls into is kept until the last connection serving it has ended");
    {
        // ⓘ The bridge stops and frees its backend while a connection thread
        // can still be inside a device call that reaches it (code review,
        // 2026-10-05). The backend goes in with the device, and every
        // connection holds it until its thread ends.
        auto device = std::make_shared<FakeDevice>();
        std::shared_ptr<int> backend = std::make_shared<int>(1);
        const std::weak_ptr<int> watch = backend;
        Server server;
        std::wstring error;
        server.add_device(device, "ctm-test-1", &error, backend);
        backend.reset();
        const uint16_t port = free_port();
        server.start(port, &error);
        Client c(port);
        CTM_CHECK(import(c, "ctm-test-1"));
        CTM_CHECK(c.send_bytes(submit(60, kUsbipDirIn, 6, 64)));     // 0x86: busy
        CTM_CHECK(device->wait_busy_call(1, std::chrono::milliseconds(2000)));
        server.remove_device("ctm-test-1");
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        CTM_CHECK(!watch.expired());
        device->release_busy();
        bool gone = false;
        for (int i = 0; i < 200 && !gone; ++i) {
            gone = watch.expired();
            if (!gone) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        CTM_CHECK(gone);
        c.close();
        server.stop();
    }

    section("usbip server: stopping waits for a connection still inside a device call");
    {
        auto device = std::make_shared<FakeDevice>();
        Server server;
        std::wstring error;
        server.add_device(device, "ctm-test-1", &error);
        const uint16_t port = free_port();
        server.start(port, &error);
        // ⛔ NEVER CLOSED, for the reason given two checks above.
        Client &c = *new Client(port);
        CTM_CHECK(import(c, "ctm-test-1"));
        CTM_CHECK(c.send_bytes(submit(70, kUsbipDirIn, 6, 64)));
        CTM_CHECK(device->wait_busy_call(1, std::chrono::milliseconds(2000)));
        std::thread release([device]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            device->release_busy();
        });
        const auto t0 = std::chrono::steady_clock::now();
        server.stop();
        const auto waited = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t0).count();
        release.join();
        // ⓘ It waited for the call to end (about 300 ms), and not the whole
        // two seconds it allows one before giving up on it.
        CTM_CHECK(waited >= 250);
        CTM_CHECK(waited < 1900);
    }

    // ⛔ NO WSACleanup. The two checks above that never close their client
    // leave a socket open on purpose, and the last cleanup closes every socket
    // the process has: a thread still waiting on one would wake into a server
    // long gone. The process ends soon enough.
    return 0;
}
