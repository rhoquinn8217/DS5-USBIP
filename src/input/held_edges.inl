// The rebinder's "was this button held on the last report" flags, per pad and
// button, for the bindings that must fire once per press: the on-screen
// keyboard's toggle, a wheel click, and the settings window's Square.
//
// ⛔⛔ WHY ONE LOCKED TABLE (code review, 2026-10-05). These were five function
// statics, a std::map each, and every pad's input thread wrote them with no
// lock: two pads pressing at once could corrupt a map in the middle of an
// insert. And nothing ever removed a pad's entries, so a later pad handed the
// same address found its first press already "held" and lost it.
//
// ⓘ Its own file so the test binary can reach it.

namespace held_edges {

enum Which { kGateSquare, kGateOsk, kGateWheel, kOsk, kWheel };

struct Key {
    int which;
    const void *device;
    int button;
    bool operator<(const Key &other) const
    {
        if (which != other.which) return which < other.which;
        if (device != other.device) return std::less<const void *>()(device, other.device);
        return button < other.button;
    }
};

inline std::mutex g_mutex;
inline std::map<Key, bool> g_held;

// Stores `now` and returns what was stored before: false for a button not seen
// on this pad since it arrived.
inline bool exchange(Which which, const void *deviceKey, int button, bool now)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    bool &slot = g_held[Key{static_cast<int>(which), deviceKey, button}];
    const bool was = slot;
    slot = now;
    return was;
}

// A pad going away takes its flags with it.
inline void forget(const void *deviceKey)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    for (auto it = g_held.begin(); it != g_held.end();) {
        if (it->first.device == deviceKey) {
            it = g_held.erase(it);
        } else {
            ++it;
        }
    }
}

// For the tests: how many flags are held for one pad, pressed or not.
inline size_t entries_for(const void *deviceKey)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    size_t n = 0;
    for (const auto &entry : g_held) {
        if (entry.first.device == deviceKey) ++n;
    }
    return n;
}

}  // namespace held_edges
