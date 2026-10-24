// The quick config switch: its setting, its gestures, and the keys a step is
// sent to the page as.
//
// ⭐ WHAT THESE PROTECT (rhoquinn8217, 2026-10-23): two fingers pressing the
// touchpad open the window, once per press; each swipe of a finger up steps a
// controller to the next config and down to the one before, one thumb resting
// and the other swiping; with the window in front in Simple or Quick (armed),
// two fingers swiped up or down step it, once per swipe, nothing sideways;
// and each step reaches the page as one press of its own key.
//
// WHAT THESE CANNOT DO. No window opens here and no pad is read: the window,
// the gate and the link are looked at with the listener running.

#include "harness.h"

#include "app/quick_switch.inl"

using namespace ctmtest;

namespace {

// A swipe from the pad's middle: armed, two fingers down, then moved.
int swipe(quick_switch::Pad &p, float dx, float dy, bool armed = true)
{
    int got = 0;
    got += quick_switch::step(p, armed, 2, 960.0f, 540.0f);
    got += quick_switch::step(p, armed, 2, 960.0f + dx / 2.0f, 540.0f + dy / 2.0f);
    got += quick_switch::step(p, armed, 2, 960.0f + dx, 540.0f + dy);
    return got;
}

void lift(quick_switch::Pad &p, bool armed = true)
{
    quick_switch::step(p, armed, 0, 0.0f, 0.0f);
}

}  // namespace

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

    section("quick switch: two fingers up is the next config, down the one before");
    {
        Pad p;
        CTM_CHECK_EQ(swipe(p, 0.0f, -400.0f), 1);
        lift(p);
        CTM_CHECK_EQ(swipe(p, 0.0f, 400.0f), -1);
        lift(p);
        // a little sideways on the way is still up
        CTM_CHECK_EQ(swipe(p, 150.0f, -400.0f), 1);
    }

    section("quick switch: nothing unarmed, too short, or sideways");
    {
        Pad p;
        CTM_CHECK_EQ(swipe(p, 0.0f, -400.0f, false), 0);
        lift(p, false);
        CTM_CHECK_EQ(swipe(p, 0.0f, -200.0f), 0);
        lift(p);
        CTM_CHECK_EQ(swipe(p, 600.0f, -100.0f), 0);
        // sideways decides the touch: coming back up after it steps nothing
        CTM_CHECK_EQ(quick_switch::step(p, true, 2, 960.0f, 0.0f), 0);
        lift(p);
        // one finger is not the gesture
        CTM_CHECK_EQ(quick_switch::step(p, true, 1, 960.0f, 540.0f), 0);
        CTM_CHECK_EQ(quick_switch::step(p, true, 1, 960.0f, 0.0f), 0);
    }

    section("quick switch: once per touch, again after every finger lifts");
    {
        Pad p;
        CTM_CHECK_EQ(swipe(p, 0.0f, -400.0f), 1);
        // the fingers keep going: still the one step
        CTM_CHECK_EQ(quick_switch::step(p, true, 2, 960.0f, -400.0f), 0);
        CTM_CHECK_EQ(quick_switch::step(p, true, 2, 960.0f, 940.0f), 0);
        lift(p);
        CTM_CHECK_EQ(swipe(p, 0.0f, -400.0f), 1);
    }

    section("quick switch: unarmed mid-touch, then armed again, measures afresh");
    {
        Pad p;
        CTM_CHECK_EQ(quick_switch::step(p, true, 2, 960.0f, 900.0f), 0);
        CTM_CHECK_EQ(quick_switch::step(p, false, 2, 960.0f, 700.0f), 0);
        // held again with the fingers at 700: 200 more up is not enough
        CTM_CHECK_EQ(quick_switch::step(p, true, 2, 960.0f, 700.0f), 0);
        CTM_CHECK_EQ(quick_switch::step(p, true, 2, 960.0f, 500.0f), 0);
        CTM_CHECK_EQ(quick_switch::step(p, true, 2, 960.0f, 390.0f), 1);
    }

    section("quick switch: two fingers pressing open it, once per press");
    {
        Press p;
        CTM_CHECK(!opens(p, 2, false));
        CTM_CHECK(opens(p, 2, true));
        CTM_CHECK(!opens(p, 2, true));     // held: not again
        CTM_CHECK(!opens(p, 1, false));
        CTM_CHECK(!opens(p, 1, true));     // one finger pressing: no
        CTM_CHECK(!opens(p, 0, false));
        CTM_CHECK(opens(p, 2, true));      // let go and pressed again: yes
        Press q;
        CTM_CHECK(!opens(q, 0, true));     // pressed with no finger: no
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
