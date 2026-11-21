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
            old.pause_on_menu || old.controls != st::input::Controls{}) return 1;
        if (std::string_view(tail).find("manual") != std::string_view::npos && old.automatic_join) return 2;
    }
    for (unsigned mask = 0; mask < 8; ++mask) {
        st::LauncherPreferences original(path);
        original.layout = st::ScreenLayout::picture_in_picture;
        original.automatic_join = mask % 2 != 0;
        original.pause_on_menu = (mask & 2) != 0;
        original.enhancements = {bool(mask & 1), bool(mask & 2), bool(mask & 4)};
        original.secondary = {mask % 2 ? 20 : 30, mask % 2 ? 50 : 100};
        original.controls.symbols = st::input::Symbols(mask % 4);
        original.controls.deadzone = 30;
        original.controls.trigger_threshold = 40;
        original.controls.game_menus = mask % 2 != 0;
        // A persisted simultaneous swap must not be rejected against intermediate defaults.
        std::swap(original.controls.bindings[size_t(st::input::Action::a)][0],
                  original.controls.bindings[size_t(st::input::Action::b)][0]);
        original.controls.bindings[size_t(st::input::Action::note1)][0] = st::input::key(SDL_SCANCODE_G);
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
            loaded.secondary != original.secondary || loaded.controls != original.controls) return 4;
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
             "controls-v1 8 49 24",
             "controls-v1 0 0 24",
             "controls-v1 0 49 9999",
             "binding-v1 instrument.note1 0 1 22",
             "binding-v1 ds.a 999 1 10",
             "binding-v1 unknown.action 0 1 10",
             "binding-v1 ds.a 0 1 99999",
         }) {
        { std::ofstream file(path); file << base << "pip\nmanual\n" << extension; }
        st::LauncherPreferences damaged(path);
        if (damaged.automatic_join || damaged.enhancements.mask() != 7 ||
            damaged.layout != st::ScreenLayout::picture_in_picture ||
            damaged.audio != st::AudioSettings{} || damaged.pause_on_menu ||
            damaged.secondary != st::SecondaryViewSettings{} || damaged.controls != st::input::Controls{}) return 6;
    }
    // The former automatic v1 preset must also change for existing users.
    for (bool custom_l3 : {false,true}) {
        auto legacy = st::input::Controls{};
        using A = st::input::Action;
        legacy.bindings[size_t(A::start)][2] = st::input::button(SDL_GAMEPAD_BUTTON_BACK);
        legacy.bindings[size_t(A::select)][2] = {};
        legacy.bindings[size_t(A::menu)][2] = st::input::button(SDL_GAMEPAD_BUTTON_START);
        legacy.bindings[size_t(A::a)][0] = st::input::key(SDL_SCANCODE_G);
        if (custom_l3) legacy.bindings[size_t(A::note1)][2] = st::input::button(SDL_GAMEPAD_BUTTON_LEFT_STICK);
        {
            std::ofstream file(path); file << base << "pip\nmanual\ncontrols-v1 2 30 40\n";
            for (size_t a=0;a<st::input::action_count;++a)
                for (size_t s=0;s<4;++s) file << "binding-v1 " << st::input::action_ids[a] << ' ' << s << ' '
                    << int(legacy.bindings[a][s].kind) << ' ' << legacy.bindings[a][s].code << '\n';
        }
        st::LauncherPreferences migrated(path);
        if (!migrated.controls.validate() || migrated.controls.symbols!=st::input::Symbols::playstation ||
            migrated.controls.bindings[size_t(A::start)][2]!=st::input::button(SDL_GAMEPAD_BUTTON_START) ||
            migrated.controls.bindings[size_t(A::a)][0]!=st::input::key(SDL_SCANCODE_G)) return 7;
        if (!custom_l3 && (migrated.controls.bindings[size_t(A::menu)][2]!=st::input::button(SDL_GAMEPAD_BUTTON_LEFT_STICK) ||
                          migrated.controls.bindings[size_t(A::select)][2]!=st::input::button(SDL_GAMEPAD_BUTTON_BACK))) return 8;
        if (custom_l3 && migrated.controls.bindings[size_t(A::note1)][2]!=legacy.bindings[size_t(A::note1)][2]) return 9;
    }
    { std::ofstream file(path); file << base << "pip\nmanual\ncontrols-v2 0 49 24\n"
        << "binding-v1 ds.start 0 1 " << int(SDL_SCANCODE_X) << '\n'
        << "binding-v1 ds.a 0 0 0\n"; }
    st::LauncherPreferences custom_old(path);
    if (!custom_old.controls.validate() || custom_old.controls.bindings[size_t(st::input::Action::start)][0]!=st::input::key(SDL_SCANCODE_X) ||
        custom_old.controls.bindings[size_t(st::input::Action::menu_accept)][0].kind!=st::input::Kind::none) return 10;
    return 0;
}
