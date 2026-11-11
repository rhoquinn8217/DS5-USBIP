// A controller's own settings, kept: what and why are in
// controller_settings_rule.inl.
//
// ⭐ THE SAME MAP THE CONFIGS ARE IN. A controller's file is loaded into
// g_device_config as a section of its own, "ctl:<serial>", beside the configs'
// "cfg:<name>", so device_config_str/int/bool read it as they read a config;
// a controller kept for one bridge has "ctl~<ordinal>", in memory alone.
// ⚠️ LOCK ORDER: g_mutex, then g_device_config_mutex, as config_store's.
// INCLUDE ORDER: after config/config_store.inl.
#pragma once

#include <fstream>
#include <sstream>

namespace controller_settings {

inline std::mutex g_mutex;
// Which bridged controller has a serial's file: serial -> ordinal. A second
// controller bridged with the same serial is kept for its bridge alone.
inline std::map<std::string, std::string> g_claims;

inline std::string file_for(const std::string &serial)
{
    return std::string(kDir) + "\\" + serial + ".txt";
}

inline std::map<std::string, std::string> read_file(const std::string &serial)
{
    std::ifstream in(file_for(serial), std::ios::binary);
    if (!in) return {};
    std::stringstream buffer;
    buffer << in.rdbuf();
    return parse(buffer.str());
}

// The section a bridged controller reads its own settings from, claimed for
// it: its serial's file, loaded, or one for this bridge alone when it has no
// usable serial or another controller bridged now holds that serial's.
// `serial` as config_store::normalise_serial writes it. ⓘ `why` says which.
inline std::string claim(const std::string &serial, const std::string &ordinal, std::string *why)
{
    bool another = false;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (!serial.empty()) {
            const auto it = g_claims.find(serial);
            another = it != g_claims.end() && it->second != ordinal;
            if (!another) g_claims[serial] = ordinal;
        }
    }
    if (keep_for(serial, another) == Keep::ForThisBridge) {
        if (why) *why = serial.empty() ? "no usable serial" : "another controller bridged now has its serial";
        const std::string section = bridge_section(ordinal);
        std::lock_guard<std::mutex> configLock(g_device_config_mutex);
        g_device_config[section].clear();
        device_config_changed_locked();
        return section;
    }
    // ⓘ Read with no shared lock held, then swapped in (config_store's rule:
    // the files sit on OneDrive, where a read can wait).
    std::map<std::string, std::string> settings = read_file(serial);
    const std::string section = kept_section(serial);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        std::lock_guard<std::mutex> configLock(g_device_config_mutex);
        g_device_config[section] = std::move(settings);
        device_config_changed_locked();
    }
    if (why) *why = file_for(serial);
    return section;
}

// The bridge is over: a serial's claim goes, and a section kept for the bridge
// alone with it. ⓘ A kept section stays loaded: nothing else reads it, and the
// next bridge of that controller loads its file again.
inline void release(const std::string &serial, const std::string &ordinal)
{
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        const auto it = g_claims.find(serial);
        if (!serial.empty() && it != g_claims.end() && it->second == ordinal) g_claims.erase(it);
    }
    std::lock_guard<std::mutex> configLock(g_device_config_mutex);
    if (g_device_config.erase(bridge_section(ordinal)) > 0) device_config_changed_locked();
}

// What a section holds, as written: a setting not there is at its default.
inline std::map<std::string, std::string> get(const std::string &section)
{
    std::lock_guard<std::mutex> configLock(g_device_config_mutex);
    const auto it = g_device_config.find(section);
    return it == g_device_config.end() ? std::map<std::string, std::string>() : it->second;
}

