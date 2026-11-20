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

// Deterministic bipolar noise at the mic's 47,605 Hz rate, with 30 ms attack and 80 ms release.
// Blow detection needs sustained energy: the former low-level band-pass noise reached the ADC but
// failed Gage's initial lesson. No recorded samples or game assets are used (ADR-0006).
class BreathSynth {
public:
    static constexpr int sample_rate = 47605;

    void reset() {
        rng_ = 0x6d2b79f5u;
        envelope_ = 0.f;
    }

    bool active() const { return envelope_ > 0.f; }

    void generate(int16_t* out, size_t count, bool blowing, int gain_percent) {
        const float gain = float(clamp_mic_gain(gain_percent)) / 100.f;
        constexpr float attack_step = 1.f / (0.030f * float(sample_rate));
        constexpr float release_step = 1.f / (0.080f * float(sample_rate));
        for (size_t i = 0; i < count; ++i) {
            if (blowing) envelope_ = std::min(1.f, envelope_ + attack_step);
            else envelope_ = std::max(0.f, envelope_ - release_step);

            if (envelope_ <= 0.f || gain <= 0.f) {
                out[i] = 0;
                continue;
            }
            // xorshift32 supplies the polarity; a near-zero DC mean and high sustained energy
            // distinguish blowing from silence without selecting a pipe or a song.
            uint32_t x = rng_;
            x ^= x << 13;
            x ^= x >> 17;
            x ^= x << 5;
            rng_ = x;
            const float polarity = (x & 0x80000000u) ? 32767.f : -32768.f;
            const float sample = polarity * envelope_ * gain;
            out[i] = int16_t(std::clamp(int(sample), -32768, 32767));
        }
    }

private:
    uint32_t rng_ = 0x6d2b79f5u;
    float envelope_ = 0.f;
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

// Numeric diagnostics since the last snapshot; never retains audio samples.
struct MicReadDiagnostics {
    uint64_t reads = 0, samples = 0;
    int32_t peak = 0;
    bool listening = false, paused = false, blowing = false;
};

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
    MicReadDiagnostics take_read_diagnostics();

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
    uint64_t diagnostic_reads_ = 0, diagnostic_samples_ = 0;
    int32_t diagnostic_peak_ = 0;
};

}  // namespace st
