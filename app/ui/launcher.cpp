// SPDX-License-Identifier: GPL-3.0-or-later
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <RmlUi/Core.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>
#include <RmlUi/Core/StringUtilities.h>

#include <array>
#include <cstdlib>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "../utf8_path.hpp"
#include "../game_session.hpp"
#include "../present/launcher_window.hpp"
#include "../present/cursor_interface.hpp"
#include "../setup/launcher_worker.hpp"
#include "../settings/launcher_preferences.hpp"

namespace fs = std::filesystem;

namespace {

constexpr const char* kTitle = "Spirit Tracks Native";
constexpr int kWidth = 800, kHeight = 610;

using st::kLayoutOptions;
using st::parse_screen_layout;
using st::layout_value;
using Preferences = st::LauncherPreferences;

constexpr std::array kEnhancementIds{"unify-cutscenes", "narrative-subtitles", "hide-secondary"};

st::ScreenEnhancements read_enhancements(Rml::ElementDocument& document) {
    auto on = [&](const char* id) {
        return static_cast<Rml::ElementFormControl&>(*document.GetElementById(id)).GetValue() != "off";
    };
    return {on(kEnhancementIds[0]), on(kEnhancementIds[1]), on(kEnhancementIds[2])};
}

void show_enhancements(Rml::ElementDocument& document, st::ScreenEnhancements settings) {
    const std::array values{settings.cutscenes, settings.subtitles, settings.hide_secondary};
    for (size_t i = 0; i < values.size(); ++i) {
        auto& control = static_cast<Rml::ElementFormControl&>(*document.GetElementById(kEnhancementIds[i]));
        const char* value = values[i] ? "on" : "off";
        if (control.GetValue() != value) control.SetValue(value);
    }
}

void enable_enhancements(Rml::ElementDocument& document, bool enabled) {
    for (const auto* id : kEnhancementIds)
        static_cast<Rml::ElementFormControl&>(*document.GetElementById(id)).SetDisabled(!enabled);
}

bool select_settings_tab(Rml::ElementDocument& document, const std::string& id) {
    if (!id.starts_with("tab-") || !document.GetElementById(id)) return false;
    for (const std::string key : {"screens", "features", "window", "game"}) {
        auto* tab = document.GetElementById("tab-" + key);
        if (!tab) continue;
        const bool selected = id == tab->GetId();
        tab->SetClass("selected", selected);
        document.GetElementById("settings-" + key)->SetClass("selected", selected);
    }
    return true;
}

class Launcher final : public Rml::EventListener {
    enum class Action { inspect, verify, play, rebuild, remove };

public:
    Launcher(Rml::ElementDocument& document, SDL_Window* window, fs::path checkout, fs::path settings)
        : document_(document), window_(window), checkout_(std::move(checkout)), settings_(std::move(settings)),
          prefs_(settings_) {
        control("scale").SetValue(prefs_.scale);
        control("language").SetValue(prefs_.language);
        control("window-mode").SetValue(prefs_.window);
        control("screen-layout").SetValue(layout_value(prefs_.layout));
        control("auto-join").SetValue(prefs_.automatic_join ? "automatic" : "manual");
        show_enhancements(document_, prefs_.enhancements);
        document_.AddEventListener(Rml::EventId::Click, this);
        document_.AddEventListener(Rml::EventId::Change, this);
        update_rom_label();
        if (!prefs_.rom.empty()) start(Action::verify);
        else if (!settings_.empty()) start(Action::inspect);
        else update_buttons();
    }

    ~Launcher() override {
        document_.RemoveEventListener(Rml::EventId::Click, this);
        document_.RemoveEventListener(Rml::EventId::Change, this);
    }

    void ProcessEvent(Rml::Event& event) override {
        if (event.GetId() == Rml::EventId::Change) {
            prefs_.scale = control("scale").GetValue();
            prefs_.language = control("language").GetValue();
            prefs_.window = control("window-mode").GetValue();
            prefs_.layout = parse_screen_layout(control("screen-layout").GetValue());
            prefs_.automatic_join = control("auto-join").GetValue() != "manual";
            prefs_.enhancements = read_enhancements(document_);
            persist();
            update_buttons();
            return;
        }
        auto* target = event.GetTargetElement();
        while (target && target->GetId().empty()) target = target->GetParentNode();
        if (!target) return;
        const auto& id = target->GetId();
        if (select_settings_tab(document_, id)) return;
        if (id == "toggle-steps") {
            show_steps(!steps_visible_);
            return;
        }
        if (id == "open-log") {
            if (!open_launcher_log(checkout_, run_log_)) show_status("Could not open the build log.", true);
            return;
        }
        if (busy()) return;
        if (id == "browse") {
            static constexpr SDL_DialogFileFilter filters[] = {{"Nintendo DS ROM", "nds"}};
            dialog_open_ = true;
            update_buttons();
            SDL_ShowOpenFileDialog(selected, this, window_, filters, 1, nullptr, false);
        } else if (id == "play" && verified_) {
            start(Action::play);
        } else if (id == "rebuild" && verified_) {
            show_steps(true);
            start(Action::rebuild);
        } else if (id == "remove-cache" && cached_builds_ > 0 && confirm_remove()) {
            start(Action::remove);
        }
    }

