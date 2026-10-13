// PC sound made into DualSense haptics: which pads get it, and when.
//
// audio_haptics_dsp.inl shapes the sound and mixes it in; pc_sound_capture
// captures it. Here:
//   - each bridged DualSense or Edge has a Pad, made by its device; the
//     settings are read from the section the device reads, its linked config
//     or its kind's own;
//   - a manager thread, twice a second, re-reads every pad's settings, keeps
//     the capture running while any pad has it on and stops it when none
//     does, and opens a pad's keep-alive while there is sound to feel,
//     closing it once there has been none for a while;
//   - the capture thread shapes each stretch of the PC's sound for every pad
//     that has it on and leaves it waiting (Ring);
//   - each pad's own sound stream takes what waits and mixes it into
//     channels 2 and 3 (into_stream), wired or Bluetooth alike.
//
// THE SETTINGS, all in the Audio section, which only a DualSense or Edge is
// offered:
//   audio_haptics            on or off (off)
//   audio_haptics_strength   0 to 200, 100 the normal strength
//   audio_haptics_bass_hz    40 to 300 (150): the highest pitch that is felt
//   audio_haptics_mode       yield (default), mix or replace: what happens
//                            while a game sends haptics of its own
//
// ⚠️ The keep-alive makes the pad's stream run, to the TV and over the
// internet for a TV away from home, for as long as there is sound to feel and
// kKeepAliveLinger after; nothing runs while all is quiet.

#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "audio_haptics_dsp.inl"
#include "pc_sound_capture.h"

namespace audio_haptics {

constexpr auto kManagerEvery = std::chrono::milliseconds(500);
constexpr auto kKeepAliveLinger = std::chrono::seconds(5);
constexpr auto kKeepAliveRetry = std::chrono::seconds(3);

// One bridged DualSense or Edge.
struct Pad {
    std::wstring usbInstanceId;              // its virtual device, for the keep-alive
    std::wstring name;                       // for the log
    std::mutex sectionMutex;
    std::string section;                     // set by its device, read by the manager

    std::atomic<bool> on{false};
    std::atomic<int> strength{kStrengthDefault};
    std::atomic<int> bassHz{kBassHzDefault};
    std::atomic<int> mode{static_cast<int>(Mode::Yield)};

    Ring ring;                               // capture thread in, the pad's stream out
    Shaper shaper;                           // capture thread only
    std::vector<int16_t> shaped;             // capture thread only
    YieldState yield;                        // the pad's stream only
    std::atomic<long long> lastSoundMs{0};   // when the gate was last open, steady ms

    void *keepAlive = nullptr;               // manager thread only
    std::chrono::steady_clock::time_point lastTry{};
    bool loggedOn = false;                   // manager thread only
};

inline std::mutex g_mutex;
inline std::vector<std::shared_ptr<Pad>> g_pads;
// Held for a whole round of the manager: a pad leaving waits for it, so its
// keep-alive is never closed by two threads at once.
inline std::mutex g_round;
inline std::atomic<bool> g_managerRun{false};
inline std::thread g_manager;

inline long long now_ms()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

inline Mode mode_named(const std::string &text)
{
    if (text == "mix") return Mode::Mix;
    if (text == "replace") return Mode::Replace;
    return Mode::Yield;
}

inline const char *mode_name(int mode)
{
    return mode == static_cast<int>(Mode::Mix) ? "mix" : mode == static_cast<int>(Mode::Replace) ? "replace" : "yield";
}

// The capture thread's work: one stretch of the PC's sound, for every pad
// that has it on.
inline void on_block(const pc_sound_capture::Block &block)
{
    std::vector<std::shared_ptr<Pad>> pads;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        pads = g_pads;
    }
    if (pads.empty() || block.frames == 0) return;
    static thread_local std::vector<float> stereo;
    to_stereo(block.samples, block.frames, block.channels, block.centre, block.lfe, &stereo);
    for (const std::shared_ptr<Pad> &pad : pads) {
        if (!pad->on.load()) continue;
        pad->shaper.configure(block.rate, pad->bassHz.load(), pad->strength.load());
        pad->shaped.clear();
        pad->shaper.process(stereo.data(), block.frames, &pad->shaped);
        if (pad->shaper.gate_open()) pad->lastSoundMs = now_ms();
        if (!pad->shaped.empty()) pad->ring.push(pad->shaped.data(), pad->shaped.size() / 2);
    }
}

