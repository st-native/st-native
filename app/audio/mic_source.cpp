// SPDX-License-Identifier: GPL-3.0-or-later
#include "mic_source.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdlib>

#include "audio_devices.hpp"

namespace st {
namespace {

class SdlMicCaptureBackend final : public MicCaptureBackend {
public:
    bool open(std::string_view device_name, int sample_rate) override {
        close();
        bool any_available = false;
        const SDL_AudioDeviceID id = resolve_recording_device(device_name, &any_available);
        if (!any_available || id == 0) return false;
        const SDL_AudioSpec spec = {SDL_AUDIO_S16, 1, sample_rate};
        stream_.reset(SDL_OpenAudioDeviceStream(id, &spec, nullptr, nullptr));
        if (!stream_ && !device_name.empty()) {
            stream_.reset(SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_RECORDING, &spec, nullptr, nullptr));
        }
        if (!stream_) return false;
        if (!SDL_ResumeAudioStreamDevice(stream_.get())) {
            stream_.reset();
            return false;
        }
        return true;
    }

    void close() override { stream_.reset(); }

    bool is_open() const override { return stream_ != nullptr; }

    size_t read(int16_t* out, size_t max_samples) override {
        if (!stream_ || max_samples == 0) return 0;
        // Bound capture latency (~25 ms): drop stale buildup if the game paused sampling briefly.
        constexpr int max_queued_bytes = (BreathSynth::sample_rate / 40) * int(sizeof(int16_t));
        if (SDL_GetAudioStreamAvailable(stream_.get()) > max_queued_bytes * 2) {
            int16_t discard[256];
            while (SDL_GetAudioStreamAvailable(stream_.get()) > max_queued_bytes) {
                if (SDL_GetAudioStreamData(stream_.get(), discard, sizeof(discard)) <= 0) break;
            }
        }
        const int got = SDL_GetAudioStreamData(stream_.get(), out, int(max_samples * sizeof(int16_t)));
        if (got <= 0) return 0;
        return size_t(got) / sizeof(int16_t);
    }

    void clear() override {
        if (stream_) SDL_ClearAudioStream(stream_.get());
    }

private:
    std::unique_ptr<SDL_AudioStream, decltype(&SDL_DestroyAudioStream)> stream_{nullptr, SDL_DestroyAudioStream};
};

}  // namespace

std::unique_ptr<MicCaptureBackend> create_sdl_mic_backend() {
    return std::make_unique<SdlMicCaptureBackend>();
}

MicSource::MicSource(std::unique_ptr<MicCaptureBackend> backend)
    : backend_(backend ? std::move(backend) : create_sdl_mic_backend()) {}

MicSource::~MicSource() {
    std::lock_guard lock(mutex_);
    close_device_locked();
}

void MicSource::set_settings(const AudioSettings& settings) {
    std::lock_guard lock(mutex_);
    const bool reopen = game_listening_ && !paused_ &&
                        (settings.mic_mode != settings_.mic_mode || settings.input_device != settings_.input_device);
    settings_ = settings;
    if (reopen) {
        close_device_locked();
        open_session_locked();
    }
}

void MicSource::set_paused(bool paused) {
    std::lock_guard lock(mutex_);
    if (paused_ == paused) return;
    paused_ = paused;
    if (paused_) {
        close_device_locked();
    } else if (game_listening_) {
        open_session_locked();
    }
}

void MicSource::set_log(FILE* log, const uint64_t* frame_counter) {
    std::lock_guard lock(mutex_);
    log_ = log;
    frame_counter_ = frame_counter;
}

void MicSource::on_start() {
    std::lock_guard lock(mutex_);
    game_listening_ = true;
    session_peak_ = 0;
    session_samples_ = 0;
    if (!paused_) open_session_locked();
    if (log_) {
        std::fprintf(log_, "frame %llu: mic_start mode=%s real=%d virtual=%d\n",
                     (unsigned long long)current_frame(), mic_mode_value(settings_.mic_mode),
                     int(session_uses_real_), int(session_allows_virtual_));
        std::fflush(log_);
    }
}

void MicSource::on_stop() {
    std::lock_guard lock(mutex_);
    if (log_ && game_listening_) {
        std::fprintf(log_, "frame %llu: mic_stop samples=%llu peak=%d\n",
                     (unsigned long long)current_frame(), (unsigned long long)session_samples_, int(session_peak_));
        std::fflush(log_);
    }
    game_listening_ = false;
    session_uses_real_ = false;
    close_device_locked();
    synth_.reset();
}

size_t MicSource::on_read(int16_t* samples, size_t max_samples) {
    if (!samples || max_samples == 0) return 0;
    std::lock_guard lock(mutex_);
    if (!game_listening_ || paused_ || settings_.mic_mode == MicMode::off) {
        std::fill_n(samples, max_samples, int16_t(0));
        return max_samples;
    }

    const bool want_blow = blowing_.load() && session_allows_virtual_;
    if (want_blow || synth_.active()) {
        // ADR-0006: while virtual blow is active (including its release tail), replace real mic rather than summing.
        if (session_uses_real_ && backend_) backend_->clear();
        synth_.generate(samples, max_samples, want_blow, settings_.mic_gain);
    } else if (!session_uses_real_) {
        std::fill_n(samples, max_samples, int16_t(0));
    } else {
        const size_t got = backend_ ? backend_->read(samples, max_samples) : 0;
        const int gain = clamp_mic_gain(settings_.mic_gain);
        for (size_t i = 0; i < got; ++i) {
            const int scaled = (int(samples[i]) * gain) / 100;
            samples[i] = int16_t(std::clamp(scaled, -32768, 32767));
        }
        if (got < max_samples) std::fill(samples + got, samples + max_samples, int16_t(0));
    }

    session_samples_ += max_samples;
    for (size_t i = 0; i < max_samples; ++i) {
        const int32_t mag = std::abs(int32_t(samples[i]));
        if (mag > session_peak_) session_peak_ = mag;
    }
    return max_samples;
}

bool MicSource::device_open() const {
    std::lock_guard lock(mutex_);
    return backend_ && backend_->is_open();
}

void MicSource::open_session_locked() {
    session_uses_real_ = false;
    session_allows_virtual_ = mic_allows_virtual_blow(settings_.mic_mode);
    if (mic_uses_device(settings_.mic_mode) && backend_) {
        session_uses_real_ = backend_->open(settings_.input_device, BreathSynth::sample_rate);
    }
}

void MicSource::close_device_locked() {
    if (backend_) backend_->close();
}

}  // namespace st
