// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <limits>
#include "nitro_ui_asset.hpp"
#include <string_view>
#include <vector>

namespace st {

// BKIE rev. 0 has independent cells for its caption, ribbon, feather and credits.
// Replace only the caption's tiles in the verified ROM's in-memory NitroFS file.
class TitleCaption {
public:
    bool apply(std::span<uint8_t> rom, uint8_t language) {
        if (!load(rom, language)) return false;
        // These localized TitleM.bin entries were identified in the ROM's FNT.
        const unsigned file = language == 2 ? 396 : language == 5 ? 3303 : 238;
        const size_t fat = nitro_ui::word(rom,0x48) + file * 8;
        const size_t start = nitro_ui::word(rom,fat), end = nitro_ui::word(rom,fat+4);
        if (start >= end || end > rom.size()) return false;
        auto archive = nitro_ui::decompress(rom.subspan(start,end-start));
        if (archive.size() < 100 || nitro_ui::word(archive,0) != 0x4352414eu || nitro_ui::word(archive,16) != 0x46415442u)
            return false;
        const size_t names = 16 + nitro_ui::word(archive,20);
        if (names >= archive.size() || nitro_ui::word(archive,names) != 0x464e5442u) return false;
        const size_t images = names + nitro_ui::word(archive,names+4);
        if (images >= archive.size() || nitro_ui::word(archive,images) != 0x46494d47u) return false;
        auto part = [&](size_t i) -> std::span<uint8_t> {
            const size_t begin = images + 8 + nitro_ui::word(archive,28+i*8);
            const size_t limit = images + 8 + nitro_ui::word(archive,32+i*8);
            return begin < limit && limit <= archive.size() ?
                std::span<uint8_t>(archive).subspan(begin,limit-begin) : std::span<uint8_t>{};
        };
        // The second cell/character bank owns title UI. Cell 2 contains the glyph
        // sprite. Its tile range ends before cells 3/4, which draw the ribbon.
        const auto cells = part(7);
        const auto characters = part(8);
        if (nitro_ui::word(cells,0) != 0x4e434552u || nitro_ui::word(characters,0) != 0x4e434752u ||
            nitro_ui::half(cells,24) != 15 || nitro_ui::half(cells,26) != 1 || characters.size() < 48) return false;
        const size_t cell_table = 24 + nitro_ui::word(cells,28);
        const size_t objects = cell_table + 15*16;
        const size_t caption = cell_table + 2*16, ribbon = caption + 16;
        const size_t object_count = nitro_ui::half(cells,caption);
        const size_t first = objects + nitro_ui::word(cells,caption+4);
        const size_t ribbon_first = objects + nitro_ui::word(cells,ribbon+4);
        if (!object_count || object_count > 20 || first + object_count*6 > cells.size() ||
            ribbon_first + 6 > cells.size()) return false;
        // Asset survey verified that the first tile of the ribbon starts just
        // after all caption tiles. Some palettes repeat across other cells.
        const size_t last_caption_tile = language == 2 ? 99 : language == 5 ? 87 : 90;
        auto tiles = characters.subspan(48);
        std::array<bool,100> visited{};
        for (size_t i = 0; i < object_count; ++i) {
            const size_t object = first+i*6;
            const unsigned attr_y = nitro_ui::half(cells,object), attr_x = nitro_ui::half(cells,object+2), attr_tile = nitro_ui::half(cells,object+4);
            const unsigned shape = attr_y >> 14, size = attr_x >> 14, tile = attr_tile & 1023;
            constexpr int widths[3][4]{{8,16,32,64},{16,32,32,64},{8,8,16,32}};
            constexpr int heights[3][4]{{8,16,32,64},{8,8,16,32},{16,32,32,64}};
            if (shape >= 3 || attr_tile >> 12 != 1 || (attr_y & 0x2000) || (attr_x & 0x3000)) return false;
            const int w = widths[shape][size], h = heights[shape][size];
            const size_t count = size_t(w*h/64);
            if (tile < 30 || tile+count > last_caption_tile || (tile+count)*32 > tiles.size()) return false;
            for (size_t n = tile; n < tile+count; ++n) {
                if (visited[n]) return false;
                visited[n] = true;
            }
            const int x = int(attr_x & 255) - int(attr_x & 256);
            const int y = int(attr_y & 127) - int(attr_y & 128);
            for (int v = 0; v < h; ++v)
                for (int u = 0; u < w; ++u) {
                    const unsigned index = palette_index(x+u+width_/2,y+v+8);
                    const size_t pixel = (tile + size_t(v/8*(w/8)+u/8))*32 + (v%8)*4 + (u%8)/2;
                    const unsigned shift = (u&1)*4;
                    tiles[pixel] = uint8_t((tiles[pixel] & ~(15u<<shift)) | (index<<shift));
                }
        }
        if (!std::all_of(visited.begin()+30,visited.begin()+last_caption_tile,[](bool value){ return value; }))
            return false;
        const auto packed = compress(archive);
        if (packed.size() > end-start) return false;
        std::copy(packed.begin(),packed.end(),rom.begin()+start);
        std::fill(rom.begin()+start+packed.size(),rom.begin()+end,uint8_t{0});
        return true;
    }

private:
    bool load(std::span<const uint8_t> rom, uint8_t language) {
        // BKIE rev. 0 FAT entry for Font/DSZ2_msg.nftr, checked against the local FNT.
        const auto fat = nitro_ui::word(rom, 0x48);
        constexpr size_t file = 339;
        const size_t lo = nitro_ui::word(rom, size_t(fat) + file * 8);
        const size_t hi = nitro_ui::word(rom, size_t(fat) + file * 8 + 4);
        if (lo >= hi || hi > rom.size() || hi - lo < 4) return false;
        const auto font = nitro_ui::decompress(rom.subspan(lo, hi-lo));
        if (font.size() < 16 || nitro_ui::word(font,0) != 0x4e465452u) return false;
        size_t glyphs = 0, widths = 0, mapping = 0;
        for (size_t pos = 16; pos + 8 <= font.size();) {
            const auto size = nitro_ui::word(font,pos+4);
            if (size < 8 || size > font.size()-pos) return false;
            switch (nitro_ui::word(font,pos)) {
            case 0x43474c50u: glyphs = pos; break; // PLGC
            case 0x43574448u: widths = pos; break; // HDWC
            case 0x434d4150u: if (!mapping) mapping = pos; break; // PAMC
            }
            pos += size;
        }
        if (!glyphs || !widths || !mapping || glyphs+16 > font.size()) return false;
        const unsigned cell_x = font[glyphs+8], cell_y = font[glyphs+9];
        const unsigned stride = nitro_ui::half(font,glyphs+10), bpp = font[glyphs+14];
        if (cell_x > 16 || cell_y > 16 || bpp != 2 || stride*8 < cell_x*cell_y*bpp || font[glyphs+15]) return false;
        std::u16string_view message = u"Press any button!";
        if (language == 2) message = u"Appuyez sur une touche !";
        else if (language == 5) message = u"¡Pulsa cualquier botón!";
        int pen = 1;
        for (uint16_t ch : message) {
            const auto index = glyph(font,mapping,ch);
            const auto tile = glyphs + 16 + size_t(index) * stride;
            const auto width = widths + 16 + size_t(index) * 3;
            if (index == 0xffff || tile + stride > font.size() || width + 3 > font.size()) return false;
            const int advance = font[width+2];
            if (!advance || pen + int(cell_x) > text_width) return false;
            for (unsigned y = 0; y < cell_y; ++y)
                for (unsigned x = 0; x < cell_x; ++x) {
                    const unsigned bit = (y*cell_x+x)*2;
                    if ((font[tile+bit/8] >> (6-bit%8) & 3) != 0)
                        ink_[y*text_width+pen+x] = true;
                }
            pen += advance;
        }
        width_ = std::min(pen + 1, text_width);
        return true;
    }

