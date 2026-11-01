// A one-millisecond timer for as long as any controller has a rate limit.
//
// ⭐ WHY. A report that comes before its turn waits on a timer (input_pace.inl),
// and Windows' timer ticks every 15.6 ms unless a program asks for finer --
// longer than the whole interval at 500, 250 or 125 a second. So while any
// controller has a limit, the listener asks for 1 ms, and gives it back when
// none has.
//
// ⚠️ AND WINDOWS 11 IGNORES THE REQUEST from a program whose windows are all
// out of sight, which the listener's are while it sits in the tray -- unless
// the program says it wants it kept anyway, which is the first call below.
// ⓘ That one is left in place: with no request standing it changes nothing.

#pragma once

#ifndef PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION
#define PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION 0x4
#endif

namespace fine_timer {

inline std::mutex &holders_mutex()
{
    static std::mutex m;
    return m;
}

inline int &holders()
{
    static int n = 0;
    return n;
}

inline void acquire()
{
    std::lock_guard<std::mutex> lock(holders_mutex());
    if (holders()++ != 0) return;
    PROCESS_POWER_THROTTLING_STATE keep = {};
    keep.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
    keep.ControlMask = PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION;
    keep.StateMask = 0;   // not ignored: kept
    SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &keep, sizeof(keep));
    timeBeginPeriod(1);
}

inline void release()
{
    std::lock_guard<std::mutex> lock(holders_mutex());
    if (holders() == 0) return;
    if (--holders() != 0) return;
    timeEndPeriod(1);
}

}  // namespace fine_timer
