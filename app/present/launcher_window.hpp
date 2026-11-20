// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <memory>

#include "../game_session.hpp"

namespace Rml { class Context; }

class LauncherWindow final : public st::GamePresentation {
public:
    LauncherWindow();
    ~LauncherWindow() override;
    bool open(const char* title, int width, int height);
    bool process_events(Rml::Context& context, bool* audio_devices_changed = nullptr);
    bool draw(Rml::Context& context, const char* preview_path = nullptr);
    bool draw_pause(Rml::Context& context);
    void menu_input(Rml::Context& context, SDL_Event& event);
    SDL_Window* window() const override;
    bool update_game_frame(const uint32_t* pixels, bool shared_scene, bool touch_captured, bool cinematic) override;
    bool present_game(const uint32_t* pixels, bool shared_scene, bool touch_captured, bool cinematic) override;
    double cinematic_aspect() const override;
    uint32_t screen_features() const override;
    bool game_coordinates(float& x, float& y, bool captured) const override;
    void set_screen_layout(st::ScreenLayout layout, bool automatic_join, st::ScreenEnhancements enhancements = {}) override;
    void toggle_classic_layout() override;
    void set_secondary_view(st::SecondaryViewSettings settings) override;
    void peek_secondary(bool held) override;
    bool save_game_preview(const char* path);
    uint64_t audio_revision() const override;
    st::AudioSettings audio_settings() const override;
    void set_audio_settings(const st::AudioSettings& audio) override;
    bool pause_on_menu() const override;
    void set_pause_on_menu(bool pause) override;

private:
    bool draw_game();
    struct State;
    std::unique_ptr<State> state_;
};
