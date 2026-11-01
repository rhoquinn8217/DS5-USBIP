// The quick config switch: its setting, its gestures, and the keys a step is
// sent to the page as.
//
// ⭐ WHAT THESE PROTECT (rhoquinn8217, 2026-10-23): two fingers pressing the
// touchpad arm the switch, once per press, and the window opens as the press
// comes up, under four seconds, the TV's release (2026-10-28); each swipe of a
// finger up steps a
// controller to the next config and down to the one before, one thumb resting
// and the other swiping, and two fingers swiped together step once; with the
// window in front in Simple or Quick (armed), the same, with no press, while
// both fingers are down (2026-10-26), one finger alone stepping nothing; the
// window a press opened closes once it is there, never before, so fingers
// lifted at once still close it (2026-10-27); and each step reaches the page
// as one press of its own key.
//
// WHAT THESE CANNOT DO. No window opens here and no pad is read: the window,
// the gate and the link are looked at with the listener running.

#include "harness.h"

#include "app/quick_switch.inl"

using namespace ctmtest;

int run_quick_switch_tests()
{
    using namespace quick_switch;

    section("quick switch: the setting, off unless the line says 1");
    {
        CTM_CHECK(parse("1"));
        CTM_CHECK(!parse("0"));
        CTM_CHECK(!parse(""));
        CTM_CHECK(!parse("yes"));
        CTM_CHECK(line(true) == "quick_config_switch = 1\n");
        CTM_CHECK(line(false) == "quick_config_switch = 0\n");
    }

    section("quick switch: on out of the box, and on once for an older install");
    {
        CTM_CHECK(kDefault);
        CTM_CHECK(default_line() == "quick_config_switch_default = on\n");
        CTM_CHECK(loaded(false, false, false));   // no file, or no line
        CTM_CHECK(loaded(true, false, false));    // "off" from before the change: dropped
        CTM_CHECK(!loaded(true, false, true));    // turned off since: kept
        CTM_CHECK(loaded(true, true, true));
        CTM_CHECK(loaded(false, false, true));
    }

    section("quick switch: two fingers pressing arm it, once per press");
    {
        Press p;
        CTM_CHECK(!arms(p, 2, false));
        CTM_CHECK(arms(p, 2, true));
        CTM_CHECK(!arms(p, 2, true));      // held: not again
        CTM_CHECK(!arms(p, 1, false));
        CTM_CHECK(!arms(p, 1, true));      // one finger pressing: no
        CTM_CHECK(!arms(p, 0, false));
        CTM_CHECK(arms(p, 2, true));       // let go and pressed again: yes
        Press q;
        CTM_CHECK(!arms(q, 0, true));      // pressed with no finger: no
    }

    section("quick switch: the window opens as the press comes up, under four seconds, a finger on");
    {
        // ⓘ rhoquinn8217, 2026-10-28: "don't bring up the quick mode until
        // the press is released and only if the press is less than 4 seconds".
        Armed a;
        CTM_CHECK(armed_report(a, false, 2, 0) == Up::None);      // not armed: nothing
        arm(a, 1000);
        CTM_CHECK(armed_report(a, true, 2, 1500) == Up::None);    // still pressed
        CTM_CHECK(armed_report(a, false, 2, 1800) == Up::Open);   // up, two fingers on
        CTM_CHECK(!a.on);
        arm(a, 0);
        CTM_CHECK(armed_report(a, true, 1, 2000) == Up::None);    // a finger off, still pressed
        CTM_CHECK(armed_report(a, false, 1, 3999) == Up::Open);   // just under, one finger on
        arm(a, 0);
        CTM_CHECK(armed_report(a, false, 0, 200) == Up::None);    // every finger up with it
        CTM_CHECK(!a.on);
    }

    section("quick switch: a press held four seconds is the TV's release, and opens nothing");
    {
        Armed a;
        arm(a, 0);
        CTM_CHECK(armed_report(a, true, 2, 3999) == Up::None);
        CTM_CHECK(armed_report(a, true, 2, 4000) == Up::TooLong); // once
        CTM_CHECK(armed_report(a, true, 2, 4500) == Up::None);
        CTM_CHECK(armed_report(a, false, 2, 4600) == Up::None);   // up, fingers on: still held
        CTM_CHECK(a.on);
        CTM_CHECK(armed_report(a, false, 0, 4700) == Up::None);   // the last finger lifts
        CTM_CHECK(!a.on);
        arm(a, 0);
        CTM_CHECK(armed_report(a, false, 2, 5000) == Up::None);   // up late, no report between
        CTM_CHECK(a.on);
        CTM_CHECK(armed_report(a, false, 0, 5100) == Up::None);
        CTM_CHECK(!a.on);
        arm(a, 0);
        CTM_CHECK(armed_report(a, true, 2, 4200) == Up::TooLong);
        arm(a, 6000);                                             // pressed again: armed afresh
        CTM_CHECK(armed_report(a, false, 2, 6300) == Up::Open);
    }

    section("quick switch: a press is seen as it goes in, with any number of fingers");
    {
        Press p;
        CTM_CHECK(press_edge(p, true));
        CTM_CHECK(!press_edge(p, true));         // held
        CTM_CHECK(!press_edge(p, false));
        CTM_CHECK(press_edge(p, true));          // in again
    }

    section("quick switch: while the window holds the touchpad, only the two asked for");
    {
        // ⓘ rhoquinn8217, 2026-10-27: "1 finger scroll up or down while the
        // other finger stays put" and "press again to lift restriction and
        // the config window persists after both fingers are lifted".
        CTM_CHECK(while_held(false, 2, 0) == Held::Stay);
        CTM_CHECK(while_held(false, 1, 0) == Held::Stay);
        CTM_CHECK(while_held(false, 2, 1) == Held::Step);
        CTM_CHECK(while_held(false, 1, -1) == Held::Step);
        CTM_CHECK(while_held(true, 2, 0) == Held::Keep);    // pressed again: kept
        CTM_CHECK(while_held(true, 1, 0) == Held::Keep);    // with one finger too
        CTM_CHECK(while_held(true, 2, 1) == Held::Keep);    // the press wins a step
        CTM_CHECK(while_held(false, 0, 0) == Held::Close);  // every finger up
    }

    section("quick switch: one thumb resting, the other swiping up then down, a step a swipe");
    {
        // ⓘ rhoquinn8217, 2026-10-23: "both thumbs touch and press and
        // release the touchpad ... Then the only the right finger is
        // lifted.  Swipe up and swipe down with only the right thumb".
        Swipes s;
        Touch t[2];
        t[0] = { true, 1, 400.0f, 500.0f };       // the left thumb, resting
        t[1] = { true, 2, 1500.0f, 500.0f };      // the right, that pressed with it
        CTM_CHECK_EQ(swipe_steps(s, t), 0);
        t[1].down = false;                       // the right lifts
        CTM_CHECK_EQ(swipe_steps(s, t), 0);
        t[1] = { true, 3, 1500.0f, 800.0f };      // and lands low
        CTM_CHECK_EQ(swipe_steps(s, t), 0);
        t[1].y = 600.0f;                         // 200 up: not yet
        CTM_CHECK_EQ(swipe_steps(s, t), 0);
        t[1].y = 450.0f;                         // 350 up: the next config
        CTM_CHECK_EQ(swipe_steps(s, t), 1);
        t[1].y = 100.0f;                         // further: still the one step
        CTM_CHECK_EQ(swipe_steps(s, t), 0);
        t[1].down = false;
        CTM_CHECK_EQ(swipe_steps(s, t), 0);
        t[1] = { true, 4, 1500.0f, 300.0f };      // lands high
        CTM_CHECK_EQ(swipe_steps(s, t), 0);
        t[1].y = 650.0f;                         // 350 down: the config before
        CTM_CHECK_EQ(swipe_steps(s, t), -1);
        t[1].down = false;
        CTM_CHECK_EQ(swipe_steps(s, t), 0);
        // the resting thumb never moved, and never stepped
    }

    section("quick switch: a finger's swipe once per touch; sideways first is none; slots may swap");
    {
        Swipes s;
        Touch t[2];
        t[0] = { true, 9, 900.0f, 800.0f };
        CTM_CHECK_EQ(swipe_steps(s, t), 0);
        t[0].x = 1500.0f;                        // 600 sideways: decided, nothing
        CTM_CHECK_EQ(swipe_steps(s, t), 0);
        t[0].y = 100.0f;                         // then up: still nothing this touch
        CTM_CHECK_EQ(swipe_steps(s, t), 0);
        // the same finger reported in the other slot is still the same finger
        Swipes q;
        Touch u[2];
        u[0] = { true, 5, 400.0f, 500.0f };
        u[1] = { true, 6, 1500.0f, 900.0f };
        CTM_CHECK_EQ(swipe_steps(q, u), 0);
        Touch w[2];
        w[0] = { true, 6, 1500.0f, 550.0f };      // finger 6, now first: 350 up
        w[1] = { true, 5, 400.0f, 500.0f };
        CTM_CHECK_EQ(swipe_steps(q, w), 1);
    }

    section("quick switch: two fingers swiped together step once, not twice");
    {
        Swipes s;
        Touch t[2];
        t[0] = { true, 1, 700.0f, 800.0f };
        t[1] = { true, 2, 1200.0f, 800.0f };
        CTM_CHECK_EQ(swipe_steps(s, t), 0);
        t[0].y = 450.0f;                         // the first gets there first: the step
        t[1].y = 600.0f;
        CTM_CHECK_EQ(swipe_steps(s, t), 1);
        t[1].y = 400.0f;                         // the second gets there too: nothing more
        CTM_CHECK_EQ(swipe_steps(s, t), 0);
        // both getting there in one report: the one step
        Swipes q;
        Touch u[2];
        u[0] = { true, 3, 700.0f, 300.0f };
        u[1] = { true, 4, 1200.0f, 300.0f };
        CTM_CHECK_EQ(swipe_steps(q, u), 0);
        u[0].y = 700.0f;
        u[1].y = 700.0f;
        CTM_CHECK_EQ(swipe_steps(q, u), -1);
        // a finger landing afresh beside one that is decided still steps
        u[1] = { true, 5, 1200.0f, 800.0f };
        CTM_CHECK_EQ(swipe_steps(q, u), 0);
        u[1].y = 450.0f;
        CTM_CHECK_EQ(swipe_steps(q, u), 1);
    }

    section("quick switch: the wheel in front, one thumb resting and the other swiping, a step a swipe");
    {
        // ⓘ rhoquinn8217, 2026-10-26: "the same 1 thumb touch and 1 thumb
        // swipe to switch to next or previous config".
        Swipes s;
        Touch t[2];
        t[0] = { true, 1, 400.0f, 500.0f };       // the left thumb, alone: the cursor's
        CTM_CHECK_EQ(front_steps(s, true, t), 0);
        t[0].y = 100.0f;                         // moving alone steps nothing
        CTM_CHECK_EQ(front_steps(s, true, t), 0);
        t[1] = { true, 2, 1500.0f, 800.0f };      // the right lands beside it
        CTM_CHECK_EQ(front_steps(s, true, t), 0);
        t[1].y = 600.0f;                         // 200 up: not yet
        CTM_CHECK_EQ(front_steps(s, true, t), 0);
        t[1].y = 450.0f;                         // 350 up: the next config
        CTM_CHECK_EQ(front_steps(s, true, t), 1);
        t[1].y = 100.0f;                         // further: still the one step
        CTM_CHECK_EQ(front_steps(s, true, t), 0);
        t[1].down = false;                       // the right lifts, the left stays
        CTM_CHECK_EQ(front_steps(s, true, t), 0);
        t[1] = { true, 3, 1500.0f, 300.0f };      // lands high
        CTM_CHECK_EQ(front_steps(s, true, t), 0);
        t[1].y = 650.0f;                         // 350 down: the config before
        CTM_CHECK_EQ(front_steps(s, true, t), -1);
    }

    section("quick switch: the wheel in front, nothing unarmed or with one finger; afresh each time");
    {
        Swipes s;
        Touch t[2];
        t[0] = { true, 1, 400.0f, 500.0f };
        t[1] = { true, 2, 1500.0f, 800.0f };
        CTM_CHECK_EQ(front_steps(s, false, t), 0);
        t[1].y = 400.0f;                         // 400 up, but not armed
        CTM_CHECK_EQ(front_steps(s, false, t), 0);
        // armed now, with the right at 400: 200 more up is not enough
        CTM_CHECK_EQ(front_steps(s, true, t), 0);
        t[1].y = 200.0f;
        CTM_CHECK_EQ(front_steps(s, true, t), 0);
        t[1].y = 90.0f;
        CTM_CHECK_EQ(front_steps(s, true, t), 1);
        // one finger alone, armed, swiping the whole pad: nothing
        Swipes q;
        Touch u[2];
        u[1] = { true, 7, 1500.0f, 900.0f };
        CTM_CHECK_EQ(front_steps(q, true, u), 0);
        u[1].y = 100.0f;
        CTM_CHECK_EQ(front_steps(q, true, u), 0);
        // a second finger landing then: measured from where the first is now
        u[0] = { true, 8, 400.0f, 500.0f };
        CTM_CHECK_EQ(front_steps(q, true, u), 0);
        u[1].y = 0.0f;                           // 100 more up: nothing
        CTM_CHECK_EQ(front_steps(q, true, u), 0);
    }

    section("quick switch: a second press's layout key goes once, after the steps");
    {
        // ⓘ rhoquinn8217, 2026-10-28: "on the second press, it should bring
        // you to simple or advance based on what you had open last".
        Keys k;
        queue(k, 1);
        queue_layout(k);
        CTM_CHECK(!idle(k));
        std::string sent;
        bool down = false;
        for (long long t = 0; t < 400; t += 4) {
            const char *code = key_now(k, t);
            if (code != nullptr && !down) sent += std::string(code) + " ";
            down = code != nullptr;
        }
        CTM_CHECK(sent == "KeyJ KeyV ");
        CTM_CHECK(idle(k));
        // alone, it goes at once
        Keys q;
        queue_layout(q);
        CTM_CHECK(std::string(key_now(q, 1000)) == "KeyV");
        CTM_CHECK(key_now(q, 1040) == nullptr);
        CTM_CHECK(key_now(q, 1080) == nullptr);
        CTM_CHECK(idle(q));
    }

    section("quick switch: lifted while the window is still coming, it closes once it is there");
    {
        // ⓘ rhoquinn8217, 2026-10-27: "If I press the touchpad with two
        // fingers then release immediately, the config window doesn't go away
        // when it should".
        const long long lift = 1000;
        CTM_CHECK(!close_now(1000, lift, true, false, -1, true));    // still coming
        CTM_CHECK(!close_now(1400, lift, true, false, -1, true));
        CTM_CHECK(close_now(1410, lift, false, false, -1, true));    // there, nothing to send: now
        // there and its page up, nothing to send: now
        CTM_CHECK(close_now(1500, lift, false, true, 1450, true));
    }

    section("quick switch: steps still to send wait for the page, then 300 ms at most");
    {
        const long long lift = 1000;
        CTM_CHECK(!close_now(1200, lift, false, false, -1, false));  // the page has not asked
        CTM_CHECK(!close_now(1600, lift, false, true, 1500, false)); // asked: 100 ms in
        CTM_CHECK(close_now(1800, lift, false, true, 1500, false));  // 300 ms after it asked
        CTM_CHECK(close_now(1300, lift, false, true, 900, false));   // asked before the lift: 300 from the lift
        CTM_CHECK(close_now(1050, lift, false, true, 900, true));    // all sent: now
    }

    section("quick switch: nothing waits past four seconds from the lift");
    {
        const long long lift = 1000;
        CTM_CHECK(!close_now(4999, lift, true, false, -1, false));
        CTM_CHECK(close_now(5000, lift, true, false, -1, false));    // a window that never came
        CTM_CHECK(close_now(5000, lift, false, false, -1, false));   // a page that never asked
    }

    section("quick switch: a step is one press of its key, then a gap");
    {
        Keys k;
        CTM_CHECK(key_now(k, 0) == nullptr);
        CTM_CHECK(idle(k));
        queue(k, 1);
        CTM_CHECK(std::string(key_now(k, 1000)) == "KeyJ");
        CTM_CHECK(std::string(key_now(k, 1039)) == "KeyJ");
        CTM_CHECK(key_now(k, 1040) == nullptr);    // let go
        CTM_CHECK(key_now(k, 1079) == nullptr);    // the gap
        CTM_CHECK(key_now(k, 1080) == nullptr);    // nothing more
        CTM_CHECK(idle(k));
    }

    section("quick switch: two steps are two presses; the one before is KeyK");
    {
        Keys k;
        queue(k, 1);
        queue(k, 1);
        int presses = 0;
        bool down = false;
        for (long long t = 0; t < 400; t += 4) {
            const char *code = key_now(k, t);
            if (code != nullptr && !down) ++presses;
            down = code != nullptr;
        }
        CTM_CHECK_EQ(presses, 2);
        CTM_CHECK(idle(k));
        queue(k, -1);
        CTM_CHECK(std::string(key_now(k, 1000)) == "KeyK");
    }

    return 0;
}
