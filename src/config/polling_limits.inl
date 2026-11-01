// A controller's polling limit, kept for the CONTROLLER rather than in a
// config (rhoquinn8217, 2026-10-16: "keep it with the controller").
//
// ⭐ WHY. The limit began as a config's polling_rate, so stepping through
// configs in Quick stepped the limit with them: one config said 250, the next
// Real time, and someone switching what the touchpad does found the polling
// rate moving under them. A rate is about the controller and how it is
// connected, not about what its buttons do.
//
// ⭐ KEYED BY THE SERIAL, by the same rule as auto link
// (config_store::normalise_serial): a DualSense's MAC, another device's own
// serial, never one of all zeros. A controller with no usable serial has
// nothing to be remembered by, so its limit lasts for the bridge it was set
// in (usbip/device.inl).
//
// ⓘ The file sits in the working directory beside configs/ and device.log,
// one line for each controller with a limit; a controller with no line is at
// Real time. Safe to delete: every controller is then at Real time.
//
// ⓘ No headers here: the tests include this inside a namespace.

namespace polling_limits {

inline const char *file_name() { return "polling-limits.txt"; }

// The limits a controller can have, as the page offers them. Real time is no
// limit, and is kept as no line at all.
inline bool is_limit(const std::string &value)
{
    return value == "500" || value == "250" || value == "125";
}

// What may be asked for: a limit, or Real time by its name or by nothing.
inline bool is_choice(const std::string &value)
{
    return value.empty() || value == "real_time" || is_limit(value);
}

// The file's text as serial -> limit.
// ⓘ Junk is dropped rather than refused: the file can be edited by hand, and a
// typo must neither stop the listener nor slow a controller down. A line with
// no "=", a value that is not a limit, and a serial that normalises to nothing
// are all skipped.
inline std::map<std::string, std::string> parse(const std::string &text)
{
    std::map<std::string, std::string> out;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);
        auto trim = [](std::string &t) {
            while (!t.empty() && (t.front() == ' ' || t.front() == '\t')) t.erase(t.begin());
            while (!t.empty() && (t.back() == ' ' || t.back() == '\t')) t.pop_back();
        };
        trim(key);
        trim(value);
        const std::string serial = config_store::normalise_serial(key);
        if (serial.empty() || !is_limit(value)) continue;
        out[serial] = value;
    }
    return out;
}

inline std::string format(const std::map<std::string, std::string> &limits)
{
    std::ostringstream out;
    out << "# DS5-USBIP polling limits: each controller's own, by its serial.\n"
        << "# Written when one is set. Safe to delete: every controller is then at Real time.\n";
    for (const auto &entry : limits) out << entry.first << " = " << entry.second << "\n";
    return out.str();
}

// ⓘ Read from the file the first time a limit is asked for, written whenever
// one changes. An empty g_path keeps it in memory, which is how the tests run.
inline std::mutex g_mutex;
inline std::map<std::string, std::string> g_limits;
inline bool g_loaded = false;
inline std::string g_path = file_name();

inline void load_locked()
{
    if (g_loaded) return;
    g_loaded = true;
    if (g_path.empty()) return;
    std::ifstream f(g_path);
    if (!f) return;
    std::ostringstream text;
    text << f.rdbuf();
    g_limits = parse(text.str());
}

// Whether this serial can be remembered at all.
inline bool has_identity(const std::string &serial)
{
    return !config_store::normalise_serial(serial).empty();
}

// The controller's limit, "500", "250" or "125", or empty for Real time.
inline std::string get(const std::string &serial)
{
    const std::string key = config_store::normalise_serial(serial);
    if (key.empty()) return std::string();
    std::lock_guard<std::mutex> lock(g_mutex);
    load_locked();
    const auto it = g_limits.find(key);
    return it == g_limits.end() ? std::string() : it->second;
}

// Sets it, and writes the file. False for a value the page does not offer or a
// serial with no identity, and then nothing changes.
// ⓘ Best effort on disk, as the window's state file is: a folder that cannot
// be written to keeps the limit for as long as the listener runs.
inline bool set(const std::string &serial, const std::string &value)
{
    const std::string key = config_store::normalise_serial(serial);
    if (key.empty() || !is_choice(value)) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    load_locked();
    if (is_limit(value)) g_limits[key] = value;
    else g_limits.erase(key);
    if (!g_path.empty()) {
        std::ofstream f(g_path, std::ios::trunc);
        if (f) f << format(g_limits);
    }
    return true;
}

}  // namespace polling_limits

// device.inl's way in, declared in main.cpp ahead of it: the device is
// included long before config_store, whose serial rule this follows.
bool ctm_polling_limit_has_identity(const std::string &serial)
{
    return polling_limits::has_identity(serial);
}

std::string ctm_polling_limit_get(const std::string &serial)
{
    return polling_limits::get(serial);
}

bool ctm_polling_limit_set(const std::string &serial, const std::string &value)
{
    return polling_limits::set(serial, value);
}
