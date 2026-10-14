// A controller's polling rate: how many input reports a second Windows gets
// from it, and the limit a config can set on that.
//
// ⭐ WHY (rhoquinn8217, 2026-10-13: "I want a polling rate settings for
// controllers like the DS5 Bridge"). Nothing here sets a rate by default. The
// TV sends each report the moment it reads it, and the listener answers the
// request Windows has waiting with it at once, so Windows gets every report
// the controller sends, as soon as it arrives. ➡️ A limit can therefore only
// LOWER the rate. The readout shows what Windows actually gets.
//
// ⭐ WHAT A LIMIT DOES. At most that many reports a second are served, and
// the one served is always the NEWEST: a report that comes before its turn
// waits for it, at most one interval, and a newer one arriving meanwhile takes
// its place, so nothing piles up behind.
//
// ⓘ Pure, with no clock of its own: device.inl hands every call the time, and
// the tests drive it the same way.

#pragma once

#include <cstdint>
#include <string>

namespace input_pace {

// The limit a config's polling_rate asks for, in reports a second, or 0 for
// none. The text is as device_config_str hands it, lowercased.
//
// ⛔ ONLY THE VALUES THE PAGE OFFERS. "real_time", blank, and anything else
// at all mean no limit: a typo in a hand-edited file must never slow a
// controller down, and a hand-typed 25 would make it feel broken. ⓘ So does
// "max", the first choice's name before it took "Real time" from the
// DS5Dongle and its forks (rhoquinn8217, 2026-10-14).
inline int limit_hz(const std::string &text)
{
    if (text == "500") return 500;
    if (text == "250") return 250;
    if (text == "125") return 125;
    return 0;
}

// ⭐ THE PACE, kept as the time the next report is due. Each report served
// moves that on by one interval, from whichever is later, the time it was due
// or now.
//
// ⭐ A REPORT MAY GO UP TO ONE INTERVAL EARLY. A controller sending at about
// the limit arrives a little early and a little late by turns, and if each
// report had to wait a full interval after the one before, every early one
// would be held back and half of them lost. With the allowance, one that comes
// early after one that came late still goes at once.
// ⓘ So a second carries at most a report or two over the limit, and two go
// close together only after the controller has been slower than the limit.
struct Pacer {
    int64_t dueUs = 0;   // when the next report is due; 0 until one is served

    static int64_t interval_us(int limit)
    {
        return limit > 0 ? 1000000 / limit : 0;
    }

    // The earliest the next report may be served.
    int64_t earliest_us(int limit) const
    {
        return dueUs - interval_us(limit);
    }

    bool may_serve(int64_t nowUs, int limit) const
    {
        return limit <= 0 || nowUs >= earliest_us(limit);
    }

    // ⓘ With no limit it forgets, so a limit set later starts afresh.
    void served(int64_t nowUs, int limit)
    {
        if (limit <= 0) {
            dueUs = 0;
            return;
        }
        dueUs = (dueUs > nowUs ? dueUs : nowUs) + interval_us(limit);
    }
};

// ⭐ THE READOUT: reports counted over windows of a second. A window closes at
// the first report a second or more after it opened, and its rate is what is
// shown until the next one closes. In the first second, before any has
// closed, it is the rate so far.
//
// ⛔ NOTHING FOR A SECOND READS 0, never the last rate: a controller that has
// stopped must not look as if it is still sending. A keyboard reads 0 between
// keys for the same reason: it sends a report only when a key changes.
struct RateMeter {
    int64_t windowStartUs = -1;  // the window's first report
    int64_t lastUs = -1;         // the latest report
    int64_t windowCount = 0;     // reports in the window, its first included
    int lastHz = -1;             // the last closed window's rate; -1 before one

    // ⓘ The rate between a window's first report and its last: the gaps
    // between them over the time they span.
    static int rate_of(int64_t count, int64_t spanUs)
    {
        if (count < 2 || spanUs <= 0) return 0;
        return static_cast<int>(((count - 1) * 1000000 + spanUs / 2) / spanUs);
    }

    void record(int64_t nowUs)
    {
        // ⓘ The first report, or the first after a silence: a new start.
        if (lastUs < 0 || nowUs - lastUs > 1000000) {
            windowStartUs = nowUs;
            windowCount = 0;
            lastHz = -1;
        }
        lastUs = nowUs;
        ++windowCount;
        if (nowUs - windowStartUs >= 1000000) {
            lastHz = rate_of(windowCount, nowUs - windowStartUs);
            // ⓘ This report closes one window and opens the next.
            windowStartUs = nowUs;
            windowCount = 1;
        }
    }

    int rate_hz(int64_t nowUs) const
    {
        if (lastUs < 0 || nowUs - lastUs > 1000000) return 0;
        if (lastHz >= 0) return lastHz;
        return rate_of(windowCount, lastUs - windowStartUs);
    }
};

}  // namespace input_pace
