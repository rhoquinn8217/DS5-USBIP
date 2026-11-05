// When Windows' default speaker is taken back from a controller.
//
// ⭐ WHAT THESE PROTECT. Two ways of going wrong, both quiet. Taking the
// default back when a person chose the controller: the pad would refuse to be
// the default speaker, with nothing to say why. And not taking it back when
// Windows handed it over as the pad arrived: what plays on the default comes
// out of the pad and, through the streaming host, the TV too (2026-11-04).
//
// WHAT THESE CANNOT DO. Whether Windows tells of its changes, and whether it
// gives the default back when asked: default_speaker.cpp, checked with a pad.

#include "harness.h"

#include <string>

#include "audio/controller_sound.inl"
#include "audio/default_speaker_rule.inl"

using namespace ctmtest;
namespace dsr = default_speaker_rule;

namespace {

// Sound device ids the shape of Windows' own.
const std::wstring kSteam = L"{0.0.0.00000000}.{f448f6c9-8846-4a05-a413-4f45f81020a6}";
const std::wstring kRealtek = L"{0.0.0.00000000}.{5a1c2f00-0000-4000-8000-000000000001}";
const std::wstring kDs4 = L"{0.0.0.00000000}.{cf52db75-6739-4359-8c75-c2ed667a804b}";
const std::wstring kEdge = L"{0.0.0.00000000}.{23004408-4390-41cc-acaf-f879536aa5eb}";

// A rule that has seen the Steam speakers as the default in every role.
dsr::Rule kept_steam()
{
    dsr::Rule rule;
    for (int role = 0; role < dsr::kRoles; ++role) rule.default_changed(role, kSteam, false, 1000);
    return rule;
}

}  // namespace

