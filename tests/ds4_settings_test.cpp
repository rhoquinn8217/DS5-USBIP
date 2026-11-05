// A DS4's own settings (2026-11-04): the DS4Dongle's settings reports, the
// rumble dials on a DS4's motors, and a dongle this listener restarted coming
// back as itself.
//
// ⚠️ WHAT THESE CANNOT DO. They cannot reach a dongle or feel a motor. They pin
// the bytes: that a request is the dongle's own protocol, that only the one
// setting is ever changed, and that a DS4's motors get the DualSense's
// arithmetic at the DS4's offsets.

#include "harness.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "audio/rumble_floor.inl"
#include "audio/ds4_rumble.inl"
#include "audio/ds4_dongle.inl"
#include "audio/ds4_speaker.inl"
#include "app/same_device.inl"
#include "app/tv_sessions.inl"
#include "app/ds4_dongle_return.inl"

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

    section("ds4 speaker: the jack is byte 30 of report 0x01, headphones or a mic");
    {
        // ⓘ The C1's DS4 through its DS4Dongle, nothing in the jack: byte 30 0x05.
        std::vector<uint8_t> r(64, 0);
        r[0] = 0x01;
        r[30] = 0x05;
        CTM_CHECK_EQ(ds4_speaker::jack_in(r.data(), r.size()), 0);
        r[30] = 0x25;
        CTM_CHECK_EQ(ds4_speaker::jack_in(r.data(), r.size()), 1);
        r[30] = 0x45;
        CTM_CHECK_EQ(ds4_speaker::jack_in(r.data(), r.size()), 1);
        r[30] = 0x65;
        CTM_CHECK_EQ(ds4_speaker::jack_in(r.data(), r.size()), 1);
        r[30] = 0x1f;       // charging, full: the cable bit is not the jack
        CTM_CHECK_EQ(ds4_speaker::jack_in(r.data(), r.size()), 0);
        r[0] = 0x11;        // a Bluetooth report: not this layout
        CTM_CHECK_EQ(ds4_speaker::jack_in(r.data(), r.size()), -1);
        r[0] = 0x01;
        CTM_CHECK_EQ(ds4_speaker::jack_in(r.data(), 30), -1);
        CTM_CHECK_EQ(ds4_speaker::jack_in(nullptr, 64), -1);
    }

    section("ds4 speaker: each frame becomes half of each channel, on both");
    {
        auto frame = [](int16_t left, int16_t right) {
            std::vector<uint8_t> f(4);
            f[0] = static_cast<uint8_t>(static_cast<uint16_t>(left) & 0xff);
            f[1] = static_cast<uint8_t>(static_cast<uint16_t>(left) >> 8);
            f[2] = static_cast<uint8_t>(static_cast<uint16_t>(right) & 0xff);
            f[3] = static_cast<uint8_t>(static_cast<uint16_t>(right) >> 8);
            return f;
        };
        auto left_of = [](const std::vector<uint8_t> &f) {
            return static_cast<int16_t>(static_cast<uint16_t>(f[0]) | (static_cast<uint16_t>(f[1]) << 8));
        };
        auto right_of = [](const std::vector<uint8_t> &f) {
            return static_cast<int16_t>(static_cast<uint16_t>(f[2]) | (static_cast<uint16_t>(f[3]) << 8));
        };
        // ⭐ Windows' Test: the left chime alone reaches the speaker now.
        std::vector<uint8_t> f = frame(20000, 0);
        ds4_speaker::mix_to_mono(f);
        CTM_CHECK_EQ(static_cast<int>(left_of(f)), 10000);
        CTM_CHECK_EQ(static_cast<int>(right_of(f)), 10000);
        f = frame(0, -20000);
        ds4_speaker::mix_to_mono(f);
        CTM_CHECK_EQ(static_cast<int>(left_of(f)), -10000);
        CTM_CHECK_EQ(static_cast<int>(right_of(f)), -10000);
        // The loudest of both, either way, and nothing wraps.
        f = frame(32767, 32767);
        ds4_speaker::mix_to_mono(f);
        CTM_CHECK_EQ(static_cast<int>(left_of(f)), 32767);
        f = frame(-32768, -32768);
        ds4_speaker::mix_to_mono(f);
        CTM_CHECK_EQ(static_cast<int>(right_of(f)), -32768);
        // A trailing part frame is left alone.
        std::vector<uint8_t> two = frame(100, 300);
        two.push_back(0xAB);
        ds4_speaker::mix_to_mono(two);
        CTM_CHECK_EQ(static_cast<int>(left_of(two)), 200);
        CTM_CHECK(two.size() == 5 && two[4] == 0xAB);
    }

    // ⭐ The C1's DS4 on its DS4Dongle, 2026-11-04, as the bridge names it.
    using namespace ds4_dongle_return;
    using std::chrono::seconds;
    Who pad;
    pad.owner = "30fec46a";
    pad.vendorId = 0x054c;
    pad.productId = 0x09cc;
    pad.serial = "30:0e:d5:a9:69:51";
    pad.product = "Sony Interactive Entertainment Wireless Controller";
    Entry restarted;
    restarted.who = pad;
    restarted.nickname = "Rogue";
    restarted.followJack = 0;
    const Clock::time_point t0 = Clock::now();

    section("ds4 dongle restart: the pad back takes its name and what it was given, once");
    {
        List list;
        list.expect(restarted, t0);
        Entry back;
        // ⓘ 6 s: the C1's DS4 was back in five, and named in one more.
        CTM_CHECK(list.take(pad, t0 + seconds(6), &back));
        CTM_CHECK(back.nickname == "Rogue");
        CTM_CHECK_EQ(back.followJack, 0);
        CTM_CHECK(!list.take(pad, t0 + seconds(7), &back));
        CTM_CHECK_EQ(static_cast<int>(list.size()), 0);
    }

    section("ds4 dongle restart: another TV's, another pad or another model takes nothing");
    {
        List list;
        list.expect(restarted, t0);
        Who otherTv = pad;
        otherTv.owner = "9b2c1d00";
        Who otherPad = pad;
        otherPad.serial = "a4:ae:12:00:00:01";
        Who otherModel = pad;
        otherModel.productId = 0x05c4;
        Who noSerial = pad;
        noSerial.serial.clear();
        CTM_CHECK(!list.take(otherTv, t0 + seconds(1), nullptr));
        CTM_CHECK(!list.take(otherPad, t0 + seconds(1), nullptr));
        CTM_CHECK(!list.take(otherModel, t0 + seconds(1), nullptr));
        CTM_CHECK(!list.take(noSerial, t0 + seconds(1), nullptr));
        CTM_CHECK_EQ(static_cast<int>(list.size()), 1);
        CTM_CHECK(list.take(pad, t0 + seconds(1), nullptr));
    }

    section("ds4 dongle restart: nothing is kept past the window");
    {
        List list;
        list.expect(restarted, t0);
        CTM_CHECK(!list.take(pad, t0 + kWindow, nullptr));
        CTM_CHECK_EQ(static_cast<int>(list.size()), 0);
        list.expect(restarted, t0);
        CTM_CHECK(list.take(pad, t0 + kWindow - seconds(1), nullptr));
    }

    section("ds4 dongle restart: a pad without a serial is known by its name");
    {
        List list;
        Entry nameless = restarted;
        nameless.who.serial.clear();
        list.expect(nameless, t0);
        Who renamed = nameless.who;
        renamed.product = "Wireless Controller (2)";
        CTM_CHECK(!list.take(renamed, t0 + seconds(1), nullptr));
        CTM_CHECK(list.take(nameless.who, t0 + seconds(1), nullptr));
    }

    section("ds4 dongle restart: a second restart of the pad replaces the first");
    {
        List list;
        list.expect(restarted, t0);
        Entry again = restarted;
        again.nickname = "Mamba";
        list.expect(again, t0 + seconds(10));
        CTM_CHECK_EQ(static_cast<int>(list.size()), 1);
        Entry back;
        // ⓘ 25 s: past the first restart's window, inside the second's.
        CTM_CHECK(list.take(pad, t0 + seconds(25), &back));
        CTM_CHECK(back.nickname == "Mamba");
    }

    return 0;
}