    void update() {
        std::optional<std::string> selection;
        {
            std::scoped_lock lock(dialog_mutex_);
            selection.swap(selection_);
        }
        if (selection) {
            dialog_open_ = false;
            if (*selection == "\n") show_status("The file picker could not be opened.", true);
            else if (!selection->empty()) verify_rom(std::move(*selection));
            update_buttons();
        }

        if (!process_) return;
        read_status();
        update_build_elapsed();
        int result = 0;
        if (!SDL_WaitProcess(process_.get(), false, &result)) return;
        read_status();
        process_.reset();
        if (build_started_) {
            update_build_elapsed();
            build_started_.reset();
            element("build-stage").SetInnerRML(result == 0 ? "Build complete" : "Build stopped");
            element("build-activity").SetInnerRML("");
            if (result == 0) build_meter().SetValue(build_meter().GetMax());
        }
        if (result != 0 && !reported_error_) show_status("Setup stopped unexpectedly. See docs/BUILDING.md.", true);
        if (action_ == Action::verify) {
            verified_ = result == 0;
            if (verified_) persist();
        } else if (action_ == Action::play && result == 0 && !module_key_.empty()) {
            const auto module = checkout_ / "build/launcher/cache" / module_key_ / "st_game.dll";
            const uint8_t language = prefs_.language == "fr" ? 2 : prefs_.language == "es" ? 5 : 1;
            ready_ = st::GameLaunch{path_from_utf8(prefs_.rom), module, language,
                                    prefs_.scale[0] - '0', prefs_.window == "fullscreen", prefs_.layout,
                                    prefs_.automatic_join, prefs_.enhancements};
        } else if (action_ == Action::play && result == 0) {
            show_status("Preparation did not return a game module.", true);
        }
        update_buttons();
    }

    bool busy() const { return dialog_open_ || process_ != nullptr; }
    void save_display_mode(bool fullscreen) {
        prefs_.window = fullscreen ? "fullscreen" : "windowed";
        control("window-mode").SetValue(prefs_.window);
        persist();
    }
    st::ScreenLayout screen_layout() const { return prefs_.layout; }
    bool automatic_join() const { return prefs_.automatic_join; }
    st::ScreenEnhancements screen_enhancements() const { return prefs_.enhancements; }
    void save_screen_settings(st::ScreenLayout layout, bool automatic_join, st::ScreenEnhancements enhancements) {
        control("screen-layout").SetValue(layout_value(layout));
        control("auto-join").SetValue(automatic_join ? "automatic" : "manual");
        show_enhancements(document_, enhancements);
        prefs_.layout = layout;
        prefs_.automatic_join = automatic_join;
        prefs_.enhancements = enhancements;
        persist();
        update_buttons();
    }
    std::optional<st::GameLaunch> take_launch() { return std::exchange(ready_, std::nullopt); }

    void finish_game(int exit_code) {
        show_status(exit_code == 0 ? "Ready to play." : "The game could not continue. See docs/BUILDING.md.", exit_code != 0);
    }

    void game_busy() { show_status("Another launcher is preparing or running the game.", true); }

    void verify_rom(std::string path) {
        prefs_.rom = std::move(path);
        update_rom_label();
        start(Action::verify);
    }

private:
    bool confirm_remove() const {
        constexpr SDL_MessageBoxButtonData buttons[] = {
            {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT | SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 0, "Cancel"},
            {0, 1, "Remove cache"},
        };
        const SDL_MessageBoxData confirmation{
            SDL_MESSAGEBOX_WARNING, window_, "Remove cached game modules?",
            "This removes every build in the launcher's cache. Your ROM, saves, settings and build logs are kept.",
            int(std::size(buttons)), buttons, nullptr,
        };
        int chosen = 0;
        return SDL_ShowMessageBox(&confirmation, &chosen) && chosen == 1;
    }

