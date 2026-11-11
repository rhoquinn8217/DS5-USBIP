// Session-side half of the config routes.
//
// WHY THIS FILE EXISTS. These read and mutate g_agent_sessions, which agent.inl
// owns; agent.inl in turn calls into rest.inl. So neither can be included
// first. rest_config.inl DECLARES these, this file DEFINES them, and this file
// is included after agent.inl -- the same split, for the same reason, as
// collect_bridge_session_snapshots().
//
// ⭐ The alternative was writing them inside agent.inl. "The data lives there"
// is not a reason for our code to live in an upstream file; that is how a fork
// accumulates edits it can no longer separate.

#pragma once

// ⭐⭐ WHICH SECTIONS THIS DEVICE SHOULD BE OFFERED (T-227).
//
// A setting a pad has no hardware for is worse than a missing one: it reads as
// broken when it does nothing. ⓘ Nothing is DETECTED here -- the layout
// already carries the answer, and this only reads it.
//
// ⚠️ WHEN T-203 LANDS, AUDIO STOPS BEING DUALSENSE-ONLY. A DS4 has a speaker
// and a headset jack too; its audio simply is not carried yet. ➡️ The audio
// line below is the ONE place to change, and it is written as a kind test
// rather than folded into the layout so it is easy to find.
static void rest_fill_capabilities(RestDeviceView *view)
{
    if (view == nullptr) return;
    const std::string k = config_store::settings_kind_for(view->kind);

    // ⭐⭐ AUDIO IS NO LONGER DUALSENSE-ONLY (T-229 item A, 2026-09-19).
    // T-227 wrote these two as one kind test and said why: *"WHEN T-203 LANDS,
    // AUDIO STOPS BEING DUALSENSE-ONLY. A DS4 has a speaker and a headset jack
    // too."* T-203 landed for Bluetooth on 2026-09-19 -- sound heard from a
    // bridged DS4's speaker and headset -- so this is that line coming due.
    // ⓘ Both rules, and the reasoning for each, are in
    // app/device_capabilities.inl, where they can be tested.
    view->hasAudio = ctm_caps::has_audio_hardware(k);
    view->hasRumbleGains = ctm_caps::has_rumble_gains(k);
    view->hasEdgeButtons = ctm_caps::has_edge_buttons(k);

    // ⚠️ A WIRED XBOX PAD ARRIVES AS "xpad", WHICH settings_kind_for() DOES NOT
    // MAP -- it lists only the kinds the TV sends for a config, and xpad is not
    // among them. So fall back to the RAW kind for the layout, or a cabled Xbox
    // pad would be offered gyro and a touchpad it has never had.
    // ⓘ Harmless if xpad never reaches here: layout_for() simply answers the
    // same nullptr it would have anyway.
    const ctm_rebind::Layout *lay = ctm_rebind::layout_for(k.c_str());
    if (lay == nullptr && (view->kind == "xpad" || view->kind == "xbox")) {
        lay = ctm_rebind::layout_for("xbox");
    }
    if (lay == nullptr) {
        // ⚠️ An unknown pad keeps everything. See the note on the defaults.
        view->hasTouchpad = true;
        view->hasGyro = true;
        return;
    }
    view->hasTouchpad = lay->touch.present;

    // ⛔⛔ THE XBOX LAYOUT NEVER GETS GYRO, WHOEVER MADE THE PAD.
    //
    // ⚠️ An earlier version trusted the layout's "no" only for vendor 0x045e
    // and kept the section for anyone else, reasoning that a third-party pad
    // emulating an Xbox pad might really have a gyro. ⛔ It might -- and we
    // still could not carry it.
    //
    // ⭐ CHECKED AGAINST THE WIDER WORLD 2026-09-19, at rhoquinn8217's asking:
    // **XInput has no slots for gyroscope data at all**, so a pad in Xbox mode
    // strips it before anything downstream sees it. Where gyro does work on
    // such pads, software reads the pad's RAW HID stream instead -- SDL carries
    // per-device code for the GameSir G7 Pro 8K to do exactly that -- or the
    // pad is switched out of Xbox mode entirely, into DirectInput over
    // Bluetooth or a Switch Pro mode.
    //
    // ➡️ So on the reports WE relay there is no gyro to offer, and showing the
    // section would promise a setting that cannot work -- the exact fault this
    // file exists to remove. ⓘ The route for someone who wants that pad's gyro
    // is its OTHER mode, where it arrives as a `hid` device with its own
    // descriptor and then needs a button table: 🔗 T-196's Blocker C.
    view->hasGyro = lay->motion.present;
}

