constexpr uint16_t kAgentDefaultPort = 48054;
// The control port this agent actually listens on (run_agent), which `agent
// <port>` or the service may move off the default: no bridge may be handed it.
static uint16_t g_agent_control_port = kAgentDefaultPort;

struct AgentBridgeSession {
    std::string kind;
    std::wstring busId;
    std::string busIdAscii;
    uint16_t port = 0;
    // ⭐ The only address its bridge port takes a connection from, in host
    // byte order; 0 for any. The control port sets it to whoever asked for
    // the bridge (unless paired_tv_proof = off), so nobody else can take its
    // port; a session started over the REST API has none.
    uint32_t onlyPeer = 0;
    // ⭐ The TV it belongs to: the fingerprint its pairing proof PROVED, or ""
    // for a TV that proved nothing. Names, stale ports, stops and retiring all
    // stay within one TV -- tv_sessions.inl.
    std::string owner;
    std::shared_ptr<CtmUsbipDevice> device;
    // ⛔ SHARED, NOT UNIQUE (code review, 2026-10-05). The settings push sends
    // on it from the agent loop after letting go of the session list lock, and
    // a stop on another thread -- a worker retiring an older session for the
    // same pad, or the REST API -- must not free it under that send.
    std::shared_ptr<CtmBackend> backend;
    std::thread worker;
    std::atomic_bool stopping{false};
    std::atomic_bool ready{false};
    std::mutex mutex;
    std::wstring lastError;
    // Per-controller config. ordinal is monotonic and never reused, so a stale
    // reference from an old device list fails rather than hitting the wrong
    // controller. linkedConfig empty means the shared [kind] section.
    std::string ordinal;
    // The word a person says out loud for this controller. Assigned with the
    // ordinal above and gone with the session, never used as a handle.
    std::string nickname;
    std::string physicalSerial;
    // The device's node on the TV (/dev/hidraw2, /dev/input/event13). With the
    // serial, it tells a re-bridge of one device from another device that
    // shares the serial -- see same_controller.inl.
    std::string tvPath;
    std::string linkedConfig;
};

static std::mutex g_agent_sessions_mutex;
static std::vector<std::unique_ptr<AgentBridgeSession>> g_agent_sessions;
static std::unique_ptr<CtmUsbipServer> g_agent_usbip_server;

// ⭐⭐ THE PAIRING PROOF (code review, 2026-10-05; rhoquinn8217's choice,
// 2026-10-07). paired_tv.inl has the whole rule; these are what the control
// port keeps for it.
// ⓘ The challenges given out. handle_agent_client runs on the agent loop
// alone, but the lock costs nothing and keeps that true if it ever moves.
static std::mutex g_paired_tv_mutex;
static paired_tv::Nonces g_paired_tv_nonces;

// [global] paired_tv_proof (rhoquinn8217, 2026-10-08: "log only until
// updated"):
//   log -- the default for now: checked and written down, nobody turned away;
//   on  -- a TV calling from the internet without a good proof is refused;
//   off -- not checked at all, and a bridge port takes a connection from
//          anywhere, as it did before.
// ⭐ "log" until the TVs at the remote site run a build that sends the proof;
// then the default becomes "on".
enum class PairedTvMode { Off, Log, On };

static PairedTvMode paired_tv_mode()
{
    const std::string mode = device_config_str("global", "paired_tv_proof");
    if (mode == "on") return PairedTvMode::On;
    if (mode == "off") return PairedTvMode::Off;
    return PairedTvMode::Log;
}

// The streaming host's paired devices, read from its state file each time a
// proof is checked, so a TV paired a minute ago is let in with no restart.
// ⓘ [global] streaming_host_state_file names the file; otherwise Apollo's,
// then Sunshine's, under Program Files. `where` says which was read, or
// tried first when none could be.
static bool load_paired_devices(std::vector<paired_tv::Device> *out, std::wstring *where)
{
    std::vector<std::wstring> candidates;
    const std::string named = device_config_str("global", "streaming_host_state_file");
    if (!named.empty()) {
        candidates.push_back(widen_ascii(named.c_str(), named.size()));
    } else {
        wchar_t programFiles[MAX_PATH] = {};
        const DWORD got = GetEnvironmentVariableW(L"ProgramFiles", programFiles, MAX_PATH);
        const std::wstring base = (got > 0 && got < MAX_PATH) ? std::wstring(programFiles)
                                                              : std::wstring(L"C:/Program Files");
        candidates.push_back(base + L"/Apollo/config/sunshine_state.json");
        candidates.push_back(base + L"/Sunshine/config/sunshine_state.json");
    }
    for (const std::wstring &path : candidates) {
        std::ifstream f(path, std::ios::binary);
        if (!f) continue;
        std::ostringstream text;
        text << f.rdbuf();
        *out = paired_tv::devices_from_state(text.str());
        if (where) *where = path;
        return true;
    }
    if (where) *where = candidates.front();
    return false;
}

// Reap requests from bridge reader threads (client lost / idle expired). The
// run_agent loop drains this on its own thread — the only thread that starts
// and stops sessions — so a reap can never race a concurrent start and never
// outlives shutdown (a detached reaper thread could UAF g_agent_sessions when
// the service stopped mid-reap).
static std::mutex g_agent_reap_mutex;
static std::vector<std::wstring> g_agent_reap_requests;

static void request_bridge_session_reap(const std::wstring &busId)
{
    std::lock_guard<std::mutex> lock(g_agent_reap_mutex);
    g_agent_reap_requests.push_back(busId);
}

static std::wstring find_relative_asset(const std::wstring &relative)
{
    // Live-edit override first: the agent/service resolves maps/profiles from
    // %ProgramData%\CTM Bridge\ exactly like the local CLI does.
    const std::wstring overridePath = programdata_override(relative);
    if (!overridePath.empty()) {
        return overridePath;
    }
    const std::wstring exeDir = module_directory();
    const std::vector<std::wstring> candidates = {
        relative,
        L"..\\" + relative,
        exeDir + L"\\" + relative,
        exeDir + L"\\..\\..\\..\\" + relative
    };
    for (const std::wstring &candidate : candidates) {
        if (file_exists(candidate)) return candidate;
    }
    return candidates[0];
}

static std::wstring bridge_profile_for_kind(const std::string &kind)
{
    if (kind == "ds4" || kind == "ds4_usb") {
        // The same pad either way. Only the wire format the map reads differs.
        return find_relative_asset(L"profiles\\descriptors\\ds4_composite.profile");
    }
    if (kind == "ds5") {
        return find_ds5_descriptor_profile();
    }
    if (kind == "ds5_usb") {
        return find_relative_asset(L"profiles\\descriptors\\ds5_composite.profile");
    }
    // ⭐ A DUALSENSE EDGE OVER BLUETOOTH (code review, 2026-10-05). The TV
    // has sent "ds5e" for one since 2026-08-05, and this side knew only
    // "ds5e_usb", so every Bluetooth Edge was refused with "ERR bad bridge
    // args". To Windows it is the same Edge as on a cable; only the wire
    // format the map reads differs, as for the plain DualSense.
    if (kind == "ds5e" || kind == "ds5e_usb") {
        return find_relative_asset(L"profiles\\descriptors\\ds5e_composite.profile");
    }
    if (kind == "puck") {
        return find_relative_asset(L"profiles\\descriptors\\steam_puck.profile");
    }
    if (kind == "xbox") {
        return find_relative_asset(L"profiles\\descriptors\\xbox_gip_usb.profile");
    }
    return L"auto";
}

static std::wstring bridge_map_for_kind(const std::string &kind)
{
    if (kind == "ds4") {
        return find_relative_asset(L"maps\\ds4_usb_over_ds4_bt.map");
    }
    if (kind == "ds4_usb") {
        // ⭐⭐ A CABLED DS4 NEEDS NO TRANSLATION. Its own report 0x01 is already
        // what the virtual wired DS4 emits, so this map is a pass-through, the
        // way the wired DualSense's is.
        //
        // ⛔ Sending a cabled pad through the BLUETOOTH map was the fault: that
        // map triggers on source report 0x11, a cabled pad sends 0x01, so
        // nothing parsed its input. The pad bridged, read PLUGGED, and did
        // nothing in the game.
        return find_relative_asset(L"maps\\ds4_usb_over_ds4_usb.map");
    }
    if (kind == "ds5" || kind == "ds5e") {   // the Bluetooth map, either pad
        return find_ds5_map_file();
    }
    if (kind == "ds5_usb") {
        return find_relative_asset(L"maps\\ds5_usb_over_ds5_usb.map");
    }
    if (kind == "ds5e_usb") {
        return find_relative_asset(L"maps\\ds5_usb_over_ds5_usb.map");
    }
    if (kind == "puck") {
        return find_relative_asset(L"maps\\steam_puck_identity.map");
    }
    if (kind == "xbox") {
        return find_relative_asset(L"maps\\xbox_gip_usb_over_xbox_bt.map");
    }
    return find_hid_identity_map_file();
}

