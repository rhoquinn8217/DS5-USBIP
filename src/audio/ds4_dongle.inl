// A DS4Dongle's own settings, through its HID feature reports (2026-11-04).
//
// ⭐ WHY THIS EXISTS. A DS4Dongle (snipem/DS4Dongle: a Pico 2 W a DS4 pairs to
// over Bluetooth) shows the TV a wired second-model DS4 and keeps a few settings
// in its flash. One of them decides whether its sound card exists while nothing
// is in the pad's headphone jack, and it ships as "only with a plug in": a DS4
// without a headset had no speaker at all. The listener sets it to play the
// speaker without headphones (ds4_dongle_keep.inl).
//
// ⛔ WINDOWS CANNOT REACH THESE REPORTS. The dongle's descriptor is a real DS4
// v2's, which declares none of them, and Windows' HID class driver refuses a
// report id its descriptor does not declare. So they go to the TV, whose Linux
// passes them to the dongle as they are (the TV core's feature get and set),
// never through the pad Windows sees.
//
// The protocol is the dongle's own, as its tools/config_tool.py (v0.3.0) speaks
// it:
//   GET 0xF8   the firmware's version, a string: only a dongle answers it
//   GET 0xF7   the config body, 21 bytes, schema 6 in its first byte
//   SET 0xF6   function 0x01 and a body updates it, 0x02 saves it to flash,
//              0x03 reconnects USB -- which is the only way the sound card
//              appears or goes, the dongle choosing it once per connection
//
// ⓘ Pure: requests built and replies read. ds4_dongle_keep.inl sends them.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ds4_dongle {

constexpr uint8_t kReportSet = 0xF6;
constexpr uint8_t kReportConfig = 0xF7;
constexpr uint8_t kReportVersion = 0xF8;
constexpr uint8_t kFuncUpdate = 0x01;
constexpr uint8_t kFuncSave = 0x02;
constexpr uint8_t kFuncReconnect = 0x03;
constexpr size_t kReportLength = 64;    // the id and 63 bytes
constexpr size_t kBodySize = 21;
constexpr uint8_t kSchema = 6;
constexpr size_t kFollowJackAt = 20;    // audio_follow_jack: 1 hides the card while the jack is empty

using Body = std::array<uint8_t, kBodySize>;

// A GET: the report's id, then zeros to its length.
inline std::vector<uint8_t> get_request(uint8_t reportId)
{
    std::vector<uint8_t> r(kReportLength, 0);
    r[0] = reportId;
    return r;
}

// The version a GET 0xF8 answered, or "" when nothing answered as a dongle
// does: printable ASCII up to its first zero, after the id if it came back.
inline std::string version_of(const std::vector<uint8_t> &reply)
{
    size_t at = (!reply.empty() && reply[0] == kReportVersion) ? 1 : 0;
    std::string v;
    for (; at < reply.size() && reply[at] != 0; ++at) {
        const uint8_t ch = reply[at];
        if (ch < 0x20 || ch > 0x7e) {
            return std::string();
        }
        v.push_back(static_cast<char>(ch));
    }
    return v;
}

// The body a GET 0xF7 answered, with or without its id first. False when short.
inline bool body_of(const std::vector<uint8_t> &reply, Body *out)
{
    if (out == nullptr) {
        return false;
    }
    const size_t at = (!reply.empty() && reply[0] == kReportConfig) ? 1 : 0;
    if (reply.size() < at + kBodySize) {
        return false;
    }
    std::copy(reply.begin() + static_cast<std::ptrdiff_t>(at),
              reply.begin() + static_cast<std::ptrdiff_t>(at + kBodySize), out->begin());
    return true;
}

// SET 0xF6 0x01: the body as it was read, with audio_follow_jack alone changed.
inline std::vector<uint8_t> update_request(const Body &body, uint8_t followJack)
{
    std::vector<uint8_t> r(kReportLength, 0);
    r[0] = kReportSet;
    r[1] = kFuncUpdate;
    std::copy(body.begin(), body.end(), r.begin() + 2);
    r[2 + kFollowJackAt] = followJack ? 1 : 0;
    return r;
}

// SET 0xF6 with a function and nothing else: save, or reconnect.
inline std::vector<uint8_t> function_request(uint8_t function)
{
    std::vector<uint8_t> r(kReportLength, 0);
    r[0] = kReportSet;
    r[1] = function;
    return r;
}

// audio_follow_jack for the speaker without headphones: on is 0, the card always there.
inline uint8_t follow_jack_for(bool speakerWithoutHeadphones)
{
    return speakerWithoutHeadphones ? 0 : 1;
}

}  // namespace ds4_dongle
