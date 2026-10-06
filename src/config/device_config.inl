// -----------------------------------------------------------------------------
// Device configuration store.
//
// Per-device settings, read from a plain text file beside the agent. Windows
// owns these settings: the TV sends nothing and needs to know nothing about
// them, so an unmodified client is unaffected.
//
// The cache is dropped whenever a bridge session starts, and refilled on the
// next lookup. That is what makes editing the file and re-plugging the
// controller apply the change -- no reload command and no protocol are needed.
// Within a session the values are stable, so a setting cannot change under a
// running game.
//
// The format deliberately mirrors the .map files -- same [section] and
// key = value shape -- so there is no new syntax to learn:
//
//   [ds5]
//   force_echo_cancel = true
//
// Unknown sections and unknown keys are ignored, so a newer file still works
// with an older build. A missing file means every setting takes its built-in
// default, which is exactly how the agent behaved before this existed.
//
// NOTE: trailing comments ARE stripped here, with '#' or ';'. The .map parser
// does not strip them, and a trailing comment there silently turns a flag off.
// The two formats look alike but this one does not carry that trap.
//
// The path is relative to the agent's working directory, which is where the
// maps folder also resolves from. If this ever needs to work for the installed
// service as well, it should move to the same asset-finding helper the maps
// use.
// -----------------------------------------------------------------------------

static const char *const kDeviceConfigFileName = "ctm-device-config.txt";

static std::mutex g_device_config_mutex;
static bool g_device_config_loaded = false;
static std::map<std::string, std::map<std::string, std::string>> g_device_config;

static std::string device_config_trim(const std::string &text)
{
    const size_t begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return std::string();
    }
    const size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

static std::string device_config_lower(std::string text)
{
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] >= 'A' && text[i] <= 'Z') {
            text[i] = static_cast<char>(text[i] - 'A' + 'a');
        }
    }
    return text;
}

using DeviceConfigSections = std::map<std::string, std::map<std::string, std::string>>;

// Reads the shared file's sections and keys into *out, and returns how many
// settings it held, or -1 when there is no file.
// ⓘ Touches nothing shared, so it runs with no lock held; see
// device_config_reload_shared().
static int device_config_read_file(DeviceConfigSections *out)
{
    std::ifstream file(kDeviceConfigFileName);
    if (!file.is_open()) {
        return -1;
    }

    std::string section;
    std::string line;
    int entries = 0;
    while (std::getline(file, line)) {
        const size_t comment = line.find_first_of("#;");
        if (comment != std::string::npos) {
            line.erase(comment);
        }
        line = device_config_trim(line);
        if (line.empty()) {
            continue;
        }
        if (line.front() == '[' && line.back() == ']') {
            section = device_config_lower(device_config_trim(line.substr(1, line.size() - 2)));
            continue;
        }
        const size_t equals = line.find('=');
        if (equals == std::string::npos || section.empty()) {
            continue;
        }
        const std::string key = device_config_lower(device_config_trim(line.substr(0, equals)));
        if (key.empty()) {
            continue;
        }
        (*out)[section][key] = device_config_trim(line.substr(equals + 1));
        ++entries;
    }
    return entries;
}

// ⭐ Only when it changed. The settings page polls every ten seconds and each
// poll reloads, so an unconditional line here said nothing and buried
// everything that mattered. ⓘ "No file" too, since 2026-10-06: it was said on
// every poll. Caller must hold g_device_config_mutex.
static void device_config_say_locked(int entries)
{
    static int lastEntries = -2;
    if (entries == lastEntries) {
        return;
    }
    lastEntries = entries;
    if (entries < 0) {
        device_log::config(device_log::msg()
            << "no " << kDeviceConfigFileName << " found, using built-in defaults");
    } else {
        device_log::config(device_log::msg()
            << "loaded " << entries << " setting(s) from " << kDeviceConfigFileName);
    }
}

// Caller must hold g_device_config_mutex. ⓘ Inserts and never erases: a
// caller that rereads erases this file's sections first.
static void device_config_load_locked()
{
    g_device_config_loaded = true;
    DeviceConfigSections read;
    const int entries = device_config_read_file(&read);
    for (const auto &section : read) {
        for (const auto &setting : section.second) {
            g_device_config[section.first][setting.first] = setting.second;
        }
    }
    device_config_say_locked(entries);
}

// ⓘ Held across one whole reload, the reading and the swap, so two reloads
// cannot cross and leave an older read in place of a newer one. Nothing on the
// input path takes it.
static std::mutex g_device_config_reload_mutex;

