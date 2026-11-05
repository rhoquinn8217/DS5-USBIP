// The rumble dials on a DS4's motors, applied to the report Windows sends it
// (2026-11-04). The bytes and the arithmetic are in ds4_rumble.inl; this reads
// the pad's own section, as ds5_apply_output_overrides does for a DualSense.
// ⓘ After ds5_output_overrides.inl: it uses device_section_for from there.

static std::atomic<uint64_t> g_ds4_rumble_scale_count{0};

static void ds4_apply_output_overrides(uint8_t *data, size_t length,
                                       const std::vector<unsigned char> &descriptor,
                                       const std::string &linkedConfig = std::string())
{
    if (data == nullptr || length <= kDs4IdxMotorHeavy || data[0] != kDs4OutReportId) {
        return;
    }
    const char *kind = device_section_for(descriptor);
    if (kind == nullptr || std::strcmp(kind, "ds4") != 0) {
        return;
    }
    const std::string resolved = device_settings_section(kind, linkedConfig);
    const char *section = resolved.c_str();
    const uint8_t softWas = data[kDs4IdxMotorSoft];
    const uint8_t heavyWas = data[kDs4IdxMotorHeavy];
    if (!ds4_scale_motors(data, length,
                          device_config_int(section, "master_rumble_gain", -1),
                          device_config_int(section, "rumble_gain_heavy", -1),
                          device_config_int(section, "rumble_gain_soft", -1),
                          device_config_int(section, "rumble_floor", -1))) {
        return;
    }
    // Rumble reports stream during play, so this is logged sparsely on purpose.
    const uint64_t count = ++g_ds4_rumble_scale_count;
    if (count <= 3 || (count % 1000) == 0) {
        device_log::report(device_log::msg()
            << section << ": DS4 rumble scaled (heavy " << static_cast<int>(heavyWas) << "->"
            << static_cast<int>(data[kDs4IdxMotorHeavy]) << ", soft " << static_cast<int>(softWas)
            << "->" << static_cast<int>(data[kDs4IdxMotorSoft]) << ", scale #" << count << ")");
    }
}