    unsigned palette_index(int x, int y) const {
        auto ink = [&](int u, int v) {
            return u >= 0 && u < width_ && v >= 0 && v < 16 && ink_[v*text_width+u];
        };
        // Original caption palette: gold face and dark contour.
        if (ink(x,y)) return y < 6 ? 2 : y < 10 ? 3 : 4;
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
                if (ink(x+dx,y+dy)) return 9;
        return ink(x-2,y-2) ? 1 : 0;
    }

    static uint16_t glyph(std::span<const uint8_t> data, size_t pos, uint16_t code) {
        for (unsigned blocks = 0; blocks < 16 && pos+22 <= data.size(); ++blocks) {
            if (nitro_ui::word(data,pos) != 0x434d4150u) break;
            const auto first = nitro_ui::half(data,pos+8), last = nitro_ui::half(data,pos+10), method = nitro_ui::half(data,pos+12);
            if (code >= first && code <= last) {
                if (method == 0) return uint16_t(nitro_ui::half(data,pos+20)+code-first);
                if (method == 1) {
                    const size_t entry = pos + 20 + size_t(code-first)*2;
                    if (entry + 4 <= data.size()) {
                        const auto value = nitro_ui::half(data,entry);
                        if (value != 0xffff) return value;
                    }
                }
            }
            const auto next = nitro_ui::word(data,pos+16);
            if (next < 8 || next-8 <= pos) break;
            pos = next-8;
        }
        return 0xffff;
    }

    // Same LZ10 format as the original, with a bounded 4 KiB search window.
    static std::vector<uint8_t> compress(std::span<const uint8_t> input) {
        std::vector<uint8_t> result{0x10,uint8_t(input.size()),uint8_t(input.size()>>8),uint8_t(input.size()>>16)};
        constexpr size_t missing = std::numeric_limits<size_t>::max();
        std::array<size_t,4096> recent;
        recent.fill(missing);
        std::vector<size_t> previous(input.size(),missing);
        auto hash = [&](size_t pos) {
            return (input[pos]*251u + input[pos+1]*41u + input[pos+2]) & 4095u;
        };
        for (size_t pos = 0; pos < input.size();) {
            const size_t flag = result.size();
            result.push_back(0);
            for (unsigned bit = 128; bit && pos < input.size(); bit >>= 1) {
                const size_t max = std::min<size_t>(18,input.size()-pos);
                size_t best_length = 0, offset = 0;
                size_t candidate = max >= 3 ? recent[hash(pos)] : missing;
                while (candidate != missing && pos-candidate <= 4096) {
                    size_t length = 0;
                    while (length < max && input[candidate+length] == input[pos+length]) ++length;
                    if (length > best_length) {
                        best_length = length;
                        offset = pos-candidate;
                        if (length == max) break;
                    }
                    candidate = previous[candidate];
                }
                size_t consumed = 1;
                if (best_length >= 3) {
                    result[flag] |= uint8_t(bit);
                    const size_t code = ((best_length-3)<<12) | (offset-1);
                    result.push_back(uint8_t(code>>8));
                    result.push_back(uint8_t(code));
                    consumed = best_length;
                } else result.push_back(input[pos]);
                while (consumed--) {
                    if (pos+2 < input.size()) {
                        const auto key = hash(pos);
                        previous[pos] = recent[key];
                        recent[key] = pos;
                    }
                    ++pos;
                }
            }
        }
        return result;
    }

    static constexpr int text_width = 180;
    std::array<bool,16*text_width> ink_{};
    int width_ = 0;
};

} // namespace st
