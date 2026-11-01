// ---------------------------------------------------------------------------
// Audio gain for the wired ISO audio path -- speaker and haptics.
//
// The DualSense's wired audio stream carries four channels in one buffer:
//
//   channels 0,1 -- the audio output. NOT "the speaker": measured 2026-08-01,
//                   these channels feed whichever destination the routing byte
//                   selects, so with audio_output = headphone they are the
//                   headset. Hence `audio_gain`, not `speaker_gain`.
//   channels 2,3 -- audio-based haptics, the fine-grained waveform PS5-native
//                   titles actually use for rumble
//
// Both are scaled here, independently, by their own config key.
//
// GAIN IS NOT VOLUME, AND THE DISTINCTION IS THE AUDIO CONVENTION:
//
//   * GAIN scales a SIGNAL on its way through. 100 means UNCHANGED.
//     `audio_gain` and `rumble_gain` are gains and live here.
//   * VOLUME sets the DEVICE's output level at the end of the chain. 100 means
//     MAXIMUM. `speaker_volume` is a volume and lives in the output-report
//     overrides, not here.
//
// !! So 100 means "do nothing" for a gain and "as loud as possible" for a
// !! volume. Both are correct in their own right; the config file comments say
// !! so explicitly because it is the one genuinely confusing part.
//
// Decibels would be the professionally correct unit for a gain, but silence is
// negative infinity dB -- awkward in a text file, and "turn rumble off" is the
// most likely thing anyone sets. Percentage-multiplier is also what game
// rumble sliders use, so it is the convention these users already know.
//
// SCALING, NOT REPLACING. Both signals are continuous expression, not
// settings: the game varies them to convey what is happening. A fixed value
// would flatten that. Percentage-as-multiplier follows Ciprian's slider
// design; NO CODE WAS COPIED.
//
// !! SPEAKER GAIN IS CAPPED AT 100 -- REDUCTION ONLY. Game audio already runs
// !! near full scale, so boosting digitally just clips, and clipped audio
// !! sounds bad in a way clipped haptics only feel flat. Rumble is allowed
// !! above 100, but expect little from it for the same headroom reason.
//
// !! SOURCE AMPLITUDE WAS DISPROVEN AS THE CAUSE OF THE ATTENUATION FAULT --
// !! that finding stands and must not be re-litigated. This is the opposite
// !! question, never tested: deliberately scaling the source to CONTROL
// !! loudness. The old finding supports it -- the controller amplifies
// !! whatever it is handed, so handing it less yields less.
//
// PERFORMANCE: runs on every audio chunk, ~100x/sec, inside the send path. The
// config is never looked up here -- values are cached in atomics and read with
// one relaxed load per chunk. A per-chunk config lookup is exactly the kind of
// cost that resurfaces later as dropouts blamed on something else.
//
// THE RULE: A RESEAT APPLIES EVERY SETTING; NOTHING NEEDS A LISTENER RESTART.
// Each pad re-reads its gains about once a second while its sound flows
// (device.inl, refresh_gains), so a changed file or link takes within a
// second. Any future setting on a hot path must do the same -- cache for
// speed, re-read on a slow beat. Caching without a refresh is what made
// rumble_gain the odd one out.
//
// BACKWARD COMPATIBILITY: nothing on the wire. No message type, no enum, no
// map key, no default, no change to what is transmitted -- only the amplitude
// of samples already in flight. Inert unless a gain is configured.
//
// INCLUDE ORDER: after config/device_config.inl, before usbip/device.inl.
// ---------------------------------------------------------------------------

namespace ctm_audio_gain {

constexpr size_t kChannels        = 4;
constexpr size_t kBytesPerFrame   = kChannels * sizeof(int16_t);
constexpr size_t kSpeakerChannels = 2;    // 0,1 speaker/headset; 2,3 haptics
constexpr int    kRumbleGainMax   = 500;  // matches the wireless clamp of 5x
constexpr int    kSpeakerGainMax  = 100;  // reduction only -- see the note above

// A gain that is absent, or set to 100, changes nothing.
inline bool gain_is_active(int gain)
{
    return gain >= 0 && gain != 100;
}

// ⭐⭐ ONE PAD'S GAINS, FROM ITS OWN SECTION (2026-10-13). They were read from
// the shared [ds5] section only, for every pad: a linked config's
// master_rumble_gain and audio_gain scaled its rumble motors and never its
// stream, and an Edge's stream followed [ds5], not [ds5_edge]. Found by
// reading, while looking into PC-sound haptics. Now each pad reads the
// section everything else of it reads (device.inl, refresh_gains), at attach,
// when its link changes, and about once a second while its sound flows.
// -1 where the section sets none.
struct Gains {
    int haptics = -1;   // master_rumble_gain, on channels 2 and 3
    int sound = -1;     // audio_gain, on channels 0 and 1
};

inline Gains read(const char *section)
{
    Gains g;
    const int rumble = device_config_int(section, "master_rumble_gain", -1);
    const int sound = device_config_int(section, "audio_gain", -1);
    g.haptics = (rumble < 0 || rumble <= kRumbleGainMax) ? rumble : kRumbleGainMax;
    g.sound = (sound < 0 || sound <= kSpeakerGainMax) ? sound : kSpeakerGainMax;
    return g;
}

// Cheap gate for the call site: lets the caller skip copying the buffer when
// there is nothing to do.
inline bool active(const Gains &g)
{
    return gain_is_active(g.haptics) || gain_is_active(g.sound);
}

inline void scale_sample(uint8_t *at, int gain)
{
    const int16_t sample = static_cast<int16_t>(
        static_cast<uint16_t>(at[0]) | (static_cast<uint16_t>(at[1]) << 8));

    // Widened before multiplying: a loud sample times a gain above 100
    // overflows int16_t, which would wrap a strong pulse into a strong pulse of
    // the opposite sign rather than clipping.
    int32_t scaled = (static_cast<int32_t>(sample) * gain) / 100;
    if (scaled > 32767) scaled = 32767;
    if (scaled < -32768) scaled = -32768;

    const uint16_t out = static_cast<uint16_t>(static_cast<int16_t>(scaled));
    at[0] = static_cast<uint8_t>(out & 0xff);
    at[1] = static_cast<uint8_t>((out >> 8) & 0xff);
}

// Scales sound and haptic channels in place, each by its own gain, leaving
// either untouched when its gain is absent or 100.
inline void apply(std::vector<uint8_t> &data, const Gains &gains)
{
    const bool doRumble = gain_is_active(gains.haptics);
    const bool doSpeaker = gain_is_active(gains.sound);
    if (!doRumble && !doSpeaker) {
        return;
    }
    const size_t frames = data.size() / kBytesPerFrame;
    for (size_t f = 0; f < frames; ++f) {
        uint8_t *frameStart = data.data() + f * kBytesPerFrame;
        for (size_t ch = 0; ch < kChannels; ++ch) {
            const bool isSpeaker = ch < kSpeakerChannels;
            if (isSpeaker ? !doSpeaker : !doRumble) {
                continue;
            }
            scale_sample(frameStart + ch * sizeof(int16_t),
                         isSpeaker ? gains.sound : gains.haptics);
        }
    }
}

}  // namespace ctm_audio_gain
