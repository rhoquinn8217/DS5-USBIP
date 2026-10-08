// The shared config's sections: which one a request means, and who reads each.
//
// ⭐ WHAT THESE PROTECT. The REST API listed every unlinked device as reading
// the shared file's [ds5] section, so an unlinked DS4, Edge or Xbox pad, and
// every keyboard and mouse, was reported as reading DualSense values it never
// reads. The rule that decides who reads which section is pinned here.
//
// WHAT THESE CANNOT DO. They build no JSON. That the endpoint serves these
// sections, with these devices under each, is checked against a running
// listener with real pads bridged.

#include "harness.h"
#include "units.h"

namespace units {
#include "app/rest_shared_section.inl"
}  // namespace units

using namespace ctmtest;
using namespace units;

int run_rest_shared_section_tests()
{
    section("shared config: an unlinked device reads its own kind's section");
    {
        CTM_CHECK(rest_shared::reads_section("", "ds5_usb", "ds5"));
        CTM_CHECK(rest_shared::reads_section("", "ds5", "ds5"));
        CTM_CHECK(rest_shared::reads_section("", "ds4_usb", "ds4"));
        CTM_CHECK(rest_shared::reads_section("", "ds5e", "ds5_edge"));
        CTM_CHECK(rest_shared::reads_section("", "xbox", "xbox"));
    }

    section("shared config: and no other kind's, which is what the old rule said");
    {
        // ⛔ Every one of these was TRUE: any unlinked device counted as [ds5].
        CTM_CHECK(!rest_shared::reads_section("", "ds4_usb", "ds5"));
        CTM_CHECK(!rest_shared::reads_section("", "ds5e_usb", "ds5"));
        CTM_CHECK(!rest_shared::reads_section("", "xbox", "ds5"));
    }

    section("shared config: a keyboard, a mouse or another part reads no section");
    {
        for (const char *section : rest_shared::kSections) {
            CTM_CHECK(!rest_shared::reads_section("", "hid", section));
            CTM_CHECK(!rest_shared::reads_section("", "puck", section));
        }
    }

    section("shared config: a device with a config linked reads none of it");
    {
        CTM_CHECK(!rest_shared::reads_section("razor_feel", "ds5_usb", "ds5"));
        CTM_CHECK(!rest_shared::reads_section("razor_feel", "ds4_usb", "ds4"));
    }

    section("shared config: ?kind= takes a session kind or a section name");
    {
        CTM_CHECK_EQ(rest_shared::section_for("ds5_usb"), std::string("ds5"));
        CTM_CHECK_EQ(rest_shared::section_for("ds4_usb"), std::string("ds4"));
        CTM_CHECK_EQ(rest_shared::section_for("ds5e"), std::string("ds5_edge"));
        CTM_CHECK_EQ(rest_shared::section_for("ds5_edge"), std::string("ds5_edge"));
        CTM_CHECK_EQ(rest_shared::section_for("xbox"), std::string("xbox"));
        CTM_CHECK_EQ(rest_shared::section_for("hid"), std::string());
        CTM_CHECK_EQ(rest_shared::section_for("nonsense"), std::string());
        CTM_CHECK_EQ(rest_shared::section_for(""), std::string());
    }

    section("shared config: one query value, by its whole name");
    {
        CTM_CHECK_EQ(rest_shared::query_value("kind=ds4", "kind"), std::string("ds4"));
        CTM_CHECK_EQ(rest_shared::query_value("a=1&kind=xbox", "kind"), std::string("xbox"));
        CTM_CHECK_EQ(rest_shared::query_value("kind=xbox&a=1", "kind"), std::string("xbox"));
        CTM_CHECK_EQ(rest_shared::query_value("kinda=ds4", "kind"), std::string());
        CTM_CHECK_EQ(rest_shared::query_value("x=kind=ds4", "kind"), std::string());
        CTM_CHECK_EQ(rest_shared::query_value("kind=", "kind"), std::string());
        CTM_CHECK_EQ(rest_shared::query_value("", "kind"), std::string());
    }

    return 0;
}