// ⭐ Which controller a device belongs to. The input path holds a device
// pointer -- the chord fires there -- while the ordinal lives with the session.
//
// ⓘ Empty when it is not ours, and every caller treats that as "no particular
// controller" rather than guessing at one.
std::string ctm_ordinal_for_device(const void *deviceKey)
{
    if (!deviceKey) return std::string();
    std::lock_guard<std::mutex> lock(g_agent_sessions_mutex);
    for (const auto &s : g_agent_sessions) {
        if (s && s->device.get() == deviceKey) return s->ordinal;
    }
    return std::string();
}

static AgentBridgeSession *find_bridge_session_locked(const std::wstring &busId)
{
    for (auto &session : g_agent_sessions) {
        if (session->busId == busId) {
            return session.get();
        }
    }
    return nullptr;
}

static void set_bridge_session_error(AgentBridgeSession *session, const std::wstring &error)
{
    if (!session) {
        return;
    }
    std::lock_guard<std::mutex> lock(session->mutex);
    session->lastError = error;
}

static bool stop_bridge_session(const std::wstring &busId);
static void drain_bridge_session_reaps();

static void bridge_session_worker(AgentBridgeSession *session)
{
    std::wstring error;
    const std::wstring profile = bridge_profile_for_kind(session->kind);
    const std::wstring map = bridge_map_for_kind(session->kind);
    const bool dynamicProfile = profile == L"auto";
    const uint8_t audioLatency = 0x60;
    const bool hasAudioBlockOverride = false;
    const uint8_t audioBlockOverride = 0;

    if (dynamicProfile) {
        if (!session->device->load_map(map, audioLatency, hasAudioBlockOverride, audioBlockOverride, &error)) {
            set_bridge_session_error(session, error);
            std::wcerr << L"agent bridge load failed busid=" << session->busId << L": " << error << L"\n";
            return;
        }
    } else if (!session->device->load(profile, map, audioLatency, hasAudioBlockOverride, audioBlockOverride, &error)) {
        set_bridge_session_error(session, error);
        std::wcerr << L"agent bridge load failed busid=" << session->busId << L": " << error << L"\n";
        return;
    }

    // ⓘ TCP is the one transport. An ENet/UDP one was offered with --enet until
    // 2026-10-08 and removed: it had drifted from the protocol and the TV no
    // longer spoke it (code review, 2026-10-05).
    CtmBackend *backendPtr = nullptr;
    {
        auto backend = std::make_unique<BridgeBackend>(session->port, session->device->bt_audio_pace_ms());
        backendPtr = backend.get();
        backend->set_only_peer(session->onlyPeer);
        // Agent-only session policy (the CLI bridge mode keeps wait-forever
        // defaults): bounded initial accept + reconnect grace, TCP keepalive
        // probing, and the idle rule — gamepads chatter constantly so silence
        // means gone (15 s). A mouse, a keyboard or any other part may sit
        // unused for as long as it likes: it has no idle rule at all, since
        // 2026-09-14, because every part of a device is bridged together and
        // a quiet one timing out would leave the device half bridged. A TV
        // that has gone is still caught by the keepalive and the reconnect
        // grace. Generic "hid" whose HELLO descriptor is a gamepad still gets
        // the 15 s window, which the backend detects and applies itself.
        //
        // ⭐ A RECONNECT GETS FIVE SECONDS (fifteen until 2026-10-08). The TV
        // redials a dropped bridge for three seconds and then takes the pad
        // back for its own use, so the port is held a little longer than the
        // TV dials and no longer: past that, the pad here is one that does
        // nothing. At fifteen, with the TV dialling as long, a quit
        // listener's pads lay dead on the TV for sixteen seconds, and in
        // every log on record a bridge that came back did so within half a
        // second. ⛔ The two numbers move together: the TV's is
        // CTM_HOST_GONE_MS in the bridge core, and a TV that dials longer
        // than this holds the port is dialling for nothing.
        backend->set_session_timeouts(30000, 5000);
        const bool gamepadKind = session->kind == "ds4" || session->kind == "ds5" ||
                                 session->kind == "ds5e" ||
                                 session->kind == "xbox" || session->kind == "puck";
        backend->set_idle_timeouts(gamepadKind ? 15000 : 0, 15000);
        // When the TV client vanishes and the reconnect grace runs out (or
        // the idle rule fires), unplug the virtual device and reap the
        // session (a reaped port also can't hijack the next plug's
        // handshake). The reap is queued to the agent loop: stop() joins the
        // reader thread that fires this callback, so reaping inline here
        // would self-join.
        {
            const std::wstring busId = session->busId;
            backend->set_closed_callback([busId]() {
                device_log::session_w() << L"agent bridge client lost busid=" << busId
                           << L" -> virtual device UNPLUGGED";
                request_bridge_session_reap(busId);
            });
        }
        std::lock_guard<std::mutex> lock(session->mutex);
        if (session->stopping.load()) {
            return;
        }
        session->backend = std::move(backend);
    }

    // ⭐⭐ THE TV'S DS5-USBIP BUTTON (rhoquinn8217, 2026-09-28: "a bridge opens
    // the config window. Can we use the same mechanism?"). The TV asks, on
    // this session's own connection, for what the ready path further down
    // does by itself at a bridge: the settings window, on this device's tab.
    // It works for anything that can be bridged. The chord a person presses
    // for the same thing is read off a touchpad, so it cannot.
    //
    // ⛔⛔ ON ITS OWN THREAD, NEVER THE ONE THAT CALLS THIS. That is the
    // session's read loop, and ctm_chord_show_ui closes any open window,
    // polls up to a second for it to go, then launches a browser: the
    // device's reports queue behind all of it and flush in a burst, which
    // reads as every button firing over and over. The ready path says how
    // that was found.
    //
    // ⛔ NOT WHEN THE WINDOW IS ALREADY IN FRONT, as for the chord and for a
    // bridge: closing and reopening it would destroy what the person is
    // doing to show them a tab they can reach in one press.
    //
    // ⓘ With or without --ui. That switch is the listener deciding to show
    // itself; this is a person asking, as the chord is.
    {
        const std::string ordinal = session->ordinal;
        backendPtr->set_open_config_callback([ordinal]() {
            if (ctm_open_ui::window_has_foreground()) {
                device_log::session_w() << L"TV asked for the settings window on "
                           << widen_ascii(ordinal.c_str(), ordinal.size())
                           << L": it is already in front, nothing to do";
                return;
            }
            device_log::session_w() << L"TV asked for the settings window on "
                       << widen_ascii(ordinal.c_str(), ordinal.size()) << L": opening it";
            std::thread([ordinal]() { ctm_chord_show_ui(ordinal); }).detach();
        });
    }

    const std::shared_ptr<CtmUsbipDevice> device = session->device;
    if (!backendPtr->start([device](const uint8_t *data, size_t length, uint8_t endpoint) {
            device->on_physical_input(data, length, endpoint);
        }, &error)) {
        set_bridge_session_error(session, error);
        if (!session->stopping.load()) {
            std::wcerr << L"agent bridge backend failed busid=" << session->busId << L": " << error << L"\n";
            // ⭐ AND THE SESSION GOES (code review, 2026-10-09). A TV that never
            // connected -- it vanished, or its own stop was refused -- left
            // the session holding its port and its listening socket until the
            // listener restarted: a port-0 request never picks a held port,
            // and a remote TV's are the forwarded ones. Queued to the agent
            // loop, as the closed callback's reap is.
            request_bridge_session_reap(session->busId);
        }
        return;
    }

    if (dynamicProfile) {
        CtmDescriptorProfile dynamicDescriptor;
        if (!make_dynamic_hid_profile(backendPtr->caps(), &dynamicDescriptor, &error) ||
            !session->device->set_profile(dynamicDescriptor, &error)) {
            set_bridge_session_error(session, error);
            backendPtr->stop();
            std::wcerr << L"agent bridge dynamic profile failed busid=" << session->busId << L": " << error << L"\n";
            return;
        }
    }

    // Composite puck: replace the static fallback profile with one built from
    // the forwarded enumeration (device + configuration descriptors + each HID
    // interface's report descriptor), presented full-speed. The enum arrives
    // during the handshake (CTMB_MSG_ENUM, before HELLO) and is stored in the
    // backend. If it did not arrive, the static profile loaded above stands.
    if (session->kind == "puck" && !backendPtr->enum_payload().empty()) {
        CtmDescriptorProfile compositeDescriptor;
        std::wstring compositeError;
        if (make_composite_profile_from_enum(backendPtr->enum_payload(), &compositeDescriptor, &compositeError) &&
            session->device->set_profile(compositeDescriptor, &compositeError)) {
            std::wcerr << L"agent bridge composite profile built busid=" << session->busId
                       << L" hid_ifaces=" << compositeDescriptor.interface_report_descriptors.size()
                       << L" full_speed=" << (compositeDescriptor.full_speed ? 1 : 0) << L"\n";
        } else {
            std::wcerr << L"agent bridge composite build failed busid=" << session->busId
                       << L" (keeping static profile): " << compositeError << L"\n";
        }
    }

    if (!session->device->attach_backend(backendPtr, &error)) {
        set_bridge_session_error(session, error);
        backendPtr->stop();
        std::wcerr << L"agent bridge attach failed busid=" << session->busId << L": " << error << L"\n";
        return;
    }

    // ⛔ NOT ONCE A STOP HAS BEGUN (an independent review, 2026-10-07). An
    // export made after the stop removed this one was plugged back into
    // Windows and stayed there, with nothing left to unplug it. The stop
    // removes it once more after this worker is gone, for the moment between
    // this check and the export.
    if (session->stopping.load()) {
        backendPtr->stop();
        return;
    }

    // ⭐ The backend goes in WITH the device, so an import thread still serving
    // Windows when this bridge stops keeps the backend it may be inside (code
    // review, 2026-10-05). server.inl, UsbipExportedDevice, says why.
    std::shared_ptr<CtmBackend> backendForExport;
    {
        std::lock_guard<std::mutex> lock(session->mutex);
        backendForExport = session->backend;
    }
    if (!g_agent_usbip_server ||
        !g_agent_usbip_server->add_device(session->device, session->busIdAscii, &error, backendForExport)) {
        set_bridge_session_error(session, error);
        backendPtr->stop();
        std::wcerr << L"agent USB/IP export failed busid=" << session->busId << L": " << error << L"\n";
        return;
    }

    // ⛔ RETIRE AN OLDER SESSION FOR THE SAME PHYSICAL CONTROLLER.
    //
    // Measured 2026-08-27: a re-bridge arriving before the previous session has
    // finished dying leaves TWO sessions attached to one pad. The controller
    // answers the OLD session's endpoint, so the new one's feature request times
    // out -- three attempts, 370ms apart, all of them -- and the gyro falls back
    // to a scale 62x too slow.
    //
    // ⚠️ The stale check upstream matches on PORT, and the TV picks a new port
    // each bridge: the log shows busid-9 on 48056 and busid-10 on 48057, same
    // serial, coexisting for five seconds. The SERIAL is what identifies the
    // physical device.
    //
    // ⛔ WITH THE TV'S NODE, since 2026-09-14: one USB device can arrive as
    // several bridges sharing its serial, and the pad of a GameSir retired its
    // own keyboard interface. same_controller.inl has the rule and its tests.
    //
    // ⛔ OUTSIDE the session->mutex block below, deliberately. Taking
    // g_agent_sessions_mutex while holding a session mutex inverts the lock
    // order the rest of this file uses, and stop_bridge_session takes both.
    std::string myTvPath;
    {
        const BackendCaps capsNow = backendPtr->caps();
        for (wchar_t c : capsNow.path) {
            if (c < 128) myTvPath.push_back(static_cast<char>(c));
        }
    }
    {
        const std::string mySerial = session->device ? session->device->physical_serial()
                                                     : std::string();
        if (!mySerial.empty()) {
            std::vector<std::wstring> older;
            {
                std::lock_guard<std::mutex> guard(g_agent_sessions_mutex);
                for (const auto &other : g_agent_sessions) {
                    if (other->busId == session->busId) continue;
                    // ⓘ A session that has not reached ready yet has no serial,
                    // so it cannot be matched -- and does not need to be: it is
                    // not holding the pad's control endpoint either.
                    // ⓘ Never another TV's: its node names mean nothing here,
                    // and a pad cannot be on two TVs (tv_sessions.inl).
                    if (!tv_sessions::same_tv(other->owner, session->owner)) continue;
                    std::lock_guard<std::mutex> otherLock(other->mutex);
                    if (same_controller::matches(mySerial, myTvPath,
                                                 other->physicalSerial, other->tvPath)) {
                        older.push_back(other->busId);
                    }
                }
            }
            for (const std::wstring &staleId : older) {
                device_log::session_w()
                    << L"retiring an older session for the same controller busid="
                    << staleId << L" -- it was still holding the pad";
                (void)stop_bridge_session(staleId);
            }
        }
    }

    // ⭐ ONE DEVICE, ONE NICKNAME (T-194). A dongle arrives as two or three
    // separate bridges and each was named on its own: "Bean" and "Luna" for
    // the two halves of one RONGYUAN dongle, measured 2026-09-19. The nickname
    // is the word said out loud, so one device has to answer to one of them.
    //
    // ⓘ HERE, NOT AT SESSION CREATION, because this is the first moment the
    // device has a name at all: the product string and the serial both arrive
    // with the TV's HELLO, about a second after the session exists. So a
    // session is born with a name of its own and adopts its siblings' name if
    // it turns out to have siblings. ⚠️ The two disagree for that second, the
    // same second in which the panel shows no serial and no product either.
    //
    // ⛔ OUTSIDE session->mutex, exactly like the retire block above: taking
    // g_agent_sessions_mutex while holding a session mutex inverts the lock
    // order the rest of this file uses.
    {
        const std::string myProduct = session->device ? session->device->product_name()
                                                      : std::string();
        std::string adopted;
        if (!myProduct.empty()) {
            const std::string mySerial = session->device ? session->device->physical_serial()
                                                         : std::string();
            const uint16_t myVendorId = session->device ? session->device->vendor_id() : 0;
            const uint16_t myProductId = session->device ? session->device->product_id() : 0;
            std::lock_guard<std::mutex> guard(g_agent_sessions_mutex);
            for (const auto &other : g_agent_sessions) {
                if (other->busId == session->busId) continue;
                if (!other->device) continue;
                // ⓘ Never another TV's, as the retire block above (code
                // review, 2026-10-09): two TVs' identical pads with no serial
                // shared one nickname, and the tabs and the tray, which group
                // by nickname, showed them as one device.
                if (!tv_sessions::same_tv(other->owner, session->owner)) continue;
                std::lock_guard<std::mutex> otherLock(other->mutex);
                if (other->nickname.empty()) continue;
                // ⓘ Read live from the other device rather than from its
                // session fields: physicalSerial is copied in a moment further
                // down its own worker, so a sibling can have answered HELLO
                // while its copy is still empty.
                // ⭐ THE FIRST MATCH WINS, AND THAT IS WHY THE ANSWER IS
                // STABLE. g_agent_sessions is in creation order, so the first
                // sibling found is the OLDEST live part -- the one part that
                // has nothing older to adopt from and therefore keeps the name
                // it was minted with. Every other part reaches the same one,
                // whatever order the parts happen to resolve in. ⛔ Taking the
                // nearest match instead would let two parts resolving in the
                // same millisecond adopt from each other and diverge.
                if (same_device::matches(myVendorId, myProductId, mySerial, myProduct,
                                         other->device->vendor_id(),
                                         other->device->product_id(),
                                         other->device->physical_serial(),
                                         other->device->product_name())) {
                    adopted = other->nickname;
                    break;
                }
            }
        }
        if (!adopted.empty()) {
            std::string ordinalNow;
            std::string previous;
            {
                std::lock_guard<std::mutex> lock(session->mutex);
                ordinalNow = session->ordinal;
                previous = session->nickname;
                session->nickname = adopted;
            }
            if (previous != adopted) {
                // ⭐ Both names on the line: the one it was announced under at
                // "agent bridge starting" a second ago, and the one everything
                // after this says. Without the old one that first line looks
                // like a session that never existed.
                device_log::session(device_log::msg()
                    << ordinalNow << " adopts the nickname " << adopted
                    << " (was " << previous << ") -- another part of \""
                    << myProduct << "\" is already bridged");
            }
        }
    }

    // Per-controller config: pick up the physical serial, then auto-link if a
    // config claims it. A manual link made later overrides this for the life of
    // the session -- auto_link decides the starting point, not the whole story.
    {
        const std::string serial = session->device ? session->device->physical_serial()
                                                   : std::string();
        std::lock_guard<std::mutex> lock(session->mutex);
        session->physicalSerial = serial;
        session->tvPath = myTvPath;
        if (session->linkedConfig.empty()) {
            session->linkedConfig = config_store::auto_link_for(serial, session->kind);
            if (!session->linkedConfig.empty()) {
                device_log::config(device_log::msg()
                    << session->ordinal << " auto-linked to " << session->linkedConfig);
            }
        }
        // The device resolves its own settings section on the output path, so
        // it needs the link too -- the session field alone would be a link
        // nothing acts on.
        if (session->device) {
            session->device->set_linked_config(session->linkedConfig);
        }
    }

    // ⭐ Read the controller's own gyro calibration, once the session is up.
    //
    // Here rather than at attach because it is a round trip to the TV: the
    // feature request has to reach the physical pad and come back. Best
    // effort -- a controller whose calibration cannot be read still works,
    // on the old fixed scale, and says so in the log.
    //
    // ⭐ OUTSIDE THE SESSION'S LOCK (code review, 2026-10-05). The round trip
    // takes up to a second, and it held the lock: the settings page's listing,
    // asked for four times a second, waited on it while holding every
    // session's lock, and a TV command arriving meanwhile could miss its own
    // one-second wait. ⓘ The kind and the ordinal are set when the session is
    // made and never change, so they are read here without the lock.
    if (session->kind == "ds5" || session->kind == "ds5_usb" ||
        session->kind == "ds5e" || session->kind == "ds5e_usb") {
        ctm_gyro_calib::fetch(session->device.get(), backendPtr, session->ordinal);
    } else if (session->kind == "ds4_usb") {
        // ⭐ A DS4 HAS A GYRO TOO, and ships its own calibration. Without it
        // the fallback scale makes motion about 60 times too slow, which looks
        // like a broken gyro rather than a missing read. On a cable it is
        // report 0x02, in the DualSense's field order.
        ctm_gyro_calib::fetch(session->device.get(), backendPtr, session->ordinal,
                              ctm_gyro_calib::kDs4UsbCalibration);
    } else if (session->kind == "ds4") {
        // ⚠️ Over Bluetooth it is report 0x05 with the plus values grouped
        // first. Read off the Linux driver, NOT measured: no TV here can pair a
        // DS4 over Bluetooth.
        ctm_gyro_calib::fetch(session->device.get(), backendPtr, session->ordinal,
                              ctm_gyro_calib::kDs4BtCalibration);
    }

    session->ready.store(true);
    device_log::session_w() << L"agent bridge ready kind=" << widen_ascii(session->kind.c_str(), session->kind.size())
               << L" port=" << session->port << L" busid=" << session->busId;

    // ⭐⭐ THE WINDOW COMES UP ON THE CONTROLLER THAT JUST BRIDGED
    // (rhoquinn8217, 2026-09-01). Bridging is always a deliberate act -- there
    // is no mechanism that re-establishes a dropped connection on its own, so
    // this cannot fire in a loop.
    //
    // ⛔ EXCEPT WHEN THE WINDOW IS ALREADY IN FRONT. Then the person is looking
    // at it and using it, and destroying what they are doing to show them a tab
    // they can reach in one press is the wrong trade. Same rule the chord
    // already follows.
    //
    // ⓘ Unfocused but open counts as closed here: it is rebuilt on the new
    // controller, which is what makes a pad picked up from the sofa land
    // somewhere useful.
    //
    // ⭐ EVERY KIND, a keyboard or a mouse as much as a controller (rhoquinn8217,
    // 2026-09-15): "I want this to be a confirmation for the user that the device
    // they wanted to bridge has." A device that takes no config still opens it.
    //
    // ⭐ AND ONLY WHILE "Open on bridge" IS ON (rhoquinn8217, 2026-10-09): a
    // switch in the tray menu and on the settings page's Overview, on until it
    // is turned off (open_on_bridge.inl).
    if (open_on_bridge::opens_window(ctm_open_ui::g_open_ui, ui_open_on_bridge(),
                                     ctm_open_ui::window_has_foreground())) {
        device_log::session_w() << L"bridge: opening the settings window on "
                   << widen_ascii(session->ordinal.c_str(), session->ordinal.size());
        // ⛔⛔ ON ITS OWN THREAD, NEVER THIS ONE (rhoquinn8217, 2026-09-01:
        // "when two controllers are bridged, all buttons are rapid firing").
        //
        // ⚠️ THIS IS THE REPORT RELAY THREAD. ctm_chord_show_ui closes any open
        // window, POLLS UP TO A SECOND for it to actually go, then launches a
        // browser -- so the controller's reports queued behind all of that and
        // flushed in a burst afterwards, which reads as every button firing
        // over and over. Two controllers, two stalled relays.
        //
        // ⓘ Detached because nothing here waits on the result: the window opens
        // when it opens, and the session must not care.
        // ⓘ The target is set even when another open is already running: the
        // one in flight reads it as it builds its URL, so bridging two pads in
        // quick succession lands on the second -- the one just picked up.
        // ⭐ BACK ON, 2026-09-01, once the cause was understood. Bridging two
        // pads rapid-fired every button, and switching this off was how that
        // was traced -- but the fault was never here. It was the synthetic
        // KEYBOARD: one shared last-writer-wins state that every gated
        // controller wrote on every report, so an idle pad cancelled a held one
        // hundreds of times a second. Opening the window was merely what put
        // two gated pads on the config page at once.
        //
        // ⓘ ctm_chord_show_ui takes the one-at-a-time claim itself and leaves
        // if it cannot get it -- and it sets the target first either way, so
        // bridging two pads in quick succession lands on the second, which is
        // the one just picked up. ⚠️ Only while the open in flight has not
        // built its URL yet; after that the window keeps the device it was
        // launched for (see the note beside claim_open_on).
        const std::string target = session->ordinal;
        std::thread([target]() { ctm_chord_show_ui(target); }).detach();
    } else if (ctm_open_ui::g_open_ui && !ui_open_on_bridge()) {
        device_log::session_w() << L"bridge: not opening the settings window on "
                   << widen_ascii(session->ordinal.c_str(), session->ordinal.size())
                   << L": Open on bridge is off";
    }
    // Bring up the synthetic gyro mouse the first time a DualSense session is
    // ready. Idempotent -- later sessions are no-ops. Always-present by design:
    // the gyro gate decides whether it MOVES, not whether it exists, so
    // enabling gyro mid-session through live config works without a reseat.
    // ⛔ A DS4 AS WELL, now that its gyro is read. The gyro never starts the mouse
    // itself -- the touchpad, stick and trigger hooks do when they emit -- so a
    // DS4 left off this list would pile movement into a mailbox nothing drains.
    if (session->kind == "ds5" || session->kind == "ds5_usb" ||
        session->kind == "ds5e" || session->kind == "ds5e_usb" || session->kind == "ds4" ||
        session->kind == "ds4_usb") {
        ctm_gyro_mouse_ensure_mouse_started();   // defined in mouse_device.inl
    }

    // ⭐ The keyboard comes up for ANY controller that can carry a config, not
    // just a DualSense -- buttons are the universal capability, and an Xbox pad
    // can be rebound too.
    //
    // ⓘ Same always-present reasoning as the mouse: a rebind configured
    // mid-session should work without a reseat. It sends nothing until
    // something is bound.
    if (config_store::kind_supports_config(session->kind)) {
        ctm_rebind_ensure_keyboard_started();
    }
    {
        std::string linked;
        {
            std::lock_guard<std::mutex> lock(session->mutex);
            linked = session->linkedConfig;
        }
        ds5_apply_initial_settings(backendPtr, linked, session->device.get());

        // ⭐ Push the audio buffer too, now that the link is known.
        //
        // ⛔ The handshake read in bridge.inl can only see the shared section:
        // it runs before the session exists, so no config is linked yet. Without
        // this, a controller linked to a config holding audio_latency_ms would
        // be sent the SHARED value at bridge and only get its own on the next
        // config change. Found 2026-08-22, when a stale shared 7 muted a
        // controller whose own config said 100.
        const std::string latencyKind = config_store::settings_kind_for(session->kind);
        if (!latencyKind.empty()) {
            const std::string section = device_settings_section(latencyKind.c_str(), linked);
            const int latency = device_config_int(section.c_str(), "audio_latency_ms", -1);
            if (latency >= 0 && latency <= 255) {
                std::wstring latencyError;
                if (backendPtr->send_audio_latency(static_cast<uint16_t>(latency), &latencyError)) {
                    device_log::report(device_log::msg()
                        << section << ": audio latency set to " << latency << " ms at bridge");
                } else {
                    // ⛔ The failure used to be silent -- the error was captured
                    // and discarded, which made an unreachable config and an
                    // unsupported feature look identical. That cost hours.
                    device_log::report(device_log::msg()
                        << section << ": audio latency FAILED -- "
                        << narrow_ascii(latencyError));
                }
            }

            // ⭐⭐ AND THE AUDIO SETTINGS, T-130, 2026-08-25.
            //
            // ⛔ THE FAULT: ds5_output_overrides.inl patches speaker volume,
            // headset volume, routing and rumble gain into the host's outbound
            // report, and every one of those overrides begins
            // `if (data[0] != 0x02) return;`. 0x02 is the WIRED report id. Over
            // Bluetooth the host sends 0x36, so all of them silently did
            // nothing. Heard rather than inferred: speaker_volume = 0 left the
            // controller at full volume.
            //
            // They travel instead of being patched here because a Bluetooth
            // output report is SIGNED, and the TV re-signs only when it patched
            // something itself. A report edited on this side alone would arrive
            // with a stale signature and be dropped by the controller.
            //
            // ⚠️⚠️ ONLY THREE OF THE FOUR TRAVEL: speaker volume, headset
            // volume and the routing mode. THE RUMBLE GAIN DOES NOT, here or
            // in the sweep -- check the payload below rather than this
            // sentence. ➡️ So plain-rumble scaling, and the floor with it, is
            // WIRED-ONLY (T-145). Recorded 2026-09-19; the fix is a protocol
            // field the TV would have to apply, which is the rest of T-145.
            //
            // ⓘ Same place as the latency send, and for the same reason the
            // comment above gives: the handshake in bridge.inl runs before the
            // session exists, so no config is linked yet and only the shared
            // section is visible.
            //
            // ⚠️ kAudioUnset for anything absent, never zero -- zero is a legal
            // percentage and means silent.
            {
                const int spk  = device_config_int(section.c_str(), "speaker_volume", -1);
                const int hset = device_config_int(section.c_str(), "headset_volume", -1);
                const std::string outStr = device_config_str(section.c_str(), "audio_output");

                uint8_t mode = CtmBridgeProtocol::kAudioUnset;
                // ⓘ The TV's enum: AUTO 0, OFF 1, SPEAKER 2, HEADSET 3, BOTH 4.
                // headset_mono has NO TV equivalent -- it is a Windows-side
                // downmix -- so it maps to HEADSET and the mono part stays on
                // this side.
                if      (outStr == "auto")         mode = 0;
                else if (outStr == "off")          mode = 1;
                else if (outStr == "speaker")      mode = 2;
                else if (outStr == "headset")      mode = 3;
                else if (outStr == "headset_mono") mode = 3;
                else if (outStr == "both")         mode = 4;

                const uint8_t spkByte  = (spk  >= 0 && spk  <= 100)
                                       ? static_cast<uint8_t>(spk)
                                       : CtmBridgeProtocol::kAudioUnset;
                const uint8_t hsetByte = (hset >= 0 && hset <= 100)
                                       ? static_cast<uint8_t>(hset)
                                       : CtmBridgeProtocol::kAudioUnset;

                if (spkByte  != CtmBridgeProtocol::kAudioUnset ||
                    hsetByte != CtmBridgeProtocol::kAudioUnset ||
                    mode     != CtmBridgeProtocol::kAudioUnset) {
                    std::wstring audioError;
                    if (backendPtr->send_audio_settings(spkByte, hsetByte, mode, &audioError)) {
                        device_log::report(device_log::msg()
                            << section << ": audio settings sent at bridge -- speaker="
                            << static_cast<int>(spkByte) << " headset="
                            << static_cast<int>(hsetByte) << " mode="
                            << static_cast<int>(mode)
                            << " (255 = leave the TV's value)");
                    } else {
                        device_log::config(device_log::msg()
                            << section << ": audio settings FAILED -- "
                            << narrow_ascii(audioError));
                    }
                }
            }
        }
    }
    if (session->stopping.load()) {
        return;     // nor attach it: the stop is removing it
    }
    if (!run_usbip_attach(session->busId, kDefaultUsbipPort)) {
        std::wcerr << L"agent local attach failed busid=" << session->busId << L"\n";
        device_log::session_w() << L"attach FAILED busid=" << session->busId
                                << L": Windows did not get this pad; the [usb] line above says why";
    }
}

