// The USB/IP wire format: the protocol's numbers, and how bytes go on to and
// come off the socket.
//
// ⓘ A file of its own, needing only winsock and the standard library, so a
// test can serve a device over a real socket with the very helpers the
// program uses (code review, 2026-10-05: nothing tested the USB/IP server).
// They lived at the top of app/common.inl, unchanged.

constexpr uint16_t kUsbipVersion = 0x0111;
constexpr uint16_t kOpReqDevlist = 0x8005;
constexpr uint16_t kOpRepDevlist = 0x0005;
constexpr uint16_t kOpReqImport = 0x8003;
constexpr uint16_t kOpRepImport = 0x0003;
constexpr uint32_t kCmdSubmit = 0x00000001;
constexpr uint32_t kCmdUnlink = 0x00000002;
constexpr uint32_t kRetSubmit = 0x00000003;
constexpr uint32_t kRetUnlink = 0x00000004;
constexpr uint32_t kUsbipDirOut = 0;
constexpr uint32_t kUsbipDirIn = 1;
constexpr uint32_t kNonIsoPackets = 0xffffffffu;
constexpr int32_t kStatusOk = 0;
constexpr int32_t kStatusStall = -32; // -EPIPE
constexpr int32_t kStatusConnReset = -104; // -ECONNRESET
constexpr uint16_t kDefaultUsbipPort = 3240;
constexpr uint32_t kUsbSpeedHigh = 3; // Linux USB_SPEED_HIGH for USB/IP.
constexpr uint32_t kUsbSpeedFull = 2; // Linux USB_SPEED_FULL (composite puck: 64B bulk eps).

static void append_be16(std::vector<uint8_t> *out, uint16_t value)
{
    out->push_back(static_cast<uint8_t>((value >> 8) & 0xff));
    out->push_back(static_cast<uint8_t>(value & 0xff));
}

static void append_be32(std::vector<uint8_t> *out, uint32_t value)
{
    out->push_back(static_cast<uint8_t>((value >> 24) & 0xff));
    out->push_back(static_cast<uint8_t>((value >> 16) & 0xff));
    out->push_back(static_cast<uint8_t>((value >> 8) & 0xff));
    out->push_back(static_cast<uint8_t>(value & 0xff));
}

static uint16_t read_be16(const uint8_t *data)
{
    return static_cast<uint16_t>((data[0] << 8) | data[1]);
}

static uint32_t read_be32(const uint8_t *data)
{
    return (static_cast<uint32_t>(data[0]) << 24) |
        (static_cast<uint32_t>(data[1]) << 16) |
        (static_cast<uint32_t>(data[2]) << 8) |
        static_cast<uint32_t>(data[3]);
}

static uint16_t read_le16(const uint8_t *data)
{
    return static_cast<uint16_t>(data[0] | (data[1] << 8));
}

static int16_t read_s16_le(const uint8_t *data)
{
    return static_cast<int16_t>(read_le16(data));
}

static void append_fixed_string(std::vector<uint8_t> *out, const char *text, size_t bytes)
{
    const size_t start = out->size();
    out->resize(start + bytes, 0);
    if (text != nullptr) {
        const size_t copy = (std::min)(strlen(text), bytes ? bytes - 1 : 0);
        if (copy != 0) {
            memcpy(out->data() + start, text, copy);
        }
    }
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
