// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <utility>

namespace st {

enum class AudioInterpolation { none, linear, cosine, cubic, gaussian };

inline constexpr std::array kAudioInterpolationOptions{
    std::pair{AudioInterpolation::none, "none"},
    std::pair{AudioInterpolation::linear, "linear"},
    std::pair{AudioInterpolation::cosine, "cosine"},
    std::pair{AudioInterpolation::cubic, "cubic"},
    std::pair{AudioInterpolation::gaussian, "gaussian"},
};

constexpr AudioInterpolation parse_audio_interpolation(std::string_view value) {
    for (const auto& [choice, key] : kAudioInterpolationOptions)
        if (value == key) return choice;
    return AudioInterpolation::none;
}

constexpr const char* audio_interpolation_value(AudioInterpolation choice) {
    for (const auto& [item, key] : kAudioInterpolationOptions)
        if (item == choice) return key;
    return "none";
}

constexpr bool valid_audio_interpolation(std::string_view value) {
    for (const auto& [_, key] : kAudioInterpolationOptions)
        if (value == key) return true;
    return false;
}

enum class MicMode { automatic, real, virtual_blow, hybrid, off };

inline constexpr std::array kMicModeOptions{
    std::pair{MicMode::automatic, "automatic"},
    std::pair{MicMode::real, "real"},
    std::pair{MicMode::virtual_blow, "virtual"},
    std::pair{MicMode::hybrid, "hybrid"},
    std::pair{MicMode::off, "off"},
};

constexpr MicMode parse_mic_mode(std::string_view value) {
    for (const auto& [choice, key] : kMicModeOptions)
        if (value == key) return choice;
    return MicMode::automatic;
}

constexpr const char* mic_mode_value(MicMode choice) {
    for (const auto& [item, key] : kMicModeOptions)
        if (item == choice) return key;
    return "automatic";
}

constexpr bool valid_mic_mode(std::string_view value) {
    for (const auto& [_, key] : kMicModeOptions)
        if (value == key) return true;
    return false;
}

constexpr int clamp_volume(int volume) { return std::clamp(volume, 0, 100); }
constexpr int clamp_mic_gain(int gain) { return std::clamp(gain, 0, 200); }

constexpr bool mic_uses_device(MicMode mode) {
    return mode == MicMode::automatic || mode == MicMode::real || mode == MicMode::hybrid;
}

constexpr bool mic_allows_virtual_blow(MicMode mode) {
    return mode == MicMode::automatic || mode == MicMode::virtual_blow || mode == MicMode::hybrid;
}

struct AudioSettings {
    int volume = 100;
    bool mute_unfocused = false;
    AudioInterpolation interpolation = AudioInterpolation::none;
    std::string output_device;
    MicMode mic_mode = MicMode::automatic;
    std::string input_device;
    int mic_gain = 100;

    bool operator==(const AudioSettings&) const = default;
};

constexpr float effective_output_gain(const AudioSettings& settings, bool window_focused) {
    if (settings.mute_unfocused && !window_focused) return 0.f;
    return float(clamp_volume(settings.volume)) / 100.f;
}

}  // namespace st
