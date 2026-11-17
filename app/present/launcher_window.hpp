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
    bool process_events(Rml::Context& context);
    bool draw(Rml::Context& context, const char* preview_path = nullptr);
    bool draw_pause(Rml::Context& context);
    void menu_input(Rml::Context& context, SDL_Event& event);
    SDL_Window* window() const override;
    bool present_game(const uint32_t* pixels) override;
    bool game_coordinates(float& x, float& y) const override;

private:
    bool draw_game();
    struct State;
    std::unique_ptr<State> state_;
};
