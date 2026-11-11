// Tests for turning per-controller audio config into the three bytes that
// travel to the TV.
//
// ⭐⭐ WHY THIS EXISTS. On 2026-08-25 `speaker_volume = 0` left a Bluetooth
// controller at FULL VOLUME. Heard, not inferred. Several things were wrong at
// once, and two of them live on this side of the wire:
//
//   ⛔ Every override in ds5_output_overrides.inl begins
//      `if (data[0] != 0x02) return;` -- 0x02 is the WIRED report id. Over
//      Bluetooth the host sends 0x36, so speaker volume, headset volume, audio
//      routing and the three rumble gains were all silently wired-only.
//
//   ⛔ So the values now TRAVEL instead. They cannot be patched here: a
//      Bluetooth output report is signed, and the TV re-signs only when it
//      patched something itself -- a report edited on this side alone arrives
//      with a stale signature and is dropped by the controller.
//
// ⚠️⚠️ THE TRAP THIS FILE GUARDS. **Zero is a legal percentage and means
// SILENT.** So it cannot be the sentinel for "not configured", and it must not
// be turned into one anywhere along the way. The same trap has already been met
// once in this project, on audio_latency_ms, where the range was deliberately
// opened so the silent end could be reached at all.
//
// ⓘ The other half of it: an OLDER host sends a zeroed reserved block. A newer
// TV reading that would see "volume 0" and mute the controller, so the TV treats
// an all-zero triple as unset -- and this side must never send one by accident.
//
// ⛔ Protecting logic, not behaviour. Whether the bytes reach a controller is
// threading through the output path and needs hardware.
//
// ⭐ The volume byte is the real one, from ctm/audio_volume.h, since a DS4 started
// getting 100 when its config says nothing (2026-11-02). The mode and the send
// guard are still mirrored.

#include "harness.h"

#include "ctm/audio_volume.h"

#include <cstdint>
#include <string>

using namespace ctmtest;

namespace {

constexpr uint8_t kAudioUnset = 0xFFu;

// Every controller but a DS4, through the real conversion.
uint8_t volume_byte(int configured)
{
    return audio_volume::volume_byte(configured, false);
}

uint8_t ds4_volume_byte(int configured)
{
    return audio_volume::volume_byte(configured, audio_volume::full_when_unset("ds4"));
}

// ⓘ The TV's enum: AUTO 0, OFF 1, SPEAKER 2, HEADSET 3, BOTH 4.
uint8_t mode_byte(const std::string &audio_output)
{
    if (audio_output == "auto")         return 0;
    if (audio_output == "off")          return 1;
    if (audio_output == "speaker")      return 2;
    if (audio_output == "headset")      return 3;
    if (audio_output == "headset_mono") return 3;
    if (audio_output == "both")         return 4;
    return kAudioUnset;
}

bool worth_sending(uint8_t spk, uint8_t hset, uint8_t mode)
{
    return spk != kAudioUnset || hset != kAudioUnset || mode != kAudioUnset;
}

}  // namespace

