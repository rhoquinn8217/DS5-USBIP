// How the installer asks a running DS5-USBIP to quit: it posts a message,
// named in app/quit_request.inl, to the tray icon's window, found by its
// class.
//
// ⭐ WHAT THIS PROTECTS. The two names are written twice, once in the listener
// and once in installer/ds5-usbip.iss, and nothing else would notice them
// drift apart: the installer would find no window, or post a message nobody
// listens for, and quietly fall back to asking the person to quit by hand.
//
// ⛔ IT READS THE INSTALLER'S SOURCE, so it runs before the scratch-directory
// switch in tests_main, like the binding-names suite.

#include "harness.h"

#include <fstream>
#include <sstream>
#include <string>

#include "app/quit_request.inl"

using namespace ctmtest;

namespace {

std::string narrow_ascii(const wchar_t *text)
{
    std::string out;
    for (; *text != 0; ++text) out.push_back(static_cast<char>(*text));
    return out;
}

// installer/ds5-usbip.iss, found from wherever the suite runs, the way the
// binding-names suite finds its source.
std::string installer_source()
{
    const char *candidates[] = {
        "installer/ds5-usbip.iss",
        "../installer/ds5-usbip.iss",
        "../../installer/ds5-usbip.iss",
        "../../../installer/ds5-usbip.iss",
        "../../../../installer/ds5-usbip.iss",
    };
    for (const char *where : candidates) {
        std::ifstream in(where, std::ios::binary);
        if (!in) continue;
        std::stringstream buffer;
        buffer << in.rdbuf();
        return buffer.str();
    }
    return std::string();
}

}  // namespace

int run_quit_request_tests()
{
    section("quit request: the installer names the listener's tray window and message");
    {
        const std::string iss = installer_source();
        CTM_CHECK(!iss.empty());
        const std::string trayClass =
            "#define TrayClassName \"" + narrow_ascii(quit_request::kTrayClass) + "\"";
        const std::string message =
            "#define QuitMessageName \"" + narrow_ascii(quit_request::kMessageName) + "\"";
        CTM_CHECK(iss.find(trayClass) != std::string::npos);
        CTM_CHECK(iss.find(message) != std::string::npos);
    }
    return 0;
}
