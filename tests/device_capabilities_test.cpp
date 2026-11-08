// What a pad is OFFERED, decided from its settings kind. ⛔ A setting that
// reports success and does nothing reads as broken, and the reader blames the
// feature rather than the pad -- so this is the rule that keeps a section off a
// controller that cannot use it, and ON one that can.
//
// ⚠️ WHAT THESE CANNOT DO. They cannot hear a DS4's speaker or feel its motors.
// They protect the RULE: that the list of pads with audio is a list of pads
// with speakers, that it did not quietly widen to everything, and that rumble
// gains stayed behind when audio moved.

#include "harness.h"

#include <string>

#include "app/device_capabilities.inl"

using namespace ctmtest;

int run_device_capabilities_tests()
{
    section("capabilities: audio for a DualSense and a DS4, and no other pad");
    {
        CTM_CHECK(ctm_caps::has_audio_hardware("ds5"));
        CTM_CHECK(ctm_caps::has_audio_hardware("ds5_edge"));

        // ⭐ A DS4 SINCE 2026-11-04. It was refused on 2026-09-20 because its
        // settings arrived and did nothing; arriving is not acting. They act
        // now: its volumes over Bluetooth since the TV sends them itself, and
        // on a cable on its own sound card. The rows that still do nothing on
        // a DS4 are hidden by the page, not by this flag.
        CTM_CHECK(ctm_caps::has_audio_hardware("ds4"));

        // ⛔ And the pads that never had the hardware at all.
        CTM_CHECK(!ctm_caps::has_audio_hardware("xbox"));
        CTM_CHECK(!ctm_caps::has_audio_hardware("hid"));
        CTM_CHECK(!ctm_caps::has_audio_hardware("puck"));
        // ⚠️ settings_kind_for() answers EMPTY for a kind that carries no
        // config at all. Empty must not fall through to "has everything".
        CTM_CHECK(!ctm_caps::has_audio_hardware(""));
    }

    section("capabilities: the rumble dials on a DualSense's report and a DS4's");
    {
        // ⛔ The DualSense's are patched by ds5_output_overrides.inl, behind
        // `if (data[0] != 0x02) return;` -- its wired report id. ⭐ A DS4's by
        // ds4_output_overrides.inl on its own report, 0x05 (2026-11-04).
        CTM_CHECK(ctm_caps::has_rumble_gains("ds5"));
        CTM_CHECK(ctm_caps::has_rumble_gains("ds5_edge"));
        CTM_CHECK(ctm_caps::has_rumble_gains("ds4"));
        // ⛔ An Xbox pad's motors are not patched by either: still none.
        CTM_CHECK(!ctm_caps::has_rumble_gains("xbox"));
        CTM_CHECK(!ctm_caps::has_rumble_gains(""));
    }

    section("capabilities: the Edge's Fn and back buttons, on an Edge alone");
    {
        CTM_CHECK(ctm_caps::has_edge_buttons("ds5_edge"));
        CTM_CHECK(!ctm_caps::has_edge_buttons("ds5"));
        CTM_CHECK(!ctm_caps::has_edge_buttons("ds4"));
        CTM_CHECK(!ctm_caps::has_edge_buttons("xbox"));
        CTM_CHECK(!ctm_caps::has_edge_buttons(""));
        CTM_CHECK(!ctm_caps::has_edge_buttons("ds5e_usb"));   // the wire's kind, not the settings'
    }

    section("capabilities: these take the SETTINGS kind, not the wire kind");
    {
        // ⛔ "ds4_usb" is how a pad is ATTACHED and "ds4" is what it IS.
        // settings_kind_for() collapses the first to the second before these
        // are called, and a caller that forgets gets a false -- which is the
        // safe direction, but silently offers a DS4 nothing.
        // ⓘ Asserted so the contract is written down where the functions are,
        // rather than only in a comment on the caller.
        CTM_CHECK(!ctm_caps::has_audio_hardware("ds4_usb"));   // the wire's kind, not the settings'
        CTM_CHECK(!ctm_caps::has_audio_hardware("ds5_usb"));
        CTM_CHECK(!ctm_caps::has_audio_hardware("ds5e_usb"));
        CTM_CHECK(!ctm_caps::is_dualsense("ds5_usb"));
    }

    return 0;
}
