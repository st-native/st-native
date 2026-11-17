// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>

namespace st {

inline constexpr int ds_width = 256, ds_screen_height = 192;

enum class ScreenLayout { focus, classic, side_by_side, picture_in_picture };

constexpr ScreenLayout effective_screen_layout(ScreenLayout selected, bool automatic, bool shared_scene, bool invert) {
    return ((automatic && shared_scene) != invert) ? ScreenLayout::classic : selected;
}

struct ScreenRect {
    int x, y, width, height;

    constexpr bool contains(float px, float py) const {
        return px >= x && py >= y && px < x + width && py < y + height;
    }
};

struct ScreenViews {
    ScreenRect top, touch;
};

struct ScreenSize { int width, height; };

constexpr ScreenRect cinematic_view(int width, int height) {
    if (width <= 0 || height <= 0) return {};
    const int unit = std::min(width / 4, height / 3);
    return {(width - 4 * unit) / 2, (height - 3 * unit) / 2, 4 * unit, 3 * unit};
}

constexpr ScreenSize initial_screen_size(ScreenLayout layout) {
    switch (layout) {
    case ScreenLayout::classic: return {ds_width, 2 * ds_screen_height};
    case ScreenLayout::side_by_side: return {2 * ds_width, ds_screen_height};
    case ScreenLayout::focus: return {ds_width + ds_width / 2 + 8, ds_screen_height};
    case ScreenLayout::picture_in_picture: return {ds_width, ds_screen_height};
    }
    return {ds_width, 2 * ds_screen_height};
}

// Use exact 4:3 tiles and center the complete arrangement in drawable pixels.
constexpr ScreenViews screen_views(ScreenLayout layout, int width, int height) {
    if (width <= 0 || height <= 0) return {};
    if (layout == ScreenLayout::focus && width >= height) {
        const int gap = std::max(6, height / 32);
        const int unit = std::min((width - gap) / 12, height / 6);
        if (unit > 0) {
            const int x = (width - 12 * unit - gap) / 2, y = (height - 6 * unit) / 2;
            return {{x + 8 * unit + gap, y, 4 * unit, 3 * unit}, {x, y, 8 * unit, 6 * unit}};
        }
    }

    if (layout == ScreenLayout::side_by_side) {
        const int unit = std::min(width / 8, height / 3);
        const int x = (width - 8 * unit) / 2, y = (height - 3 * unit) / 2;
        return {{x, y, 4 * unit, 3 * unit}, {x + 4 * unit, y, 4 * unit, 3 * unit}};
    }

    if (layout == ScreenLayout::picture_in_picture) {
        const int unit = std::min(width / 4, height / 3);
        const ScreenRect touch{(width - 4 * unit) / 2, (height - 3 * unit) / 2, 4 * unit, 3 * unit};
        const int inset = unit * 3 / 8, margin = unit / 12;
        return {{touch.x + touch.width - 4 * inset - margin, touch.y + margin, 4 * inset, 3 * inset}, touch};
    }

    const int unit = std::min(width / 4, height / 6);
    const int x = (width - 4 * unit) / 2, y = (height - 6 * unit) / 2;
    return {{x, y, 4 * unit, 3 * unit}, {x, y + 3 * unit, 4 * unit, 3 * unit}};
}

// The top view is drawn last and owns overlaps. Only a drag begun on the touch view may cross its edges.
constexpr bool to_touch(const ScreenViews& views, float& x, float& y, bool captured) {
    const auto& touch = views.touch;
    if (touch.width <= 0 || touch.height <= 0 ||
        (!captured && (!touch.contains(x, y) || views.top.contains(x, y)))) return false;
    x = std::clamp((x - touch.x) * ds_width / touch.width, 0.f, float(ds_width - 1));
    y = std::clamp((y - touch.y) * ds_screen_height / touch.height, 0.f, float(ds_screen_height - 1));
    return true;
}

}  // namespace st