static std::vector<RestDeviceView> rest_collect_devices()
{
    std::vector<RestDeviceView> out;
    std::lock_guard<std::mutex> lock(g_agent_sessions_mutex);
    out.reserve(g_agent_sessions.size());
    for (const auto &session : g_agent_sessions) {
        RestDeviceView view;
        view.ordinal = session->ordinal;
        view.nickname = session->nickname;
        view.kind = session->kind;
        view.tvAddress = session->tvAddress; // set at its start, never changed
        // ⚠️ Reported rather than filtered. A session mid-startup has no serial
        // yet and a link against it may not survive -- but hiding it entirely
        // would make a controller that is plugged in simply not appear, which
        // is worse to explain than one marked not-ready.
        view.ready = session->ready.load();
        {
            std::lock_guard<std::mutex> sessionLock(session->mutex);
            view.serial = session->physicalSerial;
            view.linkedConfig = session->linkedConfig;
            view.controllerSection = session->controllerSection;
        }
        // ⓘ Empty until the TV's HELLO has arrived, like the serial.
        if (session->device) {
            view.product = session->device->product_name();
            view.deviceType = session->device->device_kind_by_descriptor();
            view.link = session->device->link_by_bus();
            // ⭐ What the pad last said about its own charge (T-195). Left at
            // -1 for a pad with no battery byte, so the JSON omits it.
            const ctm_rebind::BatteryReading battery =
                ctm_battery::reading_for(session->device.get());
            if (battery.known) {
                view.batteryPercent = battery.percent;
                view.batteryState = ctm_battery::state_word(battery.state);
            }
            rest_fill_capabilities(&view);
            // ⭐ The polling rate column (usbip/input_pace.inl).
            view.inputRateHz = session->device->served_rate_hz();
            view.arrivalRateHz = session->device->arrived_rate_hz();
            view.takesRateLimit = session->device->takes_rate_limit();
            view.pollingRate = session->device->rate_limit_setting();
            view.rateLimitHz = input_pace::limit_hz(view.pollingRate);
        }
        out.push_back(std::move(view));
    }
    return out;
}

static bool rest_find_device(const std::string &ordinal, RestDeviceView *out)
{
    for (const RestDeviceView &view : rest_collect_devices()) {
        if (view.ordinal == ordinal) { *out = view; return true; }
    }
    return false;
}

// Links a device to a config, or unlinks when configName is empty.
//
// ⭐⭐ ANY CONFIG, ON ANY DEVICE THAT TAKES ONE (rhoquinn8217, 2026-09-12).
// This used to refuse a "kind mismatch" -- a ds5_edge config on a ds5 -- on the
// grounds that settings written for one product would misbehave on another.
// ⛔ That premise did not hold: a config stores button positions, percents and
// milliseconds, and each feature checks the pad before it acts, so a setting
// a controller cannot use is unused on it rather than misapplied.
//
// ⚠️ What IS still refused is a device that takes no config at all, a keyboard
// or a mouse. Linking one would report success and do nothing, which is the
// worst shape a setting can take.
// ⭐ A CONTROLLER'S OWN SETTINGS (controller_settings.inl): the section a
// bridged controller reads them from, empty for any other device.
static bool rest_controller_section(const std::string &ordinal, std::string *section)
{
    std::lock_guard<std::mutex> lock(g_agent_sessions_mutex);
    for (const auto &session : g_agent_sessions) {
        if (session->ordinal != ordinal) continue;
        std::lock_guard<std::mutex> sessionLock(session->mutex);
        *section = session->controllerSection;
        return !section->empty();
    }
    return false;
}