// ⓘ onlyPeer: the only address the bridge port then takes a connection from,
// in host byte order; 0 for any. See AgentBridgeSession.
// ⓘ owner: the TV asking, as tv_sessions.inl names it. `port` 0 asks for one
// from [global] bridge_port_range -- from its top for a caller at home
// (`fromHome`), from its bottom for one at another site -- and the session's
// port goes back in *startedPort either way.
static bool start_bridge_session_for(const std::string &kind, uint16_t port, const std::wstring &busId,
                                     uint32_t onlyPeer, const std::string &owner, bool fromHome,
                                     std::wstring *error, uint16_t *startedPort)
{
    const std::string busIdAscii = narrow_ascii(busId);
    if (busIdAscii.empty() || busIdAscii.size() > 31) {
        if (error) *error = L"invalid bridge busid";
        return false;
    }

    // Pending reaps first (same thread as the drain loop): a TV re-plug can
    // reuse a busid whose old session just died — deduping against that dying
    // session would leave the TV talking to nothing.
    drain_bridge_session_reaps();

    // ⭐⭐ WHICH TV, AND WHICH PORT: tv_sessions.inl. A re-plug arrives with a
    // fresh busid but the same port, and the TV's own older session still
    // holding that port (dead client, wedged handshake) must go first -- two
    // listeners on one port (SO_REUSEADDR) steal each other's connections,
    // which is how "bridge protocol header rejected" flaps happened. ⛔ Another
    // TV's session on it is a pad in use, and is never evicted.
    uint16_t firstPort = tv_sessions::kFirstPort, lastPort = tv_sessions::kLastPort;
    const std::string range = device_config_str("global", "bridge_port_range");
    if (!range.empty() && !tv_sessions::parse_range(range, &firstPort, &lastPort)) {
        device_log::session_s() << "agent bridge: [global] bridge_port_range = " << range
                                << " is not a range like 48055-48074, so 48055-48074 is used";
    }
    // ⓘ The control port this agent listens on, not the default (code review,
    // 2026-10-09): one set inside the range was handed out, and the bridge's
    // SO_REUSEADDR bind on top of it stole the control port's connections.
    const std::vector<uint16_t> reserved = {g_agent_control_port, g_rest_port, kDefaultUsbipPort};
    const std::wstring tvForLog = owner.empty() ? std::wstring(L"unproven")
                                                : widen_ascii(owner.c_str(), owner.size()).substr(0, 8);
    std::unique_lock<std::mutex> lock(g_agent_sessions_mutex);
    tv_sessions::Plan plan;
    for (int pass = 0;; ++pass) {
        std::vector<tv_sessions::Held> held;
        for (const auto &existing : g_agent_sessions) {
            held.push_back({existing->busIdAscii, existing->port, existing->owner});
        }
        plan = tv_sessions::plan_start(held, busIdAscii, port, owner, firstPort, lastPort, reserved,
                                       fromHome);
        if (plan.what != tv_sessions::Start::ReplaceStale || pass > 0) break;
        lock.unlock();
        const std::wstring staleBusId = widen_ascii(plan.stale.c_str(), plan.stale.size());
        device_log::session_w() << L"agent bridge replacing stale session busid=" << staleBusId
                   << L" (port " << port << L" requested by busid=" << busId << L")";
        (void)stop_bridge_session(staleBusId);
        lock.lock();
    }
    switch (plan.what) {
    case tv_sessions::Start::AlreadyYours:
        if (startedPort) *startedPort = plan.port;
        return true;
    case tv_sessions::Start::NameTaken:
        device_log::session_w() << L"agent bridge refused busid=" << busId << L" from tv=" << tvForLog
                                << L": another TV has a bridge by that name";
        if (error) *error = L"that busid belongs to another TV";
        return false;
    case tv_sessions::Start::PortTaken:
        device_log::session_w() << L"agent bridge refused busid=" << busId << L" from tv=" << tvForLog
                                << L": port " << port << L" is another TV's bridge";
        if (error) *error = L"port " + std::to_wstring(port) + L" is held by another TV's bridge";
        return false;
    case tv_sessions::Start::NoFreePort:
        device_log::session_w() << L"agent bridge refused busid=" << busId << L": no free port in "
                                << firstPort << L"-" << lastPort << L" ([global] bridge_port_range)";
        if (error) *error = L"no free port in bridge_port_range";
        return false;
    case tv_sessions::Start::ReplaceStale:
        if (error) *error = L"the port's older session did not stop";
        return false;
    case tv_sessions::Start::Create:
        break;
    }
    port = plan.port;

    device_config_invalidate();   // a reseat re-reads the settings file
    ctm_config_watcher::ensure_started();   // live config changes, no reseat
    ctm_config_watcher::adopt_config_store();   // ...including per-controller ones
    config_store::reload_all();             // per-controller config files
    auto session = std::make_unique<AgentBridgeSession>();
    // Assigned at CREATION, not when listed -- two consecutive device lists
    // must not disagree. Monotonic per kind and never reused: ds5_1 that
    // unbridges comes back as ds5_3, so a command naming a stale ordinal fails
    // instead of hitting a different controller.
    {
        static std::map<std::string, unsigned> nextOrdinal;
        session->ordinal = kind + "_" + std::to_string(++nextOrdinal[kind]);
    }
    // ⭐ The nickname is assigned HERE, beside the ordinal, so the two share a
    // lifetime exactly: both are born with the session and both are gone when
    // it ends. ⓘ That is what fixes the re-rolling -- the settings window is
    // killed and recreated by every chord, but the agent is not.
    //
    // ⛔ Not an identifier. The ordinal remains the handle for commands and the
    // log; this is the word you say out loud.
    {
        // ⓘ g_agent_sessions_mutex is already held here -- taking it again
        // would deadlock.
        std::vector<std::string> taken;
        for (const auto &s : g_agent_sessions) {
            if (s && !s->nickname.empty()) taken.push_back(s->nickname);
        }
        session->nickname = ctm_nickname::pick(taken);
    }
    session->kind = kind;
    session->busId = busId;
    session->busIdAscii = busIdAscii;
    session->port = port;
    session->onlyPeer = onlyPeer;
    session->owner = owner;
    session->device = std::make_shared<CtmUsbipDevice>();
    AgentBridgeSession *sessionPtr = session.get();
    const std::string ordinalForLog = session->ordinal;
    const std::string nicknameForLog = session->nickname;
    session->worker = std::thread([sessionPtr]() { bridge_session_worker(sessionPtr); });
    g_agent_sessions.push_back(std::move(session));
    // ⭐ BOTH NAMES ON THE LINE, so it greps either way: the ordinal is what a
    // command or a bug report will quote, the nickname is what the person
    // watching actually called it.
    device_log::session_w() << L"agent bridge starting "
               << widen_ascii(ordinalForLog.c_str(), ordinalForLog.size())
               << L" (" << widen_ascii(nicknameForLog.c_str(), nicknameForLog.size()) << L")"
               << L" kind=" << widen_ascii(kind.c_str(), kind.size())
               << L" port=" << port << L" busid=" << busId << L" tv=" << tvForLog;
    if (startedPort) *startedPort = port;
    return true;
}

