// The bridge between the TV and the listener, end to end over a real socket:
// the listener's own BridgeBackend on one side, this file playing the TV on
// the other, on loopback.
//
// ⭐ WHAT THESE PROTECT. The handshake had no test (code review, 2026-10-05),
// and both of its faults that the review found were in its ORDER or its
// CONTENT, which only the other end can see:
// - nothing but HOST_CONFIG may reach the TV before HOST_CONFIG has gone: a
//   reconnect once sent game output first, and the TV failed its handshake
//   and dialled again;
// - HOST_CONFIG says "unset" for the speaker, the headset and the mode, never
//   zero, and the latency is unset unless a setting names one.
// And what the session does after it: an input report reaches the input
// callback with its endpoint, the microphone's audio lands in THIS session's
// ring, a feature read goes to the TV and its answer comes back to the caller,
// the TV's "open the settings window" fires its callback, an enumeration sent
// ahead of HELLO is kept, a HELLO too short for the device's caps is refused,
// and a stop ends the link.
//
// WHAT THESE CANNOT DO. The TV here is this file, so they show that the
// listener keeps to the protocol as this file reads it -- not that aurora-tv
// reads it the same way. The structures' sizes and offsets are pinned at both
// ends by static_asserts for that; a real TV is the rest.
//
// ⓘ The backend listens on loopback only (set_listen_address): a test that
// listened on every address would raise Windows' firewall prompt the first
// time each new build of it ran.

// ⓘ As main.cpp does, and for the same reason here: the full Windows headers
// define `interface` as `struct`, and the bridge has a parameter by that name.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>
#pragma comment(lib, "Ws2_32.lib")

#include "harness.h"
#include "units.h"

#include <chrono>
#include <climits>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <iomanip>
#include <thread>

#include "ctm/map/runtime.h"
#include "ctm/profile.h"

using namespace ctmtest;

namespace bridgetest {

// ⓘ What the bridge takes from common.inl, copied as it is there: common.inl
// itself needs ENet and the USB profiles, which nothing here uses.
std::atomic_bool g_stop(false);

static bool ctm_verbose_logs() { return false; }

static std::wstring wsa_error_message(const wchar_t *prefix)
{
    std::wstring message = prefix;
    message += L": winsock error " + std::to_wstring(WSAGetLastError());
    return message;
}

static std::wstring widen_ascii(const char *text, size_t len)
{
    std::wstring out;
    for (size_t i = 0; i < len && text[i] != '\0'; ++i) {
        out.push_back(static_cast<unsigned char>(text[i]));
    }
    return out;
}

static bool recv_all(SOCKET s, uint8_t *data, size_t length)
{
    size_t got = 0;
    while (got < length) {
        const int chunk = static_cast<int>((std::min)(length - got, static_cast<size_t>(INT_MAX)));
        int rc = recv(s, reinterpret_cast<char *>(data + got), chunk, 0);
        if (rc <= 0) {
            return false;
        }
        got += static_cast<size_t>(rc);
    }
    return true;
}

static bool send_all(SOCKET s, const uint8_t *data, size_t length)
{
    size_t sent = 0;
    while (sent < length) {
        const int chunk = static_cast<int>((std::min)(length - sent, static_cast<size_t>(INT_MAX)));
        int rc = send(s, reinterpret_cast<const char *>(data + sent), chunk, 0);
        if (rc <= 0) {
            return false;
        }
        sent += static_cast<size_t>(rc);
    }
    return true;
}

// ⓘ Asked only by backend.inl's profile builders, which nothing here calls.
static std::vector<unsigned char> make_usb_string_descriptor(const std::wstring &)
{
    return std::vector<unsigned char>();
}

namespace device_log = units::device_log;
using units::device_config_int;

#include "backend/backend.inl"
static void mic_ring_push(const CtmBackend *owner, const uint8_t *data, size_t len);
static void mic_ring_reset(const CtmBackend *owner);
#include "backend/bridge.inl"
#include "audio/mic_ring.inl"

} // namespace bridgetest

namespace {

namespace proto = bridgetest::CtmBridgeProtocol;

// The TV's end of the link.
struct FakeTv {
    SOCKET s = INVALID_SOCKET;

    bool connect_to(uint16_t port, int waitMs)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(waitMs);
        while (std::chrono::steady_clock::now() < deadline) {
            s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            sockaddr_in addr = {};
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            addr.sin_port = htons(port);
            if (connect(s, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == 0) {
                DWORD timeoutMs = 3000;
                setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&timeoutMs), sizeof(timeoutMs));
                return true;
            }
            closesocket(s);
            s = INVALID_SOCKET;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        return false;
    }

