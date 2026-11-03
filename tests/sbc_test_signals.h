// The sounds and settings the SBC encoder is checked with, shared by its test
// (sbc_encoder_test.cpp) and by the tool that wrote the expected results from
// FFmpeg's own encoder (tools/sbc-golden-gen).
//
// ⭐ WHOLE NUMBERS ONLY. The test runs in a Debug build and the tool in Release, and
// the expected results only mean something if both feed in exactly the same sound,
// so nothing here goes near a floating point sum.

#pragma once

#include <cstdint>

namespace sbctest {

struct Settings {
    const char *name;
    uint8_t frequency;  // SBC_FREQ_*
    uint8_t blocks;
    uint8_t subbands;
    uint8_t mode;        // SBC_MODE_*
    uint8_t allocation;  // SBC_AM_*
    uint8_t bitpool;
    uint32_t sampleRate;
};

// ⭐ The first is the one that ships: maps/ds4_usb_over_ds4_bt.map. The second is
// the other layout that map keeps in its comments, which are also the runtime's
// own defaults. The rest are there because a map may ask for them.
inline const Settings kSettings[] = {
    {"ds4", 1, 16, 8, 3, 0, 48, 32000},
    {"ds4-other-layout", 1, 16, 8, 1, 0, 25, 32000},
    {"stereo-snr-44k", 2, 16, 8, 2, 1, 53, 44100},
    {"joint-48k", 3, 16, 8, 3, 0, 51, 48000},
    {"short-frames", 1, 8, 8, 3, 0, 32, 32000},
};
inline const int kSettingsCount = sizeof(kSettings) / sizeof(kSettings[0]);

inline const char *const kSignalNames[] = {
    "silence",        "quiet tone, both sides", "loud tone, left only",  "sweep, sides opposed",
    "noise, moderate", "loud square wave",       "noise, full scale",     "mostly the same both sides",
    "chord with gaps", "highest pitch, full scale",
};
inline const int kSignalCount = sizeof(kSignalNames) / sizeof(kSignalNames[0]);

// A wave shaped like a sine from whole numbers: phase is one turn in 2^32.
inline int wave(uint32_t phase)
{
    const int32_t x = static_cast<int32_t>(phase >> 16) - 32768;
    const int32_t rest = 32768 - (x < 0 ? -x : x);
    return static_cast<int>((static_cast<int64_t>(x) * rest) >> 13);
}

inline uint32_t step_for(uint32_t hertz, uint32_t sampleRate)
{
    return static_cast<uint32_t>((static_cast<uint64_t>(hertz) << 32) / sampleRate);
}

inline int16_t clip16(int value)
{
    if (value > 32767) return 32767;
    if (value < -32768) return -32768;
    return static_cast<int16_t>(value);
}

// One signal's samples, one pair at a time, the same every run.
class Signal {
public:
    Signal(int which, uint32_t sampleRate) : which_(which), sampleRate_(sampleRate) {}

    void next(int16_t *left, int16_t *right)
    {
        const uint64_t i = index_++;
        switch (which_) {
        case 0:
            *left = 0;
            *right = 0;
            break;
        case 1: {
            phaseA_ += step_for(440, sampleRate_);
            const int v = wave(phaseA_) / 10;
            *left = clip16(v);
            *right = clip16(v);
            break;
        }
        case 2:
            phaseA_ += step_for(1000, sampleRate_);
            *left = clip16(wave(phaseA_));
            *right = 0;
            break;
        case 3: {
            const uint32_t top = sampleRate_ / 2 - 200;
            const uint32_t hertz = 50 + static_cast<uint32_t>(i % 40000) * (top - 50) / 40000;
            phaseA_ += step_for(hertz, sampleRate_);
            const int v = wave(phaseA_) / 2;
            *left = clip16(v);
            *right = clip16(-v);
            break;
        }
        case 4:
            *left = static_cast<int16_t>(noise() / 4);
            *right = static_cast<int16_t>(noise() / 4);
            break;
        case 5: {
            phaseA_ += step_for(997, sampleRate_);
            const bool high = phaseA_ < 0x80000000u;
            *left = high ? 32767 : -32768;
            *right = high ? -32768 : 32767;
            break;
        }
        case 6:
            *left = noise();
            *right = noise();
            break;
        case 7: {
            const int v = noise() / 3;
            *left = clip16(v + noise() / 200);
            *right = clip16(v + noise() / 200);
            break;
        }
        case 8: {
            phaseA_ += step_for(262, sampleRate_);
            phaseB_ += step_for(330, sampleRate_);
            phaseC_ += step_for(392, sampleRate_);
            // A slow swell, and a short silence every so often, so the encoder
            // has to follow a sound that starts and stops.
            const int swell = static_cast<int>(i / 16 % 1024);
            const bool gap = i % 24000 < 2000;
            const int v = gap ? 0 : (wave(phaseA_) + wave(phaseB_) + wave(phaseC_)) / 4 * swell / 1024;
            const int w = gap ? 0 : (wave(phaseA_) - wave(phaseC_) / 2) / 3 * swell / 1024;
            *left = clip16(v);
            *right = clip16(w);
            break;
        }
        default:
            *left = (i & 1) ? -32768 : 32767;
            *right = (i & 1) ? 32767 : -32768;
            break;
        }
    }

private:
    int16_t noise()
    {
        state_ = state_ * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<int16_t>(state_ >> 48);
    }

    int which_;
    uint32_t sampleRate_;
    uint64_t index_ = 0;
    uint32_t phaseA_ = 0;
    uint32_t phaseB_ = 0;
    uint32_t phaseC_ = 0;
    uint64_t state_ = 0x1234567ULL;
};

// FNV-1a, for a short name for a long run of bytes.
inline uint64_t hash_more(uint64_t hash, const uint8_t *data, size_t size)
{
    for (size_t i = 0; i < size; ++i) {
        hash ^= data[i];
        hash *= 0x100000001b3ULL;
    }
    return hash;
}
inline const uint64_t kHashStart = 0xcbf29ce484222325ULL;

// How many frames the test and its expected results cover, per setting and signal.
inline const int kFramesPerRun = 400;

}  // namespace sbctest
