// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <array>
#include "menu_controls.hpp"
#include "nitro_ui_asset.hpp"

namespace st {

// Hover variants keep the pixel shape, ink, icons and edges of the game art.
// The source palettes come from the player's verified BKIE rev. 0 dump.
class MenuArt {
public:
    void load(std::span<const uint8_t> rom, unsigned language) {
        auto select = archive(rom,3195); // Screen/Layout/SelectM.bin
        read(select,38,1,slot_);
        read(select,38,3,start_);
        read(select,38,12,back_);
        read(select,38,0,orange_);
        read(select,38,4,battle_);
        read(select,38,5,tag_);
        read(select,38,6,options_);
        read(select,13,11,choice_);
        read(select,13,10,cancel_);
        read(select,13,9,ok_);
        // The name keyboard's BG artwork shows its character, Shift and space keys.
        auto keyboard = archive(rom,language == 2 ? 373 : language == 5 ? 3280 : 215);
        read(keyboard,1,12,key_);
    }

    void draw(uint32_t* pixels, GameMenu menu, size_t index, MenuItem item) const {
        const auto* palette = style(menu,index,item);
        if (!palette) return;
        for (int y = item.y; y < item.y+item.height; ++y)
            for (int x = item.x; x < item.x+item.width; ++x) {
                auto& pixel = pixels[y*256+x];
                const auto color = (pixel>>19&31) | (pixel>>11&31)<<5 | (pixel>>3&31)<<10;
                for (const auto& entry : *palette) {
                    if (entry.hover && color == entry.original) {
                        pixel = entry.hover;
                        break;
                    }
                }
            }
    }

private:
    struct Color { unsigned original = 0, hover = 0; };
    using Palette = std::array<Color,15>;

    static std::vector<uint8_t> archive(std::span<const uint8_t> rom, size_t file) {
        using namespace nitro_ui;
        const size_t fat = word(rom,0x48) + file*8;
        const size_t start = word(rom,fat), end = word(rom,fat+4);
        if (start >= end || end > rom.size()) return {};
        return decompress(rom.subspan(start,end-start));
    }

    static void read(std::span<const uint8_t> archive, size_t file, unsigned bank, Palette& output) {
        using namespace nitro_ui;
        if (word(archive,0) != 0x4352414eu || word(archive,16) != 0x46415442u ||
            half(archive,24) <= file) return;
        const size_t names = 16+word(archive,20);
        const size_t images = names+word(archive,names+4);
        if (word(archive,names) != 0x464e5442u || word(archive,images) != 0x46494d47u) return;
        const size_t palette = images+8+word(archive,28+file*8);
        if (word(archive,palette) != 0x4e434c52u || palette+40+(bank+1)*32 > archive.size()) return;
        for (size_t i = 1; i < 16; ++i) {
            const auto color = half(archive,palette+40+bank*32+i*2) & 32767u;
            const unsigned r = color&31, g = color>>5&31, b = color>>10&31;
            const auto light = std::max({r,g,b}), dark = std::min({r,g,b});
            if (light < 9) continue;
            unsigned nr = std::min(31u,r+4), ng = std::min(31u,g+4), nb = std::min(31u,b+1);
            if (light-dark < 4 && light >= 17) {
                // Warm the light faces and preserve dark lettering.
                nr = std::min(31u,r+3);
                ng = std::min(31u,g+1);
                nb = b > 7 ? b-7 : 0;
            }
            const auto expand = [](unsigned v) { return (v << 3) | (v >> 2); };
            output[i-1] = {color,0xff000000u | expand(nr)<<16 | expand(ng)<<8 | expand(nb)};
        }
    }

    const Palette* style(GameMenu menu, size_t index, MenuItem item) const {
        switch (menu) {
        case GameMenu::slots: return &slot_;
        case GameMenu::file_actions: return index == 0 ? &start_ : index == 1 ? &back_ : &orange_;
        case GameMenu::mode: {
            const std::array palettes{&start_,&battle_,&back_,&tag_,&options_};
            return index < palettes.size() ? palettes[index] : nullptr;
        }
        case GameMenu::confirm_name:
        case GameMenu::handedness:
        case GameMenu::confirm_hand: return &choice_;
        case GameMenu::name: return item.y != 164 ? &key_ : item.x < 120 ? &cancel_ : &ok_;
        default: return nullptr;
        }
    }

    Palette slot_{}, start_{}, back_{}, orange_{}, battle_{}, tag_{}, options_{},
        choice_{}, cancel_{}, ok_{}, key_{};
};

} // namespace st
