// default-config: the config a first install starts with, and the first
// controller's auto link to it.
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
//
// ➡️ So, three parts:
//   - the preset, default-config, in config_presets.inl, offered with the rest;
//   - a config of the same name made from it at the first start after any
//     install, fresh or not, when there is none (default_config.inl);
//   - after a FRESH install only, the first controller bridged that takes a
//     config is auto linked to it, through the config's own auto_link line, so
//     it holds at every bridge after, unless a config auto links it already.
//     Either way the first controller has had its turn, and no other gets one.
//
// ⓘ A FRESH install is the first start after an install with no
// window-state.txt behind it: this person has never run DS5-USBIP. An upgrade,
// or an install over a copy that ran, has one. ⓘ The stamp alone cannot say:
// the installer writes one every time it runs (install-stamp.txt).
// ⓘ This file has no windows.h, so the test binary can reach it.
#pragma once

#include <string>

namespace default_config {

// The config, and the preset it is made from: their word, "default-config".
inline const char *const kName = "default-config";

// window-state.txt's line while the first controller is still to come, written
// only then: a file without it has had its first controller, or never will.
inline const char *const kFirstKey = "default_config_first_controller";

inline std::string first_line() { return std::string(kFirstKey) + " = 1\n"; }
inline bool parse_first(const std::string &value) { return value == "1"; }

// A fresh install: a stamp not seen before, and no window state from a run
// before it.
inline bool fresh(bool stampNew, bool stateFileFound) { return stampNew && !stateFileFound; }

// What a bridged device does with the first controller's turn.
//   None   not its turn: none is waiting, or the device takes no config (a
//          keyboard's half of a receiver), which leaves the turn for a
//          controller;
//   Leave  a config auto links it already: it keeps that, and the turn is over;
//   Claim  auto linked to default-config, and the turn is over.
enum class First { None, Leave, Claim };

inline First first_controller(bool waiting, bool takesConfig, bool alreadyAutoLinked)
{
    if (!waiting || !takesConfig) return First::None;
    return alreadyAutoLinked ? First::Leave : First::Claim;
}

} // namespace default_config
