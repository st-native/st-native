// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <array>
#include <string_view>

#include "screen_layout.hpp"

namespace st {

inline constexpr int default_internal_scale = 1;
inline constexpr int max_internal_scale = 16;

struct InternalResolutionOption {
    int scale;
    const char* value;
    const char* label;
    int width;
    int height;
};

inline constexpr std::array<InternalResolutionOption, 9> internal_resolution_options{{
    {1, "256x192", "256x192 (Native)", 256, 192},
    {2, "512x384", "512x384", 512, 384},
    {3, "768x576", "768x576", 768, 576},
    {4, "1024x768", "1024x768", 1024, 768},
    {5, "1280x960", "1280x960", 1280, 960},
    {6, "1536x1152", "1536x1152", 1536, 1152},
    {8, "2048x1536", "2048x1536", 2048, 1536},
    {10, "2560x1920", "2560x1920", 2560, 1920},
    {12, "3072x2304", "3072x2304", 3072, 2304},
}};

constexpr bool valid_menu_internal_scale(int scale) {
    for (const auto& option : internal_resolution_options)
        if (option.scale == scale) return true;
    return false;
}

constexpr int normalize_internal_scale(int scale) {
    return valid_menu_internal_scale(scale) ? scale : default_internal_scale;
}

constexpr int parse_internal_scale(std::string_view text, bool allow_any_cli_scale = false) {
    if (text.empty()) return default_internal_scale;
    for (const auto& option : internal_resolution_options)
        if (text == option.value) return option.scale;
    int parsed = 0;
    for (const char c : text) {
        if (c < '0' || c > '9') return default_internal_scale;
        parsed = parsed * 10 + (c - '0');
        if (parsed > max_internal_scale) return default_internal_scale;
    }
    if (allow_any_cli_scale) return parsed >= 1 && parsed <= max_internal_scale ? parsed : default_internal_scale;
    return normalize_internal_scale(parsed);
}

constexpr const char* internal_resolution_value(int scale) {
    for (const auto& option : internal_resolution_options)
        if (option.scale == scale) return option.value;
    return internal_resolution_options[0].value;
}

constexpr const char* internal_resolution_label(int scale) {
    for (const auto& option : internal_resolution_options)
        if (option.scale == scale) return option.label;
    return internal_resolution_options[0].label;
}

constexpr ScreenSize internal_resolution_size(int scale) {
    const int clamped = std::clamp(scale, 1, max_internal_scale);
    return {ds_width * clamped, ds_screen_height * clamped};
}

}  // namespace st
