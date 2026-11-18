// SPDX-License-Identifier: GPL-3.0-or-later
#include "cinematic_projection.hpp"
#include "cinematic_overlay.hpp"
#include "../game/st/hooks/cinematic_profile.hpp"

#include <cmath>
#include <limits>

using ntr::gx::Entry;
using ntr::gx::center_split_projection;

std::vector<Entry> projection(int offset) {
    std::vector<Entry> scene{{0x10, 0}};
    for (int i = 0; i < 16; ++i) scene.push_back({0x16, 0});
    scene[1].param = 3 * 4096;
    scene[6].param = 4 * 4096;
    scene[10].param = uint32_t(offset);
    scene[12].param = uint32_t(-4096);
    return scene;
}

int main() {
    // Identity preserves the former compositor, including transparency and DS alpha weights.
    constexpr uint32_t background = 0xff112233u;
    std::vector<uint32_t> source(256 * 192, 0), output(256 * 192, background);
    source[0] = 0x1f00003f; // opaque red, in the renderer's 6-bit format
    source[2] = 0x0f003f00; // green at 16/32 coverage
    auto line = [&](int y) { return source.data() + y * 256; };
    ntr::composite_cinematic_ui(output.data(), line, {}, 0);
    if (output[0] != 0xffff0000u || output[1] != background || output[2] != 0xff089019u) return 14;
    const auto identity = output;
    for (auto invalid : {ntr::CinematicUiPlacement{-1, 0, 256, 192}, {0, 0, 0, 192},
                         {256, 192, 1, 1}, {0, 0, std::numeric_limits<int>::max(), 192}}) {
        std::fill(output.begin(), output.end(), background);
        ntr::composite_cinematic_ui(output.data(), line, invalid, 0);
        if (output != identity) return 15;
    }
    // Even a fully opaque source cannot cover the reserved prompt/footer area with this profile.
    constexpr auto title = st::opening_ui_placement;
    std::fill(source.begin(), source.end(), 0x1f00003f);
    std::fill(output.begin(), output.end(), background);
    ntr::composite_cinematic_ui(output.data(), line, title, 0);
    for (int y = 0; y < 192; ++y)
        for (int x = 0; x < 256; ++x) {
            const bool inside = x >= title.x && x < title.x + title.width &&
                                y >= title.y && y < title.y + title.height;
            if (output[y * 256 + x] != (inside ? 0xffff0000u : background)) return 16;
        }
    // Sampling uses pixel centers across the complete source extent, including all four corners.
    std::fill(source.begin(), source.end(), 0);
    std::fill(output.begin(), output.end(), background);
    source[0] = 0x1f00003f;
    source[255] = 0x1f003f00;
    source[191 * 256] = 0x1f3f0000;
    source[192 * 256 - 1] = 0x1f3f3f3f;
    ntr::composite_cinematic_ui(output.data(), line, title, 0);
    if (output[title.y * 256 + title.x] != 0xffff0000u ||
        output[title.y * 256 + title.x + title.width - 1] != 0xff00ff00u ||
        output[(title.y + title.height - 1) * 256 + title.x] != 0xff0000ffu ||
        output[(title.y + title.height - 1) * 256 + title.x + title.width - 1] != 0xffffffffu) return 18;
    if (title.y + title.height > 130 || title.width * 3 != title.height * 4 ||
        title.x * 2 + title.width != 256) return 17;

    for (unsigned flag : {0x10u, 0x80u, 1u, 2u, 4u})
        if (!ntr::cinematic_overlay_pixel(flag, 0, 0, 0)) return 10;
    for (unsigned flag : {0x20u, 0x5fu, 0xd0u, 0xc1u})
        if (ntr::cinematic_overlay_pixel(flag, 5, 0, 0x80)) return 11;
    if (ntr::cinematic_overlay_pixel(8, 5, 0, 0x80) ||
        ntr::cinematic_overlay_pixel(4, 5, 0x80, 0) ||
        ntr::cinematic_overlay_pixel(4, 6, 0, 0)) return 12;
    if (ntr::cinematic_argb6(0x3f3f3f, 0) != 0xffffffffu ||
        ntr::cinematic_argb6(0x3f3f3f, 0x8010) != 0xff000000u ||
        ntr::cinematic_argb6(0, 0x4010) != 0xffffffffu) return 13;
    for (double aspect : {4.0 / 3, 16.0 / 9, 21.0 / 9, 32.0 / 9}) {
        auto top = projection(4096), bottom = projection(-4096);
        if (!center_split_projection(top, aspect) || !center_split_projection(bottom, aspect)) return 1;
        for (size_t i = 0; i < top.size(); ++i)
            if (top[i].op != bottom[i].op || top[i].param != bottom[i].param) return 2;
        if (top[10].param != 0 || top[9].param != 0 || top[6].param != 2 * 4096 ||
            std::abs(double(top[6].param) / top[1].param - aspect) > 0.001) return 3;
    }
    if (!ntr::gx::has_centered_projection(projection(0)) ||
        ntr::gx::has_centered_projection(projection(4096))) return 7;
    auto orthographic = projection(0);
    orthographic[12].param = 0;
    orthographic[16].param = 17; // homogeneous scaling must not hide a valid orthographic UI camera
    if (!ntr::gx::has_centered_projection(orthographic)) return 8;
    auto truncated = projection(0);
    truncated.pop_back();
    if (ntr::gx::has_centered_projection(truncated)) return 9;
    auto centered = projection(0), invalid = projection(4096);
    if (center_split_projection(centered, 16.0 / 9) ||
        center_split_projection(invalid, std::numeric_limits<double>::quiet_NaN())) return 4;
    invalid.pop_back();
    if (center_split_projection(invalid, 16.0 / 9)) return 5;
    auto modelview = projection(4096);
    modelview[0].param = 1;
    if (center_split_projection(modelview, 16.0 / 9)) return 6;
    // A centered world half and an off-axis half must produce exactly the same union camera.
    for (const auto span : {std::pair{0, 9032}, {-9032, 0}, {-4096, 4096}, {1234, 5678}}) {
        ntr::CinematicCamera camera{true, span.first, span.second, false};
        auto low = projection(span.first), high = projection(span.second);
        if (!center_split_projection(low, 4.0 / 3, camera) ||
            !center_split_projection(high, 4.0 / 3, camera)) return 19;
        for (size_t i = 0; i < low.size(); ++i)
            if (low[i].param != high[i].param) return 20;
        // SDK clip-space boundaries of the two original halves map to -1/+1 in the union.
        for (auto endpoint : {std::pair{span.first - 4096, -1.0}, {span.second + 4096, 1.0}}) {
            const double y = double(endpoint.first) * int32_t(low[6].param) / (4 * 4096.0 * 4096.0) -
                double(int32_t(low[10].param)) / 4096.0;
            if (std::abs(y - endpoint.second) > 0.001) return 21;
        }
        auto ui = orthographic;
        if (center_split_projection(ui, 4.0 / 3, camera) || ui[16].param != orthographic[16].param) return 22;
    }
    if (ntr::gx::has_centered_projection(projection(0), false) ||
        !ntr::gx::has_centered_projection(orthographic, false)) return 23;
    auto explicit_invalid = projection(0);
    if (center_split_projection(explicit_invalid, 4.0 / 3, {true, 0, 0, false}) ||
        center_split_projection(explicit_invalid, 4.0 / 3, {true, -32769, 4096, false})) return 24;
    return 0;
}
