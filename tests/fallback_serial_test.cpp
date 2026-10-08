// The USB serial a virtual device gets when the TV sent none.
//
// ⭐ WHAT THESE PROTECT. Every serial-less device got the same "CTMUSBIP", and
// the Xbox id XInput keys on is a hash of the serial, so two such pads bridged
// at once were one device twice. These pin the fallback: distinct for two
// devices, the same for one device bridged again, and a serial Windows takes.

#include "harness.h"

#include <cwctype>
#include <string>

#include "usbip/fallback_serial.inl"

using namespace ctmtest;

int run_fallback_serial_tests()
{
    using usb_serial_fallback::make;

    section("fallback serial: two pads at once are two devices");
    {
        const std::wstring a = make(L"/dev/hidraw3", 0x045e, 0x028e);
        const std::wstring b = make(L"/dev/hidraw4", 0x045e, 0x028e);
        CTM_CHECK(a != b);                                   // same model, two nodes
        CTM_CHECK(make(L"/dev/input/event7", 0x045e, 0x028e) !=
                  make(L"/dev/input/event8", 0x045e, 0x028e)); // two xpad pads
        CTM_CHECK(make(L"/dev/hidraw3", 0x045e, 0x028e) !=
                  make(L"/dev/hidraw3", 0x045e, 0x02ea));     // another model on that node
    }

    section("fallback serial: the same pad bridged again keeps its id");
    {
        CTM_CHECK(make(L"/dev/hidraw3", 0x045e, 0x028e) == make(L"/dev/hidraw3", 0x045e, 0x028e));
    }

    section("fallback serial: still says what it is, and Windows takes it");
    {
        const std::wstring s = make(L"/dev/hidraw3", 0x045e, 0x028e);
        CTM_CHECK(s.rfind(L"CTMUSBIP", 0) == 0);
        CTM_CHECK_EQ(s.size(), static_cast<size_t>(16));
        bool alnum = true;
        for (wchar_t c : s) {
            if (!std::iswalnum(c) || std::iswlower(c)) alnum = false;
        }
        CTM_CHECK(alnum);                                    // survives sanitize_usb_serial unchanged
        CTM_CHECK(make(L"", 0, 0) != std::wstring(L"CTMUSBIP"));
    }
    return 0;
}
