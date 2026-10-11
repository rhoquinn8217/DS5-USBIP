// Whether a bridge brings the settings window up by itself, and how that one
// setting is kept.
//
// ⭐ rhoquinn8217, 2026-10-09: *"Add a toggle button in the overview after
// bearer input that "Open on bridge". Add a toggle button, "Open on bridge",
// menu item that is checked or unchecked under "window mode menu" item"*.
// Every bridge opened the window, on purpose: it confirms that the device
// asked for has bridged (2026-09-15). Now it is a switch, ON until it is
// turned off, and the tray menu and the settings page both set it.
//
// ⓘ Kept in window-state.txt with the window's place and size, as
// "open_on_bridge = 0" or "= 1". That file is safe to delete, and with it gone
// the switch is back on, which is what it was before there was one.
// ⓘ Its own file so the test binary can reach it.
#pragma once

#include <string>

namespace open_on_bridge {

inline const char *const kKey = "open_on_bridge";

// The value a window-state.txt line gives: off only when it says 0. On for 1,
// and for anything else, which is junk in a hand-editable file.
inline bool parse(const std::string &value)
{
    return value != "0";
}

// The line window-state.txt keeps it in.
inline std::string line(bool on)
{
    return std::string(kKey) + " = " + (on ? "1" : "0") + "\n";
}

// Whether a bridge opens the window: the listener runs with a window at all
// (--ui), the switch is on, and the window is not already in front -- where
// someone is using it, and rebuilding it would throw away what they were doing.
inline bool opens_window(bool uiEnabled, bool switchOn, bool windowInFront)
{
    return uiEnabled && switchOn && !windowInFront;
}

}  // namespace open_on_bridge
