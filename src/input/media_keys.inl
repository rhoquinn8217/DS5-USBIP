// The media keys a binding can name: volume up, down and mute, and play or
// pause, stop, previous and next.
//
// ⭐ rhoquinn8217, 2026-11-07: *"add media controls for volume up/down/mute
// and play/stop/pause back/next rebinds to the rebind list for rebindable
// buttons"*.
//
// ⭐ CARRIED AS KEYS. Whatever presses a key -- a button's rebind, a trigger,
// a touchpad tap, press or swipe -- hands its usage to the virtual keyboard
// (keyboard_device.inl). A media key travels the same way, as a usage the
// keyboard page leaves unused (0xF0 up, past every key the keyboard sends),
// and the keyboard's pump sends it in a report of its own: a consumer
// control's, which is where Windows reads media keys from. ⓘ So nothing that
// presses a key needs to know these exist.
//
// ⓘ THE DEVICE. The virtual keyboard's report descriptor has two collections
// for it, each with its report ID: the keyboard (1) and a consumer control (2)
// (profiles/descriptors/virtual_keyboard.profile).
//
// ⓘ Play and pause are one key, as on a keyboard: Windows' Play/Pause toggles.
//
// ⭐ Pure -- no device, no Windows -- so the test binary includes it as it is.
#pragma once

#include <cstddef>
#include <cstdint>

namespace media_keys {

// The usages that mark them on their way to the keyboard.
inline constexpr uint8_t kVolumeUp = 0xF0;
inline constexpr uint8_t kVolumeDown = 0xF1;
inline constexpr uint8_t kVolumeMute = 0xF2;
inline constexpr uint8_t kPlayPause = 0xF3;
inline constexpr uint8_t kStop = 0xF4;
inline constexpr uint8_t kPrevious = 0xF5;
inline constexpr uint8_t kNext = 0xF6;

// Each one's usage on the consumer page (HID Usage Tables, page 0x0C).
struct Media { uint8_t marker; uint16_t consumer; };
inline constexpr Media kMedia[] = {
    { kVolumeUp,   0x00E9 },   // Volume Increment
    { kVolumeDown, 0x00EA },   // Volume Decrement
    { kVolumeMute, 0x00E2 },   // Mute
    { kPlayPause,  0x00CD },   // Play/Pause
    { kStop,       0x00B7 },   // Stop
    { kPrevious,   0x00B6 },   // Scan Previous Track
    { kNext,       0x00B5 },   // Scan Next Track
};

// The consumer usage a marker stands for, or 0 for an ordinary key.
inline uint16_t consumer_for(uint8_t usage)
{
    for (const Media &m : kMedia) {
        if (m.marker == usage) return m.consumer;
    }
    return 0;
}

// ---- The two reports --------------------------------------------------------

inline constexpr uint8_t kKeyboardReportId = 1;
inline constexpr uint8_t kConsumerReportId = 2;
// [0] report ID  [1] modifiers  [2] reserved  [3..8] up to six keys
inline constexpr size_t kKeyboardReportLen = 9;
// [0] report ID  [1..2] the consumer usage held, low byte first, or 0
inline constexpr size_t kConsumerReportLen = 3;

// The keyboard's report, from what is held: every key but the media keys.
inline void keyboard_report(uint8_t modifiers, const uint8_t keys[6], uint8_t out[kKeyboardReportLen])
{
    out[0] = kKeyboardReportId;
    out[1] = modifiers;
    out[2] = 0;
    for (size_t i = 0; i < 6; ++i) out[3 + i] = 0;
    size_t n = 0;
    for (size_t i = 0; i < 6; ++i) {
        if (keys[i] == 0 || consumer_for(keys[i]) != 0) continue;
        out[3 + n++] = keys[i];
    }
}

// The consumer control's: the first media key held, or none. ⓘ One at a time,
// as a keyboard's media keys are: a second held with it comes once the first
// goes up.
inline void consumer_report(const uint8_t keys[6], uint8_t out[kConsumerReportLen])
{
    uint16_t usage = 0;
    for (size_t i = 0; i < 6 && usage == 0; ++i) usage = consumer_for(keys[i]);
    out[0] = kConsumerReportId;
    out[1] = static_cast<uint8_t>(usage & 0xFF);
    out[2] = static_cast<uint8_t>(usage >> 8);
}

}  // namespace media_keys
