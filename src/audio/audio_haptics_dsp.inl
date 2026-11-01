// PC sound made into DualSense haptics: the shaping and the mixing, pure.
//
// ⭐ WHY (rhoquinn8217, 2026-10-13, after the feature compare with
// SundayMoments/DS5_Bridge and its "Audio Haptics"). The low part of what the
// PC plays is felt in the controller's two actuators: an explosion, an engine,
// a bass line. A DualSense's sound stream carries four channels, and 2 and 3
// ARE the actuators, so this only has to put the right samples there.
// ⓘ Written fresh: DS5 Bridge is AGPL-3.0-only, so its approach was read and
// none of its code, constants or presets taken.
//
// THE CHAIN, per channel: the left actuator is fed from the left, the right
// from the right.
//   high-pass, 25 Hz    takes out DC and drift the actuators would only lean on
//   low-pass (bass)     keeps what is felt, not heard: audio_haptics_bass_hz
//   gate                below about -50 dBFS nothing comes out, so hiss and
//                       quiet music do not buzz the hands; held open 60 ms so
//                       a beat's tail is not chopped, opened and closed by a
//                       ramp so it never clicks
//   strength            audio_haptics_strength: 100 is the normal strength
//   soft clip           a loud part rounds off instead of cracking
// then to 48 kHz, the pad's own rate, and 16-bit.
//
// ⭐ AND HOW IT MEETS A GAME'S OWN HAPTICS (mix_into): a PS5 port already
// writes channels 2 and 3. YIELD, the default, steps aside while it does and
// comes back 200 ms after it stops; MIX adds the two; REPLACE puts ours in
// place of the game's.
//
// ⓘ Pure, so the tests reach it; audio_haptics.inl feeds it the PC's sound
// and hands its output to each pad.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <vector>

namespace audio_haptics {

constexpr unsigned kPadRate = 48000;           // the pad's sound stream
constexpr unsigned kPadChannels = 4;           // 0,1 speaker or headset; 2,3 actuators
constexpr double kHighPassHz = 25.0;
constexpr double kGateOpen = 0.0032;           // about -50 dBFS, as a fraction of full scale
constexpr double kGateClose = 0.0020;          // a little lower, so it does not flutter
constexpr double kGateHoldSeconds = 0.060;
constexpr double kGateRampSeconds = 0.003;
constexpr double kMakeUp = 2.0;                // strength 100: bass alone is quieter than the sound it came from
constexpr int kBassHzMin = 40, kBassHzMax = 300, kBassHzDefault = 150;
constexpr int kStrengthMax = 200, kStrengthDefault = 100;
// A game's own haptics count as playing above about -42 dBFS.
constexpr int kGameHapticsFloor = 256;
constexpr unsigned kYieldHoldFrames = kPadRate / 5;        // 200 ms
constexpr double kDuckSeconds = 0.005;

enum class Mode { Yield, Mix, Replace };

// A second-order filter, the usual "cookbook" one, run in transposed direct
// form II.
struct Biquad {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1 = 0, z2 = 0;

    static Biquad make(bool lowPass, double rate, double hz)
    {
        const double pi = 3.14159265358979323846;
        const double w0 = 2.0 * pi * hz / rate;
        const double c = std::cos(w0);
        const double alpha = std::sin(w0) / (2.0 * 0.7071067811865476);   // Q of 1/sqrt(2)
        const double a0 = 1.0 + alpha;
        Biquad f;
        if (lowPass) {
            f.b0 = (1.0 - c) / 2.0 / a0;
            f.b1 = (1.0 - c) / a0;
            f.b2 = (1.0 - c) / 2.0 / a0;
        } else {
            f.b0 = (1.0 + c) / 2.0 / a0;
            f.b1 = -(1.0 + c) / a0;
            f.b2 = (1.0 + c) / 2.0 / a0;
        }
        f.a1 = -2.0 * c / a0;
        f.a2 = (1.0 - alpha) / a0;
        return f;
    }

