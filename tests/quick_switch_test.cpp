// The quick config switch: its setting, its gestures, and the keys a step is
// sent to the page as.
//
// ⭐ WHAT THESE PROTECT (rhoquinn8217, 2026-10-23): two fingers pressing the
// touchpad open the window, once per press; one finger up steps a controller
// to the next config and down to the one before, a step for every quarter of
// the pad it travels; with the window in front in Simple or Quick (armed),
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

    section("quick switch: one finger up is the next config, down the one before, a step a quarter of the pad");
    {
        Drag d;
        CTM_CHECK_EQ(drag_step(d, 1, 5, 900.0f), 0);   // lands
        CTM_CHECK_EQ(drag_step(d, 1, 5, 700.0f), 0);   // 200 up: not yet
        CTM_CHECK_EQ(drag_step(d, 1, 5, 590.0f), 1);   // 310 up
        CTM_CHECK_EQ(drag_step(d, 1, 5, 400.0f), 0);   // 190 more
        CTM_CHECK_EQ(drag_step(d, 1, 5, 280.0f), 1);   // 320 above where the first step left it, 600
        CTM_CHECK_EQ(drag_step(d, 1, 5, 590.0f), 0);   // back down: 290 below where that left it, 300
        CTM_CHECK_EQ(drag_step(d, 1, 5, 610.0f), -1);  // 310 below it
    }

    section("quick switch: one finger only, measured afresh when the fingers change");
    {
        Drag d;
        CTM_CHECK_EQ(drag_step(d, 2, -1, 0.0f), 0);    // two fingers: nothing
        CTM_CHECK_EQ(drag_step(d, 1, 7, 900.0f), 0);
        CTM_CHECK_EQ(drag_step(d, 2, -1, 0.0f), 0);    // a second finger lands
        CTM_CHECK_EQ(drag_step(d, 1, 7, 650.0f), 0);   // and lifts: 650 is the new start
        CTM_CHECK_EQ(drag_step(d, 1, 7, 400.0f), 0);   // 250 up: not yet
        CTM_CHECK_EQ(drag_step(d, 1, 7, 340.0f), 1);   // 310 up
        CTM_CHECK_EQ(drag_step(d, 1, 8, 100.0f), 0);   // another finger: starts again
        CTM_CHECK_EQ(drag_step(d, 1, 8, 300.0f), 0);
        CTM_CHECK_EQ(drag_step(d, 1, 8, 410.0f), -1);  // 310 down
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