    static void SDLCALL selected(void* userdata, const char* const* files, int) {
        auto& launcher = *static_cast<Launcher*>(userdata);
        std::scoped_lock lock(launcher.dialog_mutex_);
        launcher.selection_ = files ? (files[0] ? files[0] : "") : "\n";
    }

    Rml::Element& element(const char* id) { return *document_.GetElementById(id); }

    Rml::ElementFormControl& control(const char* id) {
        return static_cast<Rml::ElementFormControl&>(element(id));
    }

    void persist() {
        if (settings_.empty()) return;
        // Persist the most recent verified path only. Retain preferences after an invalid selection.
        auto persisted = prefs_;
        if (!verified_) persisted.rom = Preferences(settings_).rom;
        if (!persisted.save(settings_)) show_status("Could not save launcher preferences.", true);
    }

    void show_status(const std::string& text, bool error = false) {
        element("status").SetClass("error", error);
        element("status").SetInnerRML(Rml::StringUtilities::EncodeRml(text));
    }

    void update_rom_label() {
        const auto filename = path_to_utf8(path_from_utf8(prefs_.rom).filename());
        auto& label = element("rom");
        label.SetClass("selected", !filename.empty());
        label.SetInnerRML(filename.empty() ? "No ROM selected" : Rml::StringUtilities::EncodeRml(filename));
    }

    void update_buttons() {
        control("browse").SetDisabled(busy());
        control("play").SetDisabled(busy() || !verified_);
        control("rebuild").SetDisabled(busy() || !verified_);
        control("remove-cache").SetDisabled(busy() || cached_builds_ == 0);
        control("open-log").SetDisabled(run_log_.empty());
        for (const auto id : {"scale", "language", "window-mode", "screen-layout", "auto-join"})
            control(id).SetDisabled(busy());
        enable_enhancements(document_, !busy() && prefs_.automatic_join);
    }

    void show_steps(bool show) {
        steps_visible_ = show;
        element("build-details").SetClass("visible", show);
        control("toggle-steps").SetValue(show ? "Hide steps" : "Show steps");
    }

    void append_step(const std::string& line) {
        build_steps_ += "<p>" + Rml::StringUtilities::EncodeRml(line) + "</p>";
        auto& list = element("build-steps");
        const bool follow = list.GetScrollTop() + list.GetClientHeight() >= list.GetScrollHeight() - 1;
        list.SetInnerRML(build_steps_);
        if (steps_visible_ && follow) {
            document_.UpdateDocument();
            list.SetScrollTop(list.GetScrollHeight());
        }
    }

    Rml::ElementProgress& build_meter() {
        return static_cast<Rml::ElementProgress&>(element("build-meter"));
    }

    void show_build_stage(const std::string& update) {
        std::istringstream fields(update);
        unsigned stage, total;
        std::string label;
        if (!(fields >> stage >> total >> std::ws) || stage == 0 || stage > total ||
            !std::getline(fields, label)) return;
        if (!build_started_) {
            build_started_ = SDL_GetTicks();
            build_elapsed_ = 0;
            element("build-elapsed").SetInnerRML("0s elapsed");
            element("build-progress").SetClass("visible", true);
        }
        build_meter().SetMax(float(total));
        build_meter().SetValue(float(stage - 1));
        element("build-stage").SetInnerRML("Stage " + std::to_string(stage) + " of " +
            std::to_string(total) + " · " + Rml::StringUtilities::EncodeRml(label));
    }

    void update_build_elapsed() {
        if (!build_started_) return;
        const auto seconds = (SDL_GetTicks() - *build_started_) / 1000;
        if (seconds == build_elapsed_) return;
        build_elapsed_ = seconds;
        std::string elapsed;
        if (seconds >= 60) elapsed = std::to_string(seconds / 60) + "m ";
        element("build-elapsed").SetInnerRML(elapsed + std::to_string(seconds % 60) + "s elapsed");
    }

