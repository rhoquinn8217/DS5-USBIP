// Windows' default speaker, taken back from a controller Windows gave it to
// as the controller arrived. default_speaker_rule.inl says when, and why.
//
// ⭐ ITS OWN TRANSLATION UNIT (default_speaker.cpp), for the reason
// pc_sound_capture.h gives: Windows' sound API comes with COM headers that
// define `interface`, and this header carries nothing of Windows.
#pragma once

#include <functional>
#include <string>

namespace default_speaker {

using Log = std::function<void(const std::wstring &)>;

// Listens to Windows' sound devices on a thread of its own, from now until
// stop(). Once: a second call does nothing.
void start(Log log);

// Stops listening, and waits a moment for its thread to finish, so it logs
// nothing once the listener is on its way out.
void stop();

}  // namespace default_speaker
