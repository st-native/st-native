// SPDX-License-Identifier: GPL-3.0-or-later
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "game_session.hpp"

int main(int argc, char** argv) {
#ifdef _WIN32
    // Ask SDL to read the original Windows command line as UTF-8.
    return SDL_RunApp(0, nullptr, st::run_game_cli, nullptr);
#else
    return st::run_game_cli(argc, argv);
#endif
}
