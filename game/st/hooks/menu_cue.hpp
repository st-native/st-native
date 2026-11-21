// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <bit>
#include "nitro_ui_asset.hpp"

namespace st {

inline std::vector<int16_t> load_menu_cue(std::span<const uint8_t> rom) {
    using namespace nitro_ui;
    // BKIE rev. 0, SoundData/final_sound_data.sdat. Two separate key presses
    // on the name keyboard started the same SPU sample. Its RAM contents
    // match FAT archive 843, wave 626, played with timer divisor 249.
    constexpr size_t sound_file = 3247, wave_archive = 843, wave = 626;
    const size_t fst = word(rom,0x48)+sound_file*8;
    const size_t lo = word(rom,fst), hi = word(rom,fst+4);
    if (lo >= hi || hi > rom.size()) return {};
    const auto sdat = rom.subspan(lo,hi-lo);
    const size_t fat = word(sdat,32);
    if (word(sdat,0) != 0x54414453u || word(sdat,fat) != 0x20544146u ||
        word(sdat,fat+8) <= wave_archive) return {};
    const size_t archive_offset = word(sdat,fat+12+16*wave_archive);
    const size_t archive_size = word(sdat,fat+16+16*wave_archive);
    if (archive_offset > sdat.size() || archive_size > sdat.size()-archive_offset) return {};
    const auto swar = sdat.subspan(archive_offset,archive_size);
    if (word(swar,0) != 0x52415753u || word(swar,56) <= wave) return {};
    const size_t offset = word(swar,60+4*wave);
    if (offset+16 > swar.size() || swar[offset] != 2 || half(swar,offset+2) != 33573) return {};
    const size_t length = (size_t(half(swar,offset+6))+word(swar,offset+8))*4;
    if (length < 8 || length > 8192 || offset+12+length > swar.size()) return {};
    const auto adpcm = swar.subspan(offset+12,length);

    // Standard IMA ADPCM steps and index changes, with the DS's lower
    // clipping bound. The source is read locally; no waveform is bundled.
    constexpr int steps[] = {
        7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,
        73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,
        408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,
        1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,
        7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,
        22385,24623,27086,29794,32767
    };
    constexpr int index_change[]{-1,-1,-1,-1,2,4,6,8};
    int value = std::bit_cast<int16_t>(half(adpcm,0)), index = std::min(88,int(adpcm[2]&127));
    std::vector<int16_t> decoded;
    decoded.reserve(1+2*(length-4));
    decoded.push_back(int16_t(value));
    for (const unsigned byte : adpcm.subspan(4))
        for (const unsigned code : {byte & 15, byte >> 4}) {
            const int step = steps[index];
            int diff = step/8;
            if (code & 1) diff += step/4;
            if (code & 2) diff += step/2;
            if (code & 4) diff += step;
            value = std::clamp(value + (code & 8 ? -diff : diff),-32767,32767);
            index = std::clamp(index+index_change[code&7],0,88);
            decoded.push_back(int16_t(value));
        }

    // Use the first pass of the original pitched cue. A short release is
    // applied to the host's one shot because the game sequence can loop it.
    constexpr unsigned playback_hz = 67297, output_hz = 48000;
    std::vector<int16_t> result((decoded.size()-1)*output_hz/playback_hz);
    for (size_t frame = 0; frame < result.size(); ++frame) {
        const size_t phase = frame*playback_hz, source = phase/output_hz;
        const int remainder = int(phase%output_hz);
        const int sample = int((int64_t(decoded[source])*(output_hz-remainder) +
                                int64_t(decoded[source+1])*remainder) / output_hz);
        const int release = int(std::min<size_t>(240,result.size()-1-frame));
        result[frame] = int16_t(sample*release/(240*3));
    }
    return result;
}

} // namespace st
