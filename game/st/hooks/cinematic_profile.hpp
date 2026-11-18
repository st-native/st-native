// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ntr/cinematic.hpp"

namespace st {

// USA/Australia rev. 0 pre-title opening, selected only by ScreenContext's verified opening rule.
// Local GX layer checks (2026-11-17) found one centered, screen-space 3D polygon during the
// title reveal. Place that title UI (the logo) separately. The prompt and copyright
// are 2D foreground. Keep those and touch coordinates unchanged; fit the entire
// 3D UI canvas above the prompt, with uniform scaling and a centered horizontal anchor.
inline constexpr ntr::CinematicUiPlacement opening_ui_placement{44, 4, 168, 126};
static_assert(ntr::valid_cinematic_ui_placement(opening_ui_placement));
static_assert(opening_ui_placement.width * 3 == opening_ui_placement.height * 4);

// New-game prologue: original top text begins around row 80; move it to the lower part of the
// art canvas. The generic compositor fits unexpected longer text rather than cropping glyphs.
inline constexpr int prologue_text_offset_y = 40;

} // namespace st
