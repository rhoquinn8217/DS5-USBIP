// default-config made at an install's first start, and the first controller's
// auto link to it: what and why are in config/default_config_rule.inl.
// ⓘ agent.inl calls both: ensure() once the install has been checked, and
// first_controller_bridged() at every bridge, before the auto link is looked up.
#pragma once

namespace default_config {

// ⭐ MADE AT THE FIRST START AFTER AN INSTALL, fresh or an upgrade, when there
// is none: from its preset, through the writers New uses (rest_config.inl's
// create), so it reads like any config and can be edited, archived or made
// again. ⓘ One that is there, edited or not, is left as it is.
inline void ensure()
{
    config_store::reload_all();
    config_store::ConfigFile existing;
    if (config_store::find_config(kName, &existing)) {
        device_log::config(device_log::msg() << kName << ": there already, left as it is");
        return;
    }
    const ctm_presets::Preset *preset = ctm_presets::find(kName);
    if (preset == nullptr) return;
    std::string error;
    if (!config_store::create_config(kName, &error)) {
        device_log::config(device_log::msg() << kName << ": not made -- " << error);
        return;
    }
    for (size_t i = 0; i < preset->count; ++i) {
        if (!config_store::set_setting(kName, preset->settings[i].key, preset->settings[i].value, &error)) {
            // ⚠️ The config exists at this point, as New leaves it: said, not undone.
            device_log::config(device_log::msg() << kName << ": made, but " << preset->settings[i].key
                                                 << " not written -- " << error);
            return;
        }
    }
    device_log::config(device_log::msg() << kName << ": made from its preset, at the first start after an install");
}

// ⭐ THE FIRST CONTROLLER AFTER A FRESH INSTALL: auto linked to default-config,
// unless a config auto links it already, by the config's own auto_link line,
// so it holds at every bridge after; the auto link this bridge looks up next
// finds it. ⓘ The turn is taken atomically, so two controllers bridged
// together cannot both have it. ⓘ Returns the config to link THIS bridge to
// when no claim could be kept (no identity to keep one by): default-config for
// this bridge, the turn over all the same; empty otherwise.
inline std::string first_controller_bridged(const std::string &ordinal, const std::string &serial,
                                            const std::string &kind)
{
    const bool takes = config_store::kind_supports_config(kind);
    const std::string already = takes ? config_store::auto_link_for(serial, kind) : std::string();
    const First what = first_controller(config_move::first_controller_waiting(), takes, !already.empty());
    if (what == First::None) return std::string();
    if (!config_move::take_first_controller()) return std::string();   // another bridge took it just now
    if (what == First::Leave) {
        device_log::config(device_log::msg() << ordinal << ": the first controller after a fresh install, auto "
                                             << "linked to " << already << " already: left as it is");
        return std::string();
    }
    std::string error;
    if (config_store::add_auto_link(kName, serial, &error)) {
        device_log::config(device_log::msg() << ordinal << ": the first controller after a fresh install, auto "
                                             << "linked to " << kName);
        return std::string();
    }
    config_store::ConfigFile cfg;
    const bool there = config_store::find_config(kName, &cfg);
    device_log::config(device_log::msg() << ordinal << ": the first controller after a fresh install, but no "
                                         << "auto link kept (" << error << ")"
                                         << (there ? std::string(": ") + kName + " for this bridge" : std::string()));
    return there ? std::string(kName) : std::string();
}

} // namespace default_config
