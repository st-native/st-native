// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "controls.hpp"
#include <map>

namespace st::input {
class Gamepads {
public:
    Gamepads() {
        int count = 0;
        if (auto* ids = SDL_GetGamepads(&count)) {
            for (int i = 0; i < count; ++i) add(ids[i]);
            SDL_free(ids);
        }
    }
    ~Gamepads() { for (auto [id,pad] : pads_) SDL_CloseGamepad(pad); }
    Gamepads(const Gamepads&) = delete;
    Gamepads& operator=(const Gamepads&) = delete;
    void event(const SDL_Event& e) {
        if (e.type == SDL_EVENT_GAMEPAD_ADDED) add(e.gdevice.which);
        else if (e.type == SDL_EVENT_GAMEPAD_REMOVED) {
            const auto it = pads_.find(e.gdevice.which);
            if (it != pads_.end()) { SDL_CloseGamepad(it->second); pads_.erase(it); }
            if (active_ == e.gdevice.which) active_ = pads_.empty() ? 0 : pads_.begin()->first;
        } else if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN && pads_.contains(e.gbutton.which)) active_ = e.gbutton.which;
        else if (e.type == SDL_EVENT_GAMEPAD_AXIS_MOTION && pads_.contains(e.gaxis.which) &&
                 std::abs(int(e.gaxis.value)) > 16000) active_ = e.gaxis.which;
    }
    SDL_JoystickID active() const { return active_; }
private:
    void add(SDL_JoystickID id) {
        if (!pads_.contains(id)) if (auto* pad = SDL_OpenGamepad(id)) {
            pads_[id] = pad;
            if (!active_) active_ = id;
        }
    }
    std::map<SDL_JoystickID,SDL_Gamepad*> pads_;
    SDL_JoystickID active_ = 0;
};
} // namespace st::input
