// SPDX-License-Identifier: GPL-3.0-or-later
// Spirit Tracks glue for the recompiled game module: the hook functions named in game/st/config/<code>/hooks.txt.
#pragma once

#include <string>

namespace st {

// Selects the release by its game code (header offset 0x0C). Only "BKIE" (USA/Australia, revision 0) is known.
bool select_release(const std::string& game_code);

// The hook the game module asks for by name (NtrHostApi::find_hook), or null.
void* find_hook(const char* name);

}  // namespace st
