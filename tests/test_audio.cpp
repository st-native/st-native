// SPDX-License-Identifier: GPL-3.0-or-later
#include "../app/audio/audio_devices.hpp"
#include "../app/audio/audio_settings.hpp"
#include "../app/audio/mic_source.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace {

class FakeMicBackend final : public st::MicCaptureBackend {
public:
    bool available = true;
    bool open_state = false;
    bool cleared = false;
    int open_calls = 0;
    std::string last_device;
    std::vector<int16_t> queued;

    bool open(std::string_view device_name, int) override {
        ++open_calls;
        last_device = std::string(device_name);
        if (!available) return false;
        open_state = true;
        return true;
    }

    void close() override { open_state = false; }
    bool is_open() const override { return open_state; }

    size_t read(int16_t* out, size_t max_samples) override {
        if (!open_state) return 0;
        const size_t n = std::min(max_samples, queued.size());
        std::copy_n(queued.begin(), n, out);
        queued.erase(queued.begin(), queued.begin() + std::ptrdiff_t(n));
        return n;
    }

    void clear() override {
        cleared = true;
        queued.clear();
    }
};

}  // namespace

int main() {
    // 1. AudioSettings helpers and gain rules
    st::AudioSettings s;
    s.volume = 75;
    s.mute_unfocused = false;
    if (std::abs(st::effective_output_gain(s, true) - 0.75f) > 1e-6f) return 1;
    if (std::abs(st::effective_output_gain(s, false) - 0.75f) > 1e-6f) return 1;
    s.mute_unfocused = true;
    if (std::abs(st::effective_output_gain(s, true) - 0.75f) > 1e-6f) return 2;
    if (st::effective_output_gain(s, false) != 0.f) return 2;

    for (const auto& [choice, key] : st::kAudioInterpolationOptions)
        if (st::parse_audio_interpolation(key) != choice ||
            std::string_view(st::audio_interpolation_value(choice)) != key) return 3;
    for (const auto& [choice, key] : st::kMicModeOptions)
        if (st::parse_mic_mode(key) != choice || std::string_view(st::mic_mode_value(choice)) != key) return 4;

    // 2. BreathSynth determinism, envelope and silence
    st::BreathSynth a, b;
    std::array<int16_t, 2400> buf_a{}, buf_b{};
    a.generate(buf_a.data(), buf_a.size(), false, 100);
    if (std::any_of(buf_a.begin(), buf_a.end(), [](int16_t v) { return v != 0; }) || a.active()) return 5;

    a.reset();
    b.reset();
    a.generate(buf_a.data(), buf_a.size(), true, 100);
    b.generate(buf_b.data(), buf_b.size(), true, 100);
    if (buf_a != buf_b || !a.active()) return 6;
    int32_t peak = 0;
    for (int16_t v : buf_a) peak = std::max<int32_t>(peak, std::abs(int32_t(v)));
    if (peak < 4000 || peak > 32768) return 7;
    // A few large peaks alone do not establish a usable blow. Once the attack settles, require
    // sustained energy and both polarities; the old quiet band-pass signal failed the real lesson.
    a.generate(buf_a.data(), buf_a.size(), true, 100);
    b.generate(buf_b.data(), buf_b.size(), true, 100);
    double square_sum = 0, mean = 0;
    for (const int16_t value : buf_a) { square_sum += double(value) * value; mean += value; }
    if (buf_a != buf_b || std::sqrt(square_sum / buf_a.size()) < 32000 ||
        std::abs(mean / buf_a.size()) > 3000) return 27;
    st::BreathSynth zero_gain, high_gain;
    zero_gain.generate(buf_b.data(), buf_b.size(), true, 0);
    if (std::any_of(buf_b.begin(), buf_b.end(), [](int16_t value) { return value != 0; })) return 28;
    high_gain.generate(buf_b.data(), buf_b.size(), true, 200);
    if (std::find(buf_b.begin(), buf_b.end(), int16_t(32767)) == buf_b.end() ||
        std::find(buf_b.begin(), buf_b.end(), int16_t(-32768)) == buf_b.end()) return 29;

    // Release decays back to exact silence after ~80 ms (~3808 samples)
    std::array<int16_t, 4200> release_buf{};
    a.generate(release_buf.data(), release_buf.size(), false, 100);
    if (a.active()) return 8;
    a.generate(buf_a.data(), 256, false, 100);
    if (std::any_of(buf_a.begin(), buf_a.begin() + 256, [](int16_t v) { return v != 0; })) return 8;

    // 3. MicSource lifecycle & ADR-0006 arbitration
    auto fake_ptr = std::make_unique<FakeMicBackend>();
    auto* fake = fake_ptr.get();
    st::MicSource mic(std::move(fake_ptr));

    // Device stays closed until on_start()
    st::AudioSettings cfg;
    cfg.mic_mode = st::MicMode::hybrid;
    cfg.input_device = "Studio Mic";
    cfg.mic_gain = 150;
    mic.set_settings(cfg);
    if (mic.device_open() || fake->open_calls != 0) return 9;

    // Real mic capture with 150% gain + underrun zero-padding
    fake->queued = {1000, -2000, 30000};
    mic.on_start();
    if (!mic.device_open() || fake->last_device != "Studio Mic") return 10;
    std::array<int16_t, 5> read_buf{99, 99, 99, 99, 99};
    if (mic.on_read(read_buf.data(), read_buf.size()) != 5) return 11;
    if (read_buf[0] != 1500 || read_buf[1] != -3000 || read_buf[2] != 32767 || read_buf[3] != 0 || read_buf[4] != 0)
        return 12;

    // Virtual blow replaces real mic (never sums) and clears queued real samples
    fake->queued = {12000, 12000, 12000, 12000};
    fake->cleared = false;
    mic.set_blowing(true);
    std::array<int16_t, 512> blow_buf{};
    mic.on_read(blow_buf.data(), blow_buf.size());
    if (!fake->cleared || !fake->queued.empty()) return 13;
    if (std::all_of(blow_buf.begin(), blow_buf.end(), [](int16_t v) { return v == 0; })) return 13;
    mic.set_blowing(false);

    // Pause closes physical device; resume reopens while game is still listening
    mic.set_paused(true);
    if (mic.device_open()) return 14;
    mic.set_paused(false);
    if (!mic.device_open()) return 14;

    // on_stop() closes physical device immediately
    mic.on_stop();
    if (mic.device_open()) return 15;

    // Virtual-only and Off never open the physical microphone
    const int opens_before = fake->open_calls;
    cfg.mic_mode = st::MicMode::virtual_blow;
    mic.set_settings(cfg);
    mic.on_start();
    if (mic.device_open() || fake->open_calls != opens_before) return 16;
    mic.on_stop();

    cfg.mic_mode = st::MicMode::off;
    mic.set_settings(cfg);
    mic.set_blowing(true);
    mic.on_start();
    mic.on_read(blow_buf.data(), blow_buf.size());
    if (mic.device_open() || std::any_of(blow_buf.begin(), blow_buf.end(), [](int16_t v) { return v != 0; })) return 17;
    mic.on_stop();

    // 4. Diagnostic reads are destructive only to counters, never to the delivered signal/envelope.
    st::MicSource measured(std::make_unique<FakeMicBackend>()), reference(std::make_unique<FakeMicBackend>());
    cfg.mic_mode = st::MicMode::virtual_blow;
    cfg.mic_gain = 100;
    for (auto* source : {&measured, &reference}) {
        source->set_settings(cfg);
        source->on_start();
        source->set_blowing(true);
    }
    measured.on_read(buf_a.data(), buf_a.size());
    reference.on_read(buf_b.data(), buf_b.size());
    auto stats = measured.take_read_diagnostics();
    if (buf_a != buf_b || stats.reads != 1 || stats.samples != buf_a.size() || stats.peak < 4000 ||
        !stats.listening || stats.paused || !stats.blowing) return 21;
    stats = measured.take_read_diagnostics();
    if (stats.reads || stats.samples || stats.peak || !stats.listening) return 22;
    measured.set_blowing(false);
    reference.set_blowing(false);
    std::array<int16_t, 4200> measured_release{}, reference_release{};
    measured.on_read(measured_release.data(), measured_release.size());
    reference.on_read(reference_release.data(), reference_release.size());
    if (measured_release != reference_release) return 23;
    measured.take_read_diagnostics();
    measured.on_read(buf_a.data(), buf_a.size());
    reference.on_read(buf_b.data(), buf_b.size());
    stats = measured.take_read_diagnostics();
    if (buf_a != buf_b || stats.reads != 1 || stats.samples != buf_a.size() || stats.peak || stats.blowing) return 24;
    measured.set_paused(true);
    measured.on_read(buf_a.data(), buf_a.size());
    stats = measured.take_read_diagnostics();
    if (stats.reads != 1 || stats.samples != buf_a.size() || stats.peak || !stats.paused) return 25;
    measured.on_stop();
    if (measured.take_read_diagnostics().listening) return 26;
    reference.on_stop();

    // 5. Device enumeration must preserve SDL's output count across all C++ argument evaluation orders
    if (SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        int raw_play = 0;
        if (SDL_AudioDeviceID* raw = SDL_GetAudioPlaybackDevices(&raw_play)) {
            SDL_free(raw);
            const auto names = st::list_playback_devices();
            if (raw_play > 0 && names.empty()) {
                SDL_QuitSubSystem(SDL_INIT_AUDIO);
                return 18;
            }
            if (!names.empty() && st::resolve_playback_device(names.front()) == SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK) {
                SDL_QuitSubSystem(SDL_INIT_AUDIO);
                return 19;
            }
        }
        int raw_rec = 0;
        if (SDL_AudioDeviceID* raw = SDL_GetAudioRecordingDevices(&raw_rec)) {
            SDL_free(raw);
            const auto names = st::list_recording_devices();
            if (raw_rec > 0 && names.empty()) {
                SDL_QuitSubSystem(SDL_INIT_AUDIO);
                return 20;
            }
        }
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }

    return 0;
}
