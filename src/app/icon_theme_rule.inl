// Which of the app's two icons a surface gets, by Windows' light or dark mode.
//
// ⭐⭐ TWO ICONS (rhoquinn8217, 2026-11-05: a white base on a dark background,
// a black one on a light background, then the light one's colours swapped for
// the dark one). installer\build-icon.ps1 draws both: app\ctm-usbip.ico for
// light backgrounds and app\ctm-usbip-dark.ico for dark ones.
//
// ⓘ The rule is its own file so the tests can reach it; icon_theme.inl reads
// the mode from Windows.
// ⛔ NOTHING HERE MAY INCLUDE windows.h.

#pragma once

namespace icon_theme_rule {

// ⚠️ THE SAME NUMBERS AS IN app\ctm-usbip.rc.
// The light-background one is resource 1, the exe's own, so it is what File
// Explorer, the Start menu and shortcuts show (their word: "bottom").
inline const int kIconLight = 1;
inline const int kIconDark = 2;

// ⭐ A MODE NEVER WRITTEN IS LIGHT. Windows' two values, SystemUsesLightTheme
// and AppsUseLightTheme, are 1 for light and 0 for dark, and a profile that
// never chose has neither: Windows shows it light. So only a value that was
// read AND is 0 means dark.
inline bool is_light(bool found, unsigned long value)
{
    return !(found && value == 0);
}

inline int icon_for(bool light)
{
    return light ? kIconLight : kIconDark;
}

}  // namespace icon_theme_rule
