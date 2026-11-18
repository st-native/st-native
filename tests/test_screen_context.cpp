// SPDX-License-Identifier: GPL-3.0-or-later
#include "../game/st/hooks/screen_policy.hpp"

st::ScreenSignals half(uint64_t frame, int offset) {
    st::ScreenSignals s;
    s.frame = s.graphics.geometry_frame = frame;
    s.capture = s.cole_overlays = true;
    s.graphics.screen_swap = offset > 0;
    s.graphics.focal_x = 3 * 4096; s.graphics.focal_y = 4 * 4096;
    s.graphics.offset_y = offset;
    s.graphics.projection.perspective_positive_y = offset > 0;
    s.graphics.projection.perspective_negative_y = offset < 0;
    s.graphics.projection.perspective_centered = offset == 0;
    return s;
}

int main() {
    st::ScreenPolicy policy;
    st::ScreenSignals opening;
    opening.opening_overlays = true;
    if (policy.update(opening)) return 1;
    opening.capture = true;
    if (!policy.update(opening)) return 2;
    opening.capture = false;
    if (!policy.update(opening) || policy.ui_placement().width != 168) return 3;
    if (policy.update({}) || policy.scene() != st::CinematicScene::none) return 4;
    if (policy.update(half(1, 4096)) || policy.update(half(2, 4096))) return 5;
    if (!policy.update(half(3, -4096)) || policy.scene() != st::CinematicScene::cole ||
        policy.ui_placement().width != 256 || policy.ui_placement().x != 0) return 6;
    if (!policy.update(half(4, 4096))) return 7;
    auto missing = half(5, -4096); missing.cole_overlays = false;
    if (policy.update(missing) || policy.update(half(6, 4096))) return 8;
    if (!policy.update(half(7, -4096))) return 9;
    auto stale = half(8, 4096); stale.graphics.geometry_frame = 7;
    if (policy.update(stale) || policy.update(half(9, -4096))) return 10;
    if (policy.update(half(10, 0))) return 11; // asymmetric Tower pair is not the Cole profile
    auto invalid = half(11, 4096); invalid.graphics.projection.complete = false;
    if (policy.update(invalid) || policy.update(half(12, -4096))) return 12;
    auto mismatch = half(13, 4096); ++mismatch.graphics.focal_y;
    if (policy.update(mismatch)) return 13;
    auto swapped = half(14, -4096); swapped.graphics.screen_swap = true;
    if (policy.update(swapped)) return 14;
    if (policy.update(half(15, 4096)) || policy.update(half(17, -4096))) return 15;
    auto gap = half(18, 4096); gap.capture = false;
    if (policy.update(gap) || policy.update(half(19, -4096))) return 16;
    auto tower_half = [](uint64_t frame, int offset) {
        auto s = half(frame, offset);
        s.cole_overlays = false; s.tower_overlays = true;
        return s;
    };
    if (policy.update(tower_half(20, 0))) return 17; // no cross-profile pairing with Cole's last half
    if (!policy.update(tower_half(21, 9032)) || policy.scene() != st::CinematicScene::tower) return 18;
    const auto camera = policy.camera();
    if (!camera.explicit_span || camera.min_offset_y != 0 || camera.max_offset_y != 9032 ||
        camera.centered_perspective_is_ui || policy.ui_placement().width != 256) return 19;
    if (!policy.update(tower_half(22, 0))) return 20;
    if (policy.update(tower_half(23, 0))) return 21; // independent centered views do not form a pair
    auto tower_bad = tower_half(24, 9032); ++tower_bad.graphics.focal_x;
    if (policy.update(tower_bad)) return 22;
    if (policy.update({}) || policy.camera().explicit_span) return 23;
    auto invalid_tower = tower_half(25, 0);
    invalid_tower.graphics.projection.perspective_centered = 0;
    invalid_tower.graphics.projection.orthographic = 1;
    if (policy.update(invalid_tower) || policy.update(tower_half(26, 9032))) return 24;
    st::ScreenPolicy cuts;
    if (cuts.update(half(1, 4096)) || !cuts.update(half(2, -4096))) return 25;
    auto cut = half(3, 4100); cut.graphics.focal_x *= 2; cut.graphics.focal_y *= 2;
    if (!cuts.update(cut)) return 26; // no one-frame layout flash at an established scene's camera cut
    auto cut_other = half(4, -4100); cut_other.graphics.focal_x *= 2; cut_other.graphics.focal_y *= 2;
    if (!cuts.update(cut_other)) return 27;
    if (cuts.update(half(5, 8192))) return 28; // a different unmatched span must still release the profile
    st::ScreenSignals narration;
    narration.narration_overlays = true;
    narration.graphics.display_control = {0x11118, 0x40211035};
    if (!policy.update(narration) || policy.scene() != st::CinematicScene::prologue ||
        policy.ui_placement().width != 256 || policy.camera().explicit_span) return 29;
    narration.graphics.display_control[0] |= 0x2000; // first in-room dialogue enables a hardware window
    if (policy.update(narration)) return 30;
    narration.graphics.display_control[0] = 0x11118;
    narration.narration_overlays = false;
    if (policy.update(narration)) return 31;
    narration.narration_overlays = true; narration.capture = true;
    if (policy.update(narration)) return 32;
    narration.capture = false; narration.graphics.screen_swap = true;
    if (policy.update(narration)) return 33;
    return 0;
}
