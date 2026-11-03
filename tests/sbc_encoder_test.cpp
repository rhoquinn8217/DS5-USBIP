// The SBC encoder: how a DS4's speaker gets its sound over Bluetooth, and the one
// thing this program ever used FFmpeg for.
//
// ⭐ WHAT THESE PROTECT. third_party/sbc is a copy of FFmpeg's encoder with the rest
// of FFmpeg cut away. A slip there would not fail loudly: a DS4 would play noise or
// nothing, and only someone listening to one would know. So every sound here has to
// come out as exactly the bytes FFmpeg itself produced. sbc_encoder_golden.inl holds
// those, as hashes, and tools/sbc-golden-gen wrote them.
//
// ⭐ And the setup has to stop installing FFmpeg and take away what an older setup
// left behind, or an upgraded PC keeps files nothing uses.

#include "harness.h"
#include "sbc_test_signals.h"

#include "sbc/sbc_encoder.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace ctmtest;
using namespace sbctest;

namespace {

struct Golden {
    int settings;
    int signal;
    uint64_t hash;
};

const Golden kGolden[] = {
#include "sbc_encoder_golden.inl"
};

SbcEncoder *encoder_for(const Settings &settings)
{
    return sbc_encoder_create(settings.frequency, settings.blocks, settings.mode,
                              settings.allocation, settings.subbands, settings.bitpool);
}

uint64_t hash_of_run(const Settings &settings, int which)
{
    uint64_t hash = kHashStart;
    Signal signal(which, settings.sampleRate);
    SbcEncoder *encoder = encoder_for(settings);
    const size_t samples = static_cast<size_t>(settings.subbands) * settings.blocks;
    std::vector<int16_t> pcm(samples * 2);
    std::vector<uint8_t> bytes(sbc_encoder_frame_length(encoder));
    for (int frame = 0; frame < kFramesPerRun; ++frame) {
        for (size_t i = 0; i < samples; ++i) {
            signal.next(&pcm[i * 2], &pcm[i * 2 + 1]);
        }
        const int size = sbc_encoder_encode(encoder, pcm.data(), bytes.data(), bytes.size());
        if (size <= 0) {
            sbc_encoder_destroy(encoder);
            return 0;
        }
        hash = hash_more(hash, bytes.data(), static_cast<size_t>(size));
    }
    sbc_encoder_destroy(encoder);
    return hash;
}

// A file of the repo, found from wherever the suite runs, the way the quit-request
// suite finds the installer.
std::string repo_file(const std::string &path)
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

}  // namespace

int run_sbc_encoder_tests()
{
    section("SBC encoder: a DS4's frames are the shape its map expects");
    {
        const Settings &ds4 = kSettings[0];
        SbcEncoder *encoder = encoder_for(ds4);
        CTM_CHECK(encoder != nullptr);
        CTM_CHECK_EQ(sbc_encoder_frame_length(encoder), static_cast<size_t>(109));

        std::vector<int16_t> pcm(static_cast<size_t>(ds4.subbands) * ds4.blocks * 2);
        std::vector<uint8_t> bytes(109);
        Signal tone(1, ds4.sampleRate);
        for (size_t i = 0; i * 2 < pcm.size(); ++i) {
            tone.next(&pcm[i * 2], &pcm[i * 2 + 1]);
        }
        CTM_CHECK_EQ(sbc_encoder_encode(encoder, pcm.data(), bytes.data(), bytes.size()), 109);
        // The start of every SBC frame: its mark, then 32 kHz, 16 blocks, joint
        // stereo, loudness and 8 subbands in one byte, then the bitpool.
        CTM_CHECK_EQ(static_cast<int>(bytes[0]), 0x9C);
        CTM_CHECK_EQ(static_cast<int>(bytes[1]), 0x7D);
        CTM_CHECK_EQ(static_cast<int>(bytes[2]), 48);

        // A buffer one byte short is refused, not overrun.
        CTM_CHECK_EQ(sbc_encoder_encode(encoder, pcm.data(), bytes.data(), bytes.size() - 1), -1);
        sbc_encoder_destroy(encoder);
    }

    section("SBC encoder: the settings tested first are the ones the DS4's map ships");
    {
        const std::string map = repo_file("maps/ds4_usb_over_ds4_bt.map");
        CTM_CHECK(!map.empty());
        CTM_CHECK(map.find("\nop.2 = codec.sbc_encode speaker_pcm sample_rate=32000 channels=2 frame_samples=256 "
                           "bytes=218 bitpool=48 blocks=16 subbands=8 channel_mode=joint allocation=loudness "
                           "out=sbc_payload") != std::string::npos);
    }

    section("SBC encoder: every sound comes out as the bytes FFmpeg produced");
    {
        const int expected = kSettingsCount * kSignalCount;
        CTM_CHECK_EQ(static_cast<int>(sizeof(kGolden) / sizeof(kGolden[0])), expected);
        for (const Golden &golden : kGolden) {
            const Settings &settings = kSettings[golden.settings];
            const uint64_t hash = hash_of_run(settings, golden.signal);
            record(hash == golden.hash,
                   std::string(settings.name) + ", " + kSignalNames[golden.signal] + ": not FFmpeg's bytes",
                   __FILE__, __LINE__);
        }
    }

    section("SBC encoder: the setup no longer installs FFmpeg, and clears what older setups left");
    {
        const std::string iss = repo_file("installer/ds5-usbip.iss");
        CTM_CHECK(!iss.empty());
        CTM_CHECK(iss.find("third_party\\ffmpeg") == std::string::npos);
        const size_t section_at = iss.find("[InstallDelete]");
        CTM_CHECK(section_at != std::string::npos);
        const size_t next_section = iss.find("\n[", section_at + 1);
        const std::string deletes = iss.substr(section_at, next_section - section_at);
        for (const char *name : {"avcodec-62.dll", "avdevice-62.dll", "avfilter-11.dll", "avformat-62.dll",
                                 "avutil-60.dll", "swresample-6.dll", "swscale-9.dll"}) {
            record(deletes.find(std::string("Type: files; Name: \"{app}\\") + name + "\"") != std::string::npos,
                   std::string("[InstallDelete] does not remove ") + name, __FILE__, __LINE__);
        }
    }

    return 0;
}
