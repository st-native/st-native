// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "cinematic_profile.hpp"
#include "ntr/graphics_context.hpp"

namespace st {

enum class CinematicScene { none, opening, cole, tower, prologue, save_menu };

struct ScreenSignals {
    uint64_t frame = 0;
    bool capture = false, opening_overlays = false, cole_overlays = false, tower_overlays = false;
    bool narration_overlays = false;
    bool menu_overlays = false;
    ntr::GraphicsContext graphics;
};

// Pure game-side policy. Residency alone is insufficient: require a fresh, matching camera pair.
// Tower signals are supplied only by the explicitly requested headless research preview.
class ScreenPolicy {
public:
    bool update(const ScreenSignals& s) {
        const bool was_cole = scene_ == CinematicScene::cole;
        camera_ = {};
        if (s.opening_overlays) {
            scene_ = (scene_ == CinematicScene::opening || s.capture) ? CinematicScene::opening : CinematicScene::none;
            previous_ = {};
            return joined();
        }
        scene_ = CinematicScene::none;
        const auto& g = s.graphics;
        const auto& p = g.projection;
        // File selection/setup/mode menu, locally replayed in USA rev. 0. The top screen is decorative.
        if (s.menu_overlays && !s.capture && !g.screen_swap &&
            (g.display_control[0] == 0x211c10 || g.display_control[0] == 0x211f10) &&
            g.display_control[1] == 0x211f30) {
            scene_ = CinematicScene::save_menu;
            previous_ = {}; previous_scene_ = CinematicScene::none;
            return true;
        }
        // USA/Australia rev. 0 prologue: observed SDK display state, not pixel brightness/content.
        // Window 0 becoming enabled in engine A (0x13118) releases this rule before the first dialogue.
        if (s.narration_overlays && !s.capture && !g.screen_swap &&
            g.display_control[0] == 0x11118 && g.display_control[1] == 0x40211035) {
            scene_ = CinematicScene::prologue;
            previous_ = {}; previous_scene_ = CinematicScene::none;
            return true;
        }
        const bool base = s.capture && g.geometry_frame == s.frame &&
            p.complete && p.other == 0 && p.perspective_centered == 0 &&
            uint64_t(p.perspective_positive_y) + p.perspective_negative_y == 1 &&
            g.focal_x > 0 && g.focal_y > 0 && g.offset_y != 0 &&
            ((g.offset_y > 0 && g.screen_swap && p.perspective_positive_y == 1) ||
             (g.offset_y < 0 && !g.screen_swap && p.perspective_negative_y == 1));
        const bool tower_half = s.tower_overlays && s.capture && g.geometry_frame == s.frame && p.complete &&
            p.other == 0 && p.perspective_negative_y == 0 &&
            uint64_t(p.perspective_positive_y) + p.perspective_centered == 1 &&
            g.focal_x > 0 && g.focal_y > 0 &&
            ((g.offset_y > 0 && g.offset_y <= 8 * 4096 && g.screen_swap && p.perspective_positive_y == 1) ||
             (g.offset_y == 0 && !g.screen_swap && p.perspective_centered == 1));
        const auto candidate = s.cole_overlays && base ? CinematicScene::cole :
            tower_half ? CinematicScene::tower : CinematicScene::none;
        if (candidate == CinematicScene::none) { previous_ = {}; previous_scene_ = candidate; return false; }
        const bool consecutive = previous_scene_ == candidate && previous_.geometry_frame &&
            g.geometry_frame > previous_.geometry_frame && g.geometry_frame - previous_.geometry_frame == 1;
        const bool matching_lens = g.focal_x == previous_.focal_x && g.focal_y == previous_.focal_y;
        const auto offset_sum = int64_t(g.offset_y) + previous_.offset_y;
        // Verified Cole cuts change focal scale and round the opposite offsets by up to four SDK units.
        // Once established, keep the layout across such a cut; still require a fresh opposite half.
        const bool cole_cut = was_cole && ((g.offset_y > 0) != (previous_.offset_y > 0)) &&
            offset_sum >= -8 && offset_sum <= 8;
        if (consecutive && candidate == CinematicScene::cole &&
            ((matching_lens && int64_t(g.offset_y) == -int64_t(previous_.offset_y)) || cole_cut))
            scene_ = candidate;
        if (consecutive && matching_lens && candidate == CinematicScene::tower &&
            ((g.offset_y == 0 && previous_.offset_y > 0) || (g.offset_y > 0 && previous_.offset_y == 0))) {
            scene_ = candidate;
            camera_.explicit_span = true;
            camera_.max_offset_y = g.offset_y > 0 ? g.offset_y : previous_.offset_y;
            camera_.centered_perspective_is_ui = false;
        }
        previous_ = g;
        previous_scene_ = candidate;
        return joined();
    }

    bool joined() const { return scene_ != CinematicScene::none; }
    CinematicScene scene() const { return scene_; }
    ntr::CinematicCamera camera() const { return camera_; }
    ntr::CinematicUiPlacement ui_placement() const {
        return scene_ == CinematicScene::opening ? opening_ui_placement : ntr::CinematicUiPlacement{};
    }

private:
    CinematicScene scene_ = CinematicScene::none;
    CinematicScene previous_scene_ = CinematicScene::none;
    ntr::GraphicsContext previous_;
    ntr::CinematicCamera camera_;
};

} // namespace st
