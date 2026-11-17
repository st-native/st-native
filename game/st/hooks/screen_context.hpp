// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ntr/runtime.hpp"
#include "cinematic_profile.hpp"

namespace st {

class ScreenContext {
public:
    bool update(ntr::Runtime& runtime, const ntr::Frame& frame) {
        // USA/Australia rev. 0. Overlay 25 distinguishes the pre-title opening from the new-game prologue
        // and field, which also use overlay 102. The opening captures 3D for the other physical screen.
        // Keep the join across capture intervals and release it when leaving the opening overlay set.
        const bool opening = runtime.is_overlay_resident(25) && runtime.is_overlay_resident(102);
        joined_ = opening && (joined_ || frame.display_capture);
        return joined_;
    }

    ntr::CinematicUiPlacement ui_placement() const {
        return joined_ ? opening_ui_placement : ntr::CinematicUiPlacement{};
    }

private:
    bool joined_ = false;
};

}  // namespace st
