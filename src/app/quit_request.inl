// How another program asks DS5-USBIP to quit: the same Quit as the tray
// menu's, asked for with a message posted to the tray icon's window.
//
// ⭐ WHY (rhoquinn8217, 2026-10-12): the installer, upgrading or uninstalling
// a DS5-USBIP that is running, quits it itself, "the way the tray's Quit
// does", instead of telling the person to. Ending the process would not do:
// every controller it had bridged would stay attached to nothing, and the
// person would have to unplug and replug it. Quit releases them first.
//
// ⓘ A window message rather than the REST API: it needs no port, no token
// and no network, and only a program on this PC, in this person's session,
// can send it. The installer finds the window by its class and posts the
// message to it.
//
// ⚠️ installer/ds5-usbip.iss names both, as TrayClassName and
// QuitMessageName; tests/quit_request_test.cpp fails when the two disagree.

#pragma once

namespace quit_request {

// The tray icon's window class: app/tray_icon.inl registers it.
inline const wchar_t *const kTrayClass = L"CtmTrayIcon";

// The registered message that asks for Quit.
inline const wchar_t *const kMessageName = L"DS5-USBIP.Quit";

}  // namespace quit_request
