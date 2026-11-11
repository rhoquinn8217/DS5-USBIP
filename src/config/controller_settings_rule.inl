// A controller's own settings: audio, rumble and its polling limit, kept for
// the controller whatever config it reads.
//
// ⭐ rhoquinn8217, 2026-11-08: *"I've determined controller specific settings
// for audio and rumble are not appropriate for configs. Each controller needs a
// settings file that DS5-USBIP maintains and they are tracked by serial(mac for
// DualSense, DualSense Edge and DS4. ... When clicking the gear icon(or R3 in
// simple and quick modes), a settings window will open. Based on the controller
// type, that settings window will now contain audio and rumble settings that
// were in the config as well as the polling rate. Config should not have an
// audio and rumble section anymore."* Their answers: *"start every controller at
// the defaults"*, nothing copied from a config; and a controller with no serial,
// with an all-zero serial, or with the serial of a controller already bridged
// keeps settings that last only as long as it is bridged.
//
// ➡️ So: a file per controller in controllers/, named by its serial as
// config_store::normalise_serial writes it, loaded into the same settings map
// the configs are, as a section of its own, "ctl:<serial>"; a controller that
// keeps nothing gets "ctl~<ordinal>", in memory alone. The fourteen keys below
// are read from that section and no other; a config that still carries one is
// not read for it. ⓘ The polling limit stays in polling_limits.inl, which has
// kept it by serial since 2026-10-16; the settings window shows it with these.
// ⓘ This file has no windows.h, so the test binary can reach it.
#pragma once

#include <cstddef>
#include <map>
#include <string>

namespace controller_settings {

// The settings a controller keeps for itself, as the configs window's schema
// names them (rest_config.inl).
// ⓘ And its device tag since 2026-11-10 (device_tag_rule.inl): the choice,
// and the tag typed for Custom. ⭐ And its auto link the same day
// (auto_link_store.inl): the config it reads at every bridge.
inline const char *const kKeys[] = {
    "audio_output", "speaker_volume", "headset_volume", "audio_gain", "audio_latency_ms",
    "force_echo_cancel", "master_rumble_gain", "rumble_gain_heavy", "rumble_gain_soft",
    "rumble_floor", "audio_haptics", "audio_haptics_strength", "audio_haptics_bass_hz",
    "audio_haptics_mode", "device_tag_mode", "device_tag", "auto_link",
};

inline bool is_key(const std::string &key)
{
    for (const char *k : kKeys) {
        if (key == k) return true;
    }
    return false;
}

// The folder the files are in, beside configs/.
inline const char *const kDir = "controllers";

// A controller's section in the settings map: kept, by its serial, or this
// bridge's alone, by its ordinal. ⓘ Lower case, as every section is looked up.
inline std::string kept_section(const std::string &serial) { return "ctl:" + serial; }
inline std::string bridge_section(const std::string &ordinal) { return "ctl~" + ordinal; }
inline bool is_kept_section(const std::string &section) { return section.rfind("ctl:", 0) == 0; }
inline bool is_bridge_section(const std::string &section) { return section.rfind("ctl~", 0) == 0; }

// Whether a bridged controller keeps its settings in a file, or for this
// bridge alone: none with no usable serial (empty, or all zeros, which
// config_store::normalise_serial already makes empty), and none when another
// controller bridged now has its serial already.
enum class Keep { InFile, ForThisBridge };
inline Keep keep_for(const std::string &normalisedSerial, bool anotherHasIt)
{
    if (normalisedSerial.empty() || anotherHasIt) return Keep::ForThisBridge;
    return Keep::InFile;
}

inline std::string trim(const std::string &text)
{
    size_t a = 0, b = text.size();
    while (a < b && (text[a] == ' ' || text[a] == '\t' || text[a] == '\r')) ++a;
    while (b > a && (text[b - 1] == ' ' || text[b - 1] == '\t' || text[b - 1] == '\r')) --b;
    return text.substr(a, b - a);
}

// A file's settings: "key = value" lines, '#' starting a comment line. Only the
// keys above are kept; a value as written, trimmed.
inline std::map<std::string, std::string> parse(const std::string &text)
{
    std::map<std::string, std::string> out;
    size_t at = 0;
    while (at <= text.size()) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        const std::string line = trim(text.substr(at, end - at));
        at = end + 1;
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trim(line.substr(0, eq));
        for (char &c : key) c = static_cast<char>((c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c);
        const std::string value = trim(line.substr(eq + 1));
        if (!is_key(key) || value.empty()) continue;
        out[key] = value;
    }
    return out;
}

// The file for those settings, CRLF as every file here is written.
inline std::string format(const std::string &serial, const std::map<std::string, std::string> &settings)
{
    std::string out = "# Settings DS5-USBIP keeps for one controller, " + serial + ",\r\n"
                      "# whatever config it reads. A setting not here is at its default.\r\n";
    for (const auto &s : settings) out += s.first + " = " + s.second + "\r\n";
    return out;
}

} // namespace controller_settings