// The same for a caller with no address to hold it to and no TV to name: the
// REST API, which answers this PC alone.
static bool start_bridge_session(const std::string &kind, uint16_t port, const std::wstring &busId, std::wstring *error)
{
    return start_bridge_session_for(kind, port, busId, 0, std::string(), true, error, nullptr);
}

static bool stop_bridge_session(const std::wstring &busId)
{
    std::unique_ptr<AgentBridgeSession> session;
    {
        std::lock_guard<std::mutex> lock(g_agent_sessions_mutex);
        for (size_t i = 0; i < g_agent_sessions.size(); ++i) {
            if (g_agent_sessions[i]->busId != busId) {
                continue;
            }
            session = std::move(g_agent_sessions[i]);
            g_agent_sessions.erase(g_agent_sessions.begin() + static_cast<std::ptrdiff_t>(i));
            break;
        }
    }
    if (!session) {
        return false;
    }
    session->stopping.store(true);
    if (g_agent_usbip_server) {
        g_agent_usbip_server->remove_device(session->busIdAscii);
    }
    {
        std::lock_guard<std::mutex> lock(session->mutex);
        if (session->backend) {
            session->backend->stop();
        }
    }
    if (session->device) {
        session->device->wake_input_waiters();
        session->device->stop();
    }
    if (session->worker.joinable()) {
        session->worker.join();
    }
    // ⭐ And once more now the worker is gone (an independent review,
    // 2026-10-07): it may have exported the device between the first removal
    // and seeing the stop, and an export left behind is a pad Windows keeps.
    if (g_agent_usbip_server) {
        g_agent_usbip_server->remove_device(session->busIdAscii);
    }
    device_log::session_w() << L"agent bridge stopped busid=" << busId;
    return true;
}

