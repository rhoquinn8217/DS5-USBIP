// default_windows_config: the config a first install starts with, and the
// first controller's auto link to it.
//
// ⭐ rhoquinn8217, 2026-11-07: *"create a pre-set that matches the aurora-tv
// windows logging bindings except for triangle for the numpad. Leave triangle
// blank. Square for the virtual keyboard should use DS5-USBIP virtual keyboard
// instead. Also add two finger natural scrolling and left stick scrolling and
// hold R3 for gyro to mouse. default-config. I want this to be created for the
// users on install and upgrades if it doesn't already exist. Only the very first
// controller bridged after a fresh install(not upgrade) will get auto link to it
// automatically unless that controller is already auto link. The intention is to
// have first time installers already have something ready to go."*
// Then, the same day: *"change "default-config" to "default_windows_config""*,
// and *"Fresh mean anytime a install happens that isn't an upgrade. Fresh
// include reinstall if DS5-USBIP was uninstalled."*
//
// ➡️ So, three parts:
//   - the preset, default_windows_config, in config_presets.inl, offered with
//     the rest;
//   - a config of the same name made from it at the first start after any
//     install, fresh or not, when there is none (default_config.inl);
//   - after a FRESH install only, the first controller bridged that takes a
//     config is auto linked to it, through the config's own auto_link line, so
//     it holds at every bridge after, unless a config auto links it already.
//     Either way the first controller has had its turn, and no other gets one.
//
// ⓘ FRESH IS THE INSTALLER'S WORD: it found no copy installed as it started,
// so a first install or one after an uninstall. Over an installed copy it is
// an upgrade, whatever the versions. It says which on the line after the stamp
// in install-stamp.txt (installer/ds5-usbip.iss, CurStepChanged).
// ⓘ This file has no windows.h, so the test binary can reach it.
#pragma once

#include <cctype>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace default_config {

// The config, and the preset it is made from: their word.
inline const char *const kName = "default_windows_config";

// window-state.txt's line while the first controller is still to come, written
// only then: a file without it has had its first controller, or never will.
inline const char *const kFirstKey = "default_windows_config_first_controller";

inline std::string first_line() { return std::string(kFirstKey) + " = 1\n"; }
inline bool parse_first(const std::string &value) { return value == "1"; }

// install-stamp.txt's second line, as the installer writes it.
inline const char *const kInstallFresh = "install = fresh";
inline const char *const kInstallUpgrade = "install = upgrade";

// Whether that line says fresh. ⓘ No line at all, from an installer before
// this one, is not fresh: nothing says it was not an upgrade.
inline bool installer_says_fresh(const std::string &line)
{
    std::string s = line.substr(0, line.find_first_of("\r\n"));
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(s.begin());
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
    return s == kInstallFresh;
}

// A fresh install's first start: a stamp not seen before, and the installer's
// word that it found no copy installed.
inline bool fresh(bool stampNew, bool installerSaysFresh) { return stampNew && installerSaysFresh; }

// What a bridged device does with the first controller's turn.
//   None   not its turn: none is waiting, or the device takes no config (a
//          keyboard's half of a receiver), which leaves the turn for a
//          controller;
//   Leave  a config auto links it already: it keeps that, and the turn is over;
//   Claim  auto linked to default_windows_config, and the turn is over.
enum class First { None, Leave, Claim };

inline First first_controller(bool waiting, bool takesConfig, bool alreadyAutoLinked)
{
    if (!waiting || !takesConfig) return First::None;
    return alreadyAutoLinked ? First::Leave : First::Claim;
}

// ⭐ A COPY MADE BEFORE, BROUGHT UP TO DATE (rhoquinn8217, 2026-11-12, asked
// whether a default_windows_config made before should change with an update:
// "2. a"): one nobody edited, still reading exactly the preset as it shipped,
// is brought to the preset now; one that differs is left as it is, and the
// log says so. `have`: the copy's settings as loaded, a blank value counting
// as none; `now` and `before`: the preset now and as it shipped, key to value.
// Keys and values compared without case, as the config reader keeps them.
enum class Copy { Current, Unedited, Edited };

using Pairs = std::vector<std::pair<std::string, std::string>>;

inline std::map<std::string, std::string> folded(const Pairs &pairs)
{
    std::map<std::string, std::string> out;
    for (const auto &p : pairs) {
        if (p.second.empty()) continue;
        std::string k, v;
        for (char c : p.first) k.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        for (char c : p.second) v.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        out[k] = v;
    }
    return out;
}

inline Copy copy_state(const Pairs &have, const Pairs &now, const Pairs &before)
{
    const std::map<std::string, std::string> h = folded(have);
    if (h == folded(now)) return Copy::Current;
    if (h == folded(before)) return Copy::Unedited;
    return Copy::Edited;
}

// The keys the preset had as it shipped and has no more, as they were
// written: what bringing an unedited copy up to date clears.
inline std::vector<std::string> dropped_keys(const Pairs &now, const Pairs &before)
{
    const std::map<std::string, std::string> n = folded(now);
    std::vector<std::string> out;
    for (const auto &p : before) {
        std::string k;
        for (char c : p.first) k.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        if (!p.second.empty() && n.find(k) == n.end()) out.push_back(p.first);
    }
    return out;
}

// ⭐ THE CONFIG A BRIDGE READS AS IT STARTS. Desktop Mode handing over
// (protocol_level::desktop_link, 2026-11-12: "The transition between aurora-tv
// managing controllers and DS5-USBIP managing controllers needs to be
// seemless"): default_windows_config, which does what Desktop Mode did, for
// this bridge alone; the controller's own auto link is not touched, and is
// what it reads at its next bridge without the word. Otherwise its auto link,
// or else the first controller's link for this bridge (first_controller).
// ⓘ A hand-over with no default_windows_config (archived, or deleted) takes
// the auto link, as a bridge without the word does.
enum class StartWhy { None, AutoLink, FirstController, DesktopMode };

struct StartLink {
    std::string config;            // "" for none: the controller's own buttons
    bool thisBridgeOnly = false;   // not its auto link: gone with the bridge
    StartWhy why = StartWhy::None;
};

inline StartLink start_link(bool handedOver, bool defaultThere, const std::string &autoLink,
                            const std::string &firstLink)
{
    StartLink s;
    if (handedOver && defaultThere) {
        s.config = kName;
        s.thisBridgeOnly = true;
        s.why = StartWhy::DesktopMode;
    } else if (!autoLink.empty()) {
        s.config = autoLink;
        s.why = StartWhy::AutoLink;
    } else if (!firstLink.empty()) {
        s.config = firstLink;
        s.thisBridgeOnly = true;
        s.why = StartWhy::FirstController;
    }
    return s;
}

} // namespace default_config
