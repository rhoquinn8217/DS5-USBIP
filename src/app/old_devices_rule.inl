// Which of the devices Windows keeps from past bridges are ours to remove.
//
// ⭐ WHY (rhoquinn8217, 2026-10-13, after DS5 Bridge's device repair: "need to
// do this first"). Every bridge plugs a virtual device into usbip-win2's
// virtual USB hub, and Windows keeps a record of every device it has ever
// seen, connected or not, with its interfaces, its HID parts and its sound
// devices: 473 of them had piled up on rhoquinn8217's PC.
//
// ⛔⛔ OURS BY PARENT, NEVER BY VENDOR AND PRODUCT. That PC has two other
// virtual USB hubs, Nefarius ViGEm and VirtualHere, and old DS4 records with
// Sony's own ids belonged to ViGEm. Windows keeps the PARENT of a device that
// is not connected, so a record is ours when its chain of parents reaches
// usbip-win2's host controller (hardware id ROOT\USBIP_WIN2\UDE).
//
// ⛔ And only a device that is NOT connected: a bridge that is up, the
// synthetic mouse and keyboard, the hub and the host controller all stay.
//
// ⓘ Pure, so the tests reach it: old_devices.inl reads the records off Windows
// and does the removing.

#pragma once

#include <algorithm>
#include <cwctype>
#include <map>
#include <string>
#include <vector>

namespace old_devices_rule {

// One device Windows keeps a record of.
struct Record {
    std::wstring id;               // its instance id
    std::wstring parent;           // the parent Windows keeps, connected or not
    bool present = false;          // connected now
    bool usbipController = false;  // a usbip-win2 host controller itself
};

// One record to remove, and how far below the host controller it sits: 1 is
// the hub, 2 a device plugged into it, 3 and more that device's parts.
struct Removal {
    std::wstring id;
    int depth = 0;
};

inline std::wstring upper(const std::wstring &text)
{
    std::wstring out(text);
    for (wchar_t &c : out) c = static_cast<wchar_t>(std::towupper(c));
    return out;
}

// How many steps up its parents a record is from a usbip-win2 host
// controller, or -1 when the chain does not reach one. ⓘ Bounded, so a chain
// that loops (it should not) ends.
inline int depth_under_usbip(const std::map<std::wstring, const Record *> &byId, const Record &record)
{
    std::wstring at = upper(record.parent);
    for (int depth = 1; depth <= 16 && !at.empty(); ++depth) {
        const auto it = byId.find(at);
        if (it == byId.end()) return -1;
        if (it->second->usbipController) return depth;
        at = upper(it->second->parent);
    }
    return -1;
}

// The records to remove, the deepest first, so a part goes before the device
// it is part of.
inline std::vector<Removal> to_remove(const std::vector<Record> &records)
{
    std::map<std::wstring, const Record *> byId;
    for (const Record &r : records) byId[upper(r.id)] = &r;
    std::vector<Removal> out;
    for (const Record &r : records) {
        if (r.present || r.usbipController) continue;
        const int depth = depth_under_usbip(byId, r);
        if (depth > 0) out.push_back({ r.id, depth });
    }
    std::stable_sort(out.begin(), out.end(), [](const Removal &a, const Removal &b) { return a.depth > b.depth; });
    return out;
}

// How many of them are devices someone plugged in, rather than a hub or a
// device's parts: what a person would call "old controllers".
inline size_t devices_among(const std::vector<Removal> &removals)
{
    return static_cast<size_t>(std::count_if(removals.begin(), removals.end(), [](const Removal &r) { return r.depth == 2; }));
}

}  // namespace old_devices_rule
