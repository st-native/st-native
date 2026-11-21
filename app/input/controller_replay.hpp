// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <SDL3/SDL.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace st::input {
// Explicit developer smoke input only. Injects public SDL events through the same
// path as a controller, rather than sending pre-translated touches to the guest.
struct ControllerStep {
    uint64_t frame = 0;
    SDL_Event event{};
    std::string shot;
};
inline std::vector<ControllerStep> read_controller_replay(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file) throw std::runtime_error("Cannot open controller replay");
    std::vector<ControllerStep> steps;
    std::string line;
    while (std::getline(file,line)) {
        const auto first = line.find_first_not_of(" \t\r");
        if (first == std::string::npos || line[first] == '#') continue;
        std::istringstream row(line);
        ControllerStep s;
        std::string kind,name;
        if (!(row >> s.frame)) throw std::runtime_error("Invalid controller replay frame");
        int value = 0;
        if (!(row >> kind >> name) || s.frame == 0 || (!steps.empty() && s.frame < steps.back().frame))
            throw std::runtime_error("Invalid controller replay row/order");
        if (kind == "shot") {
            if (name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") != std::string::npos)
                throw std::runtime_error("Invalid controller replay shot name");
            s.shot = name;
        } else if (kind == "button" && row >> value && (value == 0 || value == 1)) {
            // SDL's string API retains legacy a/b/x/y names; physical aliases are clearer in tests.
            const auto button = name == "south" ? SDL_GAMEPAD_BUTTON_SOUTH : name == "east" ? SDL_GAMEPAD_BUTTON_EAST :
                name == "west" ? SDL_GAMEPAD_BUTTON_WEST : name == "north" ? SDL_GAMEPAD_BUTTON_NORTH : SDL_GetGamepadButtonFromString(name.c_str());
            if (button == SDL_GAMEPAD_BUTTON_INVALID) throw std::runtime_error("Unknown controller replay button");
            s.event.type = value ? SDL_EVENT_GAMEPAD_BUTTON_DOWN : SDL_EVENT_GAMEPAD_BUTTON_UP;
            s.event.gbutton.button = button; s.event.gbutton.down = value != 0;
        } else if (kind == "axis" && row >> value && value >= -32768 && value <= 32767) {
            const auto axis = SDL_GetGamepadAxisFromString(name.c_str());
            if (axis == SDL_GAMEPAD_AXIS_INVALID) throw std::runtime_error("Unknown controller replay axis");
            s.event.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
            s.event.gaxis.axis = axis; s.event.gaxis.value = Sint16(value);
        } else throw std::runtime_error("Invalid controller replay event");
        steps.push_back(s);
    }
    return steps;
}
} // namespace st::input