// A pad's own sound stream, as it goes by: what waits is mixed into its
// channels 2 and 3. ⓘ Called for each transfer, about 100 a second.
inline void into_stream(Pad &pad, std::vector<uint8_t> *pcm)
{
    const size_t frames = pcm->size() / (kPadChannels * sizeof(int16_t));
    if (frames == 0) return;
    static thread_local std::vector<int16_t> haptics;
    haptics.resize(frames * 2);
    pad.ring.pull(haptics.data(), frames);
    mix_into(pcm->data(), frames, haptics.data(), static_cast<Mode>(pad.mode.load()), &pad.yield);
}

inline void log_line(const std::wstring &text)
{
    device_log::session_w() << text;
}

// The manager's round: settings, the capture, and each pad's keep-alive.
inline void manage_once()
{
    std::lock_guard<std::mutex> round(g_round);
    std::vector<std::shared_ptr<Pad>> pads;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        pads = g_pads;
    }
    bool anyOn = false;
    for (const std::shared_ptr<Pad> &pad : pads) {
        std::string section;
        {
            std::lock_guard<std::mutex> lock(pad->sectionMutex);
            section = pad->section;
        }
        const char *s = section.empty() ? nullptr : section.c_str();
        const bool on = device_config_bool(s, "audio_haptics", false);
        const int strength = (std::max)(0, (std::min)(kStrengthMax, device_config_int(s, "audio_haptics_strength", kStrengthDefault)));
        const int bass = (std::max)(kBassHzMin, (std::min)(kBassHzMax, device_config_int(s, "audio_haptics_bass_hz", kBassHzDefault)));
        const int mode = static_cast<int>(mode_named(device_config_str(s, "audio_haptics_mode")));
        const bool changed = on != pad->on.load() || strength != pad->strength.load() || bass != pad->bassHz.load() ||
                             mode != pad->mode.load();
        pad->strength = strength;
        pad->bassHz = bass;
        pad->mode = mode;
        pad->on = on;
        if (changed || !pad->loggedOn) {
            device_log::session_w() << L"audio haptics: " << pad->name << L" "
                                    << (on ? L"on" : L"off") << L", strength " << strength << L", bass "
                                    << bass << L" Hz, " << mode_name(mode) << L" (section " << std::wstring(section.begin(), section.end()) << L")";
            pad->loggedOn = true;
        }
        anyOn = anyOn || on;

        // The keep-alive: open while there is sound to feel, closed once
        // there has been none for kKeepAliveLinger, or when it is turned off.
        const auto now = std::chrono::steady_clock::now();
        const bool sounding = on && now_ms() - pad->lastSoundMs.load() < std::chrono::duration_cast<std::chrono::milliseconds>(kKeepAliveLinger).count();
        if (sounding && pad->keepAlive == nullptr && now - pad->lastTry >= kKeepAliveRetry) {
            pad->lastTry = now;
            pad->keepAlive = pc_sound_capture::keep_alive_open(pad->usbInstanceId, log_line);
        } else if (!sounding && pad->keepAlive != nullptr) {
            pc_sound_capture::keep_alive_close(pad->keepAlive);
            pad->keepAlive = nullptr;
            device_log::session_w() << L"audio haptics: " << pad->name << L" quiet, its keep-alive closed";
        }
        if (!on) {
            pad->ring.clear();
        }
    }
    if (anyOn && !pc_sound_capture::running()) {
        pc_sound_capture::start(on_block, log_line);
    } else if (!anyOn && pc_sound_capture::running()) {
        pc_sound_capture::stop();
        device_log::session_w() << L"audio haptics: no pad has it on; the PC's sound is no longer captured";
    }
}

inline void manager_loop()
{
    while (g_managerRun.load()) {
        manage_once();
        for (int i = 0; i < 10 && g_managerRun.load(); ++i) std::this_thread::sleep_for(kManagerEvery / 10);
    }
}

