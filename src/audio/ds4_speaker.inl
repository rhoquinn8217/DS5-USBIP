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
// ⭐ ON BLUETOOTH TOO (2026-11-05). The same Test on the rooted monitor's DS4
// over Bluetooth: "on the speaker, left is silent and right is heard", and on
// a headset each chime in its own ear. The map's comment that the speaker
// takes channel 0 there is not what the pad does.
//
// ⓘ Pure. device.inl reads the jack off the pad's reports and mixes its
// sound stream with these, on the wired path and on the Bluetooth one.

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ds4_speaker {

// A cabled DS4's input report 0x01 says what is in its jack in byte 30: 0x20
// headphones, 0x40 a microphone, both for a headset. 1 when either is, 0 when
// neither. ⓘ The TV core reads the same bits for its volume by jack.
//
// A DS4 on Bluetooth sends report 0x11, the same state two bytes later, so its
// jack is byte 32. There only 0x40 counts: it is the bit the Bluetooth map's
// own route follows (auto_route_input_mask in ds4_usb_over_ds4_bt.map),
// sending the sound to the headphones in stereo with it and to the speaker
// without, so the mix follows where the sound goes. -1 for any other report,
// a DS4's short Bluetooth report 0x01 included.
inline int jack_in(const uint8_t *report, size_t length)
{
    if (report == nullptr) return -1;
    if (report[0] == 0x01 && length >= 31) return (report[30] & 0x60) ? 1 : 0;
    if (report[0] == 0x11 && length >= 33) return (report[32] & 0x40) ? 1 : 0;
    return -1;
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
