// -----------------------------------------------------------------------------
// DualSense output-report overrides -- the DEFEND half of the settings pair.
//
// Runs on every report leaving Windows for the controller and rewrites fields
// the configuration claims authority over. The SET half lives in
// ds5_apply_settings.inl; see its header for how the two divide.
//
// RULE: a field named in the config file is ours; a field absent from it is
// left entirely alone. Absence means "not our business", never "use a
// default", so every field the config does not mention behaves exactly as it
// did before this file existed.
//
// This file also owns the shared DualSense report-format constants, because it
// is included before the sender and both need them.
//
// --- Which config section belongs to this device ---
//
// Each device type gets its own section and NOTHING is inherited. A DualSense
// Edge does not fall back to [ds5]: it is a different product, an owner will
// look for its own heading, and a config that inherits is a config you cannot
// read -- every value becomes "probably this, unless", and you have to hold two
// sections in your head to know what one controller is doing.
//
// The cost is duplicating a few lines to make an Edge behave like a DualSense.
// That is a small, obvious, one-time chore; the alternative is permanent
// uncertainty every time anyone reads the file.
//
// !! A DEVICE WITH NO SECTION IS LEFT COMPLETELY UNTOUCHED. Same rule the
// !! config already runs on -- absent means not our business -- one level up.
//
// !! IDS ARE ONLY LISTED ONCE THEY CAN BE TESTED. DS4 and Xbox are absent on
// !! purpose: their product ids would be filled in from memory, and a wrong id
// !! silently matches nothing, which is an evening wasted. Add a line when
// !! there is a controller in hand to check it against.
static const uint16_t kVendorSony = 0x054c;

