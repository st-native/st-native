// SPDX-License-Identifier: GPL-3.0-or-later
#include "audio_devices.hpp"

#include <algorithm>
#include <memory>

namespace st {
namespace {

std::vector<std::string> collect_device_names(SDL_AudioDeviceID* raw_ids, int count) {
    std::unique_ptr<SDL_AudioDeviceID, decltype(&SDL_free)> ids(raw_ids, SDL_free);
    std::vector<std::string> names;
    if (!ids || count <= 0) return names;
    for (int i = 0; i < count; ++i) {
        const char* raw = SDL_GetAudioDeviceName(ids.get()[i]);
        if (!raw || !*raw) continue;
        std::string name(raw);
        if (std::find(names.begin(), names.end(), name) == names.end()) names.push_back(std::move(name));
    }
    return names;
}

}  // namespace

std::vector<std::string> list_playback_devices() {
    int count = 0;
    SDL_AudioDeviceID* ids = SDL_GetAudioPlaybackDevices(&count);
    return collect_device_names(ids, count);
}

std::vector<std::string> list_recording_devices() {
    int count = 0;
    SDL_AudioDeviceID* ids = SDL_GetAudioRecordingDevices(&count);
    return collect_device_names(ids, count);
}

SDL_AudioDeviceID resolve_playback_device(std::string_view wanted_name) {
    if (wanted_name.empty()) return SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
    int count = 0;
    std::unique_ptr<SDL_AudioDeviceID, decltype(&SDL_free)> ids(SDL_GetAudioPlaybackDevices(&count), SDL_free);
    if (!ids || count <= 0) return SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
    for (int i = 0; i < count; ++i) {
        const char* name = SDL_GetAudioDeviceName(ids.get()[i]);
        if (name && wanted_name == name) return ids.get()[i];
    }
    return SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
}

SDL_AudioDeviceID resolve_recording_device(std::string_view wanted_name, bool* any_available) {
    int count = 0;
    std::unique_ptr<SDL_AudioDeviceID, decltype(&SDL_free)> ids(SDL_GetAudioRecordingDevices(&count), SDL_free);
    const bool has_any = ids && count > 0;
    if (any_available) *any_available = has_any;
    if (!has_any) return 0;
    if (!wanted_name.empty()) {
        for (int i = 0; i < count; ++i) {
            const char* name = SDL_GetAudioDeviceName(ids.get()[i]);
            if (name && wanted_name == name) return ids.get()[i];
        }
    }
    return SDL_AUDIO_DEVICE_DEFAULT_RECORDING;
}

}  // namespace st
