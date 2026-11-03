// The licences the setup installs: DS5-USBIP's own, and THIRD-PARTY-NOTICES.txt,
// which names every part the setup carries that is someone else's.
//
// ⭐ WHAT THIS PROTECTS. A setup with no licence in it builds and installs like
// any other, and a part that moved on would leave the notices naming what no
// longer ships. Nothing else would notice either.
//
// ⛔ IT READS THE REPOSITORY'S FILES, so it runs before the scratch-directory
// switch in tests_main, like the quit request suite.

#include "harness.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using namespace ctmtest;

namespace {

// The repository's root, found from wherever the suite runs, the way the quit
// request suite finds the installer.
std::string repo_root()
{
    const char *candidates[] = { ".", "..", "../..", "../../..", "../../../.." };
    for (const char *where : candidates) {
        if (std::filesystem::exists(std::string(where) + "/installer/ds5-usbip.iss")) return where;
    }
    return std::string();
}

std::string read_file(const std::string &path)
{
    std::ifstream in(path, std::ios::binary);
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

bool has(const std::string &text, const std::string &part)
{
    return text.find(part) != std::string::npos;
}

}  // namespace

int run_licence_files_tests()
{
    const std::string root = repo_root();
    const std::string iss = read_file(root + "/installer/ds5-usbip.iss");
    const std::string notices = read_file(root + "/THIRD-PARTY-NOTICES.txt");

    section("licence files: the setup installs them, and they are there");
    {
        CTM_CHECK(!iss.empty());
        CTM_CHECK(has(iss, "LICENSE\"; DestDir: \"{app}\"; DestName: \"LICENSE.txt\""));
        CTM_CHECK(has(iss, "THIRD-PARTY-NOTICES.txt\"; DestDir: \"{app}\""));
        CTM_CHECK(has(iss, "DestName: \"usbip-win2-LICENSE.txt\""));
        CTM_CHECK(has(read_file(root + "/LICENSE"), "GNU GENERAL PUBLIC LICENSE"));
        CTM_CHECK(has(notices, "the GNU General Public License, version 3"));
    }

    section("licence files: the notices name the SBC encoder and carry its licence in full");
    {
        CTM_CHECK(has(notices, "SBC encoder from FFmpeg 8.0: GNU Lesser General Public License, version 2.1"));
        std::string lgpl = read_file(root + "/third_party/sbc/LICENSE.txt");
        CTM_CHECK(lgpl.size() > 1000);
        while (!lgpl.empty() && lgpl.back() == '\n') lgpl.pop_back();
        CTM_CHECK(has(notices, lgpl));
    }

    section("licence files: the notices name nothing that was in FFmpeg's DLLs");
    {
        for (const char *gone : {"avcodec-62", "avutil", "swresample", "OpenSSL", "libaom", "zlib"}) {
            CTM_CHECK(!has(notices, gone));
        }
    }

    section("licence files: the notices name the usbip-win2 the setup carries");
    {
        const std::string before = "#define UsbipInstaller \"USBip-";
        const size_t at = iss.find(before);
        CTM_CHECK(at != std::string::npos);
        if (at != std::string::npos) {
            const size_t from = at + before.size();
            const std::string version = iss.substr(from, iss.find("-x64", from) - from);
            CTM_CHECK(has(notices, "usbip-win2 " + version + ":"));
        }
    }
    return 0;
}
