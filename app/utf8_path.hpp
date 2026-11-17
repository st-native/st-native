// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <filesystem>
#include <string>
#include <string_view>

// SDL paths and Windows SDL_main arguments are UTF-8. Keep that encoding across OS boundaries.
inline std::filesystem::path path_from_utf8(std::string_view value) {
    return std::filesystem::path(std::u8string(value.begin(), value.end()));
}

inline std::string path_to_utf8(const std::filesystem::path& path) {
    const auto value = path.u8string();
    return std::string(value.begin(), value.end());
}
