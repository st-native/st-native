// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "audio/audio_settings.hpp"
#include "present/screen_enhancements.hpp"
#include "present/internal_resolution.hpp"
#include "present/screen_layout.hpp"

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
    AudioSettings audio;
    bool pause_on_menu = false;
    SecondaryViewSettings secondary;
    int internal_scale = default_internal_scale;
};

struct GameResult {
    int exit_code;
    bool close_requested;
};

struct GameFrameInput {
    const uint32_t* top = nullptr;
    const uint32_t* bottom = nullptr;
    const uint32_t* cinematic_pixels = nullptr;
    unsigned gl_screens_texture = 0;
    unsigned gl_cinematic_texture = 0;
    int scale = 1;
    bool shared_scene = false;
    uint64_t frame_number = 0;
    uint64_t published_ns = 0;
};

struct PresentedFrameInfo {
    bool drew = false;
    bool shared_scene = false;
    bool cinematic = false;
    uint64_t frame_number = 0;
    uint64_t published_ns = 0;
};

class GamePresentation {
public:
    virtual ~GamePresentation() = default;
    virtual SDL_Window* window() const = 0;
    virtual bool bind_worker_graphics() { return false; }
    virtual void unbind_worker_graphics() {}
    virtual bool publish_game_frame(const GameFrameInput& frame) = 0;
    virtual bool present_published_game(bool touch_captured, PresentedFrameInfo& info, bool render = true) = 0;
    // Three 256x192 BGRA planes: original top, original touch, and the optional cinematic view.
    virtual bool update_game_frame(const uint32_t* pixels, bool shared_scene, bool touch_captured, bool cinematic) = 0;
    virtual bool present_game(const uint32_t* pixels, bool shared_scene, bool touch_captured, bool cinematic) = 0;
    virtual double cinematic_aspect() const = 0;
    virtual uint32_t screen_features() const = 0;
    virtual bool game_coordinates(float& x, float& y, bool captured) const = 0;
    virtual void set_screen_layout(ScreenLayout layout, bool automatic_join, ScreenEnhancements enhancements = {}) = 0;
    virtual void toggle_classic_layout() = 0;
    virtual void set_secondary_view(SecondaryViewSettings settings) = 0;
    virtual void peek_secondary(bool held) = 0;
    virtual uint64_t audio_revision() const = 0;
    virtual AudioSettings audio_settings() const = 0;
    virtual void set_audio_settings(const AudioSettings& audio) = 0;
    virtual bool pause_on_menu() const = 0;
    virtual void set_pause_on_menu(bool pause) = 0;
    virtual void set_internal_scale(int scale) = 0;
    virtual int internal_scale() const = 0;
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