// Sets one setting, or puts it back to its default with an empty value, and
// writes the controller's file when it keeps one. False, changing nothing, for
// a key that is not a controller's or a value no file can hold.
inline bool set(const std::string &section, const std::string &key, const std::string &value,
                std::string *error)
{
    if (!is_kept_section(section) && !is_bridge_section(section)) {
        *error = "no controller settings for " + section;
        return false;
    }
    if (!is_key(key)) {
        *error = key + " is not a controller's setting";
        return false;
    }
    if (!value.empty() && !config_store::valid_setting_value(value)) {
        *error = "value may not contain [ ] = # ; or a newline (max 256)";
        return false;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    std::map<std::string, std::string> settings;
    {
        std::lock_guard<std::mutex> configLock(g_device_config_mutex);
        settings = g_device_config[section];
    }
    if (value.empty()) settings.erase(key);
    else settings[key] = value;
    if (is_kept_section(section)) {
        const std::string serial = section.substr(4);
        if (!config_store::ensure_dir(kDir)) {
            *error = std::string("could not create ") + kDir;
            return false;
        }
        if (!config_store::write_text_whole(file_for(serial), format(serial, settings), error)) return false;
    }
    std::lock_guard<std::mutex> configLock(g_device_config_mutex);
    g_device_config[section] = std::move(settings);
    device_config_changed_locked();
    return true;
}

// ⭐ BY SERIAL, BRIDGED OR NOT (auto_link_store.inl): a controller's settings
// as kept, its loaded section when it has one, else its file.
inline std::map<std::string, std::string> read_for(const std::string &serial)
{
    if (serial.empty()) return {};
    {
        std::lock_guard<std::mutex> configLock(g_device_config_mutex);
        const auto it = g_device_config.find(kept_section(serial));
        if (it != g_device_config.end()) return it->second;
    }
    return read_file(serial);
}

// Sets one setting of a controller by its serial, bridged or not, as set()
// does for a claimed one: from its loaded section when it has one, else from
// its file, and the file written. ⛔ Never set() for a controller not loaded:
// its section would start empty, and the file written would lose the rest.
inline bool set_by_serial(const std::string &serial, const std::string &key, const std::string &value,
                          std::string *error)
{
    if (serial.empty()) { *error = "no usable serial"; return false; }
    if (!is_key(key)) { *error = key + " is not a controller's setting"; return false; }
    if (!value.empty() && !config_store::valid_setting_value(value)) {
        *error = "value may not contain [ ] = # ; or a newline (max 256)";
        return false;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    const std::string section = kept_section(serial);
    bool loaded = false;
    std::map<std::string, std::string> settings;
    {
        std::lock_guard<std::mutex> configLock(g_device_config_mutex);
        const auto it = g_device_config.find(section);
        if (it != g_device_config.end()) { settings = it->second; loaded = true; }
    }
    if (!loaded) settings = read_file(serial);
    if (value.empty()) settings.erase(key);
    else settings[key] = value;
    if (!config_store::ensure_dir(kDir)) {
        *error = std::string("could not create ") + kDir;
        return false;
    }
    if (!config_store::write_text_whole(file_for(serial), format(serial, settings), error)) return false;
    if (loaded) {
        std::lock_guard<std::mutex> configLock(g_device_config_mutex);
        g_device_config[section] = std::move(settings);
        device_config_changed_locked();
    }
    return true;
}

// The serials with a file in controllers/, as their file names write them.
inline std::vector<std::string> serials_with_files()
{
    std::vector<std::string> out;
    const std::string dir = kDir;
    const std::wstring pattern = std::wstring(dir.begin(), dir.end()) + L"\\*.txt";
    WIN32_FIND_DATAW find = {};
    HANDLE handle = FindFirstFileW(pattern.c_str(), &find);
    if (handle == INVALID_HANDLE_VALUE) return out;
    do {
        if (find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        const std::wstring wname(find.cFileName);
        const std::string fname(wname.begin(), wname.end());
        if (fname.size() < 5) continue;
        out.push_back(fname.substr(0, fname.size() - 4));
    } while (FindNextFileW(handle, &find));
    FindClose(handle);
    return out;
}

} // namespace controller_settings

// device.inl's way in, declared in main.cpp ahead of it: whether a section is
// kept for one bridge alone, which its polling limit then is too. ⓘ Inline:
// the tests include this file in many of their files.
inline bool ctm_controller_settings_for_bridge(const std::string &section)
{
    return controller_settings::is_bridge_section(section);
}
