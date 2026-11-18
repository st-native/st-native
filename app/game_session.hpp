// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "present/screen_layout.hpp"
#include "present/screen_enhancements.hpp"

struct SDL_Window;
union SDL_Event;

namespace st {

struct GameLaunch {
    std::filesystem::path rom, module;
    uint8_t language = 1;
    int scale = 2;
    bool fullscreen = false;
    ScreenLayout layout = ScreenLayout::focus;
    bool automatic_join = true;
    ScreenEnhancements enhancements;
};

struct GameResult {
    int exit_code;
    bool close_requested;
};

class GamePresentation {
public:
    virtual ~GamePresentation() = default;
    virtual SDL_Window* window() const = 0;
    // Three 256x192 BGRA planes: original top, original touch, and the optional cinematic view.
    virtual bool present_game(const uint32_t* pixels, bool shared_scene, bool touch_captured, bool cinematic) = 0;
    virtual double cinematic_aspect() const = 0;
    virtual uint32_t screen_features() const = 0;
    virtual bool game_coordinates(float& x, float& y, bool captured) const = 0;
    virtual void set_screen_layout(ScreenLayout layout, bool automatic_join, ScreenEnhancements enhancements = {}) = 0;
    virtual void toggle_classic_layout() = 0;
};

enum class PauseAction { stay, resume, leave };

class GamePauseMenu {
public:
    virtual ~GamePauseMenu() = default;
    virtual void open() = 0;
    virtual void close() = 0;
    virtual void input(SDL_Event& event) = 0;
    virtual bool draw() = 0;
    virtual PauseAction action() const = 0;
};

int run_game_cli(int argc, char** argv);
GameResult play_game_in_window(const GameLaunch& launch, GamePresentation& presentation, GamePauseMenu& menu,
                              uint64_t test_menu_after = 0);

}  // namespace st
