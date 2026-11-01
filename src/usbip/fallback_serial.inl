// The USB serial a virtual device gets when the TV sent none.
//
// ⛔⛔ WHY (code review, 2026-10-05). It was the constant "CTMUSBIP" for every
// such device. The serial names the device to Windows, and the Xbox id that
// XInput keys on is a hash of it (CtmMapRuntime::set_device_identity), so two
// serial-less pads bridged at once were one device twice: the second got no
// XInput slot while looking bridged from every other angle.
//
// ⭐⭐ AND THE SAME SERIAL FROM ONE BRIDGE TO THE NEXT (2026-10-13). It was a
// hash of the device's node on the TV, and /dev/hidrawN numbers follow the
// order things were plugged in, so the same mouse came back under a new serial
// and Windows built a new device for it every time: 473 old devices had piled
// up on rhoquinn8217's PC, the Razer mouse alone 155 of them. Now it is the
// device's vendor and product with a SLOT: the first such device bridged takes
// slot 0, another of the same kind bridged while it is up takes slot 1, and a
// slot is free again when its bridge ends. One mouse is slot 0 every time, so
// Windows finds the device it built last time; two at once are still two
// devices, from one TV or from two.
//
// ⚠️ This is never a controller's IDENTITY. Per-controller config keys on the
// physical serial, which stays empty for these (device.inl says why).
// ⓘ Its own file so the test binary can reach it.

#pragma once

#include <cstdint>
#include <mutex>
#include <set>
#include <string>

namespace usb_serial_fallback {

// The serial of one slot of one vendor and product: "CTMUSBIP" and eight hex
// digits, which Windows takes as they are (sanitize_usb_serial).
inline std::wstring for_slot(uint16_t vendorId, uint16_t productId, unsigned slot)
{
    uint32_t hash = 2166136261u;                       // FNV-1a, 32-bit
    auto mix = [&hash](uint32_t byte) {
        hash ^= byte & 0xffu;
        hash *= 16777619u;
    };
    mix(vendorId >> 8); mix(vendorId);
    mix(productId >> 8); mix(productId);
    mix(slot >> 8); mix(slot);
    static const wchar_t kHex[] = L"0123456789ABCDEF";
    std::wstring out = L"CTMUSBIP";
    for (int shift = 28; shift >= 0; shift -= 4) {
        out.push_back(kHex[(hash >> shift) & 0xfu]);
    }
    return out;
}

// The serials of the bridges that are up now.
class Slots {
public:
    // The lowest free slot's serial, held until released.
    std::wstring claim(uint16_t vendorId, uint16_t productId)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (unsigned slot = 0;; ++slot) {
            std::wstring serial = for_slot(vendorId, productId, slot);
            if (taken_.insert(serial).second) return serial;
        }
    }

    void release(const std::wstring &serial)
    {
        if (serial.empty()) return;
        std::lock_guard<std::mutex> lock(mutex_);
        taken_.erase(serial);
    }

private:
    std::mutex mutex_;
    std::set<std::wstring> taken_;
};

// The listener's one set.
inline Slots g_slots;

}  // namespace usb_serial_fallback
