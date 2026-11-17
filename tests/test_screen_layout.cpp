// SPDX-License-Identifier: GPL-3.0-or-later
#include "../app/present/screen_layout.hpp"

#include <array>

using st::ScreenLayout;

static_assert(st::effective_screen_layout(ScreenLayout::picture_in_picture, true, true, false) == ScreenLayout::classic);
static_assert(st::effective_screen_layout(ScreenLayout::picture_in_picture, true, true, true) == ScreenLayout::picture_in_picture);
static_assert(st::effective_screen_layout(ScreenLayout::focus, false, true, false) == ScreenLayout::focus);
static_assert(st::effective_screen_layout(ScreenLayout::focus, false, false, true) == ScreenLayout::classic);
static_assert(st::effective_screen_layout(ScreenLayout::side_by_side, true, false, false) == ScreenLayout::side_by_side);

int main() {
    constexpr std::array sizes{st::ScreenSize{390, 780}, st::ScreenSize{390, 300},
                               st::ScreenSize{1921, 1080}, st::ScreenSize{3440, 1440}};
    static_assert(st::cinematic_view(0, 192).width == 0);
    static_assert(st::cinematic_view(256, 0).height == 0);
    for (const auto& [width, height] : sizes) {
        const auto view = st::cinematic_view(width, height);
        if (view.width * 3 != view.height * 4 || view.x < 0 || view.y < 0 ||
            view.x + view.width > width || view.y + view.height > height ||
            (view.width + 4 <= width && view.height + 3 <= height)) return 9;
        float x = view.x + view.width / 2.f, y = view.y + view.height * 0.9f;
        if (!st::to_touch({{}, view}, x, y, false) || x != 128.f || y < 172.f || y > 173.f) return 10;
        x = float(view.x - 1); y = float(view.y);
        if (st::to_touch({{}, view}, x, y, false)) return 11;
    }
    constexpr std::array layouts{ScreenLayout::focus, ScreenLayout::classic, ScreenLayout::side_by_side,
                                 ScreenLayout::picture_in_picture};
    for (const auto layout : layouts) {
        for (const auto& [width, height] : sizes) {
            const auto [top, touch] = st::screen_views(layout, width, height);
            for (const auto& rect : {top, touch}) {
                if (rect.x < 0 || rect.y < 0 || rect.width <= 0 || rect.height <= 0 ||
                    rect.x + rect.width > width || rect.y + rect.height > height ||
                    rect.width * 3 != rect.height * 4) return 1;
            }
            if (layout == ScreenLayout::classic || (layout == ScreenLayout::focus && width < height)) {
                if (top.x != touch.x || top.width != touch.width ||
                    top.y + top.height != touch.y || top.height != touch.height) return 2;
            } else if (layout == ScreenLayout::focus) {
                if (touch.x + touch.width >= top.x || touch.y != top.y ||
                    touch.width != 2 * top.width || touch.height != 2 * top.height) return 3;
            } else if (layout == ScreenLayout::side_by_side) {
                if (top.x + top.width != touch.x || top.y != touch.y ||
                    top.width != touch.width || top.height != touch.height) return 4;
            } else if (!touch.contains(float(top.x), float(top.y)) ||
                       !touch.contains(float(top.x + top.width - 1), float(top.y + top.height - 1))) return 5;

            float x = touch.x + 0.5f * touch.width, y = touch.y + 0.75f * touch.height;
            if (!st::to_touch({top, touch}, x, y, false) || x != 128.f || y != 144.f) return 6;
            x = top.x + 0.5f * top.width;
            y = top.y + 0.5f * top.height;
            if (st::to_touch({top, touch}, x, y, false)) return 7;
            x = float(touch.x + touch.width);
            y = float(touch.y + touch.height);
            if (st::to_touch({top, touch}, x, y, false) ||
                !st::to_touch({top, touch}, x, y, true) || x != 255.f || y != 191.f) return 8;
        }
    }
    return 0;
}
