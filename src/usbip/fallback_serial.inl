// The USB serial a virtual device gets when the TV sent none.
//
// ⛔⛔ WHY (code review, 2026-10-05). It was the constant "CTMUSBIP" for every
// such device. The serial names the device to Windows, and the Xbox id that
// XInput keys on is a hash of it (CtmMapRuntime::set_device_identity), so two
// serial-less pads bridged at once were one device twice: the second got no
// XInput slot while looking bridged from every other angle.
//
// ➡️ So the constant gains a hash of what tells such devices apart on the TV:
// the node it was opened through, and its vendor and product. Two pads bridged
// at once are on two nodes; the same pad on the same node keeps its id, so
// Windows finds the device it built last time.
//
// ⚠️ This is never a controller's IDENTITY. Per-controller config keys on the
// physical serial, which stays empty for these (device.inl says why).
// ⓘ Its own file so the test binary can reach it.

#include <cstdint>
#include <string>

namespace usb_serial_fallback {

inline std::wstring make(const std::wstring &path, uint16_t vendorId, uint16_t productId)
{
    uint32_t hash = 2166136261u;                       // FNV-1a, 32-bit
    auto mix = [&hash](uint32_t byte) {
        hash ^= byte & 0xffu;
        hash *= 16777619u;
    };
    for (wchar_t c : path) mix(static_cast<uint32_t>(c));
    mix(vendorId >> 8); mix(vendorId);
    mix(productId >> 8); mix(productId);
    static const wchar_t kHex[] = L"0123456789ABCDEF";
    std::wstring out = L"CTMUSBIP";
    for (int shift = 28; shift >= 0; shift -= 4) {
        out.push_back(kHex[(hash >> shift) & 0xfu]);
    }
    return out;
}

}  // namespace usb_serial_fallback
