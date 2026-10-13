// When the taskbar stays behind the Controller Configs window: a window that
// fills its screen behind it, as a borderless game does, and not the desktop
// or a taskbar, which fill it too.

#include "harness.h"

#include "app/taskbar_rule.inl"

using namespace ctmtest;
namespace tr = taskbar_rule;

int run_taskbar_rule_tests()
{
    section("taskbar: a window fills its screen edge to edge, or past it");
    {
        const tr::Box screen = { 0, 0, 1920, 1080 };
        CTM_CHECK(tr::fills({ 0, 0, 1920, 1080 }, screen));        // borderless, exactly
        CTM_CHECK(tr::fills({ -8, -8, 1928, 1088 }, screen));      // past each edge
        CTM_CHECK(!tr::fills({ -8, -8, 1928, 1048 }, screen));     // maximised, the taskbar showing
        CTM_CHECK(!tr::fills({ 400, 200, 1520, 880 }, screen));    // an ordinary window
        const tr::Box second = { 1920, 0, 4480, 1440 };            // a second screen, to the right
        CTM_CHECK(tr::fills({ 1920, 0, 4480, 1440 }, second));
        CTM_CHECK(!tr::fills({ 0, 0, 1920, 1080 }, second));       // full on the other screen only
    }

    section("taskbar: the desktop and the taskbars fill a screen and are no game");
    {
        CTM_CHECK(tr::is_shell(L"Progman"));
        CTM_CHECK(tr::is_shell(L"WorkerW"));
        CTM_CHECK(tr::is_shell(L"Shell_TrayWnd"));
        CTM_CHECK(tr::is_shell(L"Shell_SecondaryTrayWnd"));
        CTM_CHECK(!tr::is_shell(L"UnrealWindow"));
        CTM_CHECK(!tr::is_shell(L"Chrome_WidgetWin_1"));
    }

    section("taskbar: the configs window by its title, before and after the page marks it");
    {
        CTM_CHECK(tr::is_configs_title(L"DS5-USBIP Controller Configs [ctm-app]", L"[ctm-app]"));
        CTM_CHECK(tr::is_configs_title(L"DS5-USBIP Controller Configs", L"[ctm-app]"));                     // just opened
        CTM_CHECK(!tr::is_configs_title(L"DS5-USBIP Controller Configs - Google Chrome", L"[ctm-app]"));     // a tab
        CTM_CHECK(!tr::is_configs_title(L"DS5-USBIP Controller Configs [ctm-app] - Google Chrome", L"[ctm-app]"));
        CTM_CHECK(!tr::is_configs_title(L"Some Game", L"[ctm-app]"));
        CTM_CHECK(!tr::is_configs_title(nullptr, L"[ctm-app]"));
    }
    return 0;
}