// BRIDGE_STOP from TV `owner`, calling from `peer`: only its own sessions, or
// one with no proof from the address it was started from (tv_sessions.inl's
// may_stop). False with *notYours set when the session is another TV's.
static bool stop_bridge_session_for(const std::wstring &busId, const std::string &owner, uint32_t peer,
                                    bool *notYours)
{
    if (notYours) *notYours = false;
    bool byAddress = false;
    {
        std::lock_guard<std::mutex> lock(g_agent_sessions_mutex);
        const AgentBridgeSession *session = find_bridge_session_locked(busId);
        if (!session) return false;
        if (!tv_sessions::may_stop(session->owner, session->onlyPeer, owner, peer)) {
            if (notYours) *notYours = true;
            return false;
        }
        byAddress = !tv_sessions::same_tv(session->owner, owner);
    }
    if (byAddress) {
        device_log::session_w() << L"agent bridge stop without a proof busid=" << busId
                                << L": let through, it comes from the address the bridge was started from";
    }
    return stop_bridge_session(busId);
}

static void stop_all_bridge_sessions()
{
    std::vector<std::wstring> busIds;
    {
        std::lock_guard<std::mutex> lock(g_agent_sessions_mutex);
        for (const auto &session : g_agent_sessions) {
            busIds.push_back(session->busId);
        }
    }
    for (const std::wstring &busId : busIds) {
        (void)stop_bridge_session(busId);
    }
}

