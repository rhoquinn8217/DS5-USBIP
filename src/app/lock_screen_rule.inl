// What a controller reads while Windows is locked, and after.
//
// ⭐ rhoquinn8217, 2026-10-22: *"I need a reliable away to use a controller to
// unlock the computer"*, with *"I would prefer that auto link during a lock
// screen is disabled, that way the native controller controls can be used to
// unlock the PC"*, then *"always on"*. Windows' PIN prompt takes a
// controller's own buttons as digits -- proven the same evening with a bridged
// DualSense on no config -- and a config's remaps can take those buttons away.
// A bad auto link could then keep someone out, with no way to the configs
// window, which the lock screen hides.
// ➡️ So while Windows is locked every controller reads no config, and at the
// unlock it reads its auto link -- the starting point the lock held back -- or,
// with none, what it read when the lock came.
//
// ⓘ Its own file, with no windows.h, so the test binary can reach the rule;
// agent.inl watches the lock and moves the controllers.
#pragma once

#include <string>

namespace lock_screen_rule {

inline std::string config_at_unlock(const std::string &autoLink, const std::string &beforeLock)
{
    return autoLink.empty() ? beforeLock : autoLink;
}

}  // namespace lock_screen_rule
