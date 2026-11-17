// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

struct SDL_Window;
union SDL_Event;

namespace st {

struct GameLaunch {
    std::filesystem::path rom, module;
    uint8_t language = 1;
    int scale = 2;
    bool fullscreen = false;
};

struct GameResult {
    int exit_code;
    bool close_requested;
};

inline constexpr int ds_width = 256, ds_screen_height = 192;

class GamePresentation {
public:
    virtual ~GamePresentation() = default;
    virtual SDL_Window* window() const = 0;
    virtual bool present_game(const uint32_t* pixels) = 0;
    virtual bool game_coordinates(float& x, float& y) const = 0;
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
