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

}  // namespace audio_volume
