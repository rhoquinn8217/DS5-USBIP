// ⭐ A DS4DONGLE'S SPEAKER PLAYS WITHOUT HEADPHONES (2026-11-04): its
// audio_follow_jack at 0, so the dongle keeps its sound card while nothing is
// in the pad's headphone jack and the DS4's own speaker plays. ds4_dongle.inl
// has the reports and why they go to the TV rather than through Windows.
//
// ⛔ NOT A SETTING. It was one for an evening, speaker_without_headphones in a
// config, and was dropped: the dongle decides whether it has a sound card when
// it connects, so every change restarts it, and stepping through configs
// restarted it at each one that said otherwise (rhoquinn8217: "The on/off will
// happen to often when switch configs"). ➡️ So it is simply on: a dongle found
// with it off at a bridge is set on and saved, and restarted once to take it.
// The TV core sees the restart go by and Auto Bridge takes the pad again as it
// returns, so the bridge drops for a moment and comes back by itself -- once
// in the dongle's life, since it keeps the setting in its flash.
//
// Run at a cabled DS4's bridge (agent.inl). ⓘ Blocking: two feature requests
// to the TV, a third of a second each at most, and two more to set it.
//
// ⭐ It answers what it wrote when it asked for a restart, so the caller can
// remember the pad until it is back (ds4_dongle_return.inl), and -1 otherwise.

struct Ds4DongleState {
    std::mutex m;
    // -1 not asked yet, 0 not a DS4Dongle, 1 one.
    std::atomic<int> isDongle{-1};
    std::string version;
    // What was written before the restart this session is back from, -1 none
    // (ds4_dongle_return.inl).
    int wroteBeforeRestart = -1;
};

static int ds4_dongle_keep(CtmBackend *backend, const std::string &label, Ds4DongleState *state)
{
    if (backend == nullptr || state == nullptr) {
        return -1;
    }
    std::lock_guard<std::mutex> lock(state->m);
    if (state->isDongle.load() == 0) {
        return -1;
    }
    const unsigned int kWaitMs = 300;
    std::vector<uint8_t> reply;
    if (state->isDongle.load() < 0) {
        // ⓘ A real DS4 has no report 0xF8 and answers nothing: then it is not a
        // dongle for the rest of this bridge, and is never asked again.
        if (!backend->remote_interface_feature(0, true, ds4_dongle::get_request(ds4_dongle::kReportVersion),
                                               &reply, kWaitMs) ||
            ds4_dongle::version_of(reply).empty()) {
            state->isDongle.store(0);
            device_log::report(device_log::msg()
                << label << ": not a DS4Dongle (no version from report 0xF8), its own settings left alone");
            return -1;
        }
        state->version = ds4_dongle::version_of(reply);
        state->isDongle.store(1);
        device_log::report(device_log::msg()
            << label << ": a DS4Dongle, firmware " << state->version);
    }

    reply.clear();
    ds4_dongle::Body body{};
    if (!backend->remote_interface_feature(0, true, ds4_dongle::get_request(ds4_dongle::kReportConfig),
                                           &reply, kWaitMs) ||
        !ds4_dongle::body_of(reply, &body)) {
        device_log::report(device_log::msg()
            << label << ": DS4Dongle settings not read (report 0xF7), left as they are");
        return -1;
    }
    if (body[0] != ds4_dongle::kSchema) {
        device_log::report(device_log::msg()
            << label << ": DS4Dongle settings are schema " << static_cast<int>(body[0])
            << ", not " << static_cast<int>(ds4_dongle::kSchema) << ": left as they are");
        return -1;
    }
    const uint8_t want = ds4_dongle::follow_jack_for(true);
    const int has = body[ds4_dongle::kFollowJackAt];
    const int wroteBefore = state->wroteBeforeRestart;
    state->wroteBeforeRestart = -1;
    if (has == static_cast<int>(want)) {
        device_log::report(device_log::msg()
            << label << ": DS4Dongle plays its speaker without headphones already");
        return -1;
    }
    // ⛔ NEVER A SECOND RESTART FOR A SETTING THE DONGLE DID NOT KEEP. Back from
    // the restart asked for a moment ago without it, it would be set and
    // restarted again at every bridge, for ever: a firmware that does not save
    // it, say. The next bridge after this one asks again.
    if (wroteBefore >= 0 && has != wroteBefore) {
        device_log::report(device_log::msg()
            << label << ": DS4Dongle came back from its restart without its speaker setting (audio_follow_jack "
            << wroteBefore << " written, " << has << " read): left as it is, and not restarted again");
        return -1;
    }

    std::vector<uint8_t> none;
    if (!backend->remote_interface_feature(0, false, ds4_dongle::update_request(body, want), &none, kWaitMs) ||
        !backend->remote_interface_feature(0, false, ds4_dongle::function_request(ds4_dongle::kFuncSave),
                                           &none, kWaitMs)) {
        device_log::report(device_log::msg()
            << label << ": DS4Dongle speaker NOT set to play without headphones: the dongle refused the write");
        return -1;
    }
    device_log::report(device_log::msg()
        << label << ": DS4Dongle set to play its speaker without headphones, and saved; it restarts "
                    "to take it, and the TV bridges it again");
    // ⓘ The dongle leaves USB as it obeys, so the answer may never come.
    (void)backend->remote_interface_feature(0, false, ds4_dongle::function_request(ds4_dongle::kFuncReconnect),
                                            &none, kWaitMs);
    return want;
}