// A pad joins: its virtual device and the section it reads.
inline std::shared_ptr<Pad> attach(const std::wstring &usbInstanceId, const std::wstring &name, const std::string &section)
{
    auto pad = std::make_shared<Pad>();
    pad->usbInstanceId = usbInstanceId;
    pad->name = name;
    pad->section = section;
    std::lock_guard<std::mutex> lock(g_mutex);
    g_pads.push_back(pad);
    if (!g_managerRun.exchange(true)) {
        if (g_manager.joinable()) g_manager.join();
        g_manager = std::thread(manager_loop);
    }
    return pad;
}

// Its device's linked config changed, and so the section it reads.
inline void set_section(Pad &pad, const std::string &section)
{
    std::lock_guard<std::mutex> lock(pad.sectionMutex);
    pad.section = section;
}

// A pad leaves: its keep-alive closed, and the manager and the capture
// stopped with the last of them.
inline void detach(const std::shared_ptr<Pad> &pad)
{
    if (!pad) return;
    bool last = false;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_pads.erase(std::remove(g_pads.begin(), g_pads.end(), pad), g_pads.end());
        last = g_pads.empty();
        if (last) g_managerRun = false;
    }
    if (last) {
        if (g_manager.joinable()) g_manager.join();
        pc_sound_capture::stop();
    }
    std::lock_guard<std::mutex> round(g_round);
    if (pad->keepAlive != nullptr) {
        pc_sound_capture::keep_alive_close(pad->keepAlive);
        pad->keepAlive = nullptr;
    }
}

// `DS5-USBIP audio-haptics-probe [seconds]`: what the PC plays and what a pad
// would feel, a line a second, from the same capture and the same shaping
// (bass 150 Hz, strength 100). ⓘ Plays nothing, opens no pad, writes no log:
// it runs with no listener and no folder of its own.
inline int probe_mode(int seconds)
{
    if (seconds <= 0) seconds = 10;
    struct Stats {
        std::mutex mutex;
        Shaper shaper;
        std::vector<int16_t> shaped;
        unsigned rate = 0, channels = 0;
        size_t frames = 0, open = 0;
        float inPeak = 0.0f;
        int outPeak = 0;
    };
    auto stats = std::make_shared<Stats>();
    pc_sound_capture::start(
        [stats](const pc_sound_capture::Block &block) {
            static thread_local std::vector<float> stereo;
            to_stereo(block.samples, block.frames, block.channels, block.centre, block.lfe, &stereo);
            std::lock_guard<std::mutex> lock(stats->mutex);
            stats->rate = block.rate;
            stats->channels = block.channels;
            for (float v : stereo) stats->inPeak = (std::max)(stats->inPeak, std::fabs(v));
            stats->shaper.configure(block.rate, kBassHzDefault, kStrengthDefault);
            stats->shaped.clear();
            stats->shaper.process(stereo.data(), block.frames, &stats->shaped);
            for (int16_t s : stats->shaped) stats->outPeak = (std::max)(stats->outPeak, std::abs(static_cast<int>(s)));
            stats->frames += block.frames;
            if (stats->shaper.gate_open()) stats->open += block.frames;
        },
        [](const std::wstring &line) { std::wcout << line << L"\n"; });
    auto db = [](double fraction) { return fraction <= 0.0 ? -120.0 : 20.0 * std::log10(fraction); };
    for (int second = 1; second <= seconds; ++second) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::lock_guard<std::mutex> lock(stats->mutex);
        wchar_t line[200];
        std::swprintf(line, 200, L"%2d s: %u Hz, %u ch, %zu frames; sound peak %.1f dBFS; felt peak %.1f dBFS; felt %.0f%% of the time",
                      second, stats->rate, stats->channels, stats->frames, db(stats->inPeak), db(stats->outPeak / 32767.0),
                      stats->frames ? 100.0 * stats->open / stats->frames : 0.0);
        std::wcout << line << L"\n";
        stats->frames = stats->open = 0;
        stats->inPeak = 0.0f;
        stats->outPeak = 0;
    }
    pc_sound_capture::stop();
    return 0;
}

}  // namespace audio_haptics
