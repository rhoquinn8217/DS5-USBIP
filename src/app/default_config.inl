// default_windows_config made at an install's first start, and the first
// controller's auto link to it: what and why are in config/default_config_rule.inl.
// ⓘ agent.inl calls both: ensure() once the install has been checked, and
// first_controller_bridged() at every bridge, before the auto link is looked up.
#pragma once

namespace default_config {

// ⭐ MADE AT THE FIRST START AFTER AN INSTALL, fresh or an upgrade, when there
// is none: from its preset, through the writers New uses (rest_config.inl's
// create), so it reads like any config and can be edited, archived or made
// again. ⓘ One that is there, edited or not, is left as it is.
// The settings a copy has as loaded, key to value.
inline Pairs copy_settings()
{
    Pairs out;
    std::lock_guard<std::mutex> lock(g_device_config_mutex);
    if (!g_device_config_loaded) device_config_load_locked();
    const auto it = g_device_config.find(config_store::config_section(kName));
    if (it == g_device_config.end()) return out;
    for (const auto &entry : it->second) out.emplace_back(entry.first, entry.second);
    return out;
}

inline Pairs preset_pairs(const ctm_presets::Setting *settings, size_t count)
{
    Pairs out;
    for (size_t i = 0; i < count; ++i) out.emplace_back(settings[i].key, settings[i].value);
    return out;
}

inline void ensure()
{
    config_store::reload_all();
    const ctm_presets::Preset *preset = ctm_presets::find(kName);
    config_store::ConfigFile existing;
    if (config_store::find_config(kName, &existing)) {
        // ⭐ A COPY MADE BEFORE (copy_state): brought to the preset now if
        // nobody edited it, left as it is otherwise (rhoquinn8217, 2026-11-12:
        // "2. a"). ⓘ Written key by key through the writers New uses, so a
        // controller reading it now follows at once.
        const Copy state = preset == nullptr ? Copy::Edited
            : copy_state(copy_settings(), preset_pairs(preset->settings, preset->count),
                         preset_pairs(ctm_presets::kDefaultWindowsConfigV1,
                                      CTM_PRESET_COUNT_OF(ctm_presets::kDefaultWindowsConfigV1)));
        if (state == Copy::Unedited) {
            std::string error;
            for (size_t i = 0; i < preset->count; ++i) {
                if (!config_store::set_setting(kName, preset->settings[i].key, preset->settings[i].value, &error)) {
                    device_log::config(device_log::msg() << kName << ": bringing it up to date, "
                                                         << preset->settings[i].key << " not written -- " << error);
                    return;
                }
            }
            // ⓘ A key the preset no longer has goes back to unmanaged (an empty
            // value, as the page's checkbox does), so the copy reads as the
            // preset now and not as edited at the next start.
            for (const auto &key : dropped_keys(
                     preset_pairs(preset->settings, preset->count),
                     preset_pairs(ctm_presets::kDefaultWindowsConfigV1,
                                  CTM_PRESET_COUNT_OF(ctm_presets::kDefaultWindowsConfigV1)))) {
                if (!config_store::set_setting(kName, key, "", &error)) {
                    device_log::config(device_log::msg() << kName << ": bringing it up to date, "
                                                         << key << " not cleared -- " << error);
                    return;
                }
            }
            device_log::config(device_log::msg() << kName << ": nobody had edited it, brought to its preset now");
        } else if (state == Copy::Edited) {
            device_log::config(device_log::msg() << kName << ": edited, so left as it is (its preset has changed)");
        } else {
            device_log::config(device_log::msg() << kName << ": there already, as its preset is");
        }
        return;
    }
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

// ⭐ THE FIRST CONTROLLER AFTER A FRESH INSTALL: auto linked to
// default_windows_config, unless it auto links to a config already, by its own file's auto_link,
// so it holds at every bridge after; the auto link this bridge looks up next
// finds it. ⓘ The turn is taken atomically, so two controllers bridged
// together cannot both have it. ⓘ Returns the config to link THIS bridge to
// when no claim could be kept (no identity to keep one by):
// default_windows_config for this bridge, the turn over all the same; empty otherwise.
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
    if (auto_link_store::add(kName, serial, &error)) {
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
