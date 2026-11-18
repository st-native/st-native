// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <RmlUi_Platform_SDL.h>

namespace st {

// RmlUi owns cursor selection in menus. The game canvas owns it while playing;
// remember UI requests so pause/launcher restore their own pointer or text cursor.
class CursorInterface final : public SystemInterface_SDL {
public:
    using SystemInterface_SDL::SystemInterface_SDL;

    void SetMouseCursor(const Rml::String& name) override {
        ui_cursor_ = name;
        if (!game_) SystemInterface_SDL::SetMouseCursor(name);
    }

    void set_game_mode(bool game) {
        if (game_ == game) return;
        game_ = game;
        SystemInterface_SDL::SetMouseCursor(game ? "arrow" : ui_cursor_);
    }

private:
    bool game_ = false;
    Rml::String ui_cursor_ = "arrow";
};

} // namespace st
