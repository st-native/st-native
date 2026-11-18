// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include "../audio/audio_settings.hpp"
#include "../present/screen_enhancements.hpp"
#include "../present/screen_layout.hpp"

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
    bool pause_on_menu = false;
    ScreenEnhancements enhancements;
    AudioSettings audio;

    explicit LauncherPreferences(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary);
        file >> std::quoted(rom) >> scale >> language >> window;
        std::string layout_key, join_key;
        if (file >> layout_key) layout = parse_screen_layout(layout_key);
        if (file >> join_key) automatic_join = join_key != "manual";
        std::string line;
        std::getline(file, line);  // finish join_key's line
        while (std::getline(file, line)) {
            std::istringstream entry(line);
            std::string version;
            if (!(entry >> version)) continue;
            if (version == "screen-enhancements-v1") {
                int cutscenes = 0, subtitles = 0, hide = 0;
                if (entry >> cutscenes >> subtitles >> hide &&
                    (cutscenes == 0 || cutscenes == 1) && (subtitles == 0 || subtitles == 1) && (hide == 0 || hide == 1))
                    enhancements = {cutscenes != 0, subtitles != 0, hide != 0};
            } else if (version == "audio-v1") {
                int volume = 100, mute = 0, mic_gain = 100;
                std::string interp_key, mic_key, out_dev, in_dev;
                if (entry >> volume >> mute >> interp_key >> mic_key >> mic_gain >>
                        std::quoted(out_dev) >> std::quoted(in_dev) &&
                    volume >= 0 && volume <= 100 && (mute == 0 || mute == 1) &&
                    valid_audio_interpolation(interp_key) && valid_mic_mode(mic_key) &&
                    mic_gain >= 0 && mic_gain <= 200) {
                    audio = {volume, mute != 0, parse_audio_interpolation(interp_key), std::move(out_dev),
                             parse_mic_mode(mic_key), std::move(in_dev), mic_gain};
                }
            } else if (version == "menu-v1") {
                int pause = 0;
                if (entry >> pause && (pause == 0 || pause == 1)) pause_on_menu = pause != 0;
            }
        }
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
             << enhancements.hide_secondary << '\n'
             << "audio-v1 " << clamp_volume(audio.volume) << ' ' << audio.mute_unfocused << ' '
             << audio_interpolation_value(audio.interpolation) << ' ' << mic_mode_value(audio.mic_mode) << ' '
             << clamp_mic_gain(audio.mic_gain) << ' ' << std::quoted(audio.output_device) << ' '
             << std::quoted(audio.input_device) << '\n'
             << "menu-v1 " << pause_on_menu << '\n';
        file.close();
        if (!file) return false;
        std::error_code error;
        std::filesystem::rename(temp, path, error);
        return !error;
    }
};

} // namespace st