int run_default_speaker_rule_tests()
{
    section("controller sound: a controller's own sound devices, by the names Windows gives them");
    {
        CTM_CHECK(controller_sound::is_controller(L"Headset Earphone (Wireless Controller-E)"));
        CTM_CHECK(controller_sound::is_controller(L"Speakers (DualSense Wireless Controller)"));
        CTM_CHECK(controller_sound::is_controller(L"Speakers (3- DualSense Edge Wireless Controller)"));
        // Control: the streaming host's speakers and a sound card are not.
        CTM_CHECK(!controller_sound::is_controller(L"Speakers (Steam Streaming Speakers)"));
        CTM_CHECK(!controller_sound::is_controller(L"SPDIF Interface (Realtek USB Audio)"));
        CTM_CHECK(!controller_sound::is_controller(L""));
    }

    section("default speaker: Windows giving it to a controller as it arrives is undone, in every role");
    {
        dsr::Rule rule = kept_steam();
        rule.arrived(kDs4, 50000);
        for (int role = 0; role < dsr::kRoles; ++role) CTM_CHECK(rule.default_changed(role, kDs4, true, 50900) == kSteam);
    }

    section("default speaker: the window's edge, a millisecond either side of it");
    {
        dsr::Rule inside = kept_steam();
        inside.arrived(kDs4, 50000);
        CTM_CHECK(inside.default_changed(0, kDs4, true, 50000 + dsr::kArrivalMs) == kSteam);
        // Control: the same change a millisecond later was a person's choice.
        dsr::Rule outside = kept_steam();
        outside.arrived(kDs4, 50000);
        CTM_CHECK(outside.default_changed(0, kDs4, true, 50000 + dsr::kArrivalMs + 1).empty());
    }

    section("default speaker: a controller chosen long after it arrived stays the default");
    {
        dsr::Rule rule = kept_steam();
        rule.arrived(kDs4, 50000);
        CTM_CHECK(rule.default_changed(1, kDs4, true, 50000 + 60000).empty());
        CTM_CHECK(rule.kept(1) == kSteam);
    }

    section("default speaker: what goes back is the last default that was not a controller");
    {
        dsr::Rule rule = kept_steam();
        rule.default_changed(0, kRealtek, false, 2000);   // a person chose the sound card
        rule.arrived(kDs4, 50000);
        CTM_CHECK(rule.default_changed(0, kDs4, true, 50100) == kRealtek);
        CTM_CHECK(rule.default_changed(1, kDs4, true, 50100) == kSteam);
    }

    section("default speaker: with nothing known to go back to, nothing is done");
    {
        dsr::Rule rule;
        rule.arrived(kDs4, 50000);
        CTM_CHECK(rule.default_changed(0, kDs4, true, 50100).empty());
    }

    section("default speaker: a change heard before the arrival counts as one just after it");
    {
        dsr::Rule rule = kept_steam();
        CTM_CHECK(rule.default_changed(0, kDs4, true, 50000) == kSteam);
        // The arrival, told of a moment later, and a change stamped before it.
        rule.arrived(kDs4, 50003);
        CTM_CHECK(rule.default_changed(1, kDs4, true, 50001) == kSteam);
    }

    section("default speaker: a controller there before the listener started is left alone");
    {
        dsr::Rule rule;
        rule.present_before(kEdge);
        for (int role = 0; role < dsr::kRoles; ++role) rule.default_changed(role, kSteam, false, 1000);
        CTM_CHECK(rule.default_changed(0, kEdge, true, 1500).empty());
        // Once it has gone and come back, it is an arrival like any other.
        rule.left(kEdge);
        rule.arrived(kEdge, 60000);
        CTM_CHECK(rule.default_changed(0, kEdge, true, 60500) == kSteam);
    }

    section("default speaker: a pad that goes and comes back is undone again");
    {
        dsr::Rule rule = kept_steam();
        rule.arrived(kDs4, 50000);
        CTM_CHECK(rule.default_changed(0, kDs4, true, 50500) == kSteam);
        rule.left(kDs4);
        rule.arrived(kDs4, 90000);
        CTM_CHECK(rule.default_changed(0, kDs4, true, 90400) == kSteam);
    }

    section("default speaker: Windows handing it straight back is not fought for ever");
    {
        dsr::Rule rule = kept_steam();
        rule.arrived(kDs4, 50000);
        int undone = 0;
        for (int i = 0; i < 20; ++i) {
            if (!rule.default_changed(i % dsr::kRoles, kDs4, true, 50000 + static_cast<uint64_t>(i)).empty()) ++undone;
        }
        CTM_CHECK_EQ(undone, dsr::kMostTakeBacks);
    }

    section("default speaker: a pad a person chose keeps the default through its next bridge");
    {
        dsr::Rule rule = kept_steam();
        rule.arrived(kDs4, 50000);
        CTM_CHECK(rule.default_changed(0, kDs4, true, 70000).empty());   // chosen, long after it came
        CTM_CHECK(rule.chosen(0) == kDs4);
        // It goes, and Windows hands the default to the TV's speaker meanwhile.
        rule.left(kDs4);
        CTM_CHECK(rule.default_changed(0, kSteam, false, 80000, false).empty());
        CTM_CHECK(rule.chosen(0) == kDs4);
        rule.arrived(kDs4, 90000);
        // Windows hands it the default again as it comes back: their choice, kept.
        CTM_CHECK(rule.default_changed(0, kDs4, true, 90500).empty());
        // Control: the role nobody chose it for is still given back.
        CTM_CHECK(rule.default_changed(1, kDs4, true, 90500) == kSteam);
    }

    section("default speaker: choosing a speaker that is not a controller ends that choice");
    {
        dsr::Rule rule = kept_steam();
        rule.arrived(kDs4, 50000);
        rule.default_changed(0, kDs4, true, 70000);          // the pad, chosen
        rule.default_changed(0, kSteam, false, 80000, true); // then the TV's speaker, the pad still there
        CTM_CHECK(rule.chosen(0).empty());
        rule.left(kDs4);
        rule.arrived(kDs4, 90000);
        CTM_CHECK(rule.default_changed(0, kDs4, true, 90500) == kSteam);
    }

    section("default speaker: a person's choice of one pad does not shield another");
    {
        dsr::Rule rule = kept_steam();
        rule.arrived(kDs4, 50000);
        rule.default_changed(0, kDs4, true, 70000);    // the DS4, chosen
        rule.arrived(kEdge, 90000);
        CTM_CHECK(rule.default_changed(0, kEdge, true, 90300) == kSteam);
    }

    section("default speaker: its own putting back is remembered, not undone");
    {
        dsr::Rule rule = kept_steam();
        rule.arrived(kDs4, 50000);
        CTM_CHECK(rule.default_changed(0, kDs4, true, 50500) == kSteam);
        CTM_CHECK(rule.default_changed(0, kSteam, false, 50600).empty());
        CTM_CHECK(rule.kept(0) == kSteam);
    }

    section("default speaker: a role Windows does not have, or no device, changes nothing");
    {
        dsr::Rule rule;
        CTM_CHECK(rule.default_changed(3, kSteam, false, 1000).empty());
        CTM_CHECK(rule.default_changed(-1, kSteam, false, 1000).empty());
        CTM_CHECK(rule.kept(0).empty());
        CTM_CHECK(rule.kept(3).empty());
        rule.default_changed(0, kSteam, false, 1000);
        CTM_CHECK(rule.default_changed(0, L"", true, 1100).empty());
        CTM_CHECK(rule.kept(0) == kSteam);
    }

    return 0;
}
