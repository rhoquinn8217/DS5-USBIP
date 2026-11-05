// ⭐ A DS4DONGLE KEPT AS ITS CONFIG SAYS (2026-11-04): speaker_without_headphones,
// on by default, which is its audio_follow_jack turned over. With it on, the
// dongle keeps its sound card while nothing is in the pad's headphone jack, so
// the DS4's own speaker plays. ds4_dongle.inl has the reports and why they go
// to the TV rather than through Windows.
//
// ⭐ AT ONCE, AS rhoquinn8217 ASKED. The dongle decides whether it has a sound
// card when it connects, so a change takes effect only when it reconnects --
// which ends the bridge. The TV core sees the reconnect go by and Auto Bridge
// takes the pad again as it returns, so the bridge drops for a moment and comes
// back by itself.
//
// Run at a cabled DS4's bridge (agent.inl) and whenever a config changes (the
// sweep). ⓘ Blocking: up to four feature requests to the TV, a third of a
// second each at most, and only the first time, or when the setting changes.

struct Ds4DongleState {
    std::mutex m;
    // -1 not asked yet, 0 not a DS4Dongle, 1 one. Read by the REST view.
    std::atomic<int> isDongle{-1};
    // Its audio_follow_jack as last read or written, -1 unknown.
    int followJack = -1;
    std::string version;
};

static void ds4_dongle_keep(CtmBackend *backend, const std::string &section,
                            Ds4DongleState *state, const char *when)
{
    if (backend == nullptr || state == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(state->m);
    if (state->isDongle.load() == 0) {
        return;
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
                << section << ": not a DS4Dongle (no version from report 0xF8), its own settings left alone");
            return;
        }
        state->version = ds4_dongle::version_of(reply);
        state->isDongle.store(1);
        device_log::report(device_log::msg()
            << section << ": a DS4Dongle, firmware " << state->version);
    }

    const bool on = device_config_bool(section.c_str(), "speaker_without_headphones", true);
    const uint8_t want = ds4_dongle::follow_jack_for(on);
    if (state->followJack == static_cast<int>(want)) {
        return;     // as last read or written: nothing to ask
    }

    reply.clear();
    ds4_dongle::Body body{};
    if (!backend->remote_interface_feature(0, true, ds4_dongle::get_request(ds4_dongle::kReportConfig),
                                           &reply, kWaitMs) ||
        !ds4_dongle::body_of(reply, &body)) {
        device_log::report(device_log::msg()
            << section << ": DS4Dongle settings not read (report 0xF7), left as they are");
        return;
    }
    if (body[0] != ds4_dongle::kSchema) {
        device_log::report(device_log::msg()
            << section << ": DS4Dongle settings are schema " << static_cast<int>(body[0])
            << ", not " << static_cast<int>(ds4_dongle::kSchema) << ": left as they are");
        return;
    }
    state->followJack = body[ds4_dongle::kFollowJackAt];
    if (state->followJack == static_cast<int>(want)) {
        device_log::report(device_log::msg()
            << section << ": DS4Dongle speaker_without_headphones is " << (on ? "on" : "off")
            << " already (" << when << ")");
        return;
    }

    std::vector<uint8_t> none;
    if (!backend->remote_interface_feature(0, false, ds4_dongle::update_request(body, want), &none, kWaitMs) ||
        !backend->remote_interface_feature(0, false, ds4_dongle::function_request(ds4_dongle::kFuncSave),
                                           &none, kWaitMs)) {
        device_log::report(device_log::msg()
            << section << ": DS4Dongle speaker_without_headphones NOT changed: the dongle refused the write");
        return;
    }
    state->followJack = want;
    device_log::report(device_log::msg()
        << section << ": DS4Dongle speaker_without_headphones set " << (on ? "on" : "off")
        << " and saved (" << when << "); the dongle restarts to take it, and the TV bridges it again");
    // ⓘ The dongle leaves USB as it obeys, so the answer may never come.
    (void)backend->remote_interface_feature(0, false, ds4_dongle::function_request(ds4_dongle::kFuncReconnect),
                                            &none, kWaitMs);
}
