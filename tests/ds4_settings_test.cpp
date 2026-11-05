// A DS4's own settings (2026-11-04): the DS4Dongle's settings reports, and the
// rumble dials on a DS4's motors.
//
// ⚠️ WHAT THESE CANNOT DO. They cannot reach a dongle or feel a motor. They pin
// the bytes: that a request is the dongle's own protocol, that only the one
// setting is ever changed, and that a DS4's motors get the DualSense's
// arithmetic at the DS4's offsets.

#include "harness.h"

#include <cstdint>
#include <string>
#include <vector>

#include "audio/rumble_floor.inl"
#include "audio/ds4_rumble.inl"
#include "audio/ds4_dongle.inl"

using namespace ctmtest;

namespace {

// A DS4 output report claiming the motors, soft then heavy as given.
std::vector<uint8_t> motors(uint8_t soft, uint8_t heavy, uint8_t flags = 0x01)
{
    std::vector<uint8_t> r(32, 0);
    r[0] = 0x05;
    r[1] = flags;
    r[4] = soft;
    r[5] = heavy;
    return r;
}

// The C1's DS4Dongle's body on 2026-11-04, read with its jack setting at 1.
const ds4_dongle::Body kBody = {0x06, 0x00, 0x00, 0x80, 0x3f, 0x64, 0x64, 0x02, 0x1e, 0x00, 0x00,
                                0x40, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};

} // namespace