    void start(Action action) {
        action_ = action;
        reported_error_ = false;
        if (action == Action::verify) verified_ = false;
        if (action == Action::verify || action == Action::play || action == Action::rebuild)
            element("build-progress").SetClass("visible", false);
        std::vector<std::string> args;
        switch (action) {
        case Action::inspect: args = {"--cache-info"}; break;
        case Action::remove: args = {"--remove-cache"}; break;
        case Action::verify: args = {"--verify", "--rom", prefs_.rom}; break;
        case Action::play: args = {"--prepare-for-window", "--rom", prefs_.rom}; break;
        case Action::rebuild: args = {"--rebuild", "--rom", prefs_.rom}; break;
        }
        module_key_.clear();
        pending_.clear();
        process_.reset(start_launcher_worker(checkout_, args));
        if (!process_) show_status("Could not start Python. See docs/BUILDING.md.", true);
        else if (action == Action::remove) show_status("Removing cached modules...");
        else if (action != Action::inspect) show_status("Checking the ROM...");
        update_buttons();
    }

    void read_status() {
        auto* output = SDL_GetProcessOutput(process_.get());
        if (!output) return;
        char bytes[512];
        for (size_t n; (n = SDL_ReadIO(output, bytes, sizeof(bytes))) != 0; ) {
            pending_.append(bytes, n);
            for (size_t end; (end = pending_.find('\n')) != std::string::npos; ) {
                auto line = pending_.substr(0, end);
                pending_.erase(0, end + 1);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (line.starts_with("MODULE ")) {
                    const auto key = line.substr(7);
                    if (key.size() == 64 && std::ranges::all_of(key, [](char c) {
                            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
                        })) module_key_ = key;
                    continue;
                }
                if (line.starts_with("CACHE ")) {
                    std::istringstream fields(line.substr(6));
                    unsigned count;
                    double mib;
                    if (fields >> count >> mib) {
                        cached_builds_ = count;
                        std::ostringstream label;
                        if (count == 0) label << "Cache empty";
                        else label << count << (count == 1 ? " cached build · " : " cached builds · ")
                                   << std::fixed << std::setprecision(1) << mib << " MiB";
                        element("cache-summary").SetInnerRML(label.str());
                        control("rebuild").SetValue(count ? "Recompile" : "Compile");
                    }
                    continue;
                }
                if (line.starts_with("LOG ")) {
                    const auto run = line.substr(4);
                    if (run != run_log_ && (action_ == Action::play || action_ == Action::rebuild)) {
                        build_steps_.clear();
                        element("build-steps").SetInnerRML("");
                        element("build-activity").SetInnerRML("");
                        show_steps(true);
                    }
                    run_log_ = run;
                    update_buttons();
                    continue;
                }
                if (line.starts_with("STAGE ")) {
                    show_build_stage(line.substr(6));
                    continue;
                }
                if (line.starts_with("DETAIL ")) {
                    element("build-activity").SetInnerRML("");
                    append_step(line.substr(7));
                    show_status("Preparing the local game module.");
                    continue;
                }
                if (line.starts_with("PROGRESS ")) {
                    element("build-activity").SetInnerRML(Rml::StringUtilities::EncodeRml(line.substr(9)));
                    continue;
                }
                reported_error_ = line.starts_with("ERROR ");
                if (reported_error_ && (action_ == Action::play || action_ == Action::rebuild) && steps_visible_)
                    append_step(line.substr(6));
                show_status(reported_error_ ? line.substr(6) : line, reported_error_);
            }
        }
    }

    Rml::ElementDocument& document_;
    SDL_Window* window_;
    fs::path checkout_;
    fs::path settings_;
    Preferences prefs_;
    std::unique_ptr<SDL_Process, decltype(&SDL_DestroyProcess)> process_{nullptr, SDL_DestroyProcess};
    std::string pending_, module_key_, run_log_, build_steps_;
    std::optional<st::GameLaunch> ready_;
    std::optional<Uint64> build_started_;
    Uint64 build_elapsed_ = 0;
    std::mutex dialog_mutex_;
    std::optional<std::string> selection_;
    Action action_ = Action::inspect;
    unsigned cached_builds_ = 0;
    bool dialog_open_ = false, verified_ = false, reported_error_ = false, steps_visible_ = false;
};

class PauseMenu final : public st::GamePauseMenu, private Rml::EventListener {
public:
    PauseMenu(Rml::Context& context, Rml::ElementDocument& document, LauncherWindow& window, Launcher& launcher)
        : context_(context), document_(document), window_(window), launcher_(launcher) {
        document_.AddEventListener(Rml::EventId::Click, this);
        document_.AddEventListener(Rml::EventId::Change, this);
    }

    ~PauseMenu() override {
        document_.RemoveEventListener(Rml::EventId::Click, this);
        document_.RemoveEventListener(Rml::EventId::Change, this);
    }

