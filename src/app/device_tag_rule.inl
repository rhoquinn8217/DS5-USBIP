// A controller's DEVICE TAG: what it is called when it is bridged.
//
// ⭐ THEIR WORDS (rhoquinn8217, 2026-11-10): *"I want to add a setting called
// "Device Tag". This settings determines how the controller get's its
// nickname or what I want to officially call device tag. On default this is
// set to "none(auto)" and the controller get's a random name every time. The
// user can write in something into this setting and that controller will
// always get that tag."* Then *"3 defaults or custom for device tag: none(auto),
// Device Type - ei "DualSense", Serial/Mac"*, *"If there are two of the same
// tag, add (2), (3) etc."*, *"change right away"* and *"12 is fine for device
// type that are longer just cut them off"*; the choice reads random.
//
// ⓘ Kept in the controller's own settings (controller_settings.inl), as its
// rate limit, audio and rumble are: a choice, device_tag_mode, and the tag
// typed for Custom, device_tag. random is the name nickname.inl draws.
//
// ⭐ Pure on purpose -- no Windows call, no session -- so the test binary
// includes it as it is. agent.inl applies it (agent_apply_device_tag).

#pragma once

#include <cctype>
#include <string>
#include <vector>

namespace device_tag {

inline const char *const kModeKey = "device_tag_mode";
inline const char *const kCustomKey = "device_tag";

// ⭐ TWELVE AT MOST (rhoquinn8217, 2026-11-10), so it fits Quick's name line:
// a longer type or serial is cut off there.
inline const size_t kMax = 12;

enum class Mode { Random, Type, Serial, Custom };

// The choice as written; anything else, nothing included, is random.
inline Mode mode_of(const std::string &value)
{
    if (value == "type") return Mode::Type;
    if (value == "serial") return Mode::Serial;
    if (value == "custom") return Mode::Custom;
    return Mode::Random;
}

inline const char *mode_words(Mode mode)
{
    switch (mode) {
    case Mode::Type: return "its device type";
    case Mode::Serial: return "its serial";
    case Mode::Custom: return "its custom tag";
    default: return "random";
    }
}

inline std::string trim(const std::string &text)
{
    size_t a = 0, b = text.size();
    while (a < b && text[a] == ' ') ++a;
    while (b > a && text[b - 1] == ' ') --b;
    return text.substr(a, b - a);
}

// ⛔ PRINTABLE ASCII ONLY: the tray's menu and the log take a name as ASCII
// (agent.inl, widen_ascii), and the settings file refuses [ ] = # ; so those
// go too. Then trimmed, cut to twelve and trimmed again, so a cut never leaves
// a space at the end.
inline std::string clean(const std::string &raw)
{
    std::string kept;
    for (char c : raw) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (u < 0x20 || u > 0x7e) continue;
        if (c == '[' || c == ']' || c == '=' || c == '#' || c == ';') continue;
        kept.push_back(c);
    }
    kept = trim(kept);
    if (kept.size() > kMax) kept = trim(kept.substr(0, kMax));
    return kept;
}

// The type's name: the model its kind names (device_names.inl), else the name
// the TV sent for it, else the sort of device it is.
inline std::string type_name(const std::string &model, const std::string &product,
                             const std::string &deviceType)
{
    for (const std::string *name : {&model, &product, &deviceType}) {
        const std::string tag = clean(*name);
        if (!tag.empty()) return tag;
    }
    return std::string();
}

// The serial as it is kept (config_store::normalise_serial: a DualSense's MAC
// is twelve characters that way); "" when it has none.
inline std::string serial_tag(const std::string &normalisedSerial)
{
    return clean(normalisedSerial);
}

// The tag a choice gives, before a number for a twin; "" is random, as is a
// choice with nothing to go by (a custom tag never typed, no serial).
inline std::string base(Mode mode, const std::string &custom, const std::string &typeName,
                        const std::string &serialTag)
{
    switch (mode) {
    case Mode::Type: return clean(typeName);
    case Mode::Serial: return clean(serialTag);
    case Mode::Custom: return clean(custom);
    default: return std::string();
    }
}

inline std::string lower(const std::string &text)
{
    std::string out = text;
    for (char &c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

// ⭐ "(2)", "(3)" FOR A TWIN: the tag itself when no other device bridged now
// has it, else the lowest number free. ⓘ Told apart without case: Titan and
// titan would be said the same.
inline std::string unique(const std::string &tag, const std::vector<std::string> &taken)
{
    auto isTaken = [&](const std::string &name) {
        const std::string want = lower(name);
        for (const std::string &t : taken) {
            if (lower(t) == want) return true;
        }
        return false;
    };
    if (!isTaken(tag)) return tag;
    for (unsigned n = 2;; ++n) {
        const std::string name = tag + " (" + std::to_string(n) + ")";
        if (!isTaken(name)) return name;
    }
}

}  // namespace device_tag
