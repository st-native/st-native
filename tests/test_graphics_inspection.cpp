// SPDX-License-Identifier: GPL-3.0-or-later
#include "gx_inspect.hpp"

std::vector<ntr::gx::Entry> matrix(int mode, int offset, bool ortho = false) {
    std::vector<ntr::gx::Entry> entries{{0x10, uint32_t(mode)}};
    for (int i = 0; i < 16; ++i) entries.push_back({0x16, 0});
    entries[10].param = uint32_t(offset);
    entries[12].param = ortho ? 0 : uint32_t(-4096);
    entries[16].param = ortho ? 17 : 0;
    return entries;
}

int main() {
    using ntr::gx::inspect_projections;
    const auto empty = inspect_projections({});
    if (!empty.complete || empty.perspective_centered || empty.orthographic) return 1;
    auto stream = matrix(0, 4096);
    for (auto next : {matrix(0, -4096), matrix(0, 0), matrix(0, 0, true), matrix(1, 4096)})
        stream.insert(stream.end(), next.begin(), next.end());
    const auto original = stream;
    const auto result = inspect_projections(stream);
    if (!result.complete || result.perspective_positive_y != 1 || result.perspective_negative_y != 1 ||
        result.perspective_centered != 1 || result.orthographic != 1 || result.other != 0) return 2;
    for (size_t i = 0; i < stream.size(); ++i)
        if (stream[i].op != original[i].op || stream[i].param != original[i].param) return 3;
    auto truncated = matrix(0, 4096);
    truncated.pop_back();
    if (inspect_projections(truncated).complete) return 4;
    auto other = matrix(0, 0);
    other[12].param = 1;
    if (inspect_projections(other).other != 1) return 5;
    auto unknown_mode = matrix(0, 4096);
    unknown_mode.erase(unknown_mode.begin());
    if (inspect_projections(unknown_mode).perspective_positive_y != 0) return 6;
    return 0;
}
