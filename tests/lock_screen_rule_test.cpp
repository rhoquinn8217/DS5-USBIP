// What a controller reads once Windows is unlocked.
//
// ⭐ WHAT THESE PROTECT (rhoquinn8217, 2026-10-22): while Windows is locked
// every controller reads no config, so its own buttons reach the PIN prompt;
// at the unlock it reads its auto link, as a bridge would have given it, or,
// with none, what it read when the lock came.
//
// WHAT THESE CANNOT DO. No lock happens here: the rule is checked with the
// names a controller would give; the lock and the unlock are looked at with
// the listener running.

#include "harness.h"

#include "app/lock_screen_rule.inl"

using namespace ctmtest;

int run_lock_screen_rule_tests()
{
    using namespace lock_screen_rule;

    section("lock screen: what a controller reads at the unlock");
    {
        // bridged while locked: its auto link, held back till now
        CTM_CHECK(config_at_unlock("gyro_to_mouse_L2_aiming", "") == "gyro_to_mouse_L2_aiming");
        // read another config before the lock: the auto link, as at a bridge
        CTM_CHECK(config_at_unlock("gyro_to_mouse_L2_aiming", "touchpad_to_mouse") ==
                  "gyro_to_mouse_L2_aiming");
        // no auto link: what it read before the lock
        CTM_CHECK(config_at_unlock("", "touchpad_to_mouse") == "touchpad_to_mouse");
        // neither: no config still
        CTM_CHECK(config_at_unlock("", "").empty());
    }

    return 0;
}
