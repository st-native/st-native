// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <span>
#include <vector>

namespace st::nitro_ui {
inline uint32_t word(std::span<const uint8_t> s, size_t i) {
    if (i > s.size() || s.size()-i < 4) return 0;
    return uint32_t(s[i]) | (uint32_t(s[i+1])<<8) | (uint32_t(s[i+2])<<16) | (uint32_t(s[i+3])<<24);
}
inline uint16_t half(std::span<const uint8_t> s, size_t i) { return word(s,i) & 0xffffu; }

inline std::vector<uint8_t> decompress(std::span<const uint8_t> input) {
    const unsigned header = word(input,0);
    const size_t size = header >> 8;
    if ((header & 255) != 0x10 || !size || size > 128*1024) return {};
    std::vector<uint8_t> out;
    out.reserve(size);
    size_t pos = 4;
    while (out.size() < size && pos < input.size()) {
        const unsigned bits = input[pos++];
        for (unsigned flag = 128; flag && out.size() < size; flag >>= 1) {
            if (pos >= input.size()) return {};
            if (!(bits & flag)) {
                out.push_back(input[pos++]);
                continue;
            }
            if (input.size()-pos < 2) return {};
            const unsigned token = input[pos]*256u + input[pos+1];
            pos += 2;
            const unsigned offset = (token & 4095)+1;
            const unsigned count = (token >> 12)+3;
            if (offset > out.size() || count > size-out.size()) return {};
            for (unsigned j = 0; j < count; ++j) out.push_back(out[out.size()-offset]);
        }
    }
    return out.size() == size ? out : std::vector<uint8_t>{};
}

} // namespace st::nitro_ui
