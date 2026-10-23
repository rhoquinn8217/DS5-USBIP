// PC sound made into DualSense haptics: the shaping and the mixing.
//
// ⭐ WHAT THESE PROTECT. What the hands feel comes out of these few
// functions: bass kept and the rest taken out, silence left silent, the
// strength setting obeyed, nothing past full scale, the right number of
// samples at the pad's rate, and a game's own haptics respected. The sound
// in channels 0 and 1 is never touched.

#include "harness.h"

#include <cmath>
#include <cstdint>
#include <vector>

#include "audio/audio_haptics_dsp.inl"

using namespace ctmtest;
namespace ah = audio_haptics;

namespace {

const double kPi = 3.14159265358979323846;

// A sine of `hz` at `amplitude`, `seconds` long, the same on both sides.
std::vector<float> sine(double hz, double amplitude, unsigned rate, double seconds)
{
    const size_t frames = static_cast<size_t>(rate * seconds);
    std::vector<float> out(frames * 2);
    for (size_t f = 0; f < frames; ++f) {
        const float v = static_cast<float>(amplitude * std::sin(2.0 * kPi * hz * f / rate));
        out[2 * f] = v;
        out[2 * f + 1] = v;
    }
    return out;
}

// The root mean square of a filter's output over the second half of a sine,
// once it has settled.
double settled_rms(ah::Biquad f, double hz, unsigned rate)
{
    const size_t frames = rate;
    double sum = 0;
    size_t n = 0;
    for (size_t i = 0; i < frames; ++i) {
        const double y = f.run(std::sin(2.0 * kPi * hz * i / rate));
        if (i >= frames / 2) {
            sum += y * y;
            ++n;
        }
    }
    return std::sqrt(sum / n);
}

int peak(const std::vector<int16_t> &samples)
{
    int p = 0;
    for (int16_t s : samples) p = (std::max)(p, std::abs(static_cast<int>(s)));
    return p;
}

std::vector<int16_t> shape(const std::vector<float> &stereo, unsigned rate, int bassHz, int strength)
{
    ah::Shaper shaper;
    shaper.configure(rate, bassHz, strength);
    std::vector<int16_t> out;
    shaper.process(stereo.data(), stereo.size() / 2, &out);
    return out;
}

// One 10 ms chunk of the pad's stream: speaker samples and game haptics.
std::vector<uint8_t> chunk(int16_t speaker, int16_t gameHaptics, size_t frames = 480)
{
    std::vector<uint8_t> pcm(frames * 8);
    for (size_t f = 0; f < frames; ++f) {
        ah::write_s16(&pcm[f * 8 + 0], speaker);
        ah::write_s16(&pcm[f * 8 + 2], speaker);
        ah::write_s16(&pcm[f * 8 + 4], gameHaptics);
        ah::write_s16(&pcm[f * 8 + 6], gameHaptics);
    }
    return pcm;
}

int16_t channel(const std::vector<uint8_t> &pcm, size_t frame, int ch)
{
    return ah::read_s16(&pcm[frame * 8 + ch * 2]);
}

}  // namespace