int run_ds4_settings_tests()
{
    section("ds4 rumble: the game's rumble stands with nothing set");
    {
        std::vector<uint8_t> r = motors(100, 200);
        CTM_CHECK(!ds4_scale_motors(r.data(), r.size(), -1, -1, -1, -1));
        CTM_CHECK(r[4] == 100 && r[5] == 200);
    }

    section("ds4 rumble: master on both motors, each motor's own on top");
    {
        std::vector<uint8_t> r = motors(100, 200);
        CTM_CHECK(ds4_scale_motors(r.data(), r.size(), 50, -1, -1, 0));
        CTM_CHECK_EQ(static_cast<int>(r[4]), 50);
        CTM_CHECK_EQ(static_cast<int>(r[5]), 100);
        r = motors(100, 200);
        // ⓘ Master 50 and heavy 50 give the heavy motor a quarter, as a mixing desk.
        CTM_CHECK(ds4_scale_motors(r.data(), r.size(), 50, 50, -1, 0));
        CTM_CHECK_EQ(static_cast<int>(r[5]), 50);
        CTM_CHECK_EQ(static_cast<int>(r[4]), 50);
        r = motors(100, 200);
        CTM_CHECK(ds4_scale_motors(r.data(), r.size(), 200, -1, -1, 0));
        CTM_CHECK_EQ(static_cast<int>(r[4]), 200);
        CTM_CHECK_EQ(static_cast<int>(r[5]), 255);   // capped, not wrapped
    }

    section("ds4 rumble: heavy is byte 5 and soft is byte 4");
    {
        std::vector<uint8_t> r = motors(100, 100);
        CTM_CHECK(ds4_scale_motors(r.data(), r.size(), -1, 0, -1, 0));
        CTM_CHECK_EQ(static_cast<int>(r[5]), 0);
        CTM_CHECK_EQ(static_cast<int>(r[4]), 100);
        r = motors(100, 100);
        CTM_CHECK(ds4_scale_motors(r.data(), r.size(), -1, -1, 0, 0));
        CTM_CHECK_EQ(static_cast<int>(r[4]), 0);
        CTM_CHECK_EQ(static_cast<int>(r[5]), 100);
    }

    section("ds4 rumble: the floor lifts a faint cue, and never a stop or a zero gain");
    {
        const int floorByte = (kRumbleFloorDefaultPercent * 255) / 100;
        std::vector<uint8_t> r = motors(5, 0);
        CTM_CHECK(ds4_scale_motors(r.data(), r.size(), -1, -1, -1, kRumbleFloorDefaultPercent));
        CTM_CHECK_EQ(static_cast<int>(r[4]), floorByte);
        CTM_CHECK_EQ(static_cast<int>(r[5]), 0);     // the game asked for nothing there
        r = motors(100, 100);
        ds4_scale_motors(r.data(), r.size(), 0, -1, -1, kRumbleFloorDefaultPercent);
        CTM_CHECK(r[4] == 0 && r[5] == 0);           // master 0 is silence
    }

    section("ds4 rumble: only a report that drives the motors");
    {
        std::vector<uint8_t> r = motors(100, 200, 0x02);   // the light alone
        CTM_CHECK(!ds4_scale_motors(r.data(), r.size(), 50, -1, -1, 0));
        CTM_CHECK(r[4] == 100 && r[5] == 200);
        std::vector<uint8_t> ds5 = motors(100, 200);
        ds5[0] = 0x02;                                      // a DualSense's report
        CTM_CHECK(!ds4_scale_motors(ds5.data(), ds5.size(), 50, -1, -1, 0));
        CTM_CHECK(!ds4_scale_motors(r.data(), 5, 50, -1, -1, 0));   // too short for byte 5
    }

    section("ds4 dongle: a version only from a dongle");
    {
        std::vector<uint8_t> reply(64, 0);
        reply[0] = 0xF8;
        const char *v = "v0.3.0";
        for (size_t i = 0; v[i]; ++i) reply[1 + i] = static_cast<uint8_t>(v[i]);
        CTM_CHECK(ds4_dongle::version_of(reply) == "v0.3.0");
        reply.erase(reply.begin());
        CTM_CHECK(ds4_dongle::version_of(reply) == "v0.3.0");   // with or without its id
        std::vector<uint8_t> garbage = {0xF8, 'v', 0x01, 'x'};
        CTM_CHECK(ds4_dongle::version_of(garbage).empty());      // not text: not a dongle
        CTM_CHECK(ds4_dongle::version_of(std::vector<uint8_t>(64, 0)).empty());
    }

    section("ds4 dongle: the body, read with or without its id");
    {
        std::vector<uint8_t> reply(64, 0);
        reply[0] = 0xF7;
        std::copy(kBody.begin(), kBody.end(), reply.begin() + 1);
        ds4_dongle::Body body{};
        CTM_CHECK(ds4_dongle::body_of(reply, &body) && body == kBody);
        CTM_CHECK(body[0] == ds4_dongle::kSchema);
        CTM_CHECK(body[ds4_dongle::kFollowJackAt] == 1);
        std::vector<uint8_t> bare(kBody.begin(), kBody.end());
        CTM_CHECK(ds4_dongle::body_of(bare, &body) && body == kBody);
        std::vector<uint8_t> shortReply(10, 0);
        CTM_CHECK(!ds4_dongle::body_of(shortReply, &body));
    }

    section("ds4 dongle: an update changes the one setting and nothing else");
    {
        const std::vector<uint8_t> r = ds4_dongle::update_request(kBody, ds4_dongle::follow_jack_for(true));
        CTM_CHECK_EQ(r.size(), static_cast<size_t>(64));
        CTM_CHECK(r[0] == 0xF6 && r[1] == 0x01);
        int others = 0;
        for (size_t i = 0; i < ds4_dongle::kBodySize; ++i) {
            if (i == ds4_dongle::kFollowJackAt) continue;
            if (r[2 + i] != kBody[i]) ++others;
        }
        CTM_CHECK_EQ(others, 0);
        CTM_CHECK(r[2 + ds4_dongle::kFollowJackAt] == 0);   // on: the card always there
        for (size_t i = 2 + ds4_dongle::kBodySize; i < r.size(); ++i) CTM_CHECK(r[i] == 0);
        CTM_CHECK(ds4_dongle::follow_jack_for(false) == 1);
    }

    section("ds4 dongle: save and reconnect are the function alone");
    {
        const std::vector<uint8_t> save = ds4_dongle::function_request(ds4_dongle::kFuncSave);
        const std::vector<uint8_t> again = ds4_dongle::function_request(ds4_dongle::kFuncReconnect);
        CTM_CHECK(save.size() == 64 && save[0] == 0xF6 && save[1] == 0x02);
        CTM_CHECK(again.size() == 64 && again[0] == 0xF6 && again[1] == 0x03);
        int nonZero = 0;
        for (size_t i = 2; i < 64; ++i) nonZero += (save[i] != 0) + (again[i] != 0);
        CTM_CHECK_EQ(nonZero, 0);
        const std::vector<uint8_t> get = ds4_dongle::get_request(ds4_dongle::kReportConfig);
        CTM_CHECK(get.size() == 64 && get[0] == 0xF7);
    }

    return 0;
}
