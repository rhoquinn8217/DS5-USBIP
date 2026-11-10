// A controller's device tag (device_tag_rule.inl).
//
// ⭐ WHAT THESE PROTECT (rhoquinn8217, 2026-11-10): a controller is called by
// a random name, its type, its serial or a custom tag, twelve characters at
// most with the rest cut off, and a second one with the same tag gets "(2)",
// a third "(3)".
//
// WHAT THESE CANNOT DO. No controller is bridged here: the agent's renaming at
// a bridge and as the settings window closes (agent_apply_device_tag,
// rest_device_tags_settle) is checked by hand.

#include "harness.h"

#include <string>
#include <vector>

#include "app/device_tag_rule.inl"

using namespace ctmtest;

int run_device_tag_rule_tests()
{
    using namespace device_tag;

    section("device tag: the choice as written, random for anything else");
    {
        CTM_CHECK(mode_of("") == Mode::Random);
        CTM_CHECK(mode_of("auto") == Mode::Random);
        CTM_CHECK(mode_of("type") == Mode::Type);
        CTM_CHECK(mode_of("serial") == Mode::Serial);
        CTM_CHECK(mode_of("custom") == Mode::Custom);
        CTM_CHECK(mode_of("Custom") == Mode::Random);
    }

    section("device tag: twelve characters at most, the rest cut off");
    {
        CTM_CHECK_EQ(type_name("DualSense Edge", "", ""), std::string("DualSense Ed"));
        CTM_CHECK_EQ(type_name("Xbox Controller", "", ""), std::string("Xbox Control"));
        CTM_CHECK_EQ(type_name("DualSense", "Wireless Controller", "controller"), std::string("DualSense"));
        CTM_CHECK_EQ(type_name("", "Wireless Controller", "controller"), std::string("Wireless Con"));
        CTM_CHECK_EQ(type_name("", "", "controller"), std::string("controller"));
        CTM_CHECK_EQ(type_name("", "", ""), std::string());
        // a DualSense's MAC as it is kept is twelve; a longer serial is cut;
        // both in capitals, "All caps the serical/mac"
        CTM_CHECK_EQ(serial_tag("a0fa9cef9b30"), std::string("A0FA9CEF9B30"));
        CTM_CHECK_EQ(serial_tag("63635e6bf4e30de12"), std::string("63635E6BF4E3"));
        CTM_CHECK_EQ(serial_tag(""), std::string());
        // trimmed, and never a space left at the end of a cut
        CTM_CHECK_EQ(clean("  Titan  "), std::string("Titan"));
        CTM_CHECK_EQ(clean("Hello World Again"), std::string("Hello World"));
        // what the settings file and the tray cannot hold goes
        CTM_CHECK_EQ(clean("Big [Boss]; #1="), std::string("Big Boss 1"));
        CTM_CHECK_EQ(clean("Se\xc3\xb1or"), std::string("Seor"));
        CTM_CHECK_EQ(clean("Tab\there"), std::string("Tabhere"));
    }

    section("device tag: what each choice gives");
    {
        CTM_CHECK_EQ(base(Mode::Random, "Titan", "DualSense", "a0fa9cef9b30"), std::string());
        CTM_CHECK_EQ(base(Mode::Type, "Titan", "DualSense", "a0fa9cef9b30"), std::string("DualSense"));
        CTM_CHECK_EQ(base(Mode::Serial, "Titan", "DualSense", serial_tag("a0fa9cef9b30")),
                     std::string("A0FA9CEF9B30"));
        // ⓘ only the serial: a type and a custom tag keep their own letters
        CTM_CHECK_EQ(base(Mode::Custom, "Titan", "DualSense", serial_tag("a0fa9cef9b30")), std::string("Titan"));
        CTM_CHECK_EQ(base(Mode::Custom, "Titan", "DualSense", "a0fa9cef9b30"), std::string("Titan"));
        CTM_CHECK_EQ(base(Mode::Custom, "A very long custom tag", "", ""), std::string("A very long"));
        // nothing to go by is random: a custom tag never typed, no serial
        CTM_CHECK_EQ(base(Mode::Custom, "   ", "DualSense", ""), std::string());
        CTM_CHECK_EQ(base(Mode::Serial, "", "DualSense", ""), std::string());
    }

    section("device tag: named again as the settings window closes, only if it changed");
    {
        // ⭐ 2026-11-10: "Don't change the tag until after the setting window is closed".
        CTM_CHECK(changed("", "", "type", ""));
        CTM_CHECK(changed("custom", "Titan", "auto", "Titan"));
        CTM_CHECK(changed("custom", "Titan", "custom", "Nova"));
        // random chosen again, or a tag typed and put back: no change
        CTM_CHECK(!changed("", "", "auto", ""));
        CTM_CHECK(!changed("auto", "", "", ""));
        CTM_CHECK(!changed("custom", "Titan", "custom", " Titan "));
        // the custom tag counts only under Custom
        CTM_CHECK(!changed("type", "Titan", "type", "Nova"));
        CTM_CHECK(!changed("serial", "", "serial", "Nova"));
    }

    section("device tag: (2), (3) for a twin, the lowest number free");
    {
        CTM_CHECK_EQ(unique("DualSense", {}), std::string("DualSense"));
        CTM_CHECK_EQ(unique("DualSense", {"Titan", "DualSense"}), std::string("DualSense (2)"));
        CTM_CHECK_EQ(unique("DualSense", {"DualSense", "DualSense (2)"}), std::string("DualSense (3)"));
        CTM_CHECK_EQ(unique("DualSense", {"DualSense", "DualSense (3)"}), std::string("DualSense (2)"));
        CTM_CHECK_EQ(unique("Xbox Control", {"Xbox Control"}), std::string("Xbox Control (2)"));
        // told apart without case: Titan and titan are said the same
        CTM_CHECK_EQ(unique("Titan", {"titan"}), std::string("Titan (2)"));
        CTM_CHECK_EQ(unique("Titan", {"Titans"}), std::string("Titan"));
    }

    return 0;
}
