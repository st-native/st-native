// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>

namespace st {

enum class ScreenFeature : uint32_t { cutscenes = 1, subtitles = 2, hide_secondary = 4 };

struct ScreenEnhancements {
    bool cutscenes = true, subtitles = true, hide_secondary = true;

    constexpr uint32_t mask() const {
        return (cutscenes ? 1u : 0u) | (subtitles ? 2u : 0u) | (hide_secondary ? 4u : 0u);
    }
    constexpr bool operator==(const ScreenEnhancements&) const = default;
};

constexpr bool screen_feature_enabled(uint32_t mask, ScreenFeature feature) {
    return (mask & uint32_t(feature)) != 0;
}

} // namespace st
