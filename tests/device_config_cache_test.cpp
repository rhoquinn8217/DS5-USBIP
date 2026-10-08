// Each thread's copy of the settings it has looked up: that it is used, and
// that it never outlives a change.
//
// ⭐ WHAT THESE PROTECT. The input hooks ask for 75 to 80 settings on every
// report, and each one took the lock every pad shares (code review,
// 2026-10-05). They are answered now from a copy the asking thread keeps,
// dropped whenever anything writes the settings. Two ways that can go wrong:
// - the copy is not used, and nothing was saved -- the first two sections;
// - the copy outlives a change, and a setting someone just changed does
//   nothing until the listener restarts. That one looks exactly like a broken
//   setting, so most of this file is that: the shared file reloaded, a setting
//   removed, a setting added after its absence was kept, and a thread that
//   read before the change reading after it.
//
// ⓘ A config file reloaded is the config store's suite: config_store_test.cpp.
// ⛔ Not here, because in this test binary each file has its OWN copy of the
// settings map (it is static), while the store's functions are inline and
// the linker keeps one of them: called from here they could write another
// file's copy of the map.
//
// WHAT THESE CANNOT DO. They do not measure the time saved on a real pad,
// and they cannot see a write to the settings made by a path they do not
// call: the rule is in device_config.inl, beside the map.

#include "harness.h"
#include "units.h"

#include <atomic>
#include <chrono>
#include <thread>

using namespace ctmtest;

namespace {

void set_shared(const std::string &body)
{
    std::ofstream out("ctm-device-config.txt", std::ios::binary | std::ios::trunc);
    out << body;
    out.close();
    units::device_config_invalidate();
}

uint64_t map_lookups()
{
    return units::g_device_config_map_lookups.load();
}

} // namespace

int run_device_config_cache_tests()
{
    section("settings cache: a setting read again comes from the thread's copy, not the map");
    {
        set_shared("[ds5]\nspeaker_volume = 40\n");
        CTM_CHECK_EQ(units::device_config_int("ds5", "speaker_volume", -1), 40);
        const uint64_t before = map_lookups();
        int wrong = 0;
        for (int i = 0; i < 1000; ++i) {
            if (units::device_config_int("ds5", "speaker_volume", -1) != 40) ++wrong;
        }
        CTM_CHECK_EQ(wrong, 0);
        CTM_CHECK_EQ(map_lookups() - before, static_cast<uint64_t>(0));
    }

    section("settings cache: a setting nobody set is kept as not there, too");
    {
        set_shared("[ds5]\nspeaker_volume = 40\n");
        CTM_CHECK_EQ(units::device_config_int("ds5", "nobody_set_this", -7), -7);
        const uint64_t before = map_lookups();
        CTM_CHECK_EQ(units::device_config_int("ds5", "nobody_set_this", -7), -7);
        CTM_CHECK_EQ(units::device_config_bool("ds5", "nobody_set_this", true), true);
        CTM_CHECK_EQ(units::device_config_str("ds5", "nobody_set_this"), std::string());
        CTM_CHECK_EQ(map_lookups() - before, static_cast<uint64_t>(0));
    }

    section("settings cache: the shared file changed and reloaded is read at once");
    {
        set_shared("[ds5]\nspeaker_volume = 40\naudio_output = speaker\nforce_echo_cancel = true\n");
        CTM_CHECK_EQ(units::device_config_int("ds5", "speaker_volume", -1), 40);
        CTM_CHECK_EQ(units::device_config_str("ds5", "audio_output"), std::string("speaker"));
        CTM_CHECK_EQ(units::device_config_bool("ds5", "force_echo_cancel", false), true);
        set_shared("[ds5]\nspeaker_volume = 55\naudio_output = headset\nforce_echo_cancel = false\n");
        CTM_CHECK_EQ(units::device_config_int("ds5", "speaker_volume", -1), 55);
        CTM_CHECK_EQ(units::device_config_str("ds5", "audio_output"), std::string("headset"));
        CTM_CHECK_EQ(units::device_config_bool("ds5", "force_echo_cancel", true), false);
    }

    section("settings cache: a setting removed from the file goes back to its fallback");
    {
        set_shared("[ds5]\nspeaker_volume = 40\n");
        CTM_CHECK_EQ(units::device_config_int("ds5", "speaker_volume", -1), 40);
        set_shared("[ds5]\n");
        CTM_CHECK_EQ(units::device_config_int("ds5", "speaker_volume", -1), -1);
    }

    section("settings cache: a setting added after its absence was kept is found");
    {
        set_shared("[ds5]\n");
        CTM_CHECK_EQ(units::device_config_int("ds5", "headset_volume", -1), -1);
        set_shared("[ds5]\nheadset_volume = 30\n");
        CTM_CHECK_EQ(units::device_config_int("ds5", "headset_volume", -1), 30);
    }

    section("settings cache: another thread that read before a change reads the change after it");
    {
        // ⭐ The shape that matters: a pad's report thread holds its copy for
        // the life of the bridge, while the change is made on another thread.
        set_shared("[ds5]\nspeaker_volume = 40\n");
        std::atomic<int> phase{0};
        int before = 0, after = 0;
        std::thread reader([&]() {
            before = units::device_config_int("ds5", "speaker_volume", -1);
            phase.store(1);
            while (phase.load() != 2) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            after = units::device_config_int("ds5", "speaker_volume", -1);
        });
        while (phase.load() != 1) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        set_shared("[ds5]\nspeaker_volume = 55\n");
        phase.store(2);
        reader.join();
        CTM_CHECK_EQ(before, 40);
        CTM_CHECK_EQ(after, 55);
    }

    section("settings cache: the case a name is written in does not matter, as before");
    {
        set_shared("[DS5]\nSpeaker_Volume = 40\n");
        CTM_CHECK_EQ(units::device_config_int("ds5", "speaker_volume", -1), 40);
        CTM_CHECK_EQ(units::device_config_int("DS5", "SPEAKER_VOLUME", -1), 40);
    }

    section("settings cache: the number, on/off and text readers read one setting their own way");
    {
        // ⓘ One cached setting serves all three; each still applies its own
        // rule, exactly as when each read the map itself.
        set_shared("[ds5]\nweird = 5O\nswitch = On\n");
        CTM_CHECK_EQ(units::device_config_int("ds5", "weird", -3), -3);       // a typo, not 5
        CTM_CHECK_EQ(units::device_config_str("ds5", "weird"), std::string("5o"));
        CTM_CHECK_EQ(units::device_config_bool("ds5", "switch", false), true);
        CTM_CHECK_EQ(units::device_config_str("ds5", "switch"), std::string("on"));
        CTM_CHECK_EQ(units::device_config_int("ds5", "switch", -4), -4);
    }

    // Leave nothing behind for a later suite.
    set_shared("");
    return 0;
}