int run_host_audio_settings_tests()
{
    // ⭐⭐ THE ONE THAT MATTERS. A configured 0 must survive as 0 and must NOT
    // collapse into the unset sentinel. This is the whole bug.
    CTM_CHECK_EQ(volume_byte(0), 0);
    CTM_CHECK(volume_byte(0) != kAudioUnset);

    // ⭐ And a configured 0 is still worth sending -- an early version of the
    // send-guard could have treated it as "nothing to say".
    CTM_CHECK(worth_sending(volume_byte(0), kAudioUnset, kAudioUnset));

    // Ordinary values pass through unchanged.
    CTM_CHECK_EQ(volume_byte(50), 50);
    CTM_CHECK_EQ(volume_byte(100), 100);

    // ⛔ Absent (-1) and out of range become unset, so the TV keeps its own.
    CTM_CHECK_EQ(volume_byte(-1), kAudioUnset);
    CTM_CHECK_EQ(volume_byte(101), kAudioUnset);
    CTM_CHECK_EQ(volume_byte(255), kAudioUnset);

    // ⚠️ Nothing configured at all: nothing is sent, so an unconfigured
    // controller behaves exactly as it did before any of this existed.
    CTM_CHECK(!worth_sending(kAudioUnset, kAudioUnset, kAudioUnset));

    // ⛔⛔ AND THE ALL-ZERO TRIPLE MUST NEVER BE PRODUCED BY ACCIDENT. The TV
    // treats it as unset to protect against an older host, and this side must
    // not lean on that guard. An absent key gives 255, never 0.
    const uint8_t spk  = volume_byte(-1);
    const uint8_t hset = volume_byte(-1);
    const uint8_t mode = mode_byte("");
    CTM_CHECK(!(spk == 0 && hset == 0 && mode == 0));

    // The mode mapping.
    CTM_CHECK_EQ(mode_byte("auto"), 0);
    CTM_CHECK_EQ(mode_byte("off"), 1);
    CTM_CHECK_EQ(mode_byte("speaker"), 2);
    CTM_CHECK_EQ(mode_byte("headset"), 3);
    CTM_CHECK_EQ(mode_byte("both"), 4);

    // ⓘ headset_mono has NO TV equivalent -- it is a downmix done on this side
    // -- so it maps to HEADSET rather than to unset. Sending unset there would
    // leave the route wherever it happened to be.
    CTM_CHECK_EQ(mode_byte("headset_mono"), 3);

    // An unrecognised or absent value leaves the TV's route alone.
    CTM_CHECK_EQ(mode_byte(""), kAudioUnset);
    CTM_CHECK_EQ(mode_byte("nonsense"), kAudioUnset);

    // ⚠️ AUTO is mode 0, which is a REAL MODE and not a sentinel. Setting
    // audio_output = auto explicitly must be distinguishable from leaving it
    // absent -- the same zero-is-legal trap in a second place.
    CTM_CHECK(mode_byte("auto") != mode_byte(""));

    // ⭐⭐ A DS4 WITH NOTHING SET IS TOLD 100, and so is sent something. Told
    // nothing, it played quietly while the configs window showed 100 for it.
    CTM_CHECK_EQ(audio_volume::kUnset, kAudioUnset);
    CTM_CHECK_EQ(ds4_volume_byte(-1), 100);
    CTM_CHECK(worth_sending(ds4_volume_byte(-1), ds4_volume_byte(-1), mode_byte("")));

    // ⛔ But what a DS4's config does say still wins, and 0 is still silent.
    CTM_CHECK_EQ(ds4_volume_byte(0), 0);
    CTM_CHECK_EQ(ds4_volume_byte(40), 40);
    CTM_CHECK_EQ(ds4_volume_byte(100), 100);

    // ⛔ Only a DS4. A DualSense, an Edge, an Xbox pad and anything without a
    // section are left exactly as they were: absent stays unset.
    CTM_CHECK(audio_volume::full_when_unset("ds4"));
    CTM_CHECK(!audio_volume::full_when_unset("ds5"));
    CTM_CHECK(!audio_volume::full_when_unset("ds5_edge"));
    CTM_CHECK(!audio_volume::full_when_unset("xbox"));
    CTM_CHECK(!audio_volume::full_when_unset(""));
    CTM_CHECK(!audio_volume::full_when_unset("ds4_usb"));

    // ⭐⭐ BACK TO ITS DEFAULT ON THE TV (2026-11-11). The TV keeps the last
    // value it was sent, so a Bluetooth DualSense stayed on the headphones
    // after "headset" and then the default, and at 30 after 30 and then 100,
    // the default, which leaves the file. A value this bridge sent before and
    // unset now is sent as its default: the mode auto, a dial 100.
    using audio_volume::back_to_default;
    CTM_CHECK_EQ(back_to_default(mode_byte(""), mode_byte("headset"), audio_volume::kModeAuto), 0);
    CTM_CHECK_EQ(back_to_default(volume_byte(-1), volume_byte(30), audio_volume::kFull), 100);
    // A speaker set to 0 and then put back is 100 again, not left silent.
    CTM_CHECK_EQ(back_to_default(volume_byte(-1), volume_byte(0), audio_volume::kFull), 100);
    // What is set now is sent as it is, whatever was sent before.
    CTM_CHECK_EQ(back_to_default(volume_byte(40), volume_byte(30), audio_volume::kFull), 40);
    CTM_CHECK_EQ(back_to_default(volume_byte(0), kAudioUnset, audio_volume::kFull), 0);
    CTM_CHECK_EQ(back_to_default(mode_byte("speaker"), mode_byte("headset"), audio_volume::kModeAuto), 2);
    // ⛔ Never sent and unset now: still unset, so a pad nobody changed is told
    // nothing and its volumes keep deferring to the game's, as before.
    CTM_CHECK_EQ(back_to_default(kAudioUnset, kAudioUnset, audio_volume::kFull), kAudioUnset);
    CTM_CHECK(!worth_sending(back_to_default(volume_byte(-1), kAudioUnset, audio_volume::kFull),
                             back_to_default(volume_byte(-1), kAudioUnset, audio_volume::kFull),
                             back_to_default(mode_byte(""), kAudioUnset, audio_volume::kModeAuto)));
    // ⛔ And the defaults never make the all-zero triple: put back, the dials
    // are 100, so the TV does not take auto for an older host's zeros.
    const uint8_t backSpk  = back_to_default(volume_byte(-1), volume_byte(0), audio_volume::kFull);
    const uint8_t backHset = back_to_default(volume_byte(-1), volume_byte(0), audio_volume::kFull);
    const uint8_t backMode = back_to_default(mode_byte(""), mode_byte("auto"), audio_volume::kModeAuto);
    CTM_CHECK(!(backSpk == 0 && backHset == 0 && backMode == 0));

    // The buffer the same way, in its own width.
    using audio_volume::latency_back_to_default;
    const uint16_t kLatUnset = audio_volume::kLatencyUnset;
    CTM_CHECK_EQ(kLatUnset, 0xFFFFu);
    CTM_CHECK_EQ(latency_back_to_default(kLatUnset, 100, 60), 60);
    CTM_CHECK_EQ(latency_back_to_default(40, 100, 60), 40);
    CTM_CHECK_EQ(latency_back_to_default(kLatUnset, kLatUnset, 60), kLatUnset);

    return 0;
}
