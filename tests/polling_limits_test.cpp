// A controller's own polling limit (config/polling_limits.inl): kept by its
// serial, by the rule auto link uses, and never by a config.

#include "harness.h"
#include "units.h"

namespace units {
#include "config/polling_limits.inl"
}  // namespace units

using namespace ctmtest;
namespace pl = units::polling_limits;

namespace {

// In memory, empty, and read: what each section starts from.
void fresh()
{
    pl::g_limits.clear();
    pl::g_loaded = true;
    pl::g_path.clear();
}

}  // namespace

int run_polling_limits_tests()
{
    const std::string keepPath = pl::g_path;

    section("polling limits: a controller keeps its own, by its serial, whatever punctuation");
    {
        fresh();
        CTM_CHECK(pl::set("14:3A:9A:CB:F6:9D", "250"));
        CTM_CHECK_EQ(pl::get("143a9acbf69d"), std::string("250"));
        CTM_CHECK_EQ(pl::get("14-3a-9a-cb-f6-9d"), std::string("250"));
        // ⓘ Another controller has none: Real time.
        CTM_CHECK_EQ(pl::get("a0:fa:9c:ef:9b:30"), std::string());
    }

    section("polling limits: Real time is no limit, kept as no line at all");
    {
        fresh();
        CTM_CHECK(pl::set("143a9acbf69d", "125"));
        CTM_CHECK(pl::set("143a9acbf69d", "real_time"));
        CTM_CHECK_EQ(pl::get("143a9acbf69d"), std::string());
        CTM_CHECK(pl::g_limits.empty());
        CTM_CHECK(pl::set("143a9acbf69d", "500"));
        CTM_CHECK(pl::set("143a9acbf69d", ""));         // nothing is Real time too
        CTM_CHECK(pl::g_limits.empty());
    }

    section("polling limits: only the page's choices, and only for a controller that can be named");
    {
        fresh();
        CTM_CHECK(!pl::set("143a9acbf69d", "1000"));
        CTM_CHECK(!pl::set("143a9acbf69d", "25"));
        CTM_CHECK(!pl::set("143a9acbf69d", "max"));
        CTM_CHECK(pl::g_limits.empty());
        // ⛔ All zeros is no identity, as for auto link: a dongle that reports
        // zeros would hand its limit to the next one of its kind.
        CTM_CHECK(!pl::has_identity("000000000000"));
        CTM_CHECK(!pl::set("00:00:00:00:00:00", "250"));
        CTM_CHECK(!pl::has_identity(""));
        CTM_CHECK(!pl::set("", "250"));
        CTM_CHECK(pl::g_limits.empty());
        // ⓘ Leading zeros are an identity (a Switch Pro reports 000000000001).
        CTM_CHECK(pl::has_identity("000000000001"));
    }

    section("polling limits: the serial rule is auto link's own, character for character");
    {
        const char *samples[] = { "14:3A:9A:CB:F6:9D", "AbC-123_xyz", "000000000000",
                                  "000000000001", "", "  spaced  out  ",
                                  "0123456789abcdef0123456789abcdef0123" };
        for (const char *s : samples) {
            CTM_CHECK_EQ(pl::has_identity(s), !units::config_store::normalise_serial(s).empty());
        }
    }

    section("polling limits: the file round trip, and junk in it is passed over");
    {
        std::map<std::string, std::string> limits;
        limits["143a9acbf69d"] = "250";
        limits["a0fa9cef9b30"] = "125";
        const std::string text = pl::format(limits);
        CTM_CHECK(pl::parse(text) == limits);
        const std::string junk =
            "# a comment\r\n"
            "143A9ACBF69D = 500\r\n"          // upper case and CRLF: still that controller
            "no equals sign here\n"
            "abcdef = 1000\n"                  // not a choice: no limit, never a slow pad
            "000000000000 = 250\n"             // no identity
            "  a0:fa:9c:ef:9b:30\t=\t125  \n";
        const std::map<std::string, std::string> read = pl::parse(junk);
        CTM_CHECK_EQ(read.size(), static_cast<size_t>(2));
        CTM_CHECK_EQ(read.at("143a9acbf69d"), std::string("500"));
        CTM_CHECK_EQ(read.at("a0fa9cef9b30"), std::string("125"));
    }

    section("polling limits: written when set, and read back by the next listener");
    {
        const std::filesystem::path file =
            std::filesystem::temp_directory_path() / "ctm-polling-limits-test.txt";
        std::error_code ec;
        std::filesystem::remove(file, ec);
        pl::g_limits.clear();
        pl::g_loaded = false;
        pl::g_path = file.string();
        CTM_CHECK(pl::set("14:3a:9a:cb:f6:9d", "250"));
        CTM_CHECK(std::filesystem::exists(file));
        // ⓘ A listener starting again: nothing in memory, the file read once.
        pl::g_limits.clear();
        pl::g_loaded = false;
        CTM_CHECK_EQ(pl::get("143a9acbf69d"), std::string("250"));
        std::filesystem::remove(file, ec);
    }

    pl::g_limits.clear();
    pl::g_loaded = false;
    pl::g_path = keepPath;
    return 0;
}
