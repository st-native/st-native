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
#include "../input/controls.hpp"
#include "../present/screen_enhancements.hpp"
#include "../present/screen_layout.hpp"

namespace st {

inline constexpr std::array kLayoutOptions{
    std::pair{ScreenLayout::single, "single"},
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
    SecondaryViewSettings secondary;
    AudioSettings audio;
    input::Controls controls;

    explicit LauncherPreferences(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary);
        file >> std::quoted(rom) >> scale >> language >> window;
        std::string layout_key, join_key;
        if (file >> layout_key) layout = parse_screen_layout(layout_key);
        if (file >> join_key) automatic_join = join_key != "manual";
        std::string line;
        bool legacy_controls = false;
        bool menu_bindings_version = false;
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
            } else if (version == "secondary-view-v1") {
                int size = 0, opacity = 75;
                if (entry >> size >> opacity && valid_secondary_size(size) && valid_secondary_opacity(opacity))
                    secondary = {size, opacity};
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
            } else if (version == "controls-v1" || version == "controls-v2" || version == "controls-v3") {
                legacy_controls = version == "controls-v1";
                menu_bindings_version = version == "controls-v3";
                int symbols = -1, deadzone = 49, threshold = 24;
                if (entry >> symbols >> deadzone >> threshold && symbols >= 0 && symbols < 4 &&
                    deadzone >= 10 && deadzone <= 90 && threshold >= 5 && threshold <= 95) {
                    controls.symbols = input::Symbols(symbols);
                    controls.deadzone = deadzone; controls.trigger_threshold = threshold;
                    int menus = 1;
                    if (version == "controls-v3" && entry >> menus && (menus == 0 || menus == 1)) controls.game_menus = menus != 0;
                }
            } else if (version == "binding-v1") {
                std::string action;
                size_t slot = 0;
                int kind = -1, code = 0;
                if (entry >> action >> slot >> kind >> code && kind >= 0 && kind <= int(input::Kind::trigger)) {
                    const auto found = std::find(input::action_ids.begin(),input::action_ids.end(),action);
                    const input::Binding binding{input::Kind(kind),code};
                    if (found != input::action_ids.end() && input::valid_slot(slot,binding) &&
                        !input::reserved(input::Action(found-input::action_ids.begin()),binding))
                        controls.bindings[size_t(found-input::action_ids.begin())][slot] = binding;
                }
            }
        }
        // Migrate the former default without silently reassigning a user's custom L3 action.
        using A = input::Action;
        if (legacy_controls && controls.bindings[size_t(A::menu)][2] == input::button(SDL_GAMEPAD_BUTTON_START) &&
            controls.bindings[size_t(A::start)][2] == input::button(SDL_GAMEPAD_BUTTON_BACK) &&
            controls.bindings[size_t(A::select)][2].kind == input::Kind::none) {
            controls.bindings[size_t(A::menu)][2] = {};
            controls.bindings[size_t(A::start)][2] = input::button(SDL_GAMEPAD_BUTTON_START);
            if (controls.assign(A::menu,2,input::button(SDL_GAMEPAD_BUTTON_LEFT_STICK)))
                controls.assign(A::select,2,input::button(SDL_GAMEPAD_BUTTON_BACK));
            else controls.assign(A::menu,2,input::button(SDL_GAMEPAD_BUTTON_BACK));
        }
        // Newly introduced contextual defaults must not invalidate an older custom preset.
        // An existing Start/utility assignment wins; the new menu action can be mapped in the editor.
        if (!menu_bindings_version)
            for (size_t a = size_t(A::menu_accept); a < input::action_count; ++a)
                for (size_t s = 0; s < 4; ++s)
                    if (const auto conflict = controls.conflict(A(a),s,controls.bindings[a][s]);
                        conflict && !input::menu_action(*conflict)) controls.bindings[a][s] = {};
        // The skip binding was introduced after the first v3 files. An
        // explicitly configured utility binding keeps priority over its default.
        auto& skip = controls.bindings[size_t(A::skip_cutscene)];
        for (size_t slot = 0; slot < skip.size(); ++slot)
            if (controls.conflict(A::skip_cutscene,slot,skip[slot])) skip[slot] = {};
        if (!controls.validate()) controls = {};
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
        file << "secondary-view-v1 " << (valid_secondary_size(secondary.size) ? secondary.size : 0) << ' '
             << (valid_secondary_opacity(secondary.opacity) ? secondary.opacity : 75) << '\n';
        const auto input = controls.validate() ? controls : st::input::Controls{};
        file << "controls-v3 " << int(input.symbols) << ' ' << input.deadzone << ' ' << input.trigger_threshold << ' ' << input.game_menus << '\n';
        for (size_t a = 0; a < st::input::action_count; ++a)
            for (size_t s = 0; s < 4; ++s)
                file << "binding-v1 " << st::input::action_ids[a] << ' ' << s << ' '
                     << int(input.bindings[a][s].kind) << ' ' << input.bindings[a][s].code << '\n';
        file.close();
        if (!file) return false;
        std::error_code error;
        std::filesystem::rename(temp, path, error);
        return !error;
    }
};

} // namespace st
