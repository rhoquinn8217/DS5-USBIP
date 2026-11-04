// Pins the upstream map defaults that the wired ISO passthrough path depends
// on. See process_iso_output_wired() in src/usbip/device.inl.
//
// The wired path bypasses the audio reservoir entirely, so it is only correct
// while these two settings stay inert:
//   ack_min_fill_ms  = 0     -> the fill gate in server.inl can never fire
//   underrun_silence = false -> the keep-alive silence lane stays inactive
//
// Neither is set by the wired map. If an upstream merge changes either default,
// or adds either setting to that map, the wired path changes behaviour with no
// error and no build failure. Nothing but this test detects that.

#include "harness.h"
#include "ctm/map/runtime.h"

#include <filesystem>
#include <fstream>
#include <string>

using namespace ctmtest;

static std::wstring widen(const std::string &s) { return std::wstring(s.begin(), s.end()); }

int run_map_defaults_tests(int argc, char **argv)
{
    const std::string mapPath =
        (argc > 1) ? argv[1] : "maps/ds5_usb_over_ds5_usb.map";
    const std::string btMapPath =
        (argc > 2) ? argv[2] : "maps/ds5_usb_over_ds5_bt.map";

    // --- Test 1: the assumption the wired path actually relies on ----------
    section("wired map leaves reservoir settings inert");
    {
        CtmMapRuntime map;
        std::wstring err;
        const bool loaded = map.load(widen(mapPath), &err);
        CTM_CHECK(loaded);
        if (loaded) {
            CTM_CHECK_EQ(map.iso_passthrough_enabled(), true);
            CTM_CHECK_EQ(map.iso_ack_min_fill_ms(), 0u);
            CTM_CHECK_EQ(map.underrun_silence(), false);
        } else {
            std::printf("  could not load %s\n", mapPath.c_str());
        }
    }

    // --- Test 2: control. Proves test 1 can fail. --------------------------
    // Without this, test 1 would still pass if the parser were broken and
    // returned 0/false for everything.
    section("control: parser does read these settings when present");
    {
        std::ifstream in(mapPath, std::ios::binary);
        std::string body((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
        in.close();
        CTM_CHECK(!body.empty());

        const std::string tmpPath = "tests_tmp_control.map";
        {
            std::ofstream out(tmpPath, std::ios::binary);
            out << body
                << "\n[path.iso.virtual_to_physical_stream]\n"
                << "ack_min_fill_ms = 25\n"
                << "underrun_silence = true\n";
        }

        CtmMapRuntime map;
        std::wstring err;
        const bool loaded = map.load(widen(tmpPath), &err);
        CTM_CHECK(loaded);
        if (loaded) {
            CTM_CHECK_EQ(map.iso_ack_min_fill_ms(), 25u);
            CTM_CHECK_EQ(map.underrun_silence(), true);
        }
        std::remove(tmpPath.c_str());
    }

    // --- Test 3: the Bluetooth map must NOT take the wired path -----------
    // handle_endpoint_out() dispatches on iso_passthrough_enabled(). The BT map
    // does not set the flag, so BT must keep entering upstream's unchanged
    // process_iso_output(). If this ever reads true, BT audio silently routes
    // down the wired path and breaks with no error.
    //
    // Paired with test 1 this is also a real read-check: the same accessor
    // returns true for one map and false for another, so neither result can be
    // a parser that just returns a default for everything.
    section("bt map does not enable the wired passthrough path");
    {
        CtmMapRuntime map;
        std::wstring err;
        const bool loaded = map.load(widen(btMapPath), &err);
        CTM_CHECK(loaded);
        if (loaded) {
            CTM_CHECK_EQ(map.iso_passthrough_enabled(), false);
        } else {
            std::printf("  could not load %s\n", btMapPath.c_str());
        }
    }

    // --- Test 4: the DualSense's wired figures, pinned ----------------------
    // ⭐ Every number the wired path works from (the ISO OUT hold, the gains'
    // channels, the microphone's timing and its ring) comes from these, and
    // the DualSense's map sets none of them. A changed default changes a
    // DualSense on a cable with no error, so they are pinned here.
    section("the DualSense's wired map keeps its four channels at 48 kHz, and a 48 kHz stereo mic");
    {
        CtmMapRuntime map;
        std::wstring err;
        const bool loaded = map.load(widen(mapPath), &err);
        CTM_CHECK(loaded);
        if (loaded) {
            CTM_CHECK_EQ(static_cast<unsigned>(map.usb_iso_out_endpoint()), 0x01u);
            CTM_CHECK_EQ(map.iso_out_sample_rate(), 48000u);
            CTM_CHECK_EQ(static_cast<unsigned>(map.iso_channels()), 4u);
            CTM_CHECK_EQ(static_cast<unsigned>(map.usb_iso_in_endpoint()), 0x82u);
            CTM_CHECK_EQ(map.iso_in_sample_rate(), 48000u);
            CTM_CHECK_EQ(static_cast<unsigned>(map.iso_in_channels()), 2u);
        }
    }

    // --- Test 5: a cabled DS4's own figures ---------------------------------
    // Its sound takes the same wired path as the DualSense's, at a DS4's
    // rates: a 32 kHz stereo speaker and a 16 kHz mono headset microphone.
    // ⓘ With test 4 this is the read-check: the same accessors give two maps'
    // different numbers.
    section("the cabled DS4's map takes the wired path at a DS4's rates");
    {
        // Beside the DualSense's: the run starts in the build's output folder.
        const std::string ds4MapPath =
            (std::filesystem::path(mapPath).parent_path() / "ds4_usb_over_ds4_usb.map").string();
        CtmMapRuntime map;
        std::wstring err;
        const bool loaded = map.load(widen(ds4MapPath), &err);
        CTM_CHECK(loaded);
        if (loaded) {
            CTM_CHECK_EQ(map.iso_passthrough_enabled(), true);
            // The wired path's two inert settings, as test 1 has them for the DualSense.
            CTM_CHECK_EQ(map.iso_ack_min_fill_ms(), 0u);
            CTM_CHECK_EQ(map.underrun_silence(), false);
            CTM_CHECK_EQ(static_cast<unsigned>(map.usb_iso_out_endpoint()), 0x01u);
            CTM_CHECK_EQ(map.iso_out_sample_rate(), 32000u);
            CTM_CHECK_EQ(static_cast<unsigned>(map.iso_channels()), 2u);
            CTM_CHECK_EQ(static_cast<unsigned>(map.usb_iso_in_endpoint()), 0x82u);
            CTM_CHECK_EQ(map.iso_in_sample_rate(), 16000u);
            CTM_CHECK_EQ(static_cast<unsigned>(map.iso_in_channels()), 1u);
        } else {
            std::printf("  could not load %s\n", ds4MapPath.c_str());
        }
    }

    return 0;   // main() in tests_main.cpp reports the combined summary
}
