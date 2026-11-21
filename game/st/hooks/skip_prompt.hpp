// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace st {

// BKIE rev. 0 prologue. The original black control and its two white
// chevrons were measured in the touch replay before and after Start.
// The scene comes from ScreenContext. This checks whether the original
// control is already drawn before placing the keycap or tapping Skip.
inline bool native_skip_visible(const uint32_t* touch) {
    constexpr int width = 256;
    const auto rgb = [&](int x, int y) { return touch[y * width + x] & 0xffffffu; };
    const auto white = [&](int x, int y) { return (rgb(x,y) & 0xf8f8f8u) == 0xf8f8f8u; };
    return rgb(168,8) == 0 && rgb(175,14) == 0 &&
        white(238,3) && white(246,3) && white(243,8) && white(251,8);
}

namespace skip_art {

// A compact 5x7 face for the new keycap, at native DS pixel size.
inline constexpr std::string_view characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 +-./,[]()=\\;'`";
inline constexpr std::array<std::array<uint8_t,7>, characters.size()> glyphs{{
    {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30}, {14,17,16,16,16,17,14},
    {30,17,17,17,17,17,30}, {31,16,16,30,16,16,31}, {31,16,16,30,16,16,16},
    {14,17,16,23,17,17,14}, {17,17,17,31,17,17,17}, {14,4,4,4,4,4,14},
    {7,2,2,2,2,18,12}, {17,18,20,24,20,18,17}, {16,16,16,16,16,16,31},
    {17,27,21,21,17,17,17}, {17,25,21,19,17,17,17}, {14,17,17,17,17,17,14},
    {30,17,17,30,16,16,16}, {14,17,17,17,21,18,13}, {30,17,17,30,20,18,17},
    {15,16,16,14,1,1,30}, {31,4,4,4,4,4,4}, {17,17,17,17,17,17,14},
    {17,17,17,17,17,10,4}, {17,17,17,21,21,27,17}, {17,17,10,4,10,17,17},
    {17,17,10,4,4,4,4}, {31,1,2,4,8,16,31},
    {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14}, {14,17,1,6,8,16,31},
    {30,1,1,14,1,1,30}, {2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
    {14,16,16,30,17,17,14}, {31,1,2,4,8,8,8}, {14,17,17,14,17,17,14},
    {14,17,17,15,1,1,14},
    {0,0,0,0,0,0,0}, {0,4,4,31,4,4,0}, {0,0,0,31,0,0,0}, {0,0,0,0,0,6,6},
    {1,1,2,4,8,16,16}, {0,0,0,0,6,6,4}, {14,8,8,8,8,8,14}, {14,2,2,2,2,2,14},
    {2,4,8,8,8,4,2}, {8,4,2,2,2,4,8}, {0,0,31,0,31,0,0}, {16,16,8,4,2,1,1},
    {0,6,6,0,6,6,4}, {6,6,4,0,0,0,0}, {6,6,2,0,0,0,0},
}};
inline constexpr std::array<uint8_t,7> unknown{14,17,1,2,4,0,4};

inline void text(uint32_t* pixels, int x, std::string_view value, uint32_t ink) {
    for (auto c : value) {
        if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
        const auto i = characters.find(c);
        const auto& glyph = i == characters.npos ? unknown : glyphs[i];
        for (int y = 0; y < 7; ++y)
            for (int col = 0; col < 5; ++col)
                if (glyph[y] & (16 >> col)) pixels[(y + 6) * 256 + x + col] = ink;
        x += 6;
    }
}

inline void panel(uint32_t* pixels, int x, int width, uint32_t border, uint32_t fill) {
    for (int y = 3; y < 16; ++y)
        for (int col = 0; col < width; ++col) {
            const bool cap = y == 3 || y == 15;
            const bool side = col == 0 || col == width - 1;
            if (!cap || !side) pixels[y * 256 + x + col] = cap || side ? border : fill;
        }
}

} // namespace skip_art

inline void draw_skip_prompt(uint32_t* pixels, std::string_view binding, unsigned language, bool native_visible) {
    if (binding.empty()) return;
    binding = binding.substr(0,22);
    const std::string_view action = language == 2 ? "Passer" : language == 5 ? "Saltar" : "Skip";
    const int key_width = int(binding.size()) * 6 + 7;
    const int action_width = native_visible ? 0 : int(action.size()) * 6 + 9;
    const int left = (native_visible ? 162 : 253) - action_width - key_width;
    skip_art::panel(pixels,left,key_width,0xff989789u,0xff232725u);
    skip_art::text(pixels,left+4,binding,0xfffaf6e5u);
    if (!native_visible) {
        skip_art::panel(pixels,left+key_width+2,action_width-2,0xff19201fu,0xff101715u);
        skip_art::text(pixels,left+key_width+6,action,0xffe9e8dfu);
    }
}

} // namespace st
