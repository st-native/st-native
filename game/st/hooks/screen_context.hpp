// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "ntr/runtime.hpp"
#include "screen_policy.hpp"

namespace st {

class ScreenContext {
public:
    explicit ScreenContext(bool tower_preview = false) : tower_preview_(tower_preview) {}
    bool update(ntr::Runtime& runtime, const ntr::Frame& frame, uint64_t frame_number) {
        // USA/Australia rev. 0. Overlay 25 distinguishes the pre-title opening from the new-game prologue
        // and field, which also use overlay 102. The opening captures 3D for the other physical screen.
        // Keep the join across capture intervals and release it when leaving the opening overlay set.
        ScreenSignals signals;
        signals.frame = frame_number;
        signals.capture = frame.display_capture;
        signals.opening_overlays = runtime.is_overlay_resident(25) && runtime.is_overlay_resident(102);
        // USA/Australia rev. 0, local Cole replay (2026-11-17): these four probe sets coexist during the
        // opposed-frustum capture interval, then 102 leaves at the transition. Not a logical scene ID.
        // Require the SDK camera pair as well; stale bytes in an overlay must never enable joining alone.
        signals.cole_overlays = !signals.opening_overlays && frame.display_capture &&
            runtime.is_overlay_resident(26) && runtime.is_overlay_resident(31) &&
            runtime.is_overlay_resident(88) && runtime.is_overlay_resident(102);
        // Local Tower-after-Byrne replay: persistent probes 31/48/70/93/95 during the captured pair.
        // Offset zero is world geometry here. The profile requires a fresh positive/centered pair as evidence.
        // This is a headless research preview only: interactive Tower scenes also need inverse touch mapping.
        signals.tower_overlays = tower_preview_ && !signals.opening_overlays && !signals.cole_overlays && frame.display_capture &&
            runtime.is_overlay_resident(31) && runtime.is_overlay_resident(48) && runtime.is_overlay_resident(70) &&
            runtime.is_overlay_resident(93) && runtime.is_overlay_resident(95);
        if (signals.cole_overlays || signals.tower_overlays) signals.graphics = runtime.inspect_graphics();
        // Local new-game replay, dense SDK-register/foreground inspection (2026-11-18).
        // Probe matches persist into gameplay; the exact display signature in ScreenPolicy is essential.
        signals.narration_overlays = !signals.opening_overlays && !signals.cole_overlays &&
            !signals.tower_overlays && !frame.display_capture && runtime.is_overlay_resident(31) &&
            runtime.is_overlay_resident(62) && runtime.is_overlay_resident(102);
        if (signals.narration_overlays) signals.graphics = runtime.inspect_graphics(false);
        return policy_.update(signals);
    }

    ntr::CinematicUiPlacement ui_placement() const {
        return policy_.ui_placement();
    }

    CinematicScene scene() const { return policy_.scene(); }
    ntr::CinematicCamera camera() const { return policy_.camera(); }
    bool narration() const { return policy_.scene() == CinematicScene::prologue; }
    int narration_text_offset() const { return prologue_text_offset_y; }

private:
    ScreenPolicy policy_;
    bool tower_preview_ = false;
};

}  // namespace st
