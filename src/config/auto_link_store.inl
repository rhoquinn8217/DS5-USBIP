// A controller's AUTO LINK: the config it reads at every bridge, kept in the
// controller's own file (controller_settings.inl) as `auto_link = <config>`.
//
// ⭐ rhoquinn8217, 2026-11-10: *"Thoughts on how auto link should be stored. It
// might make better sense to store it controller settings instead configs."*,
// and to the plan, *"yes after device tag merges"*. So one answer per
// controller, which two configs can never both claim; configs hold no serial
// or MAC, and can be copied or shared as they are; and one identity, the one
// the controller's file is named by (a DualSense's MAC, else a non-zero
// serial, config_store::normalise_serial).
//
// ⓘ config_store::reload_all reads every controller's file into its config's
// autoLink list, so auto_link_for, the configs window's lists and its Auto
// link button work as they did. An older config's own `auto_link =` line still
// counts for a serial no controller's file names, and is moved into that
// controller's file at the next bridge (move_old_claims).
// INCLUDE ORDER: after config/controller_settings.inl.
#pragma once

namespace auto_link_store {

// ⭐ AN OLDER CONFIG'S OWN LINE, MOVED (the plan, 2026-11-10: "a one-time move
// of every claim"): each serial it names goes into that controller's file,
// unless that file names a config already, and the line is blanked once every
// one of them is kept. ⓘ At every bridge's start and every change the watcher
// sees, so a config copied in from an older install is moved too; with no
// older line anywhere it reads nothing from disk.
inline void move_old_claims()
{
    std::vector<config_store::ConfigFile> older;
    for (const config_store::ConfigFile &cfg : config_store::list_configs()) {
        if (!cfg.oldClaims.empty()) older.push_back(cfg);
    }
    if (older.empty()) return;
    for (const config_store::ConfigFile &cfg : older) {
        bool allKept = true;
        for (const std::string &serial : cfg.oldClaims) {
            const std::map<std::string, std::string> own = controller_settings::read_for(serial);
            const auto it = own.find("auto_link");
            if (it != own.end()) {
                device_log::config(device_log::msg() << cfg.name << "'s auto_link line named " << serial
                                                     << ", which auto links to " << it->second
                                                     << " by its own file: that is kept");
                continue;
            }
            std::string error;
            if (controller_settings::set_by_serial(serial, "auto_link", cfg.name, &error)) {
                device_log::config(device_log::msg() << serial << " auto links to " << cfg.name
                                                     << ", moved from the config's own line into its file");
            } else {
                allKept = false;
                device_log::config(device_log::msg() << serial << ": " << cfg.name
                                                     << "'s auto link not moved -- " << error);
            }
        }
        std::string error;
        if (allKept && !config_store::set_auto_link_line(cfg, "", &error)) {
            device_log::config(device_log::msg() << cfg.name << "'s auto_link line not blanked -- " << error);
        }
    }
    config_store::reload_all();
    config_store::notify_changed();
}

// The controller reads this config at every bridge, whatever it auto linked
// to before. ⓘ No refusal for a config that had it: with one answer per
// controller there is no second claim to refuse.
inline bool add(const std::string &config, const std::string &serial, std::string *error)
{
    move_old_claims();
    config_store::ConfigFile cfg;
    if (!config_store::find_config(config, &cfg)) { *error = "no config named " + config; return false; }
    const std::string s = config_store::normalise_serial(serial);
    if (s.empty()) { *error = "no usable serial"; return false; }
    if (!controller_settings::set_by_serial(s, "auto_link", cfg.name, error)) return false;
    device_log::config(device_log::msg() << s << " auto links to " << cfg.name);
    config_store::reload_all();
    config_store::notify_changed();
    return true;
}

// The controller stops auto linking to this config. ⓘ One that auto links to
// another config is left as it is.
inline bool remove(const std::string &config, const std::string &serial, std::string *error)
{
    move_old_claims();
    const std::string s = config_store::normalise_serial(serial);
    if (s.empty()) { *error = "no usable serial"; return false; }
    const std::map<std::string, std::string> own = controller_settings::read_for(s);
    const auto it = own.find("auto_link");
    if (it == own.end() || config_store::lower(it->second) != config_store::lower(config)) return true;
    if (!controller_settings::set_by_serial(s, "auto_link", "", error)) return false;
    device_log::config(device_log::msg() << s << " no longer auto links to " << config);
    config_store::reload_all();
    config_store::notify_changed();
    return true;
}

// ⭐ A RENAMED CONFIG KEEPS ITS CONTROLLERS (the plan: "rename re-points"):
// every controller auto linking to the old name names the new one.
inline void follow_rename(const std::string &oldName, const std::string &newName)
{
    for (const std::string &serial : controller_settings::serials_with_files()) {
        const std::map<std::string, std::string> own = controller_settings::read_for(serial);
        const auto it = own.find("auto_link");
        if (it == own.end() || config_store::lower(it->second) != config_store::lower(oldName)) continue;
        std::string error;
        if (controller_settings::set_by_serial(serial, "auto_link", newName, &error)) {
            device_log::config(device_log::msg() << serial << " auto links to " << newName
                                                 << ", renamed from " << oldName);
        } else {
            device_log::config(device_log::msg() << serial << ": its auto link to " << oldName
                                                 << " not renamed -- " << error);
        }
    }
    config_store::reload_all();
    config_store::notify_changed();
}

} // namespace auto_link_store
