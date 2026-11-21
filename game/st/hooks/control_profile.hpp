// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <string_view>

namespace st {
// Labels for the proposed host instrument actions. Not guest coordinates or a context detector.
inline constexpr std::array<std::string_view,6> pipe_control_titles{
    "Green pipe","White pipe","Blue pipe","Orange pipe","Yellow pipe","Purple pipe"
};
} // namespace st