    double run(double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

// Rounds a loud sample off on its way to full scale, never past it. ⓘ A
// rational stand-in for tanh, exact enough to feel and cheap enough for every
// sample.
inline double soft_clip(double x)
{
    if (x >= 3.0) return 1.0;
    if (x <= -3.0) return -1.0;
    const double x2 = x * x;
    return x * (27.0 + x2) / (27.0 + 9.0 * x2);
}

inline int16_t to_s16(double x)
{
    const double scaled = std::round(x * 32767.0);
    if (scaled > 32767.0) return 32767;
    if (scaled < -32768.0) return -32768;
    return static_cast<int16_t>(scaled);
}

// The PC's sound, as stereo, made into what the actuators get. One per pad:
// its strength and bass are that pad's settings.
class Shaper {
public:
    // A different rate or bass setting starts the filters afresh; strength
    // alone does not need to.
    void configure(unsigned inputRate, int bassHz, int strength)
    {
        bassHz = (std::max)(kBassHzMin, (std::min)(kBassHzMax, bassHz));
        strength = (std::max)(0, (std::min)(kStrengthMax, strength));
        gain_ = kMakeUp * strength / 100.0;
        if (inputRate == rate_ && bassHz == bassHz_) return;
        rate_ = inputRate;
        bassHz_ = bassHz;
        for (int ch = 0; ch < 2; ++ch) {
            highPass_[ch] = Biquad::make(false, rate_, kHighPassHz);
            lowPass_[ch] = Biquad::make(true, rate_, bassHz_);
        }
        envelope_ = 0.0;
        holdLeft_ = 0;
        gateGain_ = 0.0;
        position_ = 0.0;
        havePrevious_ = false;
        releaseStep_ = std::exp(-1.0 / (0.020 * rate_));                  // the envelope falls in about 20 ms
        holdFrames_ = static_cast<unsigned>(kGateHoldSeconds * rate_);
        rampStep_ = 1.0 / (kGateRampSeconds * rate_);
    }

    unsigned rate() const { return rate_; }

    // `frames` stereo samples at the configured rate, -1 to 1, in; the same
    // stretch of time at 48 kHz, 16-bit stereo, appended to `out`.
    void process(const float *stereo, size_t frames, std::vector<int16_t> *out)
    {
        if (rate_ == 0) return;
        for (size_t f = 0; f < frames; ++f) {
            double s[2];
            for (int ch = 0; ch < 2; ++ch) {
                s[ch] = lowPass_[ch].run(highPass_[ch].run(stereo[2 * f + ch]));
            }
            // The gate follows the louder side, so both open and close together.
            const double level = (std::max)(std::fabs(s[0]), std::fabs(s[1]));
            envelope_ = (std::max)(level, envelope_ * releaseStep_);
            if (envelope_ >= kGateOpen) {
                holdLeft_ = holdFrames_;
            } else if (envelope_ < kGateClose && holdLeft_ > 0) {
                --holdLeft_;
            }
            const double target = holdLeft_ > 0 ? 1.0 : 0.0;
            if (gateGain_ < target) gateGain_ = (std::min)(target, gateGain_ + rampStep_);
            else if (gateGain_ > target) gateGain_ = (std::max)(target, gateGain_ - rampStep_);
            double shaped[2];
            for (int ch = 0; ch < 2; ++ch) shaped[ch] = soft_clip(s[ch] * gain_ * gateGain_);
            emit(shaped, out);
        }
    }

    // Open now: for a pad's keep-alive, which only runs while there is
    // something to feel.
    bool gate_open() const { return gateGain_ > 0.0; }

private:
    // To 48 kHz by straight lines between neighbours, which is exact enough
    // for sound that has nothing above a few hundred hertz left in it. The
    // place between input samples carries over from one call to the next.
    void emit(const double shaped[2], std::vector<int16_t> *out)
    {
        if (rate_ == kPadRate) {
            out->push_back(to_s16(shaped[0]));
            out->push_back(to_s16(shaped[1]));
            return;
        }
        if (!havePrevious_) {
            previous_[0] = shaped[0];
            previous_[1] = shaped[1];
            havePrevious_ = true;
            return;
        }
        const double step = static_cast<double>(rate_) / kPadRate;   // input samples per output sample
        while (position_ < 1.0) {
            for (int ch = 0; ch < 2; ++ch) {
                out->push_back(to_s16(previous_[ch] + (shaped[ch] - previous_[ch]) * position_));
            }
            position_ += step;
        }
        position_ -= 1.0;
        previous_[0] = shaped[0];
        previous_[1] = shaped[1];
    }

    unsigned rate_ = 0;
    int bassHz_ = 0;
    double gain_ = kMakeUp;
    Biquad highPass_[2], lowPass_[2];
    double envelope_ = 0.0, releaseStep_ = 0.0;
    unsigned holdLeft_ = 0, holdFrames_ = 0;
    double gateGain_ = 0.0, rampStep_ = 1.0;
    double position_ = 0.0;
    double previous_[2] = { 0.0, 0.0 };
    bool havePrevious_ = false;
};

// What a pad has waiting: 48 kHz stereo haptic samples, from the capture to
// the pad's sound stream. The two run on different clocks, so it is kept
// SHORT: past 40 ms it drops back to 10, and run dry it gives silence.
// Latency, not completeness, is what matters to something felt.
class Ring {
public:
    static constexpr size_t kTargetFrames = kPadRate / 100;        // 10 ms
    static constexpr size_t kCapFrames = kPadRate / 25;            // 40 ms

    void push(const int16_t *stereo, size_t frames)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        samples_.insert(samples_.end(), stereo, stereo + frames * 2);
        const size_t have = samples_.size() / 2;
        if (have > kCapFrames) {
            samples_.erase(samples_.begin(), samples_.begin() + static_cast<std::ptrdiff_t>((have - kTargetFrames) * 2));
            ++drops_;
        }
    }

    // `frames` stereo samples into `out`, silence where there are none.
    void pull(int16_t *out, size_t frames)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const size_t have = (std::min)(frames, samples_.size() / 2);
        std::copy(samples_.begin(), samples_.begin() + static_cast<std::ptrdiff_t>(have * 2), out);
        samples_.erase(samples_.begin(), samples_.begin() + static_cast<std::ptrdiff_t>(have * 2));
        std::fill(out + have * 2, out + frames * 2, int16_t(0));
        if (have < frames) ++shortfalls_;
    }

