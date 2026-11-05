// Which of Windows' sound devices is a controller's own.
//
// ⓘ By the name Windows gives it: a DualSense's is "DualSense Wireless
// Controller", an Edge's "DualSense Edge Wireless Controller", a DS4's
// "Wireless Controller". Shared by the PC sound capture, which will not
// capture one (its own vibration would come back into it), and the default
// speaker, which takes the default back from one. Pure, so the tests reach it.

#pragma once

#include <string>

namespace controller_sound {

inline bool is_controller(const std::wstring &name)
{
    return name.find(L"DualSense") != std::wstring::npos || name.find(L"Wireless Controller") != std::wstring::npos;
}

}  // namespace controller_sound
