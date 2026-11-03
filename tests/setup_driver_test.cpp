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

    return 0;
}