    size_t frames() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return samples_.size() / 2;
    }

    void clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        samples_.clear();
    }

    size_t drops() const { std::lock_guard<std::mutex> lock(mutex_); return drops_; }
    size_t shortfalls() const { std::lock_guard<std::mutex> lock(mutex_); return shortfalls_; }

private:
    mutable std::mutex mutex_;
    std::vector<int16_t> samples_;
    size_t drops_ = 0;
    size_t shortfalls_ = 0;
};

// How a pad's stream is stepping aside for a game's own haptics.
struct YieldState {
    unsigned holdFrames = 0;
    double duck = 1.0;
};

inline int16_t read_s16(const uint8_t *at)
{
    return static_cast<int16_t>(static_cast<uint16_t>(at[0]) | (static_cast<uint16_t>(at[1]) << 8));
}

inline void write_s16(uint8_t *at, int32_t value)
{
    if (value > 32767) value = 32767;
    if (value < -32768) value = -32768;
    const uint16_t bits = static_cast<uint16_t>(static_cast<int16_t>(value));
    at[0] = static_cast<uint8_t>(bits & 0xff);
    at[1] = static_cast<uint8_t>(bits >> 8);
}

// Puts `frames` stereo haptic samples into channels 2 and 3 of the pad's
// 4-channel, 16-bit stream, as `mode` says. ⛔ Channels 0 and 1, the sound
// itself, are never touched.
inline void mix_into(uint8_t *pcm, size_t frames, const int16_t *haptics, Mode mode, YieldState *state)
{
    const size_t frameBytes = kPadChannels * sizeof(int16_t);
    if (mode == Mode::Yield) {
        for (size_t f = 0; f < frames; ++f) {
            const uint8_t *frame = pcm + f * frameBytes;
            if (std::abs(read_s16(frame + 4)) > kGameHapticsFloor || std::abs(read_s16(frame + 6)) > kGameHapticsFloor) {
                state->holdFrames = kYieldHoldFrames;
                break;
            }
        }
    }
    const double duckStep = 1.0 / (kDuckSeconds * kPadRate);
    for (size_t f = 0; f < frames; ++f) {
        uint8_t *frame = pcm + f * frameBytes;
        double target = 1.0;
        if (mode == Mode::Yield && state->holdFrames > 0) {
            target = 0.0;
            --state->holdFrames;
        }
        if (state->duck < target) state->duck = (std::min)(target, state->duck + duckStep);
        else if (state->duck > target) state->duck = (std::max)(target, state->duck - duckStep);
        const int32_t left = static_cast<int32_t>(std::lround(haptics[2 * f] * state->duck));
        const int32_t right = static_cast<int32_t>(std::lround(haptics[2 * f + 1] * state->duck));
        if (mode == Mode::Replace) {
            write_s16(frame + 4, left);
            write_s16(frame + 6, right);
        } else {
            write_s16(frame + 4, read_s16(frame + 4) + left);
            write_s16(frame + 6, read_s16(frame + 6) + right);
        }
    }
}

// The PC's sound in whatever layout Windows mixes it, as stereo: front left
// and right, with the centre and the LFE folded into both when there are any.
// `centre` and `lfe` are channel numbers, or -1.
inline void to_stereo(const float *in, size_t frames, unsigned channels, int centre, int lfe, std::vector<float> *out)
{
    out->resize(frames * 2);
    for (size_t f = 0; f < frames; ++f) {
        const float *frame = in + f * channels;
        float left = channels > 0 ? frame[0] : 0.0f;
        float right = channels > 1 ? frame[1] : left;
        float shared = 0.0f;
        if (centre >= 0 && static_cast<unsigned>(centre) < channels) shared += 0.7071f * frame[centre];
        if (lfe >= 0 && static_cast<unsigned>(lfe) < channels) shared += frame[lfe];
        (*out)[2 * f] = left + shared;
        (*out)[2 * f + 1] = right + shared;
    }
}

}  // namespace audio_haptics
