// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <SDL3/SDL.h>

#include <string>
#include <string_view>
#include <vector>

namespace st {

std::vector<std::string> list_playback_devices();
std::vector<std::string> list_recording_devices();

SDL_AudioDeviceID resolve_playback_device(std::string_view wanted_name);
SDL_AudioDeviceID resolve_recording_device(std::string_view wanted_name, bool* any_available = nullptr);

}  // namespace st
