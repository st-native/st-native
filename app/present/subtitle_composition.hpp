// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cstdint>

namespace st {

struct SubtitleStyle {
    int offset_y = 0, margin = 8, padding = 8;
    unsigned shade = 176;
};

// Compose an explicitly selected transparent foreground; alpha bounds are used only for drawing,
// never to identify a scene. Keep the original glyphs, spacing, colors and typewriter progress.
inline bool compose_subtitles(uint32_t* output, const uint32_t* art, const uint32_t* text,
                              SubtitleStyle style = {}) {
    constexpr int width = 256, height = 192;
    if (!output || !art || !text || output == text || style.margin < 0 || style.margin >= height / 2 ||
        style.padding < 0 || style.padding > height || style.shade > 255) return false;
    int left = width, top = height, right = -1, bottom = -1;
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            if (text[y * width + x] >> 24) {
                left = std::min(left, x); right = std::max(right, x);
                top = std::min(top, y); bottom = std::max(bottom, y);
            }
    if (output != art) std::copy_n(art, width * height, output);
    if (right < left) return true; // pauses/fades with no glyphs do not produce an empty black panel
    const int source_width = right - left + 1, source_height = bottom - top + 1;
    const double scale = std::min({1.0, double(width - 2 * style.margin) / source_width,
                                  double(height - 2 * style.margin) / source_height});
    const int draw_width = std::max(1, int(source_width * scale));
    const int draw_height = std::max(1, int(source_height * scale));
    const int x = std::clamp(left, style.margin, width - style.margin - draw_width);
    // Clamp in 64 bits so even an invalid caller's translation cannot overflow.
    const int y = int(std::clamp<int64_t>(int64_t(top) + style.offset_y, style.margin,
                                        height - style.margin - draw_height));
    const int band_top = std::max(0, y - style.padding);
    const int band_bottom = std::min(height, y + draw_height + style.padding);
    for (int row = band_top; row < band_bottom; ++row)
        for (int column = style.margin; column < width - style.margin; ++column) {
            auto& pixel = output[row * width + column];
            const auto darken = [&](unsigned value) { return value * (255 - style.shade) / 255; };
            pixel = 0xff000000u | darken((pixel >> 16) & 255) << 16 |
                darken((pixel >> 8) & 255) << 8 | darken(pixel & 255);
        }
    for (int row = 0; row < draw_height; ++row)
        for (int column = 0; column < draw_width; ++column) {
            const int source_x = left + (2 * column + 1) * source_width / (2 * draw_width);
            const int source_y = top + (2 * row + 1) * source_height / (2 * draw_height);
            const auto pixel = text[source_y * width + source_x];
            const unsigned alpha = pixel >> 24;
            if (!alpha) continue;
            auto& target = output[(row + y) * width + column + x];
            const auto blend = [alpha](unsigned a, unsigned b) { return (a * alpha + b * (255 - alpha) + 127) / 255; };
            target = 0xff000000u | blend((pixel >> 16) & 255, (target >> 16) & 255) << 16 |
                blend((pixel >> 8) & 255, (target >> 8) & 255) << 8 | blend(pixel & 255, target & 255);
        }
    return true;
}

} // namespace st