// Drained once per run_agent loop tick (~1 s cadence). Runs on the same thread
// that serves BRIDGE_START/STOP, so reaps serialize with every other session
// lifecycle change.
static void drain_bridge_session_reaps()
{
    std::vector<std::wstring> requests;
    {
        std::lock_guard<std::mutex> lock(g_agent_reap_mutex);
        requests.swap(g_agent_reap_requests);
    }
    for (const std::wstring &busId : requests) {
        (void)stop_bridge_session(busId);
    }
}


#include "agent_session_sweep.inl"

static void send_text(SOCKET sock, const std::string &text)
{
    send(sock, text.c_str(), static_cast<int>(text.size()), 0);
}

static void handle_agent_client(SOCKET client, const sockaddr_in &peer)
{
    // ⛔⛔ BOUNDED, because this runs inline on the one loop that answers every
    // TV, the REST API and discovery. With no limit, a peer that connected and
    // sent nothing -- a port scanner waiting for a banner, a TV whose network
    // dropped between connect and send -- held this recv, and the whole
    // listener with it, until that peer let go (code review, 2026-10-05).
    // ⓘ One second: the TV gives each call one second to be answered, so a
    // command that has not arrived by then has nobody waiting for its reply.
    DWORD timeoutMs = 1000;
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char *>(&timeoutMs), sizeof(timeoutMs));
    setsockopt(client, SOL_SOCKET, SO_SNDTIMEO,
               reinterpret_cast<const char *>(&timeoutMs), sizeof(timeoutMs));

    // ⓘ Up to a newline: a command with its pairing proof is about 460
    // bytes, which one read usually holds but need not. Each read still waits
    // at most the second above.
    char line[2048] = {};
    int got = 0;
    while (got < static_cast<int>(sizeof(line)) - 1) {
        const int n = recv(client, line + got, static_cast<int>(sizeof(line)) - 1 - got, 0);
        if (n <= 0) break;
        got += n;
        if (memchr(line, '\n', static_cast<size_t>(got)) != nullptr) break;
    }
    if (got <= 0) {
        return;
    }
    line[got] = '\0';
    char *nl = strpbrk(line, "\r\n");
    if (nl) *nl = '\0';

    std::istringstream input(line);
    std::string command;
    input >> command;

    // ⛔ TWO COMMANDS ARE THIS PC'S ALONE (code review, 2026-10-05): importing
    // a USB device from the caller's machine, and dropping every bridge. The
    // TV sends neither, and this port is forwarded to the internet for remote
    // play. control_caller.inl says why each one.
    if (!control_caller::allowed(command, ntohl(peer.sin_addr.s_addr))) {
        char peerIp[INET_ADDRSTRLEN] = {};
        inet_ntop(AF_INET, &peer.sin_addr, peerIp, sizeof(peerIp));
        device_log::session_s() << "control: refused " << command << " from " << peerIp
                                << ": only this PC may send it";
        send_text(client, "ERR only this PC may send " + command + "\n");
        return;
    }

    const uint32_t peerAddr = ntohl(peer.sin_addr.s_addr);

    // ⭐ A CHALLENGE FOR THE PAIRING PROOF (paired_tv.inl). Anyone may ask: it
    // costs a random number, and the list of them is bounded.
    if (command == "PROOF_CHALLENGE") {
        const std::string nonce = paired_tv::new_nonce();
        if (nonce.empty()) {
            send_text(client, "ERR no challenge to give\n");
            return;
        }
        {
            std::lock_guard<std::mutex> lock(g_paired_tv_mutex);
            g_paired_tv_nonces.issue(nonce, peerAddr, std::chrono::steady_clock::now());
        }
        send_text(client, "OK NONCE " + nonce + "\n");
        return;
    }

    // ⭐⭐ WHO MAY BRIDGE (paired_tv.inl). A TV calling from the internet must
    // prove it is paired with this PC's streaming host; this PC and the home
    // network need not. A proof sent from home is checked and written down
    // all the same. In "log" mode, the default until the remote TVs send the
    // proof, nobody is turned away: the verdict is only written down.
    uint32_t onlyPeer = 0;
    // ⭐ The TV asking, when its proof was PROVEN: tv_sessions.inl says what it
    // decides, and why nothing less than proven may name a TV.
    std::string owner;
    if (command == "BRIDGE_START" || command == "BRIDGE_STOP") {
        const PairedTvMode mode = paired_tv_mode();
        if (mode != PairedTvMode::Off) {
            const paired_tv::Need need = paired_tv::need_for(
                peerAddr, true, device_config_bool("global", "paired_tv_proof_at_home", false));
            const bool sent = std::string(line).find(paired_tv::kProofWord) != std::string::npos;
            if (need == paired_tv::Need::Required || sent) {
                std::vector<paired_tv::Device> devices;
                std::wstring where;
                const bool haveDevices = load_paired_devices(&devices, &where);
                std::string who;
                paired_tv::Verdict verdict;
                {
                    std::lock_guard<std::mutex> lock(g_paired_tv_mutex);
                    verdict = paired_tv::check(line, peerAddr, &g_paired_tv_nonces,
                                               haveDevices ? &devices : nullptr, &who,
                                               std::chrono::steady_clock::now());
                }
                if (verdict == paired_tv::Verdict::Proven) {
                    // ⓘ The fingerprint check() just matched; asking check()
                    // again would find its nonce spent.
                    std::string unsigned_;
                    paired_tv::Proof proof;
                    if (paired_tv::split(std::string(line), &unsigned_, &proof)) owner = proof.fingerprint;
                }
                const bool letIn = paired_tv::lets_in(verdict, need);
                char peerIp[INET_ADDRSTRLEN] = {};
                inet_ntop(AF_INET, &peer.sin_addr, peerIp, sizeof(peerIp));
                device_log::session_s()
                    << "control: " << command << " from " << peerIp << ": " << paired_tv::describe(verdict)
                    << (who.empty() ? std::string() : " (" + who + ")")
                    << (need == paired_tv::Need::Required ? "" : "; none is needed from the home network")
                    << (!letIn && mode == PairedTvMode::Log ? "; let in, the check is log-only" : "")
                    << (!letIn && mode == PairedTvMode::On ? "; REFUSED" : "")
                    << (haveDevices ? std::string() : "; the host's paired devices could not be read from " + narrow_ascii(where));
                if (!letIn && mode == PairedTvMode::On) {
                    send_text(client, std::string("ERR ") + paired_tv::describe(verdict) + "\n");
                    return;
                }
            }
            // ⭐ AND THE BRIDGE PORT IS THE CALLER'S ALONE, from home as from
            // the internet, in "log" as in "on". The bridge ports are forwarded
            // for remote play like this one, so a port that took anyone could
            // be reached from the internet in the 30 s before its TV connects,
            // or the 5 s after it drops, and typed through. It turns no TV
            // away: a TV connects from the address it asked from.
            onlyPeer = peerAddr;
        }
    }

    if (command == "STATUS") {
        // ⭐ With this listener's protocol level, and the oldest TV level it
        // works with: the TV refuses to bridge to a listener it is too new for
        // (protocol_level.inl).
        send_text(client, protocol_level::status_reply());
        return;
    }

    if (command == "USBIP_ATTACH") {
        std::string busIdAscii;
        input >> busIdAscii;
        if (busIdAscii.empty() || busIdAscii.size() > 31) {
            send_text(client, "ERR bad busid\n");
            return;
        }
        char peerIp[INET_ADDRSTRLEN] = {};
        inet_ntop(AF_INET, &peer.sin_addr, peerIp, sizeof(peerIp));
        if (run_usbip_attach_to(widen_ascii(peerIp, strlen(peerIp)), widen_ascii(busIdAscii.c_str(), busIdAscii.size()), kDefaultUsbipPort)) {
            send_text(client, "OK usbip attached\n");
        } else {
            send_text(client, "ERR usbip attach failed\n");
        }
        return;
    }

    if (command == "BRIDGE_START") {
        std::string kind;
        unsigned long port = 0;
        std::string busIdAscii;
        input >> kind >> port >> busIdAscii;
        if ((kind != "ds4" && kind != "ds4_usb" && kind != "ds5" && kind != "ds5_usb" &&
             kind != "ds5e" && kind != "ds5e_usb" && kind != "hid" && kind != "puck" && kind != "xbox") ||
            (port != 0 && (port < 1024 || port > 65535)) ||
            busIdAscii.empty() || busIdAscii.size() > 31) {
            send_text(client, "ERR bad bridge args\n");
            return;
        }
        // ⭐⭐ A TV TOO OLD TO WORK WITH THIS LISTENER IS TOLD SO (rhoquinn8217,
        // 2026-10-08; protocol_level.inl), in words its bridge panel shows.
        const std::string tooOld = protocol_level::refusal_for(line);
        if (!tooOld.empty()) {
            char peerIp[INET_ADDRSTRLEN] = {};
            inet_ntop(AF_INET, &peer.sin_addr, peerIp, sizeof(peerIp));
            device_log::session_s() << "control: BRIDGE_START from " << peerIp << " refused: the TV app is level "
                                    << protocol_level::tv_level(line) << ", and this listener needs "
                                    << protocol_level::kMinTv << " or newer";
            send_text(client, tooOld);
            return;
        }
        std::wstring error;
        uint16_t startedPort = 0;
        // ⓘ Home is this PC or the home network, as the proof reads it
        // (paired_tv.inl): no router between, so no forwarded port needed.
        const bool fromHome = paired_tv::is_this_pc(peerAddr) || paired_tv::is_home_network(peerAddr);
        if (start_bridge_session_for(kind, static_cast<uint16_t>(port), widen_ascii(busIdAscii.c_str(), busIdAscii.size()),
                                     onlyPeer, owner, fromHome, &error, &startedPort)) {
            // ⭐ The port it was given, to a TV that asked for one with port 0;
            // an older TV chose its own and reads only the OK.
            send_text(client, port == 0 ? "OK bridge starting port=" + std::to_string(startedPort) + "\n"
                                        : std::string("OK bridge starting\n"));
        } else {
            std::string err = narrow_ascii(error);
            send_text(client, "ERR " + (err.empty() ? std::string("bridge start failed") : err) + "\n");
        }
        return;
    }

    if (command == "BRIDGE_STOP") {
        std::string busIdAscii;
        input >> busIdAscii;
        if (busIdAscii.empty()) {
            send_text(client, "ERR bad busid\n");
            return;
        }
        // Actually tear the session down: removes the device from the shared
        // USB/IP server (Windows sees an unplug), stops the cmub_ backend,
        // joins the worker. This is what makes "controller off" cleanly
        // remove the virtual device instead of leaving a stale export.
        const std::wstring busId = widen_ascii(busIdAscii.c_str(), busIdAscii.size());
        bool notYours = false;
        if (stop_bridge_session_for(busId, owner, peerAddr, &notYours)) {
            send_text(client, "OK bridge stopped\n");
        } else if (notYours) {
            device_log::session_w() << L"agent bridge stop refused busid=" << busId
                                    << L": it belongs to another TV";
            send_text(client, "ERR that bridge belongs to another TV\n");
        } else {
            send_text(client, "OK no such bridge\n");
        }
        return;
    }

    if (command == "RESTART") {
        std::string arg;
        input >> arg;
        if (arg == "hard") {
            // Full restart of the process via the SCM (service mode only). The
            // session/USB-IP state is rebuilt from scratch when the TV
            // re-issues BRIDGE_START after the service comes back.
            if (request_service_restart()) {
                send_text(client, "OK service restarting\n");
            } else {
                send_text(client, "ERR hard restart requires service mode\n");
            }
            return;
        }
        // Soft reset: tear down every bridge session and its USB/IP export
        // (Windows sees each virtual device unplug); the agent keeps listening
        // and rebuilds on the next BRIDGE_START.
        stop_all_bridge_sessions();
        send_text(client, "OK bridges reset\n");
        return;
    }

    send_text(client, "ERR unknown command\n");
}

