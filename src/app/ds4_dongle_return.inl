#pragma once
// ⭐ A DS4DONGLE THIS LISTENER RESTARTED COMES BACK AS ITSELF (2026-11-04).
//
// Setting a DS4Dongle to play its speaker without headphones restarts it
// (ds4_dongle_keep.inl), which ends the bridge, and the TV bridges the pad again
// a few seconds later as a NEW session, under a new nickname: the restart
// would read as another pad arriving. ⛔ And a dongle that did not keep the
// setting would be set and restarted again at every bridge, for ever.
//
// ➡️ So the restart is remembered: which pad, from which TV, its nickname, and
// what was written to it. The same pad back from the same TV within kWindow
// takes its name back, and its bridge knows what it was given, once.
//
// ⓘ Pure: the agent holds one List behind its own mutex and gives it the clock.

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace ds4_dongle_return {

using Clock = std::chrono::steady_clock;

// ⓘ Longer than the TV's own window for taking the pad back unasked (15 s, in
// the TV app's glue): its session starts a second or so after the TV bridges it.
constexpr std::chrono::seconds kWindow{20};

struct Who {
    std::string owner;          // the TV, as tv_sessions.inl names it
    uint16_t vendorId = 0;
    uint16_t productId = 0;
    std::string serial;
    std::string product;
};

struct Entry {
    Who who;
    std::string nickname;
    // The audio_follow_jack written to it before it left, -1 none.
    int followJack = -1;
    Clock::time_point until{};
};

// One pad, as a re-bridge of it is told from another: the same TV, and the same
// device by same_device.inl's rule.
inline bool same_pad(const Who &a, const Who &b)
{
    return tv_sessions::same_tv(a.owner, b.owner) &&
           same_device::matches(a.vendorId, a.productId, a.serial, a.product,
                                b.vendorId, b.productId, b.serial, b.product);
}

class List {
public:
    // A restart was asked for. Replaces whatever was kept for the same pad.
    void expect(const Entry &entry, Clock::time_point now)
    {
        drop_expired(now);
        for (Entry &kept : entries_) {
            if (same_pad(kept.who, entry.who)) {
                kept = entry;
                kept.until = now + kWindow;
                return;
            }
        }
        entries_.push_back(entry);
        entries_.back().until = now + kWindow;
    }

    // The pad is back: what was kept for it, once. False when nothing was.
    bool take(const Who &who, Clock::time_point now, Entry *out)
    {
        drop_expired(now);
        for (auto it = entries_.begin(); it != entries_.end(); ++it) {
            if (!same_pad(it->who, who)) continue;
            if (out != nullptr) *out = *it;
            entries_.erase(it);
            return true;
        }
        return false;
    }

    size_t size() const { return entries_.size(); }

private:
    void drop_expired(Clock::time_point now)
    {
        entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                      [now](const Entry &e) { return e.until <= now; }),
                       entries_.end());
    }

    std::vector<Entry> entries_;
};

} // namespace ds4_dongle_return
