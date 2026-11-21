// SPDX-License-Identifier: GPL-3.0-or-later
#include "../app/present/subtitle_composition.hpp"
#include "../game/st/hooks/skip_prompt.hpp"

#include <climits>
#include <vector>

int main() {
    constexpr uint32_t background = 0xff305070, glyph = 0xfff0e0d0;
    std::vector<uint32_t> art(256 * 192, background), text(art.size(), 0), output(art.size(), 0);
    if (!st::compose_subtitles(output.data(), art.data(), text.data(), {40}) || output != art) return 1;
    text[81 * 256 + 21] = glyph;
    if (!st::compose_subtitles(output.data(), art.data(), text.data(), {40}) ||
        output[121 * 256 + 21] != glyph || output[81 * 256 + 21] != background ||
        output[112 * 256 + 21] != background || output[121 * 256 + 7] != background ||
        output[121 * 256 + 8] == background) return 2;
    text[81 * 256 + 22] = glyph;
    text[102 * 256 + 22] = glyph;
    if (!st::compose_subtitles(output.data(), art.data(), text.data(), {40}) ||
        output[121 * 256 + 21] != glyph || output[121 * 256 + 22] != glyph ||
        output[142 * 256 + 22] != glyph) return 3; // typewriter progress does not recenter the earlier text
    std::fill(text.begin(), text.end(), 0);
    text[80 * 256 + 30] = 0x80ff0000;
    if (!st::compose_subtitles(output.data(), art.data(), text.data(), {0, 8, 8, 0}) ||
        output[80 * 256 + 30] != 0xff982838) return 4;
    for (int offset : {INT_MIN, INT_MAX}) {
        if (!st::compose_subtitles(output.data(), art.data(), text.data(), {offset})) return 5;
        const int row = offset == INT_MIN ? 8 : 183;
        if (output[row * 256 + 30] == background) return 6;
    }
    // Long source layers fit inside the canvas; never write beyond the output allocation.
    std::fill(text.begin(), text.end(), glyph);
    std::vector<uint32_t> guarded(art.size() + 2, 0x12345678);
    if (!st::compose_subtitles(guarded.data() + 1, art.data(), text.data(), {INT_MAX}) ||
        guarded.front() != 0x12345678 || guarded.back() != 0x12345678) return 7;
    if (st::compose_subtitles(nullptr, art.data(), text.data()) ||
        st::compose_subtitles(output.data(), nullptr, text.data()) ||
        st::compose_subtitles(text.data(), art.data(), text.data()) ||
        st::compose_subtitles(output.data(), art.data(), text.data(), {0, 96})) return 8;
    if (st::native_skip_visible(art.data())) return 9;
    auto prompt = art;
    st::draw_skip_prompt(prompt.data(),"Enter",1,false);
    if (prompt[6 * 256 + 250] == background || prompt[16 * 256 + 230] != background) return 10;
    art[8 * 256 + 168] = art[14 * 256 + 175] = 0xff000000u;
    art[3 * 256 + 238] = art[3 * 256 + 246] = art[8 * 256 + 243] = art[8 * 256 + 251] = 0xfffbfbfbu;
    if (!st::native_skip_visible(art.data())) return 11;
    prompt = art;
    st::draw_skip_prompt(prompt.data(),"Menu",2,true);
    if (prompt[6 * 256 + 154] == background) return 12;
    for (int y = 0; y < 192; ++y)
        for (int x = 0; x < 256; ++x)
            if ((x >= 162 || y < 3 || y > 15) && prompt[y * 256 + x] != art[y * 256 + x]) return 13;
    return 0;
}