// Reads the vendor and product id straight out of the USB device descriptor
// (bytes 8-11, little-endian). Taken by reference on purpose: this runs on
// every outbound report, and asking the backend for its caps would copy
// several strings and a vector each time.
// The settings section a device reads.
//
// ⭐ THIS IS WHERE A LINKED CONFIG TAKES EFFECT. Without a link the answer is
// the shared section for the device type, exactly as before. With one, it is
// that config file's section -- the same shape, so every accessor works
// unchanged.
//
// ⭐⭐ AND THE CONFIG'S SECTION CARRIES NO KIND (2026-09-12). An Xbox pad and a
// DualSense linked to one config read the same section; each uses what it can.
// ⚠️ The kind still has to be non-null: a device with no kind at all is one
// that takes no config, and gets no settings either way.
//
// ⓘ Built inline rather than calling config_store::section_for, because this
// file is included before config_store.inl. Keep the two in step: the format
// is "cfg:<lowered name>".
static std::string device_settings_section(const char *kind, const std::string &linkedConfig)
{
    if (kind == nullptr) return std::string();
    if (linkedConfig.empty()) return std::string(kind);
    std::string name;
    for (char c : linkedConfig) {
        name.push_back((c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c);
    }
    return "cfg:" + name;
}

// ⭐ WHERE A CONTROLLER'S AUDIO AND RUMBLE ARE READ: its own settings
// section (config/controller_settings.inl), whatever config is linked; its
// kind's shared section when it has none, as a test has, or a pad before the
// agent has claimed it one.
static std::string controller_settings_section(const char *kind, const std::string &controllerSection)
{
    if (!controllerSection.empty()) return controllerSection;
    return kind == nullptr ? std::string() : std::string(kind);
}

static const char *device_section_for(const std::vector<unsigned char> &descriptor)
{
    if (descriptor.size() < 12) {
        return nullptr;
    }
    const uint16_t vendor = static_cast<uint16_t>(
        descriptor[8] | (static_cast<uint16_t>(descriptor[9]) << 8));
    const uint16_t product = static_cast<uint16_t>(
        descriptor[10] | (static_cast<uint16_t>(descriptor[11]) << 8));

    if (vendor != kVendorSony) {
        return nullptr;
    }
    switch (product) {
        case 0x0ce6: return "ds5";
        case 0x0df2: return "ds5_edge";
        case 0x09cc: return "ds4";        // DualShock 4, second revision
        case 0x05c4: return "ds4";        // DualShock 4, first revision
        default:     return nullptr;
    }
}

// ⛔ SEPARATE FROM device_section_for, and the separation matters.
//
// That function's null return was carrying TWO meanings: "no config for this
// device" and "not a DualSense". The audio paths relied on the second -- they
// take any non-null kind as licence to build a DUALSENSE OUTPUT REPORT from it.
//
// ⚠️ So teaching device_section_for about other controllers, which the config
// system needs, would have started writing DS5 audio bytes into an Xbox report.
// Nothing would have failed loudly; the controller would just behave oddly.
//
// ➡️ Each caller now says which question it is asking. This one is "does this
// device speak the DS5 output report format", and only the two DualSense
// product ids do.
// ⓘ Motion used to be asked here too, by a device_has_ds5_motion() that answered
// the same as this, with a note that a DS4 motion path would change it. ✅ That
// path was written (2026-09-15), and the question moved rather than changed:
// whether a pad has a motion sensor, and where, is now its layout's to say
// (ctm_rebind::Layout::motion, via device_input_pad_for below). This one is still
// only about the OUTPUT report, and still only the two DualSense ids.
static bool device_has_ds5_audio(const std::vector<unsigned char> &descriptor)
{
    if (descriptor.size() < 12) {
        return false;
    }
    const uint16_t vendor = static_cast<uint16_t>(
        descriptor[8] | (static_cast<uint16_t>(descriptor[9]) << 8));
    const uint16_t product = static_cast<uint16_t>(
        descriptor[10] | (static_cast<uint16_t>(descriptor[11]) << 8));

    // ⓘ The DS4 has a speaker and a headphone jack, and its map already encodes
    // SBC for it -- but its OUTPUT REPORT is a different layout entirely, so the
    // DS5 byte positions do not apply. It needs its own apply path before it can
    // be listed here.
    return vendor == kVendorSony && (product == 0x0ce6 || product == 0x0df2);
}

// ⓘ "Does this device's INPUT report use DualSense byte positions?" was a third
// question here, device_has_ds5_input_layout(), answering the same as the audio
// one with a note that a DS4 input path would change it. Measured 2026-09-11: a
// DS4 puts its buttons at bytes 5/6/7 where a DualSense puts them at 8/9/10, and
// an Xbox GIP report at 4/5 behind a 4-byte header. ✅ Its last caller, the
// on-screen keyboard's guard, went on 2026-09-15: every pad's input is read
// through its own layout now, which device_input_pad_for() below hands out.

// Which settings section does this pad's BUTTONS read from?
//
// ⛔ NOT device_section_for(). That one answers "does this speak the DS5 output
// report", and its null return is what kept every non-Sony pad out of the
// rebinder entirely -- an Xbox pad could be given a config and it would do
// nothing, which is the silent-success failure this file warns about.
//
// ⓘ The vocabulary matches config_store::settings_kind_for() on purpose: a name
// minted here that the config store does not know would create a section nothing
// can ever link to.
//
// ⚠️ "puck" is deliberately absent. Composite devices are forwarded verbatim and
// never reach a path a config acts on -- structural, not a gap to fill.
static const char *device_button_section_for(const std::vector<unsigned char> &descriptor)
{
    if (descriptor.size() < 12) {
        return nullptr;
    }
    const uint16_t vendor = static_cast<uint16_t>(
        descriptor[8] | (static_cast<uint16_t>(descriptor[9]) << 8));
    const uint16_t product = static_cast<uint16_t>(
        descriptor[10] | (static_cast<uint16_t>(descriptor[11]) << 8));

    if (vendor == kVendorSony) {
        if (product == 0x0ce6) return "ds5";
        if (product == 0x0df2) return "ds5_edge";
        if (product == 0x09cc || product == 0x05c4) return "ds4";
        return nullptr;
    }
    // Microsoft, the captured GIP pad the xbox profile describes.
    if (vendor == 0x045e && product == 0x0b12) return "xbox";
    return nullptr;
}

#include "input/button_layout.inl"
// ⓘ Included here rather than left to main.cpp's order: a test that pulls
// this file in gets the floor with it, so the shipped clamp is the tested one.
#include "audio/rumble_floor.inl"

// ⭐⭐ WHICH SETTINGS A PAD'S INPUT HOOKS USE, AND WHICH BYTES THEY READ, AS ONE
// ANSWER. The gyro, touchpad, stick and trigger mouse, the settings-window chord
// and the mouse-exclusive blanking all ask this, and only this.
//
// ⛔ They used to ask device_section_for(), which answers a different question --
// does this pad speak the DualSense OUTPUT report -- and says yes to a DS4, whose
// sensors and touchpad sit at other offsets. So a DS4 was not left alone by those
// hooks; it was READ WRONG: its touch-packet count taken for a finger, its hat
// and face buttons for trigger travel, a timestamp for the PS button.
//
// ⓘ Both halves come from the same function the rebinder asks, so the section a
// preset writes and the section a hook reads can never disagree. A pad no table
// describes gets nullptr for both.
struct InputPad {
    const char *kind = nullptr;
    const ctm_rebind::Layout *layout = nullptr;
};

static InputPad device_input_pad_for(const std::vector<unsigned char> &descriptor)
{
    InputPad pad;
    pad.layout = ctm_rebind::layout_for(device_button_section_for(descriptor));
    if (pad.layout != nullptr) pad.kind = device_button_section_for(descriptor);
    return pad;
}

// DualSense USB output report layout. Positions and claim bits are ours, from
// on-wire capture, independently cross-checked against daidr/dualsense-tester
// (MIT) -- every one agreed. NO CODE WAS COPIED.
//
// A CLAIMED field is applied even when it is zero, so claiming something we do
// not intend to set is an active change, not a no-op.
static const uint8_t kDs5OutReportId        = 0x02;
static const size_t  kDs5IdxValidFlag0      = 1;
static const size_t  kDs5IdxHeadsetVolume   = 5;
static const size_t  kDs5IdxSpeakerVolume   = 6;
static const size_t  kDs5IdxAudioControl    = 8;
static const uint8_t kDs5AllowHeadsetVolume = 0x10;
static const uint8_t kDs5AllowSpeakerVolume = 0x20;
static const uint8_t kDs5AllowAudioControl  = 0x80;
static const uint8_t kDs5EchoNoiseCancel    = 0x0c;  // echo + noise cancellation ON
static const uint8_t kDs5RouteToSpeaker     = 0x30;
static const uint8_t kDs5RouteMask          = 0x30;
static const uint8_t kDs5SpeakerVolumeMax   = 0x64;  // speaker full scale
static const uint8_t kDs5HeadsetVolumeMax   = 0x7f;  // headset full scale -- NOT the same

// Microphone mute. ⭐ THE PERMISSION LIVES IN A DIFFERENT FLAG BYTE from every
// setting above: byte 1 is flag panel 1 (volumes, audio control), byte 2 is
// flag panel 2 (mute LED, audio-mute controls). Claiming mute in panel 1 does
// nothing at all.
//
// Positions and bits corroborated 2026-08-04 against TWO independent sources
// that agree exactly: the Linux kernel's hid-playstation driver and
// nowrep/dualsensectl. Both set the same permission bit and the same mute bit.
// NO CODE WAS COPIED -- these are hardware register positions, read and
// re-expressed.
//
// ⚠️ The mute byte is SHARED with the speaker mute, the headphone mute, the
// haptic mute and four power-save switches. Writing the whole byte to mute a
// microphone would silence the speaker as a side effect. Only bit 4 is ever
// set here, and the rest of the byte is left at zero, which is the
// everything-on state.
static const size_t  kDs5IdxValidFlag1      = 2;     // flag panel 2
static const size_t  kDs5IdxMuteLed         = 9;     // mute button's light
static const size_t  kDs5IdxPowerSaveMute   = 10;    // mutes + power-save switches
static const uint8_t kDs5AllowMuteLed       = 0x01;  // panel 2, bit 0
static const uint8_t kDs5AllowPowerSaveMute = 0x02;  // panel 2, bit 1
static const uint8_t kDs5MicMute            = 0x10;  // byte 10, bit 4
static const uint8_t kDs5MuteLedOn          = 0x01;  // light solid on
static const uint8_t kDs5MuteLedOff         = 0x00;

// !! THE OUTPUTS HAVE A FLOOR, AND IT IS HIGH. The Linux hid-playstation patch
// !! series records the speaker's accepted range as 0x3d..0x64 -- so roughly
// !! the bottom 60% of a naive 0..max mapping is BELOW THE HARDWARE FLOOR and
// !! simply silent. Measured independently 2026-08-01: 35% was the edge of
// !! audibility and 40% was very faint, which is that floor showing through.
// !!
// !! So a percentage maps onto the USABLE range, not onto 0..max: 0 is silence,
// !! and 1..100 spans floor..full. Without this a slider wastes most of its
// !! travel on values that do nothing.
static const uint8_t kDs5SpeakerVolumeFloor = 0x3d;   // documented by the kernel

// !! The headset's floor has NOT been measured. This is the speaker's floor
// !! scaled to the headset's larger range -- a reasonable guess, nothing more.
// !! Replace it the moment someone measures where the headset stops being
// !! audible.
static const uint8_t kDs5HeadsetVolumeFloor = 0x4d;
static const size_t  kDs5OutReportLen       = 48;    // every host report on the wire
static const uint8_t kDs5ClaimRumbleA       = 0x01;
static const uint8_t kDs5ClaimRumbleB       = 0x02;

// Shared by both halves so a configured percentage always lands on the same
// raw value, whether it is being SET or DEFENDED.
static uint8_t ds5_volume_raw_from_percent(int percent, uint8_t fullScale, uint8_t floor)
{
    if (percent <= 0) {
        return 0;            // genuinely off, below the floor on purpose
    }
    if (percent > 100) percent = 100;
    const int span = static_cast<int>(fullScale) - static_cast<int>(floor);
    return static_cast<uint8_t>(floor + (percent * span) / 100);
}

// Which output the config asks for.
//
// !! THE AUDIO-CONTROL FIELD IS THREE SINKS, NOT A LIST OF DESTINATIONS.
// !! Bits 4-5 select one of four routings, and each decides what feeds the
// !! headset's two ears and the mono speaker. From the Linux hid-playstation
// !! patch series (Cristian Ciocaltea, Collabora, May 2025) and confirmed by
// !! ear on the wired path 2026-08-01:
//
//     value   headset L   headset R   speaker
//       0       Left        Right      muted     <- stereo headset
//       1       Left        Left       muted     <- mono headset
//       2       Left        Left       Right     <- mono headset + speaker
//       3       muted       muted      Right     <- speaker only
//
// !! SO THERE IS NO STEREO-HEADSET-PLUS-SPEAKER MODE. The speaker is mono and
// !! is fed the RIGHT channel, so anything using the speaker costs the headset
// !! its right channel. "both" is mono in the ears, unavoidably.
//
//   auto / absent -- touch nothing. Route and volumes stay as the game set them.
//   headset       -- stereo headset, speaker muted
//   headset_mono  -- headset with the left channel in both ears
//   speaker       -- speaker only, headset muted
//   both          -- speaker plus mono headset (⛔ NOT OFFERED, below)
//   off           -- everything muted
//
// ⛔ "both" IS NO LONGER OFFERED (rhoquinn8217, 2026-11-07: "lets remove the
// both option for now from both DualSense and DS4 configs"). On a cabled Edge
// it played the headphones alone while they were plugged in, and the speaker
// only once they were out, though both volumes were sent; so it never played
// both. Taken off the choices (rest_config.inl) until that is looked into. A
// file that still says "both" is routed as before.
//
// Names rather than numbers on purpose: this is a choice between named things,
// not a quantity. "audio_output = 0" needs a lookup table to read and fails
// silently when someone writes 9.
enum class Ds5AudioOutput { Auto, Headset, HeadsetMono, Speaker, Both, Off };

static Ds5AudioOutput ds5_audio_output_for(const char *section)
{
    const std::string value = device_config_str(section, "audio_output");
    if (value == "headset")      return Ds5AudioOutput::Headset;
    if (value == "headset_mono") return Ds5AudioOutput::HeadsetMono;
    if (value == "speaker")      return Ds5AudioOutput::Speaker;
    if (value == "both")         return Ds5AudioOutput::Both;
    if (value == "off")          return Ds5AudioOutput::Off;
    return Ds5AudioOutput::Auto;   // absent, or anything unrecognised
}

// The routing bits a mode asks for, shifted into place at bits 4-5.
static uint8_t ds5_route_bits_for(Ds5AudioOutput output)
{
    switch (output) {
        case Ds5AudioOutput::Headset:     return 0x00;
        case Ds5AudioOutput::HeadsetMono: return 0x10;
        case Ds5AudioOutput::Both:        return 0x20;
        case Ds5AudioOutput::Speaker:     return 0x30;
        case Ds5AudioOutput::Off:
        default:                          return 0x30;   // see the note below
    }
}

// "off" routes to speaker-only and then silences the speaker volume. Routing
// alone cannot mute everything -- value 0 still feeds the headset -- so the
// silence comes from the volumes, and this picks the routing that leaves the
// headset out of the picture.
static bool ds5_speaker_is_active(Ds5AudioOutput output)
{
    return output == Ds5AudioOutput::Speaker || output == Ds5AudioOutput::Both;
}

static bool ds5_headset_is_active(Ds5AudioOutput output)
{
    return output == Ds5AudioOutput::Headset ||
           output == Ds5AudioOutput::HeadsetMono ||
           output == Ds5AudioOutput::Both;
}

// The level an output should carry under a given mode. -1 means "leave it
// alone"; 0 means "actively silence it because this mode does not use it".
static int ds5_level_for(Ds5AudioOutput output, bool speaker, const char *section)
{
    const char *key = speaker ? "speaker_volume" : "headset_volume";
    if (output == Ds5AudioOutput::Auto) {
        return device_config_int(section, key, -1);
    }
    const bool active = speaker ? ds5_speaker_is_active(output)
                                : ds5_headset_is_active(output);
    if (!active) {
        return 0;
    }
    return device_config_int(section, key, -1);
}

// ⭐ THE LEVEL OUR OWN SETTINGS REPORT SENDS (ds5_apply_settings.inl). It is
// ds5_level_for's answer except in one case: an output that audio_output plays
// through (auto, which is also what no audio_output means, plays through the
// speaker) is sent its dial even when the dial was never moved -- 100, the
// default the configs window shows for both (rest_config.inl).
//
// ⛔ WITHOUT IT A MODE UNDID ITSELF ON A CABLE (rhoquinn8217, 2026-11-06, a
// cabled DualSense and Edge on the C1). "speaker" sends the headset 0, and the
// pad keeps that 0 until it is unplugged: the TV's own report at each bridge
// sets the speaker's volume, never the headset's. "headset" or "both" with no
// headset_volume then claimed nothing, so the headset stayed silent while its
// dial read 100. The speaker had the same hole after "headset",
// "headset_mono" or "off".
//
// ⭐ AND NO audio_output AT ALL IS AUTO HERE TOO (rhoquinn8217, 2026-11-10, a
// cabled DualSense on the C1). This used to send nothing for a pad with no
// audio_output, to leave a pad nobody configured exactly as it was. But a
// setting put back to its default is taken out of the controller's file, so
// "headset" (the speaker sent 0) and then "(default)" claimed nothing: the pad
// stayed on the headphones with its speaker at 0, silent with nothing in the
// jack, until a volume was moved. With no audio_output, a dial put back to
// 100, its default, did the same: the speaker stayed at the level before it.
// And auto, set or not, already sends the pad its route at every bridge
// (ds5_follow_jack), so "left as it was" no longer held anyway.
//
// ⓘ OUR REPORT ONLY. The overrides below keep ds5_level_for, so an unset key
// still leaves a game's own volume write alone.
static const int kDs5DialUnsetPercent = 100;

static int ds5_settings_level_for(Ds5AudioOutput output, bool speaker, const char *section)
{
    const int level = ds5_level_for(output, speaker, section);
    if (level >= 0) {
        return level;
    }
    // Under auto our report routes to the speaker alone (kDs5RouteToSpeaker),
    // so the speaker is the one output it plays through.
    const bool playsThrough = (output == Ds5AudioOutput::Auto)
        ? speaker
        : (speaker ? ds5_speaker_is_active(output) : ds5_headset_is_active(output));
    return playsThrough ? kDs5DialUnsetPercent : level;
}

// ⭐⭐ AUTO FOLLOWS THE HEADPHONE JACK (rhoquinn8217, 2026-11-07: "Audio is
// default to auto and the speaker plays audio correctly however when
// connecting a headset, the audio doesn't automatically switch to the
// headset unless headset audio has been enabled in DS5-USBIP"). A PS5
// switches because the console watches the jack; on Windows nothing did, and
// our report under auto always routes to the speaker. So with audio_output
// auto, or not set, which is auto: something in the pad's jack sends the
// headset route, nothing in it the speaker route, each with its own dial
// (100 for a dial never moved, as the window shows it). Sent as the jack
// changes (device.inl), and our settings report follows the jack too
// (ds5_apply_settings.inl), so saving a config does not route it back.
// ⓘ A game's own routing is still left alone under auto: the next plug going
// in or out wins over it.
// ⚠️ Over Bluetooth the TV fills the speaker into its own audio reports under
// auto, so there it needs the TV to follow the jack as well.
//
// The jack is byte 54 of the USB-shaped report 0x01, the status byte after
// the battery's (button_layout.inl, kBatteryDs5Status at 53): bit 0 headphones
// in, bit 1 a microphone (the Linux driver's DS_STATUS1_HP_DETECT and
// DS_STATUS1_MIC_DETECT, "HID: playstation: Support DualSense audio jack
// hotplug detection"). The headphones' bit decides: it is what a headset sets
// too. ✅ MEASURED on the C1, 2026-11-07, the DualSense Edge through its
// DS5Dongle: four plugs and three unplugs each read off bit 0 as they
// happened, and the route followed every one (device.log 23:35:49-23:38:35).
static const size_t  kDs5IdxJack         = 54;
static const uint8_t kDs5JackHeadphones  = 0x01;

// 1 headphones in the jack, 0 nothing, -1 not a report that says.
static int ds5_jack_in(const uint8_t *report, size_t length)
{
    if (report == nullptr || length <= kDs5IdxJack || report[0] != 0x01) return -1;
    return (report[kDs5IdxJack] & kDs5JackHeadphones) != 0 ? 1 : 0;
}

// The report auto sends when the jack changes: the headset route (0x00, stereo,
// no echo cancel, which is the speaker's) with the headset's dial, or the
// speaker route with echo cancel, as our settings report always sends it,
// with the speaker's dial. Only the audio control and that one volume are
// claimed: nothing else on the pad changes.
static std::vector<uint8_t> ds5_auto_route_report(const char *section, bool jackIn)
{
    std::vector<uint8_t> report(kDs5OutReportLen, 0);
    report[0] = kDs5OutReportId;
    uint8_t claim = kDs5AllowAudioControl;
    if (jackIn) {
        const int pct = device_config_int(section, "headset_volume", kDs5DialUnsetPercent);
        claim = static_cast<uint8_t>(claim | kDs5AllowHeadsetVolume);
        report[kDs5IdxHeadsetVolume] =
            ds5_volume_raw_from_percent(pct, kDs5HeadsetVolumeMax, kDs5HeadsetVolumeFloor);
        report[kDs5IdxAudioControl] = 0x00;
    } else {
        const int pct = device_config_int(section, "speaker_volume", kDs5DialUnsetPercent);
        claim = static_cast<uint8_t>(claim | kDs5AllowSpeakerVolume);
        report[kDs5IdxSpeakerVolume] =
            ds5_volume_raw_from_percent(pct, kDs5SpeakerVolumeMax, kDs5SpeakerVolumeFloor);
        uint8_t control = kDs5RouteToSpeaker;
        if (device_config_bool(section, "force_echo_cancel", true)) {
            control = static_cast<uint8_t>(control | kDs5EchoNoiseCancel);
        }
        report[kDs5IdxAudioControl] = control;
    }
    report[kDs5IdxValidFlag0] = claim;
    return report;
}

static std::atomic<uint64_t> g_ds5_echo_patch_count{0};

// --- Override 1: the audio-control byte (routing + echo cancellation) ---
//
// Routing and echo cancellation share one byte, so one function owns it.
//
// !! ECHO CANCELLATION IS CONDITIONAL ON THE SPEAKER BEING ACTIVE, and that is
// !! not a nicety. The controller mutes its own speaker when cancellation is
// !! off because the microphone sits beside it -- but with audio going to the
// !! headset there is no speaker output and nothing to cancel. Both this
// !! project's and Ciprian's Bluetooth code set the audio flags to zero for
// !! headset mode, cancellation included. Forcing it on regardless would be
// !! asserting a setting for a path that is not in use.
//
// When audio_output is absent, routing is left exactly as the game set it and
// only the echo-cancel force applies -- the behaviour before routing existed.
static void ds5_override_audio_control(uint8_t *data, size_t length, const char *section)
{
    if (data == nullptr || length <= kDs5IdxAudioControl) {
        return;
    }
    if (data[0] != kDs5OutReportId) {
        return;
    }
    if ((data[kDs5IdxValidFlag0] & kDs5AllowAudioControl) == 0) {
        return;  // the host is not setting audio control; nothing to correct
    }

    const Ds5AudioOutput output = ds5_audio_output_for(section);
    const uint8_t before = data[kDs5IdxAudioControl];
    uint8_t value = before;

    if (output != Ds5AudioOutput::Auto) {
        value = static_cast<uint8_t>(value & ~kDs5RouteMask);
        value = static_cast<uint8_t>(value | ds5_route_bits_for(output));
    }

    // Under auto, the speaker is active if the game routed it there.
    const bool speakerActive = (output == Ds5AudioOutput::Auto)
        ? ((value & kDs5RouteMask) != 0)
        : ds5_speaker_is_active(output);

    // ⛔ DEFAULTS TO FALSE HERE, unlike ds5_apply_settings.inl -- and the two
    // are RIGHT to differ.
    //
    // That file constructs OUR OWN report at bridge time, so echo cancel must
    // be on or we reproduce the attenuation bug: the controller mutes itself
    // when it is off. This function intercepts a GAME'S report, where the rule
    // is that an absent key leaves the report untouched.
    //
    // ⚠️ Defaulting to true here was tried on 2026-08-21 and immediately broke
    // "absent config keys leave a report completely untouched" -- an empty
    // config rewrote the audio-control byte on every device. Games already send
    // echo cancel on; correcting a write nobody configured is the same class of
    // fault as the stale speaker_volume that attenuated the fleet.
    if (speakerActive && device_config_bool(section, "force_echo_cancel", false)) {
        value = static_cast<uint8_t>(value | kDs5EchoNoiseCancel);
    }

    if (value == before) {
        return;
    }
    data[kDs5IdxAudioControl] = value;

    const uint64_t count = ++g_ds5_echo_patch_count;
    if (count <= 5 || (count % 100) == 0) {
        device_log::report(device_log::msg()
            << section << ": audio control: rewrote an audio-control write"
            << " (change #" << count << ")");
    }
}

// --- Override 2: speaker volume ---
//
// Replaces the volume in any report that claims it. Unlike the echo-cancel
// override this REPLACES A DELIBERATE CHOICE rather than correcting a fault:
// something writes maximum volume at every game launch, so without this the
// configured value silently stops meaning anything the moment a game starts.
//
// Justified as a user preference -- a nominal clang is inappropriate in a quiet
// room -- but it is the first override of its kind. Do not generalise it to
// other fields without a deliberate decision about who should win.
static std::atomic<uint64_t> g_ds5_volume_override_count{0};

static void ds5_override_speaker_volume(uint8_t *data, size_t length, const char *section)
{
    if (data == nullptr || length <= kDs5IdxSpeakerVolume) {
        return;
    }
    if (data[0] != kDs5OutReportId) {
        return;
    }
    if ((data[kDs5IdxValidFlag0] & kDs5AllowSpeakerVolume) == 0) {
        return;  // nothing is setting the volume; leave it alone
    }
    const int configured = ds5_level_for(ds5_audio_output_for(section), true, section);
    if (configured < 0) {
        return;  // no key and no mode forcing it: the game's value stands
    }

    const uint8_t wanted = ds5_volume_raw_from_percent(configured, kDs5SpeakerVolumeMax, kDs5SpeakerVolumeFloor);
    if (data[kDs5IdxSpeakerVolume] == wanted) {
        return;
    }
    data[kDs5IdxSpeakerVolume] = wanted;

    const uint64_t count = ++g_ds5_volume_override_count;
    if (count <= 5 || (count % 100) == 0) {
        device_log::report(device_log::msg()
            << section << ": speaker volume: overrode a write to " << configured
            << "% (override #" << count << ")");
    }
}



// --- Override 3: rumble gain ---
//
// SCALES rather than replaces, and the distinction matters. Rumble is not a
// setting, it is a signal: the game varies it continuously to express what is
// happening. Replacing it with a fixed number would flatten that expression
// into a constant buzz. So the configured value is a MULTIPLIER applied to
// whatever the game sends -- 100 leaves it untouched, 50 halves it, 0 disables
// rumble entirely, 200 doubles it.
//
// The percentage-as-multiplier approach is Ciprian's; his slider works this
// way. NO CODE WAS COPIED.
//
// Deliberately a straight multiply for now. The wireless haptics path applies
// a perceptual curve instead, because perceived strength is not linear in
// amplitude -- but that curve was tuned for haptic audio samples, not these
// motor bytes, and we have no measurement of how the motors respond. A
// straight multiply is predictable and gives a later curve something to be
// validated against.
static const size_t  kDs5IdxRumbleRight = 3;
static const size_t  kDs5IdxRumbleLeft  = 4;
static const int     kDs5RumbleGainMax  = 500;   // matches the wireless clamp of 5x

static std::atomic<uint64_t> g_ds5_rumble_scale_count{0};

static uint8_t ds5_scale_rumble(uint8_t value, int gainPercent)
{
    const int scaled = (static_cast<int>(value) * gainPercent) / 100;
    if (scaled > 255) {
        return 255;
    }
    return static_cast<uint8_t>(scaled);
}

// ⭐ WATCHING THE TRIGGER BLOCKS A HOST SENDS.
//
// ⛔ WHY IT HAD TO EXIST. Our own settings report sets a trigger effect, the
// write succeeds, and the trigger feels like nothing (2026-09-10). Everything
// checkable was checked and agreed: the claim bit, the offsets, an encoding
// that rebuilds a real capture byte for byte. So the next question is not
// answerable by reading -- it is what a KNOWN GOOD report looks like on this
// hardware, and how it differs from ours.
//
// ⓘ Anything that drives the bridged controller reaches the pad through here,
// including a browser talking to the virtual device over WebHID. That is what
// makes this the place to watch from: it sees what a working tool sends.
//
// ⛔ LOGS ONLY WHAT CHANGED. The blocks are re-sent on every report, so logging
// each one gives hundreds of identical lines a second and buries the moment a
// tool actually set something.
inline std::mutex g_ds5TriggerSeenMutex;
inline std::map<std::string, std::vector<uint8_t>> g_ds5TriggerSeen;

static std::string ds5_hex(const uint8_t *p, size_t n)
{
    static const char *digits = "0123456789abcdef";
    std::string out;
    for (size_t i = 0; i < n; ++i) {
        if (i) out += ' ';
        out += digits[p[i] >> 4];
        out += digits[p[i] & 0x0f];
    }
    return out;
}

static void ds5_observe_trigger_blocks(const uint8_t *data, size_t length, const char *section)
{
    // 11..21 is R2, 22..32 is L2, both absolute within the report WITH the id
    // at index 0.
    if (data == nullptr || length < 33) return;
    if (data[0] != kDs5OutReportId) return;

    // The claim byte travels with them: a block full of bytes that is not
    // claimed does nothing, and the two look identical without it.
    std::vector<uint8_t> now(23, 0);
    now[0] = data[kDs5IdxValidFlag0];
    memcpy(now.data() + 1, data + 11, 22);

    {
        std::lock_guard<std::mutex> lock(g_ds5TriggerSeenMutex);
        auto it = g_ds5TriggerSeen.find(section);
        if (it != g_ds5TriggerSeen.end() && it->second == now) return;
        g_ds5TriggerSeen[section] = now;
    }

    device_log::config(device_log::msg()
        << "[trigger-in] " << section
        << " flag0=" << ds5_hex(now.data(), 1)
        << " R2=" << ds5_hex(now.data() + 1, 11)
        << " L2=" << ds5_hex(now.data() + 12, 11));
}

static void ds5_override_rumble(uint8_t *data, size_t length, const char *section)
{
    if (data == nullptr || length <= kDs5IdxRumbleLeft) {
        return;
    }
    if (data[0] != kDs5OutReportId) {
        return;
    }
    // Both motors are governed by the two rumble claim bits. If the game is not
    // claiming them it is not driving rumble, and the fields are not ours.
    if ((data[kDs5IdxValidFlag0] & (kDs5ClaimRumbleA | kDs5ClaimRumbleB)) == 0) {
        return;
    }
    // Master applies to both motors; the per-motor gains multiply on top, like
    // a mixing desk -- master 50 with heavy 50 gives the big weight 25%.
    // Confirmed from the DualSense Tester source 2026-08-01: LEFT is the heavy
    // motor (the big weight), RIGHT is the soft one.
    const int master = device_config_int(section, "master_rumble_gain", -1);
    const int heavy  = device_config_int(section, "rumble_gain_heavy", -1);
    const int soft   = device_config_int(section, "rumble_gain_soft", -1);
    const int floorPct = device_config_int(section, "rumble_floor", -1);
    if (master < 0 && heavy < 0 && soft < 0 && floorPct < 0) {
        return;  // no keys means the game's rumble stands
    }
    // ⓘ The floor acts alone: someone who wants only the floor should not have
    // to set a gain to 100 to get it.
    const int rumbleFloor = floorPct < 0 ? kRumbleFloorDefaultPercent
        : (floorPct > 100 ? 100 : floorPct);
    const int masterGain = master < 0 ? 100
        : (master > kDs5RumbleGainMax ? kDs5RumbleGainMax : master);
    const int heavyGain = heavy < 0 ? 100
        : (heavy > kDs5RumbleGainMax ? kDs5RumbleGainMax : heavy);
    const int softGain = soft < 0 ? 100
        : (soft > kDs5RumbleGainMax ? kDs5RumbleGainMax : soft);

    const uint8_t beforeRight = data[kDs5IdxRumbleRight];
    const uint8_t beforeLeft  = data[kDs5IdxRumbleLeft];
    // ⓘ The per-motor gain is passed so the floor can tell OUR arithmetic
    // erasing a cue from the PERSON asking for silence -- a motor gained to 0
    // stays at 0, whatever the floor says.
    const int softEffective = (masterGain * softGain) / 100;
    const int heavyEffective = (masterGain * heavyGain) / 100;
    data[kDs5IdxRumbleRight] = rumble_apply_floor(
        ds5_scale_rumble(beforeRight, softEffective), beforeRight, rumbleFloor, softEffective);
    data[kDs5IdxRumbleLeft] = rumble_apply_floor(
        ds5_scale_rumble(beforeLeft, heavyEffective), beforeLeft, rumbleFloor, heavyEffective);

    if (beforeRight == data[kDs5IdxRumbleRight] && beforeLeft == data[kDs5IdxRumbleLeft]) {
        return;  // nothing changed -- usually both motors already at rest
    }

    // Rumble reports stream during play, so this is logged sparsely on purpose.
    const uint64_t count = ++g_ds5_rumble_scale_count;
    if (count <= 3 || (count % 1000) == 0) {
        device_log::report(device_log::msg()
            << section << ": rumble: scaled heavy to " << (masterGain * heavyGain) / 100
            << "% soft to " << (masterGain * softGain) / 100
            << "% floor " << rumbleFloor << "%"
            << " (heavy " << static_cast<int>(beforeLeft) << "->"
            << static_cast<int>(data[kDs5IdxRumbleLeft])
            << ", soft " << static_cast<int>(beforeRight) << "->"
            << static_cast<int>(data[kDs5IdxRumbleRight])
            << ", scale #" << count << ")");
    }
}

// --- Override 4: headset volume ---
//
// Byte 5, its own claim bit -- separate from the speaker's volume at byte 6.
// Confirmed 2026-08-01 from a captured tester report claiming 0x90 (headset
// volume plus audio control) where the speaker case claims 0xa0.
static std::atomic<uint64_t> g_ds5_headset_volume_count{0};

static void ds5_override_headset_volume(uint8_t *data, size_t length, const char *section)
{
    if (data == nullptr || length <= kDs5IdxHeadsetVolume) {
        return;
    }
    if (data[0] != kDs5OutReportId) {
        return;
    }
    if ((data[kDs5IdxValidFlag0] & kDs5AllowHeadsetVolume) == 0) {
        return;
    }
    const int configured = ds5_level_for(ds5_audio_output_for(section), false, section);
    if (configured < 0) {
        return;
    }
    const uint8_t wanted = ds5_volume_raw_from_percent(configured, kDs5HeadsetVolumeMax, kDs5HeadsetVolumeFloor);
    if (data[kDs5IdxHeadsetVolume] == wanted) {
        return;
    }
    data[kDs5IdxHeadsetVolume] = wanted;

    const uint64_t count = ++g_ds5_headset_volume_count;
    if (count <= 5 || (count % 100) == 0) {
        device_log::report(device_log::msg()
            << section << ": headset volume: overrode a write to " << configured
            << "% (override #" << count << ")");
    }
}

// Single entry point called from the outbound report path. Safe on every
// report: each override returns immediately for anything that is not its own
// business, before any lookup.
// `linkedConfig` names a per-controller config file, or is empty for the shared
// section. Defaulted so every existing caller is unaffected.
static void ds5_apply_output_overrides(uint8_t *data, size_t length,
                                      const std::vector<unsigned char> &descriptor,
                                      const std::string &linkedConfig = std::string(),
                                      const std::string &controllerSection = std::string())
{
    // Resolved once per report, before anything else: an unrecognised device
    // costs two comparisons and is then left entirely alone. The per-report
    // format checks stay inside each override, since report ids differ by
    // device -- a DualSense uses 0x02 and others do not.
    // ⛔ The CAPABILITY question, not the kind question -- this patches DS5 output report bytes, so it must not run for anything else.
    if (!device_has_ds5_audio(descriptor)) {
        return;
    }
    const char *kind = device_section_for(descriptor);
    if (kind == nullptr) {
        return;
    }
    const std::string resolved = device_settings_section(kind, linkedConfig);
    const char *section = resolved.c_str();
    // ⭐ Audio and rumble are the CONTROLLER's settings, not its config's
    // (controller_settings.inl): read from its own section. The triggers stay
    // the config's.
    const std::string ctlName = controller_settings_section(kind, controllerSection);
    const char *ctl = ctlName.c_str();
    // First, so the log shows what the HOST sent rather than what we left.
    ds5_observe_trigger_blocks(data, length, section);
    ds5_override_audio_control(data, length, ctl);
    ds5_override_speaker_volume(data, length, ctl);
    ds5_override_headset_volume(data, length, ctl);
    ds5_override_rumble(data, length, ctl);
}
