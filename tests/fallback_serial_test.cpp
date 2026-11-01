// The USB serial a virtual device gets when the TV sent none.
//
// ⭐ WHAT THESE PROTECT. Every serial-less device got the same "CTMUSBIP", and
// the Xbox id XInput keys on is a hash of the serial, so two such pads bridged
// at once were one device twice. Then the serial hashed the device's node on
// the TV, which follows the order things were plugged in, and the same mouse
// became a new Windows device on every bridge: 473 of them on rhoquinn8217's
// PC (2026-10-13). These pin it: the same serial bridge after bridge, two at
// once are two devices, a slot comes free when its bridge ends, and a serial
// Windows takes.

#include "harness.h"

#include <cwctype>
#include <string>

#include "usbip/fallback_serial.inl"

using namespace ctmtest;
namespace usf = usb_serial_fallback;

int run_fallback_serial_tests()
{
    section("fallback serial: the same device bridged again keeps its serial");
    {
        usf::Slots slots;
        const std::wstring first = slots.claim(0x1532, 0x0094);    // the Razer mouse
        slots.release(first);                                      // its bridge ends
        CTM_CHECK(slots.claim(0x1532, 0x0094) == first);           // and it comes back as itself
    }

    section("fallback serial: two of one kind at once are two devices");
    {
        usf::Slots slots;
        const std::wstring a = slots.claim(0x045e, 0x028e);
        const std::wstring b = slots.claim(0x045e, 0x028e);
        CTM_CHECK(a != b);
        CTM_CHECK(a == usf::for_slot(0x045e, 0x028e, 0));
        CTM_CHECK(b == usf::for_slot(0x045e, 0x028e, 1));
        slots.release(a);                                          // the first one's bridge ends
        CTM_CHECK(slots.claim(0x045e, 0x028e) == a);               // the next takes slot 0 again
        CTM_CHECK(slots.claim(0x045e, 0x028e) == usf::for_slot(0x045e, 0x028e, 2));  // b holds 1
    }

    section("fallback serial: another kind is another serial, from slot 0");
    {
        CTM_CHECK(usf::for_slot(0x045e, 0x028e, 0) != usf::for_slot(0x045e, 0x02ea, 0));
        CTM_CHECK(usf::for_slot(0x1532, 0x0094, 0) != usf::for_slot(0x25a7, 0xfa17, 0));
        usf::Slots slots;
        const std::wstring mouse = slots.claim(0x1532, 0x0094);
        const std::wstring keyboard = slots.claim(0x25a7, 0xfa17);
        CTM_CHECK(mouse == usf::for_slot(0x1532, 0x0094, 0));
        CTM_CHECK(keyboard == usf::for_slot(0x25a7, 0xfa17, 0));
    }

    section("fallback serial: releasing nothing, or twice, is harmless");
    {
        usf::Slots slots;
        slots.release(L"");
        const std::wstring a = slots.claim(0x1532, 0x0094);
        slots.release(a);
        slots.release(a);
        CTM_CHECK(slots.claim(0x1532, 0x0094) == a);
    }

    section("fallback serial: still says what it is, and Windows takes it");
    {
        const std::wstring s = usf::for_slot(0x045e, 0x028e, 0);
        CTM_CHECK(s.rfind(L"CTMUSBIP", 0) == 0);
        CTM_CHECK_EQ(s.size(), static_cast<size_t>(16));
        bool alnum = true;
        for (wchar_t c : s) {
            if (!std::iswalnum(c) || std::iswlower(c)) alnum = false;
        }
        CTM_CHECK(alnum);                                    // survives sanitize_usb_serial unchanged
        CTM_CHECK(usf::for_slot(0, 0, 0) != std::wstring(L"CTMUSBIP"));
    }
    return 0;
}
