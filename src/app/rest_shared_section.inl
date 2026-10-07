// The shared config's sections: which one a request means, and who reads each.
//
// ⛔⛔ WHY THIS EXISTS (code review, 2026-10-05). The REST API served the shared
// file as its [ds5] section alone and listed EVERY unlinked device as reading
// it. An unlinked Edge reads [ds5_edge], a DS4 [ds4] and an Xbox pad [xbox],
// and a keyboard or mouse reads no section at all, so the answer to "what is in
// effect for this device" was wrong for every one of those.
//
// ⓘ Split out of rest_config.inl so the test binary can reach these rules:
// that file needs the agent's session list, these need only config_store.

namespace rest_shared {

// The shared file's sections a device can read, by its settings kind.
static const char *const kSections[] = {"ds5", "ds5_edge", "ds4", "xbox"};

// The section a request's `kind=` names, or "" for one that names none. Takes
// a session kind ("ds4_usb") or a section name ("ds5_edge").
inline std::string section_for(const std::string &kind)
{
    const std::string viaSession = config_store::settings_kind_for(kind);
    if (!viaSession.empty()) return viaSession;
    for (const char *section : kSections) {
        if (kind == section) return kind;
    }
    return std::string();
}

// Whether a device reads this section of the shared file: it has no config of
// its own linked, AND its kind reads this section.
// ⛔ The old rule was the first half alone.
inline bool reads_section(const std::string &linkedConfig, const std::string &deviceKind,
                          const std::string &section)
{
    return linkedConfig.empty() && config_store::settings_kind_for(deviceKind) == section;
}

// One query parameter's value, or "" when it is absent. The values this API
// takes are plain words, so nothing is percent-decoded.
inline std::string query_value(const std::string &query, const std::string &name)
{
    size_t start = 0;
    while (start <= query.size()) {
        size_t end = query.find('&', start);
        if (end == std::string::npos) end = query.size();
        const std::string pair = query.substr(start, end - start);
        if (pair.compare(0, name.size() + 1, name + "=") == 0) {
            return pair.substr(name.size() + 1);
        }
        start = end + 1;
    }
    return std::string();
}

}  // namespace rest_shared
