// The PC's sound as it plays, for audio haptics (audio_haptics.inl), and the
// silent stream that keeps a pad's own sound open while it is used.
//
// ⭐ ITS OWN TRANSLATION UNIT (pc_sound_capture.cpp). Windows' sound API comes
// with COM headers, which define `interface` as a word of their own; main.cpp
// includes everything into one unit, and bridge.inl names a variable
// `interface`. Kept apart, neither can break the other, and this header
// carries nothing of Windows.

#pragma once

#include <cstddef>
#include <functional>
#include <string>

namespace pc_sound_capture {

// One stretch of the PC's sound: `frames` frames of `channels` interleaved
// samples, -1 to 1, at `rate`. `centre` and `lfe` are the channel numbers of
// the centre and the low-frequency channel, or -1 when there are none.
struct Block {
    const float *samples;
    size_t frames;
    unsigned channels;
    unsigned rate;
    int centre;
    int lfe;
};

using Sink = std::function<void(const Block &)>;
using Log = std::function<void(const std::wstring &)>;

// Starts capturing what the default output plays, on a thread of its own,
// handing each stretch to `sink` on that thread. It follows the default
// output when it changes. ⛔ It refuses a controller's own sound device as
// the source: its vibration would come back into it and run away.
void start(Sink sink, Log log);

// Stops, and waits for the capture thread to end.
void stop();

bool running();

// A silent stream into one pad's own sound device. Windows sends a pad its
// sound only while something plays to it, so without this there is no stream
// for the haptics to ride on. `usbInstanceId` is the virtual pad's instance id,
// USB\VID_054C&PID_0CE6\<serial>. ⓘ Silence mixed with a game's sound is no
// sound at all.
void *keep_alive_open(const std::wstring &usbInstanceId, Log log);
void keep_alive_close(void *handle);

}  // namespace pc_sound_capture