    bool send_msg(uint16_t type, uint32_t flags, uint32_t requestId, const std::vector<uint8_t> &payload)
    {
        proto::Header h = {};
        h.magic = proto::kMagic;
        h.version = proto::kVersion;
        h.type = type;
        h.flags = flags;
        h.request_id = requestId;
        h.payload_len = static_cast<uint32_t>(payload.size());
        return bridgetest::send_all(s, reinterpret_cast<const uint8_t *>(&h), sizeof(h)) &&
               (payload.empty() || bridgetest::send_all(s, payload.data(), payload.size()));
    }

    bool recv_msg(proto::Header *h, std::vector<uint8_t> *payload)
    {
        if (!bridgetest::recv_all(s, reinterpret_cast<uint8_t *>(h), sizeof(*h))) return false;
        payload->assign(h->payload_len, 0);
        return h->payload_len == 0 || bridgetest::recv_all(s, payload->data(), payload->size());
    }

    // True when the listener has closed its end. ⛔ A receive that merely
    // timed out is NOT a close: the link is still there, saying nothing.
    bool sees_close()
    {
        char c = 0;
        const int rc = recv(s, &c, 1, 0);
        return rc == 0 || (rc < 0 && WSAGetLastError() != WSAETIMEDOUT);
    }

    void close()
    {
        if (s != INVALID_SOCKET) closesocket(s);
        s = INVALID_SOCKET;
    }
};

// A DualSense as the TV describes it in HELLO.
std::vector<uint8_t> hello_payload(uint16_t inputLen = 64, uint16_t outputLen = 48)
{
    proto::DeviceCaps caps = {};
    caps.vendor_id = 0x054c;
    caps.product_id = 0x0ce6;
    caps.version = 0x0100;
    caps.bus = 3;
    caps.input_report_len = inputLen;
    caps.output_report_len = outputLen;
    caps.feature_report_len = 64;
    strcpy_s(caps.path, "/dev/hidraw7");
    strcpy_s(caps.serial, "a0:b1:c2:d3:e4:f5");
    strcpy_s(caps.product, "DualSense Wireless Controller");
    std::vector<uint8_t> out(sizeof(caps));
    memcpy(out.data(), &caps, sizeof(caps));
    return out;
}

// One backend, started on a free loopback port in its own thread: start()
// returns only once a TV has connected and the handshake is done.
struct Started {
    std::unique_ptr<bridgetest::BridgeBackend> backend;
    std::thread thread;
    std::atomic<int> result{-1};          // -1 running, 0 failed, 1 started
    std::wstring error;
    uint16_t port = 0;
    std::mutex inputMutex;
    std::condition_variable inputCv;
    std::vector<std::pair<std::vector<uint8_t>, uint8_t>> inputs;

    // ⓘ `configure` runs before the backend starts, so nothing set there
    // races its threads (an independent review, 2026-10-07).
    void launch(uint16_t candidate,
                const std::function<void(bridgetest::BridgeBackend &)> &configure = {})
    {
        port = candidate;
        // ⛔ Each try starts clean: a stale result read as this try's would
        // check the last failure's error against this one's.
        result.store(-1);
        error.clear();
        backend.reset(new bridgetest::BridgeBackend(port, 10.0));
        backend->set_listen_address(INADDR_LOOPBACK);
        backend->set_session_timeouts(5000, 500);
        if (configure) configure(*backend);
        thread = std::thread([this]() {
            const bool ok = backend->start([this](const uint8_t *data, size_t len, uint8_t endpoint) {
                std::lock_guard<std::mutex> lock(inputMutex);
                inputs.emplace_back(std::vector<uint8_t>(data, data + len), endpoint);
                inputCv.notify_all();
            }, &error);
            result.store(ok ? 1 : 0);
        });
    }

    bool wait_started(int waitMs)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(waitMs);
        while (result.load() < 0 && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return result.load() == 1;
    }

    void finish()
    {
        if (backend) backend->stop();
        if (thread.joinable()) thread.join();
    }
};