// ⭐⭐ READ FIRST, LOCK AFTER (code review, 2026-10-05). The shared file was
// read from disk under g_device_config_mutex, the lock every input hook takes
// on every report, and it sits on OneDrive, where a read can wait: every
// bridge start dropped the cache and the next lookup, any pad's, read the file
// under that lock. It is read with no lock now, and the lock is held only to
// swap the sections this file owns.
static void device_config_reload_shared()
{
    std::lock_guard<std::mutex> reloading(g_device_config_reload_mutex);
    DeviceConfigSections read;
    const int entries = device_config_read_file(&read);
    std::lock_guard<std::mutex> guard(g_device_config_mutex);
    // ⚠️ Only the sections THIS file owns: see device_config_invalidate().
    for (auto it = g_device_config.begin(); it != g_device_config.end(); ) {
        if (it->first.rfind("cfg:", 0) == 0) ++it;
        else it = g_device_config.erase(it);
    }
    for (auto &section : read) {
        g_device_config[section.first] = std::move(section.second);
    }
    g_device_config_loaded = true;
    device_config_say_locked(entries);
}

// Re-read the file now. Called when a bridge session starts: that is what
// turns a virtual reseat into "apply my edited settings", which is the model
// this file exists to serve.
// ⓘ It dropped the cached copy and left the next lookup to re-read it, which
// was a pad's lookup, under the input lock; device_config_reload_shared() says
// why it reads now instead.
// ⚠️ Erase only the sections THIS file owns. Per-controller config files
// load their settings into "cfg:<name>/<kind>" sections in the same map,
// and this function does not reload those -- a blanket clear() wiped them
// on every bridge and every save of the shared file, after which every
// linked device silently fell back to the shared section. Intermittent,
// and invisible until someone noticed their config had stopped applying.
static void device_config_invalidate()
{
    device_config_reload_shared();
}

// Look up a text setting. Returns an empty string when the file, section, key
// or value is missing, so callers can treat empty as "not configured".
static std::string device_config_str(const char *section, const char *key)
{
    if (section == nullptr || key == nullptr) {
        return std::string();
    }
    std::lock_guard<std::mutex> guard(g_device_config_mutex);
    if (!g_device_config_loaded) {
        device_config_load_locked();
    }
    const auto sectionIt = g_device_config.find(device_config_lower(section));
    if (sectionIt == g_device_config.end()) {
        return std::string();
    }
    const auto keyIt = sectionIt->second.find(device_config_lower(key));
    if (keyIt == sectionIt->second.end()) {
        return std::string();
    }
    return device_config_lower(keyIt->second);
}

// Look up a whole-number setting. Returns fallback when the file, section, key
// or a parsable value is missing. Callers use a fallback outside the valid
// range (e.g. -1) to mean "not configured, do nothing".
static int device_config_int(const char *section, const char *key, int fallback)
{
    if (section == nullptr || key == nullptr) {
        return fallback;
    }
    std::lock_guard<std::mutex> guard(g_device_config_mutex);
    if (!g_device_config_loaded) {
        device_config_load_locked();
    }
    const auto sectionIt = g_device_config.find(device_config_lower(section));
    if (sectionIt == g_device_config.end()) {
        return fallback;
    }
    const auto keyIt = sectionIt->second.find(device_config_lower(key));
    if (keyIt == sectionIt->second.end()) {
        return fallback;
    }
    // The WHOLE value must be a number. Checking only that parsing got
    // started is not enough: std::stoi reads as far as it can, so the typo
    // "5O" (five, letter O) parses as 5 and looks like a deliberate setting.
    // Caught by device_config_test.cpp on its first run, 2026-08-01.
    const std::string &text = keyIt->second;
    try {
        size_t consumed = 0;
        const int parsed = std::stoi(text, &consumed);
        if (consumed != text.size()) {
            return fallback;   // trailing junk means a typo, not a value
        }
        return parsed;
    } catch (...) {
        return fallback;   // unparsable is "not configured", never an error
    }
}

// Look up a boolean setting. Returns fallback when the file, the section, the
// key, or a recognisable value is missing -- every failure path is "behave as
// before", never an error.
static bool device_config_bool(const char *section, const char *key, bool fallback)
{
    if (section == nullptr || key == nullptr) {
        return fallback;
    }
    std::lock_guard<std::mutex> guard(g_device_config_mutex);
    if (!g_device_config_loaded) {
        device_config_load_locked();
    }
    const auto sectionIt = g_device_config.find(device_config_lower(section));
    if (sectionIt == g_device_config.end()) {
        return fallback;
    }
    const auto keyIt = sectionIt->second.find(device_config_lower(key));
    if (keyIt == sectionIt->second.end()) {
        return fallback;
    }
    const std::string value = device_config_lower(keyIt->second);
    if (value == "true" || value == "1" || value == "yes" || value == "on") {
        return true;
    }
    if (value == "false" || value == "0" || value == "no" || value == "off") {
        return false;
    }
    return fallback;
}
