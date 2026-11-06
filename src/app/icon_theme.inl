// Windows' light or dark mode, read where the app's icons need it.
//
// ⓘ WINDOWS KEEPS TWO MODES, and a person can set them apart ("Choose your
// mode: Custom"): SystemUsesLightTheme for the taskbar, the notification area
// and the Start menu, AppsUseLightTheme for apps' own windows.
// - The tray icon and the configs window's taskbar button both sit on the
//   taskbar, so both follow the FIRST (tray_icon.inl, window_icon.inl).
// - The window's title bar is the browser's and follows the SECOND, which the
//   page reads for itself as prefers-color-scheme and answers by swapping its
//   favicon (page 2.66.136; /favicon-dark.ico, ui_page.inl).
// ⛔ The exe's own icon follows neither: File Explorer, the Start menu,
// shortcuts and the setup read resource 1 from the file, which is the
// light-background one at rhoquinn8217's word.
// 🔗 icon_theme_rule.inl for the rule and the resource numbers.

#pragma once

namespace icon_theme {

inline bool read_light(const wchar_t *name)
{
    DWORD value = 1;
    DWORD size = sizeof(value);
    const LSTATUS rc = RegGetValueW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        name, RRF_RT_REG_DWORD, nullptr, &value, &size);
    return icon_theme_rule::is_light(rc == ERROR_SUCCESS, value);
}

// The taskbar's mode, and the icon drawn for it.
inline bool taskbar_is_light()
{
    return read_light(L"SystemUsesLightTheme");
}

inline int taskbar_icon()
{
    return icon_theme_rule::icon_for(taskbar_is_light());
}

// What a log line calls each icon.
inline const wchar_t *icon_words(int id)
{
    return id == icon_theme_rule::kIconDark ? L"the one for a dark taskbar"
                                            : L"the one for a light taskbar";
}

}  // namespace icon_theme
