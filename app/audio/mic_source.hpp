// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

#include "audio_settings.hpp"

namespace st {

// Deterministic band-limited breath generator (~200 Hz - 3.2 kHz at 47,605 Hz) with 30 ms attack
// and 80 ms release (ADR-0006, CONTROLS_AND_MICROPHONE.md §6.3). Contains no game data or assets.
class BreathSynth {
public:
    static constexpr int sample_rate = 47605;

    void reset() {
        rng_ = 0x6d2b79f5u;
        envelope_ = 0.f;
        lp_ = 0.f;
        hp_in_ = 0.f;
        hp_out_ = 0.f;
        mod_phase_ = 0.f;
    }

    bool active() const { return envelope_ > 0.f; }

    void generate(int16_t* out, size_t count, bool blowing, int gain_percent) {
        const float gain = float(clamp_mic_gain(gain_percent)) / 100.f;
        constexpr float attack_step = 1.f / (0.030f * float(sample_rate));
        constexpr float release_step = 1.f / (0.080f * float(sample_rate));
        constexpr float mod_step = 6.5f / float(sample_rate);
        for (size_t i = 0; i < count; ++i) {
            if (blowing) envelope_ = std::min(1.f, envelope_ + attack_step);
            else envelope_ = std::max(0.f, envelope_ - release_step);

            if (envelope_ <= 0.f || gain <= 0.f) {
                out[i] = 0;
                continue;
            }
            // xorshift32 -> uniform [-1, 1]
            uint32_t x = rng_;
            x ^= x << 13;
            x ^= x >> 17;
            x ^= x << 5;
            rng_ = x;
            const float white = (float(int32_t(x >> 8)) - 8388608.f) / 8388608.f;

            // High-pass (~200 Hz) followed by two-stage low-pass (~3.2 kHz) for breath turbulence.
            const float hp = 0.974f * (hp_out_ + white - hp_in_);
            hp_in_ = white;
            hp_out_ = hp;
            lp_ += 0.34f * (hp - lp_);

            // Smooth triangle-parabolic modulation (~6.5 Hz, +/- 10%) so steady blows look natural.
            mod_phase_ += mod_step;
            if (mod_phase_ >= 1.f) mod_phase_ -= 1.f;
            const float tri = mod_phase_ < 0.5f ? (4.f * mod_phase_ - 1.f) : (3.f - 4.f * mod_phase_);
            const float flutter = 0.90f + 0.10f * tri * (2.f - std::abs(tri));

            const float sample = lp_ * envelope_ * flutter * gain * 26000.f;
            out[i] = int16_t(std::clamp(int(sample), -30000, 30000));
        }
    }

private:
    uint32_t rng_ = 0x6d2b79f5u;
    float envelope_ = 0.f;
    float lp_ = 0.f;
    float hp_in_ = 0.f;
    float hp_out_ = 0.f;
    float mod_phase_ = 0.f;
};

class MicCaptureBackend {
public:
    virtual ~MicCaptureBackend() = default;
    virtual bool open(std::string_view device_name, int sample_rate) = 0;
    virtual void close() = 0;
    virtual bool is_open() const = 0;
    virtual size_t read(int16_t* out, size_t max_samples) = 0;
    virtual void clear() = 0;
};

std::unique_ptr<MicCaptureBackend> create_sdl_mic_backend();

class MicSource {
public:
    explicit MicSource(std::unique_ptr<MicCaptureBackend> backend = nullptr);
    ~MicSource();

    void set_settings(const AudioSettings& settings);
    void set_blowing(bool blowing) { blowing_.store(blowing); }
    void set_paused(bool paused);
    void set_log(FILE* log, const uint64_t* frame_counter);

    void on_start();
    void on_stop();
    size_t on_read(int16_t* samples, size_t max_samples);

    bool device_open() const;

private:
    void open_session_locked();
    void close_device_locked();
    uint64_t current_frame() const { return frame_counter_ ? *frame_counter_ : 0; }

    mutable std::mutex mutex_;
    std::unique_ptr<MicCaptureBackend> backend_;
    AudioSettings settings_{};
    BreathSynth synth_{};
    std::atomic<bool> blowing_{false};
    bool game_listening_ = false;
    bool paused_ = false;
    bool session_uses_real_ = false;
    bool session_allows_virtual_ = true;
    FILE* log_ = nullptr;
    const uint64_t* frame_counter_ = nullptr;
    int32_t session_peak_ = 0;
    uint64_t session_samples_ = 0;
};

}  // namespace st
