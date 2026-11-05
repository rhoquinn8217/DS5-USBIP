#pragma once
// ⭐ A DS4'S OWN SPEAKER HEARS BOTH CHANNELS (2026-11-04).
//
// A DS4 has one speaker, and it plays only the RIGHT channel of the stereo it
// is sent. Windows' speaker Test, a chime on the left and then one on the
// right, was heard once on the C1's DS4 through its DS4Dongle (rhoquinn8217:
// "i don't here the first one but I do hear the second one"), where a headset
// had the same stream in both ears that morning. ➡️ So while nothing is in its
// headphone jack, the two channels are mixed into both, half of each so
// nothing clips; a headset keeps its stereo.
//
// ⓘ Pure. device.inl reads the jack off the pad's reports and mixes its
// sound stream with these.

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ds4_speaker {

// A cabled DS4's input report 0x01 says what is in its jack in byte 30: 0x20
// headphones, 0x40 a microphone, both for a headset. 1 when either is, 0 when
// neither, -1 when this is not that report. ⓘ The TV core reads the same bits
// for its volume by jack.
inline int jack_in(const uint8_t *report, size_t length)
{
    if (report == nullptr || length < 31 || report[0] != 0x01) return -1;
    return (report[30] & 0x60) ? 1 : 0;
}

// 16-bit little-endian stereo, each frame made (left + right) / 2 on both
// channels, in place. A trailing part frame is left as it is.
inline void mix_to_mono(std::vector<uint8_t> &data)
{
    const size_t frames = data.size() / 4;
    for (size_t f = 0; f < frames; ++f) {
        uint8_t *p = data.data() + f * 4;
        const int16_t left = static_cast<int16_t>(
            static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8));
        const int16_t right = static_cast<int16_t>(
            static_cast<uint16_t>(p[2]) | (static_cast<uint16_t>(p[3]) << 8));
        const int32_t mixed = (static_cast<int32_t>(left) + static_cast<int32_t>(right)) / 2;
        const uint16_t out = static_cast<uint16_t>(static_cast<int16_t>(mixed));
        p[0] = p[2] = static_cast<uint8_t>(out & 0xff);
        p[1] = p[3] = static_cast<uint8_t>((out >> 8) & 0xff);
    }
}

}  // namespace ds4_speaker