static int run_agent(uint16_t port)
{
    g_agent_control_port = port;
    // ⭐ The program and its build first, the relay after (2026-10-12). The line
    // named only the relay, "ctm-usbip 0.0.2 (build 2)", the same in every
    // build, so the log never said which DS5-USBIP had started.
    // ⓘ Tools find the line by "agent starting", which stays.
    device_log::session_w() << widen_ascii(PRODUCT_NAME, strlen(PRODUCT_NAME)) << L" "
               << widen_ascii(PRODUCT_VERSION_DISPLAY, strlen(PRODUCT_VERSION_DISPLAY))
               << L" agent starting (relay: ctm-usbip "
               << widen_ascii(CTM_VERSION_DISPLAY, strlen(CTM_VERSION_DISPLAY)) << L")";
    WSADATA data = {};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        start_report::failed(L"DS5-USBIP could not start: Windows networking did not come up.\n\n" +
                             wsa_error_message(L"WSAStartup failed"));
        return 4;
    }

    SOCKET udp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    SOCKET tcp = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (udp == INVALID_SOCKET || tcp == INVALID_SOCKET) {
        start_report::failed(L"DS5-USBIP could not start: no socket could be opened.\n\n" +
                             wsa_error_message(L"agent socket failed"));
        WSACleanup();
        return 4;
    }

    BOOL reuse = TRUE;
    setsockopt(udp, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&reuse), sizeof(reuse));
    setsockopt(tcp, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&reuse), sizeof(reuse));

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(udp, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == SOCKET_ERROR ||
        bind(tcp, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == SOCKET_ERROR ||
        listen(tcp, 8) == SOCKET_ERROR) {
        // ⓘ The usual reason by far: something already has the port, and most
        // often that is another copy of this program started without --ui.
        const std::wstring why = wsa_error_message(L"agent bind/listen failed");
        start_report::failed(L"DS5-USBIP could not start: port " + std::to_wstring(port) +
                             L" is not free. Another copy may already be running; look for its icon in the tray.\n\n" + why);
        closesocket(udp);
        closesocket(tcp);
        WSACleanup();
        return 4;
    }

    g_agent_usbip_server = std::make_unique<CtmUsbipServer>();
    std::wstring usbipError;
    if (!g_agent_usbip_server->start(kDefaultUsbipPort, &usbipError)) {
        start_report::failed(L"DS5-USBIP could not start: its USB/IP server did not come up on port " +
                             std::to_wstring(kDefaultUsbipPort) + L".\n\nagent USB/IP server failed: " + usbipError);
        closesocket(udp);
        closesocket(tcp);
        WSACleanup();
        return 4;
    }

    g_rest_agent_start = std::chrono::steady_clock::now();
    SOCKET rest = INVALID_SOCKET;
    if (g_rest_port != 0) {
        std::wstring restError;
        rest = rest_open_listener(&restError);
        if (rest == INVALID_SOCKET) {
            // --rest was asked for explicitly; a silently missing API would be
            // worse than failing startup, and every other bind above is fatal.
            start_report::failed(L"DS5-USBIP could not start: the settings page's port, " +
                                 std::to_wstring(g_rest_port) + L", is not free.\n\nagent REST listener failed: " + restError);
            g_agent_usbip_server->stop();
            g_agent_usbip_server.reset();
            closesocket(udp);
            closesocket(tcp);
            WSACleanup();
            return 4;
        }
        device_log::session_w() << L"ctm agent REST API on " << (g_rest_bind_lan ? L"0.0.0.0" : L"127.0.0.1")
                   << L":" << g_rest_port
                   << (g_rest_token.empty() ? L"" : L" (bearer token required)");
    }

    device_log::session_w() << L"ctm agent listening udp/tcp port " << port;
    {
        // ⛔ EVERY RELATIVE PATH RESOLVES HERE -- configs/, device.log, the
        // shared settings file. Added 2026-08-31 after configs created in one
        // session were unfindable in the next: the answer to "where did that
        // file go" must be a grep of this line, never a theory.
        wchar_t cwd[MAX_PATH] = L"?";
        GetCurrentDirectoryW(MAX_PATH, cwd);
        // ⓘ And HOW it came to be this folder (home_folder.inl): found from
        // the exe, or named with --home. It used to be wherever the listener
        // was started from, which is the question this line was added to answer.
        device_log::session_w() << L"working directory: " << cwd
            << L" (configs/, logs and settings resolve here"
            << (home_folder::g_chosen_how.empty() ? L"" : L"; ")
            << home_folder::g_chosen_how << L")";
        // T-239: the window's place, size and layout, from the last run. Read
        // HERE because the line above is the one that settles what a relative
        // path means -- reading it earlier would resolve somewhere else.
        config_move::state_load();
    }

    // ⭐ Open the page HERE, once the agent is actually listening.
    //
    // ⛔ main.cpp only FOCUSES an existing window, and only opens a new one when
    // it finds the port already taken. So a first run -- nothing running, no
    // window -- opened nothing at all, and the page appeared only on the second
    // launch. This is the missing half.
    //
    // ⓘ Late rather than early on purpose: a page opened before the agent binds
    // would load, fail its first poll and show "cannot reach the listener".
    // ⓘ Not for Vibepollo's start (--no-start-window), whose window waits for
    // a bridge (open_on_bridge.inl).
    if (open_on_bridge::opens_at_start(ctm_open_ui::g_open_ui, ctm_open_ui::g_no_start_window,
                                       ctm_open_ui::g_ui_already_focused)) {
        ctm_open_ui::open_new(g_rest_port);
    }
    while (!g_stop.load()) {
        drain_bridge_session_reaps();
        sweep_bridge_sessions();
        apply_pending_config_to_sessions();   // push a config edit to live sessions
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(udp, &readfds);
        FD_SET(tcp, &readfds);
        if (rest != INVALID_SOCKET) {
            FD_SET(rest, &readfds);
        }
        timeval timeout {};
        timeout.tv_sec = 1;
        int rc = select(0, &readfds, nullptr, nullptr, &timeout);
        if (rc <= 0) {
            continue;
        }
        if (FD_ISSET(udp, &readfds)) {
            char buf[128] = {};
            sockaddr_in from = {};
            int fromLen = sizeof(from);
            int n = recvfrom(udp, buf, sizeof(buf) - 1, 0, reinterpret_cast<sockaddr *>(&from), &fromLen);
            if (n > 0) {
                buf[n] = '\0';
                if (strncmp(buf, "CTM_DISCOVER_V1", 15) == 0) {
                    std::string reply = "CTM_AGENT_V1 port=" + std::to_string(port) + "\n";
                    sendto(udp, reply.c_str(), static_cast<int>(reply.size()), 0,
                           reinterpret_cast<sockaddr *>(&from), fromLen);
                }
            }
        }
        if (FD_ISSET(tcp, &readfds)) {
            sockaddr_in peer = {};
            int peerLen = sizeof(peer);
            SOCKET client = accept(tcp, reinterpret_cast<sockaddr *>(&peer), &peerLen);
            if (client != INVALID_SOCKET) {
                handle_agent_client(client, peer);
                closesocket(client);
            }
        }
        if (rest != INVALID_SOCKET && FD_ISSET(rest, &readfds)) {
            sockaddr_in peer = {};
            int peerLen = sizeof(peer);
            SOCKET client = accept(rest, reinterpret_cast<sockaddr *>(&peer), &peerLen);
            if (client != INVALID_SOCKET) {
                // Inline on the loop thread, like handle_agent_client — this is
                // what lets rest_route call start/stop_bridge_session directly.
                rest_handle_client(client, port);
                closesocket(client);
            }
        }
    }

    // ⭐ THE SETTINGS WINDOW GOES WHEN THE LISTENER GOES, whichever way it was
    // stopped: the tray's Quit, Ctrl+C, or its console window being closed. A
    // page with no listener behind it can only say that it cannot reach one.
    // ⓘ Before the sockets close and the bridges come down, so the window is
    // gone at once rather than after the teardown.
    // ⛔ Only past the loop, so only a listener that actually RAN gets here. One
    // that failed to start has returned above, and the window it would find
    // belongs to the listener that is running.
    // ⓘ A listener that is KILLED runs none of this; the next start clears
    // what is left (main.cpp, "any window out there is stale").
    ui_close_window();
    // ⭐ And the tray icon, for the same reason and at the same moment: it is
    // the sign that the listener is running, and from here on it is not.
    tray_icon_remove();

    if (rest != INVALID_SOCKET) {
        closesocket(rest);
    }
    stop_all_bridge_sessions();
    // ⭐ The synthetic mouse and keyboard, BEFORE the server they are exported
    // through. Their pump threads have to be joined here or the program cannot
    // end: see synthetic_devices_stop() in main.cpp for what it did instead.
    synthetic_devices_stop();
    if (g_agent_usbip_server) {
        g_agent_usbip_server->stop();
        g_agent_usbip_server.reset();
    }
    closesocket(udp);
    closesocket(tcp);
    WSACleanup();
    return 0;
}