    void open() override {
        action_ = st::PauseAction::stay;
        update_display_button();
        control("pause-screen-layout").SetValue(layout_value(launcher_.screen_layout()));
        control("auto-join").SetValue(launcher_.automatic_join() ? "automatic" : "manual");
        show_enhancements(document_, launcher_.screen_enhancements());
        enable_enhancements(document_, launcher_.automatic_join());
        document_.Show();
        document_.GetElementById("resume")->Focus();
    }
    void close() override { document_.Hide(); }
    void input(SDL_Event& event) override { window_.menu_input(context_, event); }
    bool draw() override { return window_.draw_pause(context_); }
    st::PauseAction action() const override { return action_; }

private:
    bool fullscreen() const { return SDL_GetWindowFlags(window_.window()) & SDL_WINDOW_FULLSCREEN; }

    void update_display_button() {
        document_.GetElementById("display")->SetAttribute("value", fullscreen() ? "Fullscreen" : "Windowed");
    }

    Rml::ElementFormControl& control(const char* id) {
        return static_cast<Rml::ElementFormControl&>(*document_.GetElementById(id));
    }

    void ProcessEvent(Rml::Event& event) override {
        if (event.GetId() == Rml::EventId::Change) {
            if (document_.IsVisible()) {
                const auto layout = parse_screen_layout(control("pause-screen-layout").GetValue());
                const bool automatic_join = control("auto-join").GetValue() != "manual";
                const auto enhancements = read_enhancements(document_);
                enable_enhancements(document_, automatic_join);
                if (layout != launcher_.screen_layout() || automatic_join != launcher_.automatic_join() ||
                    enhancements != launcher_.screen_enhancements()) {
                    window_.set_screen_layout(layout, automatic_join, enhancements);
                    launcher_.save_screen_settings(layout, automatic_join, enhancements);
                }
            }
            return;
        }
        auto* target = event.GetTargetElement();
        while (target && target->GetId().empty()) target = target->GetParentNode();
        if (!target) return;
        const auto& id = target->GetId();
        if (select_settings_tab(document_, id)) return;
        if (id == "resume") action_ = st::PauseAction::resume;
        else if (id == "leave") action_ = st::PauseAction::leave;
        else if (id == "display" && SDL_SetWindowFullscreen(window_.window(), !fullscreen())) {
            update_display_button();
            launcher_.save_display_mode(fullscreen());
        }
    }

