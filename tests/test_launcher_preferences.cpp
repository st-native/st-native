// SPDX-License-Identifier: GPL-3.0-or-later
#include "../app/settings/launcher_preferences.hpp"

#include <chrono>

int main() {
    namespace fs = std::filesystem;
    const auto path = fs::temp_directory_path() / ("st-screen-preferences-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".cfg");
    struct Cleanup { fs::path path; ~Cleanup() { std::error_code error; fs::remove(path, error); } } cleanup{path};
    const std::string base = "\"C:/source files/own dump.nds\"\n3\nfr\nfullscreen\n";
    for (const auto tail : {"", "pip\n", "pip\nmanual\n"}) {
        { std::ofstream file(path); file << base << tail; }
        st::LauncherPreferences old(path);
        if (old.rom != "C:/source files/own dump.nds" || old.scale != "3" || old.language != "fr" ||
            old.window != "fullscreen" || old.enhancements.mask() != 7 || old.audio != st::AudioSettings{} ||
            old.pause_on_menu) return 1;
        if (std::string_view(tail).find("manual") != std::string_view::npos && old.automatic_join) return 2;
    }
    for (unsigned mask = 0; mask < 8; ++mask) {
        st::LauncherPreferences original(path);
        original.layout = st::ScreenLayout::picture_in_picture;
        original.automatic_join = mask % 2 != 0;
        original.pause_on_menu = (mask & 2) != 0;
        original.enhancements = {bool(mask & 1), bool(mask & 2), bool(mask & 4)};
        original.secondary = {mask % 2 ? 20 : 30, mask % 2 ? 50 : 100};
        original.audio = {
            int(mask * 10),
            (mask & 1) != 0,
            st::AudioInterpolation::cubic,
            "Headphones (USB Audio Device)",
            st::MicMode::hybrid,
            "Desk Microphone (2- USB Audio)",
            145,
        };
        if (!original.save(path)) return 3; // replacement must work on an already existing Windows file
        st::LauncherPreferences loaded(path);
        if (loaded.layout != original.layout || loaded.enhancements != original.enhancements ||
            loaded.automatic_join != original.automatic_join || loaded.rom != original.rom ||
            loaded.audio != original.audio || loaded.pause_on_menu != original.pause_on_menu ||
            loaded.secondary != original.secondary) return 4;
        for (auto feature : {st::ScreenFeature::cutscenes, st::ScreenFeature::subtitles, st::ScreenFeature::hide_secondary})
            if (st::screen_feature_enabled(loaded.enhancements.mask(), feature) != bool(mask & unsigned(feature))) return 5;
    }
    for (const auto extension : {
             "screen-enhancements-v1 1 0",
             "screen-enhancements-v1 0 0 7",
             "future-format 0 0 0",
             "audio-v1 150 0 none automatic 100 \"\" \"\"",
             "audio-v1 80 2 none automatic 100 \"\" \"\"",
             "audio-v1 80 0 invalid automatic 100 \"\" \"\"",
             "audio-v1 80 0 linear invalid 100 \"\" \"\"",
             "audio-v1 80 0 linear real 250 \"\" \"\"",
             "menu-v1",
             "menu-v1 2",
             "secondary-view-v1 40 75",
             "secondary-view-v1 20 25",
             "secondary-view-v1 20",
         }) {
        { std::ofstream file(path); file << base << "pip\nmanual\n" << extension; }
        st::LauncherPreferences damaged(path);
        if (damaged.automatic_join || damaged.enhancements.mask() != 7 ||
            damaged.layout != st::ScreenLayout::picture_in_picture ||
            damaged.audio != st::AudioSettings{} || damaged.pause_on_menu ||
            damaged.secondary != st::SecondaryViewSettings{}) return 6;
    }
    return 0;
}
