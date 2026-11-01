// A controller's polling rate: the limit a config can set, how reports are
// paced under it, and the readout of what Windows gets.

#include "harness.h"

#include <cstdint>
#include <vector>

#include "usbip/input_pace.inl"

using namespace ctmtest;
namespace ip = input_pace;

namespace {

// ⭐ Serves a stream of reports the way handle_interrupt_in does under a
// limit: a report that may go goes at once; one that may not waits for its
// turn, unless a newer one arrives first and takes its place. Returns when
// each served report went out.
std::vector<int64_t> serve_times(const std::vector<int64_t> &arrivals, int limit)
{
    ip::Pacer pace;
    std::vector<int64_t> served;
    for (size_t i = 0; i < arrivals.size(); ++i) {
        int64_t at = arrivals[i];
        if (!pace.may_serve(at, limit)) {
            const int64_t turn = pace.earliest_us(limit);
            if (i + 1 < arrivals.size() && arrivals[i + 1] <= turn) {
                continue;   // a newer report came before its turn
            }
            at = turn;
        }
        pace.served(at, limit);
        served.push_back(at);
    }
    return served;
}

// Reports every `gapUs` from 0 to `untilUs`, both included.
std::vector<int64_t> steady(int64_t gapUs, int64_t untilUs)
{
    std::vector<int64_t> out;
    for (int64_t t = 0; t <= untilUs; t += gapUs) out.push_back(t);
    return out;
}

}  // namespace