    Rml::Context& context_;
    Rml::ElementDocument& document_;
    LauncherWindow& window_;
    Launcher& launcher_;
    st::PauseAction action_ = st::PauseAction::stay;
};

bool check_layout(LauncherWindow& window, Rml::Context& context, Rml::ElementDocument& document,
                  int width, int height, bool stacked, bool scrollable, const fs::path& preview) {
    const float dpi = SDL_GetWindowDisplayScale(window.window());
    if (!SDL_SetWindowSize(window.window(), int(width * dpi), int(height * dpi)) || !SDL_SyncWindow(window.window()))
        return false;
    const auto path = path_to_utf8(preview);
    if (!window.draw(context, preview.empty() ? nullptr : path.c_str())) return false;
    const auto bounds = context.GetDimensions();
    auto* viewport = document.GetElementById("viewport");
    if (!viewport) return false;

    struct Rect { float left, top, right, bottom; };
    auto rect = [](Rml::Element& element) {
        const auto start = element.GetAbsoluteOffset(Rml::BoxArea::Border);
        const auto size = element.GetBox().GetSize(Rml::BoxArea::Border);
        return Rect{start.x, start.y, start.x + size.x, start.y + size.y};
    };
    constexpr std::array ids{"mark", "game-heading", "rom", "browse", "status", "play", "settings-heading",
                             "tab-screens", "tab-features", "tab-window", "tab-game",
                             "unify-cutscenes", "narrative-subtitles", "hide-secondary",
                             "window-mode", "screen-layout", "auto-join", "scale", "language", "cache-summary", "rebuild", "remove-cache",
                             "toggle-steps", "open-log"};
    std::array<Rect, ids.size()> controls{};
    for (size_t i = 0; i < ids.size(); ++i) {
        auto* element = document.GetElementById(ids[i]);
        if (!element) return false;
        if (!element->IsVisible(true)) continue;
        const auto box = rect(*element);
        if (box.left < 12 * dpi || box.right > bounds.x - 12 * dpi || box.top < 0 ||
            box.right <= box.left || box.bottom <= box.top || (!scrollable && box.bottom > bounds.y + 1)) return false;
        for (size_t j = 0; j < i; ++j) {
            const auto other = controls[j];
            if (box.left < other.right && other.left < box.right && box.top < other.bottom && other.top < box.bottom)
                return false;
        }
        controls[i] = box;
    }
    const auto game = rect(*document.GetElementById("game"));
    const auto settings = rect(*document.GetElementById("settings"));
    if (stacked ? settings.top <= game.bottom : settings.left <= game.right) return false;
    const float scroll = viewport->GetScrollHeight() - viewport->GetClientHeight();
    if (!scrollable) return scroll <= 1;
    if (scroll <= 1) return false;
    viewport->SetScrollTop(scroll);
    window.draw(context);
    auto* activity = document.GetElementById("build-activity");
    auto* last_control = activity->IsVisible(true) ? activity : document.GetElementById("open-log");
    const bool last_control_reachable = rect(*last_control).bottom <= bounds.y + 1;
    viewport->SetScrollTop(0);
    return last_control_reachable;
}

int run_launcher(LauncherWindow& window, bool smoke_test, const fs::path& previews, const fs::path& game_smoke) {
    auto* base = SDL_GetBasePath();
    std::unique_ptr<char, decltype(&SDL_free)> pref(SDL_GetPrefPath("st-native", "st-native"), SDL_free);
    if (!base || !pref) return 1;

    const fs::path executable_dir = path_from_utf8(base);
    const fs::path checkout = launcher_checkout(executable_dir);
    if (checkout.empty()) return 1;
    const fs::path assets = executable_dir / "launcher-assets";
    if (!Rml::LoadFontFace(path_to_utf8(assets / "Cinzel.ttf")) ||
        !Rml::LoadFontFace(path_to_utf8(assets / "AlegreyaSans-Regular.ttf"))) return 1;
    auto* context = Rml::CreateContext("launcher", {kWidth, kHeight});
    if (!context) return 1;
    auto* document = context->LoadDocument(path_to_utf8(assets / "launcher.rml"));
    if (!document) return 1;
    auto* pause_document = context->LoadDocument(path_to_utf8(assets / "pause.rml"));
    if (!pause_document) return 1;
    Launcher launcher(*document, window.window(), checkout, smoke_test || !game_smoke.empty() ? fs::path{} :
                      path_from_utf8(pref.get()) / "launcher.cfg");
    PauseMenu pause(*context, *pause_document, window, launcher);
    document->Show();
    if (!game_smoke.empty()) {
        auto wait_for_worker = [&](Uint64 timeout) {
            const auto deadline = SDL_GetTicks() + timeout;
            while (launcher.busy() && SDL_GetTicks() < deadline) {
                window.process_events(*context);
                launcher.update();
                window.draw(*context);
            }
            return !launcher.busy();
        };
        launcher.verify_rom(path_to_utf8(fs::absolute(game_smoke)));
        if (!wait_for_worker(30'000)) return 8;
        std::optional<st::GameLaunch> game;
        // Developer replay tests can reuse an already verified local module without rebuilding it.
        if (const char* module = std::getenv("ST_NATIVE_SMOKE_MODULE")) {
            game = st::GameLaunch{};
            game->rom = fs::absolute(game_smoke);
            game->module = fs::absolute(path_from_utf8(module));
        } else {
            document->GetElementById("play")->DispatchEvent(Rml::EventId::Click, {});
            if (!wait_for_worker(15 * 60'000)) return 8;
            game = launcher.take_launch();
        }
        auto lock = lock_launcher_session(checkout);
        if (!game || !lock) return 8;

        const auto properties = SDL_GetWindowProperties(window.window());
        const auto* handle = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
        if (!handle) return 8;
        for (const auto& option : kLayoutOptions) {
            const auto layout = option.first;
            if (const char* selected = std::getenv("ST_NATIVE_SMOKE_LAYOUT"))
                if (std::string_view(selected) != option.second) continue;
            uint64_t smoke_frames = 300;
            if (const char* count = std::getenv("ST_NATIVE_SMOKE_FRAMES"))
                smoke_frames = std::clamp<uint64_t>(std::strtoull(count, nullptr, 10), 180, 36000);
            if (std::getenv("ST_NATIVE_SMOKE_ORIGINAL")) game->automatic_join = false;
            if (const char* text = std::getenv("ST_NATIVE_SMOKE_FEATURES")) {
                char* end = nullptr;
                const auto mask = std::strtol(text, &end, 10);
                if (end == text || *end || mask < 0 || mask > 7) return 8;
                game->enhancements = {bool(mask & 1), bool(mask & 2), bool(mask & 4)};
            }
            game->layout = layout;
            game->fullscreen = layout == st::ScreenLayout::classic || layout == st::ScreenLayout::picture_in_picture;
            launcher.save_screen_settings(game->layout, game->automatic_join, game->enhancements);
            document->Hide();
            const auto result = st::play_game_in_window(*game, window, pause, smoke_frames);
            document->Show();
            if (result.exit_code || result.close_requested ||
                handle != SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr) ||
                !window.draw(*context)) return 8;
        }
        return 0;
    }
    if (smoke_test) {
        auto preview = [&previews](const char* name) { return previews.empty() ? fs::path{} : previews / name; };
        if (!check_layout(window, *context, *document, kWidth, kHeight, false, false, preview("launcher-wide.bmp")))
            return 2;
        if (!check_layout(window, *context, *document, 1040, 610, false, false, {})) return 4;
        for (const auto& [tab, field] : {std::pair{"tab-window", "window-mode"}, std::pair{"tab-game", "language"},
                                         std::pair{"tab-features", "unify-cutscenes"},
                                         std::pair{"tab-screens", "screen-layout"}}) {
            document->GetElementById(tab)->DispatchEvent(Rml::EventId::Click, {});
            if (!window.draw(*context) || !document->GetElementById(field)->IsVisible(true)) return 11;
        }
        if (!check_layout(window, *context, *document, 420, 780, true, false, preview("launcher-compact.bmp"))) return 5;
        if (!check_layout(window, *context, *document, 390, 300, true, true, {})) return 6;
        document->GetElementById("tab-features")->DispatchEvent(Rml::EventId::Click, {});
        if (!check_layout(window, *context, *document, kWidth, kHeight, false, false, preview("features-wide.bmp")) ||
            !check_layout(window, *context, *document, 390, 300, true, true, preview("features-short.bmp"))) return 13;
        for (unsigned mask = 0; mask < 8; ++mask) {
            show_enhancements(*document, {bool(mask & 1), bool(mask & 2), bool(mask & 4)});
            if (launcher.screen_enhancements().mask() != mask) return 14;
        }
        static_cast<Rml::ElementFormControl&>(*document->GetElementById("auto-join")).SetValue("manual");
        for (const auto* id : kEnhancementIds)
            if (!static_cast<Rml::ElementFormControl&>(*document->GetElementById(id)).IsDisabled()) return 14;
        static_cast<Rml::ElementFormControl&>(*document->GetElementById("auto-join")).SetValue("automatic");
        document->GetElementById("tab-screens")->DispatchEvent(Rml::EventId::Click, {});
        auto* toggle_steps = document->GetElementById("toggle-steps");
        auto* details = document->GetElementById("build-details");
        toggle_steps->DispatchEvent(Rml::EventId::Click, {});
        if (!window.draw(*context) || !details->IsVisible()) return 7;
        toggle_steps->DispatchEvent(Rml::EventId::Click, {});
        if (!window.draw(*context) || details->IsVisible()) return 7;
        auto* progress = document->GetElementById("build-progress");
        progress->SetClass("visible", true);
        document->GetElementById("build-stage")->SetInnerRML("Stage 2 of 6 · Discover game functions");
        document->GetElementById("build-elapsed")->SetInnerRML("1m 23s elapsed");
        document->GetElementById("build-activity")->SetInnerRML("Pass 3 · Scanning function pointers · 40 / 60 modules scanned");
        auto* meter = static_cast<Rml::ElementProgress*>(document->GetElementById("build-meter"));
        meter->SetMax(6);
        meter->SetValue(1);
        if (!check_layout(window, *context, *document, kWidth, kHeight, false, false, preview("launcher-progress.bmp")) ||
            !check_layout(window, *context, *document, 390, 300, true, true, {})) return 9;
        progress->SetClass("visible", false);
        // Render both screen layers and change layout through the pause menu without starting a ROM.
        auto& layout_control = static_cast<Rml::ElementFormControl&>(*pause_document->GetElementById("pause-screen-layout"));
        std::vector<uint32_t> tiles(3 * st::ds_width * st::ds_screen_height, 0xff315374);
        std::fill(tiles.begin() + st::ds_width * st::ds_screen_height, tiles.end(), 0xff66845c);
        document->Hide();
        pause.open();
        pause_document->GetElementById("tab-features")->DispatchEvent(Rml::EventId::Click, {});
        for (unsigned mask = 0; mask < 8; ++mask) {
            show_enhancements(*pause_document, {bool(mask & 1), bool(mask & 2), bool(mask & 4)});
            if (window.screen_features() != mask || launcher.screen_enhancements().mask() != mask ||
                read_enhancements(*document).mask() != mask || !pause.draw()) return 14;
        }
        pause_document->GetElementById("tab-screens")->DispatchEvent(Rml::EventId::Click, {});
        for (const auto& [layout, key] : kLayoutOptions) {
            layout_control.SetValue(key);
            if (launcher.screen_layout() != layout || !window.present_game(tiles.data(), false, false, false) ||
                !pause.draw()) return 10;
        }
        pause_document->GetElementById("tab-window")->DispatchEvent(Rml::EventId::Click, {});
        if (!pause.draw() || !pause_document->GetElementById("display")->IsVisible(true) ||
            layout_control.IsVisible(true)) return 11;
        // A disabled launcher control must not leave its forbidden cursor on the game canvas.
        auto& cursors = static_cast<st::CursorInterface&>(*Rml::GetSystemInterface());
        cursors.SetMouseCursor("arrow");
        const auto* arrow = SDL_GetCursor();
        cursors.SetMouseCursor("text");
        const auto* text = SDL_GetCursor();
        cursors.SetMouseCursor("unavailable");
        const auto* unavailable = SDL_GetCursor();
        if (!arrow || !text || !unavailable || arrow == text || arrow == unavailable) return 12;
        if (!window.present_game(tiles.data(), false, false, false) || SDL_GetCursor() != arrow) return 12;
        cursors.SetMouseCursor("unavailable"); // even a hidden UI request cannot take over the game cursor
        if (SDL_GetCursor() != arrow) return 12;
        cursors.set_game_mode(false);
        if (SDL_GetCursor() != unavailable) return 12;
        if (!window.present_game(tiles.data(), false, false, false) || SDL_GetCursor() != arrow || !pause.draw()) return 12;
        cursors.SetMouseCursor("text");
        if (SDL_GetCursor() != text) return 12;
        if (!window.present_game(tiles.data(), false, false, false) || SDL_GetCursor() != arrow) return 12;
        pause.close();
        document->Show();
        if (!window.draw(*context)) return 12;
        cursors.SetMouseCursor("text");
        if (SDL_GetCursor() != text) return 12;
        cursors.SetMouseCursor("arrow");
        // Exercise the same worker and status updates as the picker with a file we know is not a ROM.
        launcher.verify_rom(path_to_utf8(assets / "launcher.rml"));
        const auto deadline = SDL_GetTicks() + 5000;
        while (launcher.busy() && SDL_GetTicks() < deadline) {
            window.process_events(*context);
            launcher.update();
        }
        const auto message = document->GetElementById("status")->GetInnerRML();
        return !launcher.busy() && message.starts_with("Unsupported ROM.") ? 0 : 3;
    }
    while (true) {
        const bool open = window.process_events(*context);
        launcher.update();
        if (!open && !launcher.busy()) break;
        if (const auto game = launcher.take_launch()) {
            auto lock = lock_launcher_session(checkout);
            if (!lock) launcher.game_busy();
            else {
                SDL_StopTextInput(window.window());
                document->Hide();
                const auto result = st::play_game_in_window(*game, window, pause);
                if (result.close_requested) break;
                document->Show();
                launcher.finish_game(result.exit_code);
            }
        }
        window.draw(*context);
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    const bool smoke_test = argc >= 2 && std::string_view(argv[1]) == "--smoke-test";
    const fs::path previews = smoke_test && argc == 3 ? path_from_utf8(argv[2]) : fs::path{};
    const fs::path game_smoke = argc == 3 && std::string_view(argv[1]) == "--game-smoke" ?
                                path_from_utf8(argv[2]) : fs::path{};
    LauncherWindow window;
    if (!window.open(kTitle, kWidth, kHeight)) {
        if (!smoke_test && game_smoke.empty())
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, kTitle, SDL_GetError(), nullptr);
        return 1;
    }
    const bool initialized = Rml::Initialise();
    const int result = initialized ? run_launcher(window, smoke_test, previews, game_smoke) : 1;
    if (result && !smoke_test && game_smoke.empty()) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, kTitle,
                                                      "Open st_launcher.exe from its build folder in the project checkout.",
                                                      nullptr);
    if (initialized) Rml::Shutdown();
    return result;
}