// What the page reads: whether they are kept in the controller's file, and
// what is set, as written; a setting not there is at its default.
static std::string rest_controller_settings_json(const std::string &ordinal, const std::string &section)
{
    std::string out = "{\"ordinal\":\"" + rest_json_escape(ordinal) + "\"";
    out += ",\"kept\":" + std::string(controller_settings::is_kept_section(section) ? "true" : "false");
    out += ",\"settings\":{";
    bool first = true;
    for (const auto &s : controller_settings::get(section)) {
        if (!first) out += ",";
        first = false;
        out += "\"" + rest_json_escape(s.first) + "\":\"" + rest_json_escape(s.second) + "\"";
    }
    out += "}}";
    return out;
}

// Applied at once: the gains and the PC-sound haptics read again, and the
// settings report and the TV's audio settings sent again by the next sweep,
// as a change of config sends them (rest_link_device says why a flag).
static void rest_apply_controller_settings(const std::string &ordinal)
{
    std::lock_guard<std::mutex> lock(g_agent_sessions_mutex);
    for (const auto &session : g_agent_sessions) {
        if (session->ordinal != ordinal) continue;
        std::string section;
        {
            std::lock_guard<std::mutex> sessionLock(session->mutex);
            section = session->controllerSection;
        }
        if (session->device) session->device->set_controller_section(section);
        ctm_config_watcher::change_pending().store(true, std::memory_order_relaxed);
        return;
    }
}

// ⭐ A DEVICE TAG CHANGED IN THE SETTINGS WINDOW, WAITING FOR IT TO CLOSE
// (rhoquinn8217, 2026-11-10: "Don't change the tag until after the setting
// window is closed"). By ordinal: the section its settings are in, and its
// choice and custom tag before the window's first write, which stand until
// the window closes.
struct RestTagPending {
    std::string section;
    std::string mode;
    std::string custom;
};
static std::mutex g_restTagPendingMutex;
static std::map<std::string, RestTagPending> g_restTagPending;

static void rest_device_tag_written(const std::string &ordinal, const std::string &section,
                                    const std::string &modeBefore, const std::string &customBefore)
{
    std::lock_guard<std::mutex> lock(g_restTagPendingMutex);
    g_restTagPending.emplace(ordinal, RestTagPending{section, modeBefore, customBefore});
}

// The window of `ordinal` closed, or with "" the page went: each controller
// waiting is named by its tag now, if it changed (device_tag::changed), random
// drawing a new name (agent_apply_device_tag). ⓘ One no longer bridged is
// passed over there.
static void rest_device_tags_settle(const std::string &ordinal)
{
    std::vector<std::pair<std::string, RestTagPending>> due;
    {
        std::lock_guard<std::mutex> lock(g_restTagPendingMutex);
        for (auto it = g_restTagPending.begin(); it != g_restTagPending.end();) {
            if (ordinal.empty() || it->first == ordinal) {
                due.emplace_back(it->first, it->second);
                it = g_restTagPending.erase(it);
            } else {
                ++it;
            }
        }
    }
    for (const auto &p : due) {
        const auto now = controller_settings::get(p.second.section);
        auto value = [&](const char *key) {
            const auto it = now.find(key);
            return it == now.end() ? std::string() : it->second;
        };
        if (!device_tag::changed(p.second.mode, p.second.custom, value(device_tag::kModeKey),
                                 value(device_tag::kCustomKey))) {
            device_log::session(device_log::msg()
                << p.first << " keeps its name: its device tag is as it was when its settings opened");
            continue;
        }
        agent_apply_device_tag(p.first, p.second.section, true);
    }
}

