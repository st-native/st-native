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
            old.window != "fullscreen" || old.enhancements.mask() != 7) return 1;
        if (std::string_view(tail).find("manual") != std::string_view::npos && old.automatic_join) return 2;
    }
    for (unsigned mask = 0; mask < 8; ++mask) {
        st::LauncherPreferences original(path);
        original.layout = st::ScreenLayout::picture_in_picture;
        original.automatic_join = mask % 2 != 0;
        original.enhancements = {bool(mask & 1), bool(mask & 2), bool(mask & 4)};
        if (!original.save(path)) return 3; // replacement must work on an already existing Windows file
        st::LauncherPreferences loaded(path);
        if (loaded.layout != original.layout || loaded.enhancements != original.enhancements ||
            loaded.automatic_join != original.automatic_join || loaded.rom != original.rom) return 4;
        for (auto feature : {st::ScreenFeature::cutscenes, st::ScreenFeature::subtitles, st::ScreenFeature::hide_secondary})
            if (st::screen_feature_enabled(loaded.enhancements.mask(), feature) != bool(mask & unsigned(feature))) return 5;
    }
    for (const auto extension : {"screen-enhancements-v1 1 0", "screen-enhancements-v1 0 0 7", "future-format 0 0 0"}) {
        { std::ofstream file(path); file << base << "pip\nmanual\n" << extension; }
        st::LauncherPreferences damaged(path);
        if (damaged.automatic_join || damaged.enhancements.mask() != 7 ||
            damaged.layout != st::ScreenLayout::picture_in_picture) return 6;
    }
    return 0;
}
