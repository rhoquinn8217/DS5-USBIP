// Whether a bridge brings the settings window up by itself, and how that one
// setting is kept.
//
// ⭐ rhoquinn8217, 2026-10-09: *"Add a toggle button in the overview after
// bearer input that "Open on bridge". Add a toggle button, "Open on bridge",
// menu item that is checked or unchecked under "window mode menu" item"*.
// Every bridge opened the window, on purpose: it confirms that the device
// asked for has bridged (2026-09-15). Now it is a switch, and the tray menu
// and the settings page both set it.
//
// ⭐⭐ OFF UNTIL IT IS TURNED ON, SINCE 2026-10-15 (rhoquinn8217, making a
// DualSense's setup as seamless as possible: "DS5-USBIP controller configs
// open on bridge off", and "off for all"). The TV bridges a DualSense by
// itself now, at a stream's start and whenever one connects, and a window at
// each of those would be in the way. ⓘ For an existing install too, once:
// see loaded() below.
//
// ⓘ Kept in window-state.txt with the window's place and size, as
// "open_on_bridge = 0" or "= 1", under the line "open_on_bridge_default =
// off". That file is safe to delete, and with it gone the switch is off.
// ⓘ Its own file so the test binary can reach it.
#pragma once

#include <string>

namespace open_on_bridge {

inline const char *const kKey = "open_on_bridge";

// The switch's default: off (2026-10-15; on before).
inline constexpr bool kDefault = false;

// ⭐ The line every save writes since the default became off. A file without
// it was written when the default was on, so its "open_on_bridge = 1" said
// only that, and is dropped once (loaded() below).
inline const char *const kDefaultKey = "open_on_bridge_default";

// The value a window-state.txt line gives: on only when it says 1. Off for 0,
// and for anything else, which is junk in a hand-editable file.
inline bool parse(const std::string &value)
{
    return value == "1";
}

// The line window-state.txt keeps it in.
inline std::string line(bool on)
{
    return std::string(kKey) + " = " + (on ? "1" : "0") + "\n";
}

// The line that says the file was written since the default became off.
inline std::string default_line()
{
    return std::string(kDefaultKey) + " = off\n";
}

// The switch a whole window-state.txt gives: its own line, but only in a file
// that also has the default's line. Anything else is the default, off: a file
// written before 2026-10-15 included, which is how an existing install is
// turned off once.
inline bool loaded(bool sawLine, bool lineOn, bool sawDefaultLine)
{
    return sawDefaultLine && sawLine ? lineOn : kDefault;
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