static bool rest_link_device(const std::string &ordinal, const std::string &configName,
                             std::string *error)
{
    if (!configName.empty()) {
        config_store::ConfigFile cfg;
        if (!config_store::find_config(configName, &cfg)) {
            *error = "no config named " + configName;
            return false;
        }
    }

    std::lock_guard<std::mutex> lock(g_agent_sessions_mutex);
    for (const auto &session : g_agent_sessions) {
        if (session->ordinal != ordinal) continue;
        // ⓘ Unlinking is always allowed: it only returns a device to the
        // shared settings.
        if (!configName.empty() && !config_store::kind_supports_config(session->kind)) {
            RestDeviceView view;
            view.ordinal = ordinal;
            view.kind = session->kind;
            *error = rest_no_config_error(view);
            return false;
        }
        std::lock_guard<std::mutex> sessionLock(session->mutex);
        session->linkedConfig = configName;
        // ⓘ A config chosen while Windows is locked is the one kept at the
        // unlock (lock_screen_rule.inl).
        session->heldForLock = false;
        session->beforeLock.clear();
        // ⭐ Push it to the device as well. The session field is what the API
        // reports; the DEVICE field is what actually resolves settings on the
        // output path. Setting only the first would make every link a no-op
        // that still answered 200.
        if (session->device) {
            session->device->set_linked_config(configName);
        }
        // ⭐⭐ AND ASK FOR THE SETTINGS REPORT TO BE RE-SENT.
        //
        // ⛔ WITHOUT THIS, LINKING A CONFIG ONLY HALF APPLIED IT. The two lines
        // above change what the per-report overrides RESOLVE, which covers
        // every setting that rides on a report the game is already sending.
        // They do nothing for the settings we SET once -- audio routing,
        // volume, the microphone, and the trigger effects. Those only reached
        // the controller when the config FILE changed or the pad was
        // re-bridged, so linking a config and feeling nothing happen was the
        // documented behaviour rather than a fault (found 2026-09-10, when an
        // L2 wall from another config outlived a link that had already moved).
        //
        // ⓘ Raised as a flag rather than sent from here on purpose. The sweep
        // that acts on it runs on the AGENT loop, which is the only thread
        // where a session cannot be torn down underneath a backend pointer --
        // see the warning above apply_pending_config_to_sessions(). This
        // borrows that guarantee instead of needing its own.
        ctm_config_watcher::change_pending().store(true, std::memory_order_relaxed);

        device_log::config(device_log::msg()
            << ordinal << (configName.empty()
                ? std::string(" back on the shared section")
                : std::string(" now reads ") + configName)
            << " -- settings queued for the next sweep");
        return true;
    }
    // ⭐ Ordinals are monotonic and never reused, so a stale reference from an
    // old device list can only ever FAIL here -- never silently act on a
    // different physical controller. That is what removes the need for a
    // session token in the list response.
    *error = ordinal + " is not connected";
    return false;
}

// ⭐ A CONTROLLER'S POLLING LIMIT (config/polling_limits.inl; rhoquinn8217,
// 2026-10-16: "keep it with the controller"). Kept for the controller, so no
// link changes it, and set without one. Refused for a device that takes no
// limit, a keyboard or a mouse, and for a value the page does not offer.
static bool rest_set_polling_rate(const std::string &ordinal, const std::string &value,
                                  std::string *error)
{
    std::lock_guard<std::mutex> lock(g_agent_sessions_mutex);
    for (const auto &session : g_agent_sessions) {
        if (session->ordinal != ordinal) continue;
        std::lock_guard<std::mutex> sessionLock(session->mutex);
        if (!session->device || !session->device->takes_rate_limit()) {
            *error = ordinal + " takes no polling limit: only a controller is limited";
            return false;
        }
        if (!session->device->set_rate_limit_setting(value)) {
            *error = "no polling limit " + value + ": it is real_time, 500, 250 or 125";
            return false;
        }
        device_log::config(device_log::msg()
            << ordinal << " polling limit: "
            << (value.empty() ? std::string("real_time") : value));
        return true;
    }
    *error = ordinal + " is not connected";
    return false;
}
