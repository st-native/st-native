// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <string>
#include <string_view>
#include <utility>

#include "../present/screen_layout.hpp"
#include "../present/screen_enhancements.hpp"

namespace st {

inline constexpr std::array kLayoutOptions{
    std::pair{ScreenLayout::focus, "focus"}, std::pair{ScreenLayout::classic, "classic"},
    std::pair{ScreenLayout::side_by_side, "side-by-side"}, std::pair{ScreenLayout::picture_in_picture, "pip"},
};

inline ScreenLayout parse_screen_layout(std::string_view value) {
    for (const auto& [layout, key] : kLayoutOptions)
        if (value == key) return layout;
    return ScreenLayout::focus;
}

inline const char* layout_value(ScreenLayout layout) {
    for (const auto& [choice, key] : kLayoutOptions)
        if (choice == layout) return key;
    return "focus";
}

struct LauncherPreferences {
    std::string rom, scale = "2", language = "en", window = "windowed";
    ScreenLayout layout = ScreenLayout::focus;
    bool automatic_join = true;
    ScreenEnhancements enhancements;

    explicit LauncherPreferences(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary);
        file >> std::quoted(rom) >> scale >> language >> window;
        std::string layout_key, join_key;
        if (file >> layout_key) layout = parse_screen_layout(layout_key);
        if (file >> join_key) automatic_join = join_key != "manual";
        std::string version;
        int cutscenes, subtitles, hide;
        if (file >> version && version == "screen-enhancements-v1" && file >> cutscenes >> subtitles >> hide &&
            (cutscenes == 0 || cutscenes == 1) && (subtitles == 0 || subtitles == 1) && (hide == 0 || hide == 1))
            enhancements = {cutscenes != 0, subtitles != 0, hide != 0};
        if (scale != "1" && scale != "2" && scale != "3") scale = "2";
        if (language != "en" && language != "fr" && language != "es") language = "en";
        if (window != "fullscreen") window = "windowed";
    }

    bool save(const std::filesystem::path& path) const {
        auto temp = path;
        temp += ".tmp";
        std::ofstream file(temp, std::ios::binary | std::ios::trunc);
        file << std::quoted(rom) << '\n' << scale << '\n' << language << '\n' << window << '\n'
             << layout_value(layout) << '\n' << (automatic_join ? "automatic" : "manual") << '\n'
             << "screen-enhancements-v1 " << enhancements.cutscenes << ' ' << enhancements.subtitles << ' '
             << enhancements.hide_secondary << '\n';
        file.close();
        if (!file) return false;
        std::error_code error;
        std::filesystem::rename(temp, path, error);
        return !error;
    }
};

} // namespace st
