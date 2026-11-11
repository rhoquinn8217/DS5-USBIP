// What a controller's volume setting becomes on its way to the TV.
//
// ⭐ A DS4 IS TOLD FULL VOLUME WHEN NOTHING SETS ONE (rhoquinn8217, 2026-11-02:
// "make 100 the default for every DS4"). Told nothing, a DS4 plays at its own
// level, which is quiet, while the configs window shows 100 for it: their DS4
// over Bluetooth was heard low on two TVs, and loud once its config said 100.
// A DualSense is left as it was: its level also comes in the host's own
// reports, so "not set" still leaves the TV's value alone.
//
// ⚠️ Zero is a legal level and means SILENT, so "not set" is kUnset, never 0.
// A setting of 0 stays 0 on a DS4 too.

#pragma once

#include <cstdint>
#include <string>

namespace audio_volume {

// CtmBridgeProtocol::kAudioUnset: the TV keeps the value it has.
constexpr uint8_t kUnset = 0xFFu;
constexpr uint8_t kFull = 100;

// settingsKind as config_store::settings_kind_for gives it, which is "ds4" for
// a DS4 on Bluetooth and on a cable alike.
inline bool full_when_unset(const std::string &settingsKind)
{
    return settingsKind == "ds4";
}

// configured is the setting as read, -1 when absent.
inline uint8_t volume_byte(int configured, bool fullWhenUnset)
{
    if (configured >= 0 && configured <= 100) {
        return static_cast<uint8_t>(configured);
    }
    return fullWhenUnset ? kFull : kUnset;
}

// ⭐ BACK TO ITS DEFAULT ON THE TV (rhoquinn8217, 2026-11-11). The TV keeps the
// last value it was sent until it is sent another, and a setting put back to
// its default is taken out of the controller's file, so the TV was sent
// nothing: a Bluetooth DualSense stayed on the headphones after "headset" and
// then the default, and at 30 after 30 and then 100, until its next bridge.
// So a value this bridge sent before (sentBefore) that is unset now is sent as
// its default. One never sent stays unset: a pad nobody changed is told
// nothing, as before, and its volumes keep deferring to the game's.
// ⓘ The defaults are the configs window's: the mode auto, both dials 100,
// the buffer 60 (the TV's own too).
constexpr uint8_t kModeAuto = 0;   // the TV's AUTO
constexpr uint16_t kLatencyUnset = 0xFFFFu;   // CtmBridgeProtocol::kLatencyUnset
constexpr uint16_t kLatencyDefault = 60;

inline uint8_t back_to_default(uint8_t now, uint8_t sentBefore, uint8_t byDefault)
{
    if (now != kUnset) return now;
    return sentBefore != kUnset ? byDefault : kUnset;
}

inline uint16_t latency_back_to_default(uint16_t now, uint16_t sentBefore, uint16_t byDefault)
{
    if (now != kLatencyUnset) return now;
    return sentBefore != kLatencyUnset ? byDefault : kLatencyUnset;
}

}  // namespace audio_volume