int run_audio_haptics_dsp_tests()
{
    section("audio haptics: the bass filter keeps bass and takes out the rest");
    {
        const double low = settled_rms(ah::Biquad::make(true, 48000, 150), 50, 48000);
        const double high = settled_rms(ah::Biquad::make(true, 48000, 150), 1000, 48000);
        CTM_CHECK(low > 0.6 && low < 0.8);          // a sine's own RMS is 0.707
        CTM_CHECK(high < 0.035);                    // 1 kHz: down by more than 26 dB
    }

    section("audio haptics: the high-pass takes out a constant");
    {
        ah::Biquad f = ah::Biquad::make(false, 48000, ah::kHighPassHz);
        double y = 1.0;
        for (int i = 0; i < 48000; ++i) y = f.run(0.5);
        CTM_CHECK(std::fabs(y) < 0.001);
    }

    section("audio haptics: silence and hiss stay silent");
    {
        CTM_CHECK_EQ(peak(shape(sine(60, 0.0, 48000, 0.5), 48000, 150, 100)), 0);
        CTM_CHECK_EQ(peak(shape(sine(60, 0.001, 48000, 0.5), 48000, 150, 100)), 0);   // -60 dBFS
        CTM_CHECK(peak(shape(sine(60, 0.3, 48000, 0.5), 48000, 150, 100)) > 10000);    // a bass note
    }

    section("audio haptics: treble alone barely moves the actuators");
    {
        const int bass = peak(shape(sine(60, 0.3, 48000, 0.5), 48000, 150, 100));
        const int treble = peak(shape(sine(3000, 0.3, 48000, 0.5), 48000, 150, 100));
        CTM_CHECK(treble < bass / 20);
    }

    section("audio haptics: strength is obeyed, 0 is nothing");
    {
        const std::vector<float> note = sine(60, 0.05, 48000, 0.5);
        CTM_CHECK_EQ(peak(shape(note, 48000, 150, 0)), 0);
        const int normal = peak(shape(note, 48000, 150, 100));
        const int doubled = peak(shape(note, 48000, 150, 200));
        CTM_CHECK(normal > 0);
        CTM_CHECK(doubled > normal * 17 / 10);      // nearly twice, below the soft clip
    }

    section("audio haptics: never past full scale, and rounded rather than cut");
    {
        CTM_CHECK_EQ(ah::soft_clip(5.0), 1.0);
        CTM_CHECK_EQ(ah::soft_clip(-5.0), -1.0);
        CTM_CHECK(ah::soft_clip(0.5) < 0.5 && ah::soft_clip(0.5) > 0.45);
        CTM_CHECK(ah::soft_clip(0.9) < ah::soft_clip(1.2));
        CTM_CHECK(peak(shape(sine(60, 1.0, 48000, 0.3), 48000, 150, 200)) <= 32767);
    }

    section("audio haptics: the pad's rate from any rate the PC mixes at");
    {
        CTM_CHECK_EQ(shape(sine(60, 0.3, 48000, 1.0), 48000, 150, 100).size() / 2, static_cast<size_t>(48000));
        // In 10 ms pieces, as the capture delivers it: the place between
        // samples must carry over, or a second would come out short or long.
        ah::Shaper shaper;
        shaper.configure(44100, 150, 100);
        const std::vector<float> second = sine(60, 0.3, 44100, 1.0);
        std::vector<int16_t> out;
        for (size_t at = 0; at < 44100; at += 441) shaper.process(second.data() + at * 2, 441, &out);
        const long frames = static_cast<long>(out.size() / 2);
        CTM_CHECK(frames >= 47998 && frames <= 48001);
        const size_t fromDouble = shape(sine(60, 0.3, 96000, 1.0), 96000, 150, 100).size() / 2;
        CTM_CHECK(fromDouble >= 47999 && fromDouble <= 48001);      // a second is a second
    }

    section("audio haptics: replace puts ours in, the sound itself untouched");
    {
        std::vector<uint8_t> pcm = chunk(1234, 5000);
        const std::vector<int16_t> ours(480 * 2, 700);
        ah::YieldState st;
        ah::mix_into(pcm.data(), 480, ours.data(), ah::Mode::Replace, &st);
        CTM_CHECK_EQ(channel(pcm, 100, 2), 700);
        CTM_CHECK_EQ(channel(pcm, 100, 3), 700);
        CTM_CHECK_EQ(channel(pcm, 100, 0), 1234);
        CTM_CHECK_EQ(channel(pcm, 100, 1), 1234);
    }

    section("audio haptics: mix adds the two, and stops at full scale");
    {
        std::vector<uint8_t> pcm = chunk(-20, 1000);
        const std::vector<int16_t> ours(480 * 2, 700);
        ah::YieldState st;
        ah::mix_into(pcm.data(), 480, ours.data(), ah::Mode::Mix, &st);
        CTM_CHECK_EQ(channel(pcm, 7, 2), 1700);
        CTM_CHECK_EQ(channel(pcm, 7, 0), -20);
        std::vector<uint8_t> loud = chunk(0, 30000);
        const std::vector<int16_t> big(480 * 2, 10000);
        ah::mix_into(loud.data(), 480, big.data(), ah::Mode::Mix, &st);
        CTM_CHECK_EQ(channel(loud, 7, 2), 32767);
    }

    section("audio haptics: yield plays ours while the game sends none");
    {
        std::vector<uint8_t> pcm = chunk(0, 0);
        const std::vector<int16_t> ours(480 * 2, 900);
        ah::YieldState st;
        ah::mix_into(pcm.data(), 480, ours.data(), ah::Mode::Yield, &st);
        CTM_CHECK_EQ(channel(pcm, 479, 2), 900);
    }

    section("audio haptics: yield steps aside for the game's own, and comes back after 200 ms");
    {
        const std::vector<int16_t> ours(480 * 2, 900);
        ah::YieldState st;
        std::vector<uint8_t> pcm = chunk(0, 3000);                 // the game's haptics play
        ah::mix_into(pcm.data(), 480, ours.data(), ah::Mode::Yield, &st);
        CTM_CHECK_EQ(channel(pcm, 479, 2), 3000);                  // ours gone by the chunk's end
        CTM_CHECK(channel(pcm, 0, 2) > 3000);                      // faded, not cut: no click
        for (int i = 0; i < 19; ++i) {                             // 190 ms more of a quiet game
            std::vector<uint8_t> quiet = chunk(0, 0);
            ah::mix_into(quiet.data(), 480, ours.data(), ah::Mode::Yield, &st);
            CTM_CHECK_EQ(channel(quiet, 479, 2), 0);               // still stepped aside
        }
        std::vector<uint8_t> after = chunk(0, 0);
        ah::mix_into(after.data(), 480, ours.data(), ah::Mode::Yield, &st);
        std::vector<uint8_t> later = chunk(0, 0);
        ah::mix_into(later.data(), 480, ours.data(), ah::Mode::Yield, &st);
        CTM_CHECK_EQ(channel(later, 479, 2), 900);                 // back once the hold ran out
    }

    section("audio haptics: what waits for a pad is short, in order, and silent when dry");
    {
        ah::Ring ring;
        std::vector<int16_t> out(100 * 2, 7);
        ring.pull(out.data(), 100);
        CTM_CHECK_EQ(out[0], 0);
        CTM_CHECK_EQ(ring.shortfalls(), static_cast<size_t>(1));
        std::vector<int16_t> in(100 * 2);
        for (size_t i = 0; i < in.size(); ++i) in[i] = static_cast<int16_t>(i);
        ring.push(in.data(), 100);
        ring.pull(out.data(), 50);
        CTM_CHECK_EQ(out[0], 0);
        CTM_CHECK_EQ(out[99], 99);
        CTM_CHECK_EQ(ring.frames(), static_cast<size_t>(50));
        std::vector<int16_t> flood(3000 * 2);
        for (size_t i = 0; i < flood.size(); ++i) flood[i] = static_cast<int16_t>(i % 30000);
        ring.push(flood.data(), 3000);
        CTM_CHECK_EQ(ring.frames(), ah::Ring::kTargetFrames);        // back to 10 ms
        CTM_CHECK_EQ(ring.drops(), static_cast<size_t>(1));
        std::vector<int16_t> last(ah::Ring::kTargetFrames * 2);
        ring.pull(last.data(), ah::Ring::kTargetFrames);
        CTM_CHECK_EQ(last.back(), flood.back());                     // the newest kept
    }

    section("audio haptics: surround folds into two sides");
    {
        // 5.1 as Windows orders it: front left, front right, centre, LFE, side left, side right.
        const float frame[6] = { 0.1f, 0.2f, 0.4f, 0.3f, 0.9f, 0.9f };
        std::vector<float> stereo;
        ah::to_stereo(frame, 1, 6, 2, 3, &stereo);
        CTM_CHECK(std::fabs(stereo[0] - (0.1f + 0.7071f * 0.4f + 0.3f)) < 1e-5);
        CTM_CHECK(std::fabs(stereo[1] - (0.2f + 0.7071f * 0.4f + 0.3f)) < 1e-5);
        const float mono[1] = { 0.5f };
        ah::to_stereo(mono, 1, 1, -1, -1, &stereo);
        CTM_CHECK_EQ(stereo[0], 0.5f);
        CTM_CHECK_EQ(stereo[1], 0.5f);
    }
    return 0;
}