int run_input_pace_tests()
{
    section("polling rate: the limits a config can choose, and nothing else");
    {
        CTM_CHECK_EQ(ip::limit_hz("500"), 500);
        CTM_CHECK_EQ(ip::limit_hz("250"), 250);
        CTM_CHECK_EQ(ip::limit_hz("125"), 125);
        CTM_CHECK_EQ(ip::limit_hz("real_time"), 0);
        CTM_CHECK_EQ(ip::limit_hz("max"), 0);         // its name before
        CTM_CHECK_EQ(ip::limit_hz(""), 0);
        // ⛔ A value the page does not offer is no limit, never a slow pad.
        CTM_CHECK_EQ(ip::limit_hz("1000"), 0);
        CTM_CHECK_EQ(ip::limit_hz("25"), 0);
        CTM_CHECK_EQ(ip::limit_hz("250x"), 0);
        CTM_CHECK_EQ(ip::limit_hz(" 250"), 0);
        CTM_CHECK_EQ(ip::limit_hz("0"), 0);
    }

    section("polling rate: with no limit every report goes the moment it arrives");
    {
        const std::vector<int64_t> arrivals = steady(1000, 1000000);
        const std::vector<int64_t> served = serve_times(arrivals, 0);
        CTM_CHECK_EQ(served.size(), arrivals.size());
        CTM_CHECK(served == arrivals);
    }

    section("polling rate: a pad at 1000 a second, limited to 250");
    {
        const std::vector<int64_t> served = serve_times(steady(1000, 1000000), 250);
        // ⓘ The first two go together, then one every 4 ms: 2 + 250.
        CTM_CHECK_EQ(served.size(), 252u);
        bool even = true;
        for (size_t i = 3; i < served.size(); ++i) {
            if (served[i] - served[i - 1] != 4000) even = false;
        }
        CTM_CHECK(even);
    }

    section("polling rate: a pad at 250 a second, limited to 125");
    {
        const std::vector<int64_t> served = serve_times(steady(4000, 1000000), 125);
        CTM_CHECK_EQ(served.size(), 127u);
        bool even = true;
        for (size_t i = 3; i < served.size(); ++i) {
            if (served[i] - served[i - 1] != 8000) even = false;
        }
        CTM_CHECK(even);
    }

    section("polling rate: a pad at about the limit, early and late by turns, loses nothing");
    {
        // A Bluetooth pad at 250 a second, each report up to 0.3 ms off.
        std::vector<int64_t> arrivals;
        for (int64_t k = 0; k < 250; ++k) {
            arrivals.push_back(k * 4000 + ((k * 37) % 7 - 3) * 100);
        }
        arrivals[0] = 0;
        const std::vector<int64_t> served = serve_times(arrivals, 250);
        CTM_CHECK_EQ(served.size(), arrivals.size());
        CTM_CHECK(served == arrivals);   // and none of them waited
        // ⓘ A limit above the pad's own rate changes nothing either.
        CTM_CHECK(serve_times(arrivals, 500) == arrivals);
    }

    section("polling rate: a report before its turn waits for it, at most one interval");
    {
        // ⭐ The last report of a burst, with nothing after it, still goes:
        // at its turn, not when the next report happens to come.
        const std::vector<int64_t> served = serve_times({ 0, 1000, 2000 }, 250);
        CTM_CHECK_EQ(served.size(), 3u);
        CTM_CHECK_EQ(served[0], 0);
        CTM_CHECK_EQ(served[1], 1000);
        CTM_CHECK_EQ(served[2], 4000);

        ip::Pacer pace;
        pace.served(0, 125);
        pace.served(0, 125);
        CTM_CHECK(!pace.may_serve(7999, 125));
        CTM_CHECK(pace.may_serve(8000, 125));
        CTM_CHECK_EQ(pace.earliest_us(125), 8000);
    }

    section("polling rate: taking the limit off lets the next report straight through");
    {
        ip::Pacer pace;
        pace.served(0, 125);
        pace.served(100, 125);
        CTM_CHECK(!pace.may_serve(200, 125));
        CTM_CHECK(pace.may_serve(200, 0));
        pace.served(200, 0);
        CTM_CHECK_EQ(pace.dueUs, 0);
        // ⓘ And a limit set again starts afresh.
        CTM_CHECK(pace.may_serve(300, 250));
    }

    section("polling rate: the readout of a pad at 250 a second");
    {
        ip::RateMeter meter;
        CTM_CHECK_EQ(meter.rate_hz(0), 0);              // nothing yet
        meter.record(0);
        CTM_CHECK_EQ(meter.rate_hz(0), 0);              // one report says no rate
        for (int64_t t = 4000; t <= 500000; t += 4000) meter.record(t);
        CTM_CHECK_EQ(meter.rate_hz(500000), 250);       // the first second, so far
        for (int64_t t = 504000; t <= 1000000; t += 4000) meter.record(t);
        CTM_CHECK_EQ(meter.rate_hz(1000000), 250);      // a whole second
        CTM_CHECK_EQ(meter.lastHz, 250);
    }

    section("polling rate: the readout follows a change of rate");
    {
        ip::RateMeter meter;
        for (int64_t t = 0; t <= 2000000; t += 4000) meter.record(t);
        CTM_CHECK_EQ(meter.rate_hz(2000000), 250);
        for (int64_t t = 2008000; t <= 4000000; t += 8000) meter.record(t);
        CTM_CHECK_EQ(meter.rate_hz(4000000), 125);
        // A pad at 1000 a second.
        for (int64_t t = 4001000; t <= 6000000; t += 1000) meter.record(t);
        CTM_CHECK_EQ(meter.rate_hz(6000000), 1000);
    }

    section("polling rate: nothing for a second reads 0, not the last rate");
    {
        ip::RateMeter meter;
        for (int64_t t = 0; t <= 2000000; t += 4000) meter.record(t);
        CTM_CHECK_EQ(meter.rate_hz(2500000), 250);      // half a second quiet
        CTM_CHECK_EQ(meter.rate_hz(3000001), 0);        // a second quiet
        // ⓘ A keyboard: one report a key, a second and a half apart.
        ip::RateMeter keys;
        keys.record(0);
        keys.record(1500000);
        keys.record(3000000);
        CTM_CHECK_EQ(keys.rate_hz(3000000), 0);
    }

    return 0;
}
