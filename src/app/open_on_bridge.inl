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

// ⭐ AND NOT AT A STREAM'S START (rhoquinn8217, 2026-10-12: "When the stream
// starts, before, nothing told us is was on. Now we have the badge and the
// the button in aurora-tv that become enabled when it's on. So on stream
// start, no controller configs window. However we still want the window to
// open when a controller is bridged unless it's the setting in the tray menu
// or controller configs window to not open on bridge. This is default to
// open on bridge."). Vibepollo's start runs the listener with
// --no-start-window (stream_start.inl): its tray icon and its API as --ui
// gives them, and the window only on a bridge, by opens_window above.
//
// Whether the listener opens the window as it starts: it has one at all
// (--ui), it was not told to start without (--no-start-window), and none was
// brought forward already.
inline bool opens_at_start(bool uiEnabled, bool noStartWindow, bool alreadyFocused)
{
    return uiEnabled && !noStartWindow && !alreadyFocused;
}

}  // namespace open_on_bridge
