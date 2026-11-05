// When Windows' default speaker is taken back from a controller.
//
// ⭐ WHY (rhoquinn8217, 2026-11-04: "yes, put the default speaker back"). A
// bridged controller with a speaker is one of Windows' sound devices, and
// Windows can make it the default speaker as it arrives: a cabled DS4's,
// which Windows takes for a headset, did so for all three roles at a bridge.
// What plays on the default then comes out of the pad, and the streaming
// host, which streams whatever the default plays, sends it to the TV as well.
// ⓘ A DualSense's is "Speakers" to Windows, and none was seen taking it; the
// rule is the same for both.
//
// ⭐ ONLY WHAT WINDOWS DID. The default is taken back when it goes to a
// controller within kArrivalMs of that controller arriving. Later, it was a
// person's choice, and it stays.
// ⓘ A change heard before the arrival counts as one just after it: Windows
// tells of the two separately, and they can come in either order.
// ⛔ And never more than kMostTakeBacks times for one arrival, so a Windows
// that hands it straight back again is not fought for ever.
//
// ⓘ Pure, so the tests reach it: default_speaker.cpp hears Windows and does
// the taking back.

#pragma once

#include <cstdint>
#include <map>
#include <string>

namespace default_speaker_rule {

// Windows' three roles, in ERole's order: console, multimedia, communications.
constexpr int kRoles = 3;

// How long after a controller arrives the default going to it is still
// Windows' doing. At the bridges of 2026-11-04 it went within a second.
constexpr uint64_t kArrivalMs = 10000;

// Each role twice: the most one arrival is given back.
constexpr int kMostTakeBacks = 2 * kRoles;

class Rule {
public:
    // A controller's sound device that was there before anything was heard:
    // it is left alone, whatever it is given.
    void present_before(const std::wstring &id) { arrivals_[id] = Arrival{ kBefore, 0 }; }

    // A controller's sound device arriving, or coming back.
    void arrived(const std::wstring &id, uint64_t atMs) { arrivals_[id] = Arrival{ atMs, 0 }; }

    // A sound device going.
    void left(const std::wstring &id) { arrivals_.erase(id); }

    // Windows made `id` the default speaker for `role` at `atMs`. The device
    // to make the default again, or an empty string to leave it as it is.
    std::wstring default_changed(int role, const std::wstring &id, bool controller, uint64_t atMs)
    {
        if (role < 0 || role >= kRoles || id.empty()) return std::wstring();
        if (!controller) {
            kept_[role] = id;
            return std::wstring();
        }
        auto found = arrivals_.find(id);
        if (found == arrivals_.end()) found = arrivals_.emplace(id, Arrival{ atMs, 0 }).first;
        Arrival &arrival = found->second;
        if (arrival.atMs == kBefore) return std::wstring();
        const uint64_t since = atMs > arrival.atMs ? atMs - arrival.atMs : 0;
        if (since > kArrivalMs || kept_[role].empty() || arrival.takenBack >= kMostTakeBacks) return std::wstring();
        ++arrival.takenBack;
        return kept_[role];
    }

    // The last default for `role` that was not a controller's.
    std::wstring kept(int role) const { return role >= 0 && role < kRoles ? kept_[role] : std::wstring(); }

private:
    struct Arrival {
        uint64_t atMs;
        int takenBack;
    };
    static constexpr uint64_t kBefore = UINT64_MAX;
    std::map<std::wstring, Arrival> arrivals_;
    std::wstring kept_[kRoles];
};

}  // namespace default_speaker_rule
