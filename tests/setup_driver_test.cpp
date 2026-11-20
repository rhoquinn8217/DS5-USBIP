// The setup's driver step (installer/ds5-usbip.iss).
//
// ⭐ WHAT THESE PROTECT (code review, 2026-11-01). usbip-win2's own setup ran
// from [Run], where its exit code is never read: a driver that failed to
// install finished like one that worked, and DS5-USBIP then bridged nothing
// with nothing on screen to say why. And a usbip.exe whose driver was gone was
// taken for a good install and never repaired. The setup itself can only be
// run on a PC; these keep the two fixes from being undone.
// ⓘ And the setup keeps a log of its own (2026-11-03): a clean PC's install
// wrote none without /LOG, so the exit code it read was then lost.

#include "harness.h"

#include <fstream>
#include <sstream>
#include <string>

using namespace ctmtest;

namespace {

// A file of the repo, found from wherever the suite runs, the way the
// quit-request suite finds the installer.
std::string repo_text(const std::string &path)
{
    const char *prefixes[] = {"", "../", "../../", "../../../", "../../../../"};
    for (const char *prefix : prefixes) {
        std::ifstream in(prefix + path, std::ios::binary);
        if (!in) continue;
        std::stringstream buffer;
        buffer << in.rdbuf();
        return buffer.str();
    }
    return std::string();
}

bool has(const std::string &text, const std::string &part)
{
    return text.find(part) != std::string::npos;
}

// Whether a file of the repo is there, found the way repo_text finds one.
bool repo_has(const std::string &path)
{
    const char *prefixes[] = {"", "../", "../../", "../../../", "../../../../"};
    for (const char *prefix : prefixes) {
        if (std::ifstream(prefix + path, std::ios::binary)) return true;
    }
    return false;
}

}  // namespace

int run_setup_driver_tests()
{
    const std::string iss = repo_text("installer/ds5-usbip.iss");

    section("setup: the driver's own setup runs from code that reads its exit code");
    {
        CTM_CHECK(!iss.empty());
        // Not from [Run], where no exit code is read.
        CTM_CHECK(!has(iss, "Filename: \"{tmp}\\{#UsbipInstaller}\""));
        CTM_CHECK(has(iss, "Exec(ExpandConstant('{tmp}\\{#UsbipInstaller}')"));
        CTM_CHECK(has(iss, "GUsbipFailed := Code <> 0;"));
        CTM_CHECK(has(iss, "  InstallUsbip();"));
        // And a failure is said, not only logged.
        CTM_CHECK(has(iss, "if GUsbipFailed then\n    SuppressibleMsgBox("));
    }

    section("setup: a usbip.exe whose driver is gone is installed again");
    {
        CTM_CHECK(has(iss, "RegKeyExists(HKLM, 'SYSTEM\\CurrentControlSet\\Services\\usbip2_ude')"));
    }

    section("setup: it writes a log of its own, so the driver step's exit code is kept");
    {
        CTM_CHECK(has(iss, "\nSetupLogging=yes\n"));
    }

    section("setup: the log says whether the driver's setup runs or is skipped");
    {
        CTM_CHECK(has(iss, "Log('usbip-win2: missing or broken, its setup will run')"));
        CTM_CHECK(has(iss, "Log('usbip-win2: already installed, its setup is skipped')"));
    }

    // ⭐ (rhoquinn8217, 2026-11-19): Vibepollo installs usbip-win2's driver
    // alone, with no usbip.exe, and usbip-win2's setup run over it made a second
    // host controller with two builds of the driver mixed: every attach failed.
    section("setup: a driver another installer put there is left alone");
    {
        CTM_CHECK(has(iss, "Where-Object HardwareID -Contains ''ROOT\\USBIP_WIN2\\UDE''"));
        CTM_CHECK(has(iss, "GInstallUsbip := not UsbipControllerPresent();"));
        CTM_CHECK(!has(iss, "GInstallUsbip := (not FileExists("));
    }

    section("setup: usbip.exe is supplied where the driver came without it");
    {
        CTM_CHECK(has(iss, "DestDir: \"{app}\\usbip-win2\"; Flags: ignoreversion; Check: UsbipToolsNeeded"));
        const std::string cli = repo_text("src/app/cli.inl");
        const size_t own = cli.find("L\"C:\\\\Program Files\\\\USBip\\\\usbip.exe\"");
        const size_t copy = cli.find("L\"usbip-win2\\\\usbip.exe\"");
        CTM_CHECK(own != std::string::npos);
        CTM_CHECK(copy != std::string::npos);
        // usbip-win2's own usbip.exe, which matches its driver, before the copy.
        CTM_CHECK(own < copy);
    }

    // ⭐ (a clean Windows 11 VM, 2026-11-19): a clean Windows has none of the
    // Microsoft runtime DLLs usbip.exe loads, and 0.9.8.1's usbip.exe loads one
    // that 0.9.7.7's did not (msvcp140_atomic_wait.dll). It also loads
    // resources.dll by name. The copy the setup supplies starts only with every
    // one of them beside it.
    section("setup: the usbip.exe it supplies has everything it loads beside it");
    {
        const std::string tools = "third_party/usbip-win2/tools/";
        CTM_CHECK(repo_has(tools + "usbip.exe"));
        CTM_CHECK(repo_has(tools + "libusbip.dll"));
        CTM_CHECK(repo_has(tools + "resources.dll"));
        CTM_CHECK(repo_has(tools + "msvcp140.dll"));
        CTM_CHECK(repo_has(tools + "msvcp140_atomic_wait.dll"));
        CTM_CHECK(repo_has(tools + "vcruntime140.dll"));
        CTM_CHECK(repo_has(tools + "vcruntime140_1.dll"));
    }

    return 0;
}
