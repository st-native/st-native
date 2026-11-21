// SPDX-License-Identifier: GPL-3.0-or-later
#include "../app/present/internal_resolution.hpp"

#include <string_view>

static_assert(st::default_internal_scale == 1);
static_assert(std::string_view(st::internal_resolution_value(1)) == "256x192");
static_assert(std::string_view(st::internal_resolution_label(1)) == "256x192 (Native)");
static_assert(st::parse_internal_scale("256x192") == 1);
static_assert(st::parse_internal_scale("512x384") == 2);
static_assert(st::parse_internal_scale("1536x1152") == 6);
static_assert(st::parse_internal_scale("3072x2304") == 12);
static_assert(st::parse_internal_scale("6") == 6);
static_assert(st::parse_internal_scale("7") == 1);
static_assert(st::parse_internal_scale("7", true) == 7);
static_assert(st::parse_internal_scale("0", true) == 1);
static_assert(st::parse_internal_scale("17", true) == 1);
static_assert(st::parse_internal_scale("invalid") == 1);
static_assert(st::parse_internal_scale("") == 1);
static_assert(st::internal_resolution_size(1).width == 256 && st::internal_resolution_size(1).height == 192);
static_assert(st::internal_resolution_size(6).width == 1536 && st::internal_resolution_size(6).height == 1152);
static_assert(st::internal_resolution_size(12).width == 3072 && st::internal_resolution_size(12).height == 2304);

int main() {
    if (st::internal_resolution_options.front().scale != 1) return 1;
    int previous_scale = 0;
    for (const auto& option : st::internal_resolution_options) {
        if (option.scale <= previous_scale || option.scale > st::max_internal_scale) return 2;
        previous_scale = option.scale;
        if (option.width != st::ds_width * option.scale || option.height != st::ds_screen_height * option.scale)
            return 3;
        if (option.width * 3 != option.height * 4) return 4;
        if (st::parse_internal_scale(option.value) != option.scale) return 5;
        if (std::string_view(st::internal_resolution_value(option.scale)) != option.value) return 6;
        if (std::string_view(st::internal_resolution_label(option.scale)) != option.label) return 7;
    }
    if (std::string_view(st::internal_resolution_value(99)) != "256x192") return 8;
    if (std::string_view(st::internal_resolution_label(-1)) != "256x192 (Native)") return 9;
    return 0;
}