// A loopback port nothing holds, as the system hands one out: bind to port 0
// and read back what it chose.
// ⛔ NOT ONE PICKED FROM A RANGE. Ports picked from 50000 up failed to bind
// in two runs of five with winsock error 10013 (2026-10-07): Hyper-V and
// Docker reserve blocks of ports up there, and binding inside one is refused
// outright. The system never hands out a reserved one. ⓘ The port is free again once this
// returns; the tries in each section cover the rare case of losing it.
uint16_t pick_port(int)
{
    SOCKET probe = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    uint16_t port = 0;
    if (bind(probe, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == 0) {
        sockaddr_in bound = {};
        int len = sizeof(bound);
        if (getsockname(probe, reinterpret_cast<sockaddr *>(&bound), &len) == 0) {
            port = ntohs(bound.sin_port);
        }
    }
    closesocket(probe);
    return port;
}

} // namespace

int run_bridge_handshake_tests()
{
    WSADATA wsa = {};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        CTM_CHECK(false);   // no sockets at all: nothing below can run
        return 0;
    }
    // ⓘ The shared settings file empty, so the latency is the "unset" one.
    {
        std::ofstream out("ctm-device-config.txt", std::ios::binary | std::ios::trunc);
    }
    units::device_config_invalidate();

    section("bridge handshake: HOST_CONFIG is the first thing the TV hears, with the audio unset");
    {
        Started b;
        FakeTv tv;
        bool connected = false;
        std::atomic<int> opened{0};
        for (int attempt = 0; attempt < 5 && !connected; ++attempt) {
            b.finish();
            // ⓘ Held to the loopback caller it really is, which must change
            // nothing; and the open-config callback set before it starts.
            b.launch(pick_port(attempt), [&opened](bridgetest::BridgeBackend &backend) {
                backend.set_only_peer(INADDR_LOOPBACK);
                backend.set_open_config_callback([&opened]() { opened.fetch_add(1); });
            });
            // ⛔ Output from the PC before any TV is there is dropped, not
            // queued for the handshake to trip over.
            std::wstring err;
            CTM_CHECK(b.backend->send_output_report({0x31, 0x01, 0x02}, false, &err));
            connected = tv.connect_to(b.port, 1500);
        }
        CTM_CHECK(connected);
        CTM_CHECK(tv.send_msg(proto::MsgHello, 0, 0, hello_payload()));
        proto::Header h = {};
        std::vector<uint8_t> payload;
        CTM_CHECK(tv.recv_msg(&h, &payload));
        CTM_CHECK_EQ(h.magic, proto::kMagic);
        CTM_CHECK_EQ(static_cast<int>(h.type), static_cast<int>(proto::MsgHostConfig));
        CTM_CHECK_EQ(h.flags & proto::kFlagOk, proto::kFlagOk);
        CTM_CHECK_EQ(payload.size(), sizeof(proto::HostConfig));
        proto::HostConfig hc = {};
        if (payload.size() == sizeof(hc)) memcpy(&hc, payload.data(), sizeof(hc));
        CTM_CHECK_EQ(static_cast<int>(hc.speaker_volume_pct), static_cast<int>(proto::kAudioUnset));
        CTM_CHECK_EQ(static_cast<int>(hc.headset_volume_pct), static_cast<int>(proto::kAudioUnset));
        CTM_CHECK_EQ(static_cast<int>(hc.audio_mode), static_cast<int>(proto::kAudioUnset));
        CTM_CHECK_EQ(static_cast<int>(hc.latency_ms), static_cast<int>(proto::kLatencyUnset));
        CTM_CHECK_EQ(static_cast<int>(hc.input_report_len), 64);
        CTM_CHECK_EQ(static_cast<int>(hc.output_report_len), 48);
        CTM_CHECK_EQ(static_cast<int>(hc.feature_report_len), 64);
        CTM_CHECK_EQ(hc.bt_pace_us, static_cast<uint32_t>(10000));
        CTM_CHECK(b.wait_started(2000));

        section("bridge handshake: the device's caps are what HELLO said");
        {
            const bridgetest::BackendCaps caps = b.backend->caps();
            CTM_CHECK_EQ(static_cast<int>(caps.vendorId), 0x054c);
            CTM_CHECK_EQ(static_cast<int>(caps.productId), 0x0ce6);
            CTM_CHECK(caps.serial == L"a0:b1:c2:d3:e4:f5");
            CTM_CHECK(caps.path == L"/dev/hidraw7");
            CTM_CHECK(caps.product == L"DualSense Wireless Controller");
            CTM_CHECK_EQ(static_cast<int>(caps.bus), 3);
        }

        section("bridge handshake: once it is done, the PC's output reaches the TV");
        {
            std::wstring err;
            CTM_CHECK(b.backend->send_output_report({0x31, 0xAA, 0xBB}, false, &err));
            proto::Header oh = {};
            std::vector<uint8_t> out;
            CTM_CHECK(tv.recv_msg(&oh, &out));
            CTM_CHECK_EQ(static_cast<int>(oh.type), static_cast<int>(proto::MsgOutputReport));
            CTM_CHECK(out == std::vector<uint8_t>({0x31, 0xAA, 0xBB}));
            CTM_CHECK_EQ(oh.flags & proto::kFlagPaced, static_cast<uint32_t>(0));
        }

        section("bridge handshake: an input report reaches the input callback with its endpoint");
        {
            CTM_CHECK(tv.send_msg(proto::MsgInputReport, 0, 0x84, {0x01, 0x80, 0x7F, 0x55}));
            std::unique_lock<std::mutex> lock(b.inputMutex);
            const bool got = b.inputCv.wait_for(lock, std::chrono::seconds(2), [&]() { return !b.inputs.empty(); });
            CTM_CHECK(got);
            if (got) {
                CTM_CHECK(b.inputs[0].first == std::vector<uint8_t>({0x01, 0x80, 0x7F, 0x55}));
                CTM_CHECK_EQ(static_cast<int>(b.inputs[0].second), 0x84);
            }
        }

        section("bridge handshake: the microphone's audio lands in this session's ring");
        {
            CTM_CHECK(tv.send_msg(proto::MsgMicAudio, 0, 0, {9, 8, 7, 6}));
            std::vector<uint8_t> mic;
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            while (mic.empty() && std::chrono::steady_clock::now() < deadline) {
                bridgetest::mic_ring_pop_fill(b.backend.get(), &mic, 64);
                if (mic.empty()) std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            CTM_CHECK(mic == std::vector<uint8_t>({9, 8, 7, 6}));
        }

        section("bridge handshake: a feature read goes to the TV and its answer comes back");
        {
            std::vector<uint8_t> reply;
            std::atomic<int> ok{-1};
            std::thread caller([&]() {
                ok.store(b.backend->remote_interface_feature(1, true, {0x05}, &reply, 2000) ? 1 : 0);
            });
            proto::Header fh = {};
            std::vector<uint8_t> ask;
            const bool asked = tv.recv_msg(&fh, &ask);
            CTM_CHECK(asked);
            CTM_CHECK_EQ(static_cast<int>(fh.type), static_cast<int>(proto::MsgFeatureGet));
            CTM_CHECK_EQ(static_cast<int>(fh.request_id >> 24), 1);    // the interface, in the high byte
            CTM_CHECK(ask == std::vector<uint8_t>({0x05}));
            if (asked) {
                CTM_CHECK(tv.send_msg(proto::MsgFeatureReport, proto::kFlagOk, fh.request_id, {0x05, 0x10, 0x20}));
            }
            caller.join();
            CTM_CHECK_EQ(ok.load(), 1);
            CTM_CHECK(reply == std::vector<uint8_t>({0x05, 0x10, 0x20}));
        }

        section("bridge handshake: the TV's open-the-settings-window message fires its callback");
        {
            CTM_CHECK(tv.send_msg(proto::MsgOpenConfig, 0, 0, {}));
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            while (opened.load() == 0 && std::chrono::steady_clock::now() < deadline) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            CTM_CHECK_EQ(opened.load(), 1);
        }

        section("bridge handshake: a stop ends the link");
        {
            b.backend->stop();
            CTM_CHECK(tv.sees_close());
        }
        bridgetest::mic_ring_reset(b.backend.get());
        b.finish();
        tv.close();
    }

    section("bridge handshake: on a reconnect, nothing reaches the TV before HOST_CONFIG");
    {
        // ⭐ The case the guard in send_message() exists for (code review,
        // 2026-10-05). On a RECONNECT the session is live: other threads keep
        // asking it to send, and one message reaching the TV ahead of
        // HOST_CONFIG fails the TV's handshake. A thread asks for a feature
        // read every millisecond through the whole reconnect -- one of the
        // sends that does not wait for the link to look ready first.
        Started b;
        FakeTv tv;
        bool connected = false;
        for (int attempt = 30; attempt < 35 && !connected; ++attempt) {
            b.finish();
            b.launch(pick_port(attempt), [](bridgetest::BridgeBackend &backend) {
                backend.set_session_timeouts(5000, 3000);
            });
            connected = tv.connect_to(b.port, 1500);
        }
        CTM_CHECK(connected);
        CTM_CHECK(tv.send_msg(proto::MsgHello, 0, 0, hello_payload()));
        proto::Header h = {};
        std::vector<uint8_t> payload;
        CTM_CHECK(tv.recv_msg(&h, &payload));
        CTM_CHECK(b.wait_started(2000));

        std::atomic<bool> asking{true};
        std::thread asker([&]() {
            while (asking.load()) {
                std::vector<uint8_t> reply;
                (void)b.backend->remote_interface_feature(0, true, {0x05}, &reply, 1);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
        tv.close();                                   // the TV drops off...
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        FakeTv again;
        CTM_CHECK(again.connect_to(b.port, 1500));    // ...and comes back
        std::this_thread::sleep_for(std::chrono::milliseconds(200));   // HELLO takes a while
        CTM_CHECK(again.send_msg(proto::MsgHello, 0, 0, hello_payload()));
        proto::Header first = {};
        std::vector<uint8_t> firstPayload;
        CTM_CHECK(again.recv_msg(&first, &firstPayload));
        CTM_CHECK_EQ(static_cast<int>(first.type), static_cast<int>(proto::MsgHostConfig));
        asking.store(false);
        asker.join();
        b.finish();
        again.close();
    }

    section("bridge handshake: a port held for one caller turns another away, and keeps waiting");
    {
        // ⭐ A TV that proved over the internet that it is paired gets its
        // bridge port to itself (the agent's paired_tv.inl): a connection from
        // any other address is closed, and the port waits on for the right one.
        // ⓘ This test can only call from loopback, so the port is held for an
        // address that is not it.
        Started b;
        FakeTv stranger;
        bool connected = false;
        for (int attempt = 40; attempt < 45 && !connected; ++attempt) {
            b.finish();
            b.launch(pick_port(attempt), [](bridgetest::BridgeBackend &backend) {
                backend.set_only_peer((203u << 24) | (0u << 16) | (113u << 8) | 7u);
                backend.set_session_timeouts(1500, 500);
            });
            connected = stranger.connect_to(b.port, 1000);
        }
        CTM_CHECK(connected);
        CTM_CHECK(stranger.sees_close());              // turned away, not handshaken
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(4);
        while (b.result.load() < 0 && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        // ⓘ It waited out its first-connection window for the right caller.
        CTM_CHECK_EQ(b.result.load(), 0);
        CTM_CHECK(b.error == L"bridge initial accept timed out");
        b.finish();
        stranger.close();
    }

    section("bridge handshake: an enumeration sent ahead of HELLO is kept, and the handshake still completes");
    {
        Started b;
        FakeTv tv;
        bool connected = false;
        for (int attempt = 10; attempt < 15 && !connected; ++attempt) {
            b.finish();
            b.launch(pick_port(attempt));
            connected = tv.connect_to(b.port, 1500);
        }
        CTM_CHECK(connected);
        CTM_CHECK(tv.send_msg(proto::MsgEnum, 0, 0, {0xE1, 0xE2, 0xE3}));
        CTM_CHECK(tv.send_msg(proto::MsgHello, 0, 0, hello_payload()));
        proto::Header h = {};
        std::vector<uint8_t> payload;
        CTM_CHECK(tv.recv_msg(&h, &payload));
        CTM_CHECK_EQ(static_cast<int>(h.type), static_cast<int>(proto::MsgHostConfig));
        CTM_CHECK(b.wait_started(2000));
        CTM_CHECK(b.backend->enum_payload() == std::vector<uint8_t>({0xE1, 0xE2, 0xE3}));
        b.finish();
        tv.close();
    }

    section("bridge handshake: a HELLO too short to hold the device's caps is refused");
    {
        Started b;
        FakeTv tv;
        bool connected = false;
        for (int attempt = 20; attempt < 25 && !connected; ++attempt) {
            b.finish();
            b.launch(pick_port(attempt));
            connected = tv.connect_to(b.port, 1500);
            if (!connected) {
                std::printf("  (port %u: no connection; the backend says %d %ls; winsock %d)\n",
                            static_cast<unsigned>(b.port), b.result.load(), b.error.c_str(), WSAGetLastError());
            }
        }
        CTM_CHECK(connected);
        CTM_CHECK(tv.send_msg(proto::MsgHello, 0, 0, std::vector<uint8_t>(10, 0)));
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (b.result.load() < 0 && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        CTM_CHECK_EQ(b.result.load(), 0);
        CTM_CHECK(b.error == L"bridge hello failed");
        // ⓘ And nothing was said to the TV: its end is simply closed.
        CTM_CHECK(tv.sees_close());
        b.finish();
        tv.close();
    }

    WSACleanup();
    return 0;
}
