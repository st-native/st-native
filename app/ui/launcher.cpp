// SPDX-License-Identifier: GPL-3.0-or-later
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <RmlUi/Core.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>
#include <RmlUi/Core/Elements/ElementFormControlSelect.h>
#include <RmlUi/Core/StringUtilities.h>

#include <array>
#include <cmath>
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
#include "../audio/audio_devices.hpp"
#include "../game_session.hpp"
#include "../present/launcher_window.hpp"
#include "../present/cursor_interface.hpp"
#include "../setup/launcher_worker.hpp"
#include "../settings/launcher_preferences.hpp"
#include "controls_panel.hpp"

namespace fs = std::filesystem;

namespace {

constexpr const char* kTitle = "Spirit Tracks Native";
constexpr int kWidth = 800, kHeight = 610;

using st::kLayoutOptions;
using st::parse_screen_layout;
using st::layout_value;
using Preferences = st::LauncherPreferences;

constexpr std::array kEnhancementIds{"unify-cutscenes", "narrative-subtitles", "hide-secondary"};

st::SecondaryViewSettings read_secondary_view(Rml::ElementDocument& document) {
    auto get = [&](const char* id) {
        return static_cast<Rml::ElementFormControl&>(*document.GetElementById(id)).GetValue();
    };
    return {std::atoi(get("secondary-size").c_str()), std::atoi(get("secondary-opacity").c_str())};
}

void show_secondary_view(Rml::ElementDocument& document, st::SecondaryViewSettings settings) {
    static_cast<Rml::ElementFormControl&>(*document.GetElementById("secondary-size")).SetValue(std::to_string(settings.size));
    static_cast<Rml::ElementFormControl&>(*document.GetElementById("secondary-opacity")).SetValue(std::to_string(settings.opacity));
}

void enable_secondary_view(Rml::ElementDocument& document, st::ScreenLayout layout, st::SecondaryViewSettings settings) {
    document.GetElementById("single-screen-note")->SetProperty("display", layout == st::ScreenLayout::single ? "block" : "none");
    static_cast<Rml::ElementFormControl&>(*document.GetElementById("secondary-size")).SetDisabled(layout != st::ScreenLayout::single);
    static_cast<Rml::ElementFormControl&>(*document.GetElementById("secondary-opacity")).SetDisabled(
        layout != st::ScreenLayout::single || settings.size == 0);
}

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

void populate_device_select(Rml::ElementDocument& document, const char* id,
                            const std::vector<std::string>& devices, const std::string& selected_device) {
    auto& select = static_cast<Rml::ElementFormControlSelect&>(*document.GetElementById(id));
    select.RemoveAll();
    select.Add("System default", "");
    bool found = selected_device.empty();
    for (const auto& name : devices) {
        select.Add(Rml::StringUtilities::EncodeRml(name), name);
        if (name == selected_device) found = true;
    }
    if (!found) {
        select.Add(Rml::StringUtilities::EncodeRml(selected_device + " (disconnected)"), selected_device);
    }
    select.SetValue(selected_device);
}

void populate_audio_devices(Rml::ElementDocument& document, const st::AudioSettings& audio) {
    populate_device_select(document, "output-device", st::list_playback_devices(), audio.output_device);
    populate_device_select(document, "input-device", st::list_recording_devices(), audio.input_device);
}

void update_audio_labels(Rml::ElementDocument& document, const st::AudioSettings& audio) {
    document.GetElementById("master-volume-value")->SetInnerRML(
        std::to_string(st::clamp_volume(audio.volume)) + "%");
    document.GetElementById("mic-gain-value")->SetInnerRML(
        std::to_string(st::clamp_mic_gain(audio.mic_gain)) + "%");
}

void enable_audio_controls(Rml::ElementDocument& document, bool enabled, st::MicMode mic_mode) {
    for (const auto* id : {"master-volume", "mute-unfocused", "audio-interp", "output-device", "mic-mode"})
        static_cast<Rml::ElementFormControl&>(*document.GetElementById(id)).SetDisabled(!enabled);
    const bool use_device = enabled && st::mic_uses_device(mic_mode);
    const bool use_gain = enabled && mic_mode != st::MicMode::off;
    static_cast<Rml::ElementFormControl&>(*document.GetElementById("input-device")).SetDisabled(!use_device);
    static_cast<Rml::ElementFormControl&>(*document.GetElementById("mic-gain")).SetDisabled(!use_gain);
    document.GetElementById("master-volume-value")->SetClass("disabled", !enabled);
    document.GetElementById("mic-gain-value")->SetClass("disabled", !use_gain);
}

void show_audio_settings(Rml::ElementDocument& document, const st::AudioSettings& audio) {
    auto set = [&](const char* id, const std::string& value) {
        auto& ctrl = static_cast<Rml::ElementFormControl&>(*document.GetElementById(id));
        if (ctrl.GetValue() != value) ctrl.SetValue(value);
    };
    set("master-volume", std::to_string(st::clamp_volume(audio.volume)));
    set("mute-unfocused", audio.mute_unfocused ? "on" : "off");
    set("audio-interp", st::audio_interpolation_value(audio.interpolation));
    set("output-device", audio.output_device);
    set("mic-mode", st::mic_mode_value(audio.mic_mode));
    set("input-device", audio.input_device);
    set("mic-gain", std::to_string(st::clamp_mic_gain(audio.mic_gain)));
    update_audio_labels(document, audio);
}

st::AudioSettings read_audio_settings(Rml::ElementDocument& document) {
    auto get = [&](const char* id) {
        return static_cast<Rml::ElementFormControl&>(*document.GetElementById(id)).GetValue();
    };
    const int volume = st::clamp_volume(int(std::lround(std::atof(get("master-volume").c_str()))));
    const bool mute_unfocused = get("mute-unfocused") == "on";
    const auto interpolation = st::parse_audio_interpolation(get("audio-interp"));
    const std::string output_device = get("output-device");
    const auto mic_mode = st::parse_mic_mode(get("mic-mode"));
    const std::string input_device = get("input-device");
    const int mic_gain = st::clamp_mic_gain(int(std::lround(std::atof(get("mic-gain").c_str()))));
    return {volume, mute_unfocused, interpolation, output_device, mic_mode, input_device, mic_gain};
}

bool select_settings_tab(Rml::ElementDocument& document, const std::string& id) {
    if (!id.starts_with("tab-") || !document.GetElementById(id)) return false;
    if (auto* content = document.GetElementById("content")) content->SetClass("controls-open",id == "tab-controls");
    if (auto* viewport = document.GetElementById("viewport")) viewport->SetScrollTop(0);
    if (auto* panel = document.GetElementById("pause-panel")) panel->SetScrollTop(0);
    for (const std::string key : {"screens", "map", "features", "audio", "controls", "window", "game"}) {
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
    Launcher(Rml::ElementDocument& document, LauncherWindow& presentation, fs::path checkout, fs::path settings)
        : document_(document), window_(presentation.window()), presentation_(presentation), checkout_(std::move(checkout)), settings_(std::move(settings)),
          prefs_(settings_) {
        presentation_.set_controls_settings(prefs_.controls);
        controls_ = std::make_unique<st::ControlsPanel>(document_,[this] { return prefs_.controls; },
            [this](const auto& settings) { save_controls(settings); },[this] { return presentation_.active_gamepad(); });
        syncing_ = true;
        control("scale").SetValue(prefs_.scale);
        control("language").SetValue(prefs_.language);
        control("pause-on-menu").SetValue(prefs_.pause_on_menu ? "on" : "off");
        control("window-mode").SetValue(prefs_.window);
        control("screen-layout").SetValue(layout_value(prefs_.layout));
        control("auto-join").SetValue(prefs_.automatic_join ? "automatic" : "manual");
        show_enhancements(document_, prefs_.enhancements);
        show_secondary_view(document_, prefs_.secondary);
        enable_secondary_view(document_, prefs_.layout, prefs_.secondary);
        populate_audio_devices(document_, prefs_.audio);
        show_audio_settings(document_, prefs_.audio);
        syncing_ = false;
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
        if (controls_->process(event)) return;
        if (event.GetId() == Rml::EventId::Change) {
            if (syncing_) return;
            prefs_.scale = control("scale").GetValue();
            prefs_.language = control("language").GetValue();
            prefs_.pause_on_menu = control("pause-on-menu").GetValue() == "on";
            prefs_.window = control("window-mode").GetValue();
            prefs_.layout = parse_screen_layout(control("screen-layout").GetValue());
            prefs_.automatic_join = control("auto-join").GetValue() != "manual";
            prefs_.enhancements = read_enhancements(document_);
            prefs_.secondary = read_secondary_view(document_);
            enable_secondary_view(document_, prefs_.layout, prefs_.secondary);
            prefs_.audio = read_audio_settings(document_);
            update_audio_labels(document_, prefs_.audio);
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
        controls_->refresh();
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
                                    prefs_.automatic_join, prefs_.enhancements, prefs_.audio,
                                    prefs_.pause_on_menu, prefs_.secondary, prefs_.controls};
        } else if (action_ == Action::play && result == 0) {
            show_status("Preparation did not return a game module.", true);
        }
        update_buttons();
    }

    bool busy() const { return dialog_open_ || process_ != nullptr; }
    bool input(const SDL_Event& event) { return controls_->input(event); }
    const st::input::Controls& controls_settings() const { return prefs_.controls; }
    void save_controls(const st::input::Controls& settings) {
        if (!settings.validate()) return;
        prefs_.controls = settings;
        presentation_.set_controls_settings(settings);
        persist();
    }
    void save_display_mode(bool fullscreen) {
        syncing_ = true;
        prefs_.window = fullscreen ? "fullscreen" : "windowed";
        control("window-mode").SetValue(prefs_.window);
        syncing_ = false;
        persist();
    }
    st::ScreenLayout screen_layout() const { return prefs_.layout; }
    bool automatic_join() const { return prefs_.automatic_join; }
    bool pause_on_menu() const { return prefs_.pause_on_menu; }
    st::ScreenEnhancements screen_enhancements() const { return prefs_.enhancements; }
    st::SecondaryViewSettings secondary_view() const { return prefs_.secondary; }
    void save_secondary_view(st::SecondaryViewSettings settings) {
        syncing_ = true;
        prefs_.secondary = settings;
        show_secondary_view(document_, settings);
        enable_secondary_view(document_, prefs_.layout, settings);
        syncing_ = false;
        persist();
    }
    const st::AudioSettings& audio_settings() const { return prefs_.audio; }
    void save_pause_on_menu(bool pause) {
        syncing_ = true;
        control("pause-on-menu").SetValue(pause ? "on" : "off");
        syncing_ = false;
        prefs_.pause_on_menu = pause;
        persist();
        update_buttons();
    }
    void refresh_audio_devices() {
        syncing_ = true;
        populate_audio_devices(document_, prefs_.audio);
        show_audio_settings(document_, prefs_.audio);
        syncing_ = false;
        update_buttons();
    }
    void save_screen_settings(st::ScreenLayout layout, bool automatic_join, st::ScreenEnhancements enhancements) {
        syncing_ = true;
        control("screen-layout").SetValue(layout_value(layout));
        control("auto-join").SetValue(automatic_join ? "automatic" : "manual");
        show_enhancements(document_, enhancements);
        syncing_ = false;
        prefs_.layout = layout;
        prefs_.automatic_join = automatic_join;
        prefs_.enhancements = enhancements;
        enable_secondary_view(document_, layout, prefs_.secondary);
        persist();
        update_buttons();
    }
    void save_audio_settings(const st::AudioSettings& audio) {
        syncing_ = true;
        populate_audio_devices(document_, audio);
        show_audio_settings(document_, audio);
        syncing_ = false;
        prefs_.audio = audio;
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
        for (const auto id : {"scale", "language", "pause-on-menu", "window-mode", "screen-layout", "auto-join"})
            control(id).SetDisabled(busy());
        enable_enhancements(document_, !busy() && prefs_.automatic_join);
        enable_audio_controls(document_, !busy(), prefs_.audio.mic_mode);
        controls_->enabled(!busy());
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
    LauncherWindow& presentation_;
    std::unique_ptr<st::ControlsPanel> controls_;
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
    bool dialog_open_ = false, verified_ = false, reported_error_ = false, steps_visible_ = false, syncing_ = false;
};

class PauseMenu final : public st::GamePauseMenu, private Rml::EventListener {
public:
    PauseMenu(Rml::Context& context, Rml::ElementDocument& document, LauncherWindow& window, Launcher& launcher)
        : context_(context), document_(document), window_(window), launcher_(launcher) {
        controls_ = std::make_unique<st::ControlsPanel>(document_,[this] { return launcher_.controls_settings(); },
            [this](const auto& settings) { launcher_.save_controls(settings); },[this] { return window_.active_gamepad(); });
        document_.AddEventListener(Rml::EventId::Click, this);
        document_.AddEventListener(Rml::EventId::Change, this);
    }

    ~PauseMenu() override {
        document_.RemoveEventListener(Rml::EventId::Click, this);
        document_.RemoveEventListener(Rml::EventId::Change, this);
    }

    void open() override {
        controls_->cancel();
        action_ = st::PauseAction::stay;
        syncing_ = true;
        update_display_button();
        update_pause_mode_button();
        control("pause-screen-layout").SetValue(layout_value(launcher_.screen_layout()));
        control("auto-join").SetValue(launcher_.automatic_join() ? "automatic" : "manual");
        show_enhancements(document_, launcher_.screen_enhancements());
        enable_enhancements(document_, launcher_.automatic_join());
        show_secondary_view(document_, launcher_.secondary_view());
        enable_secondary_view(document_, launcher_.screen_layout(), launcher_.secondary_view());
        populate_audio_devices(document_, launcher_.audio_settings());
        show_audio_settings(document_, launcher_.audio_settings());
        enable_audio_controls(document_, true, launcher_.audio_settings().mic_mode);
        syncing_ = false;
        document_.Show();
        document_.GetElementById("resume")->Focus();
    }
    void close() override { controls_->cancel(); document_.Hide(); }
    bool capturing_input() const override { return controls_->capturing(); }
    void input(SDL_Event& event) override {
        if (controls_->input(event)) return;
        if (st::gamepad_menu_input(context_,event)) return;
        if (event.type == SDL_EVENT_AUDIO_DEVICE_ADDED || event.type == SDL_EVENT_AUDIO_DEVICE_REMOVED) {
            syncing_ = true;
            populate_audio_devices(document_, launcher_.audio_settings());
            show_audio_settings(document_, launcher_.audio_settings());
            syncing_ = false;
        }
        window_.menu_input(context_, event);
    }
    bool draw() override { controls_->refresh(); return window_.draw_pause(context_); }
    st::PauseAction action() const override { return action_; }

private:
    bool fullscreen() const { return SDL_GetWindowFlags(window_.window()) & SDL_WINDOW_FULLSCREEN; }

    void update_display_button() {
        document_.GetElementById("display")->SetAttribute("value", fullscreen() ? "Fullscreen" : "Windowed");
    }

    void update_pause_mode_button() {
        const bool paused = launcher_.pause_on_menu();
        document_.SetClass("paused", paused);
        document_.GetElementById("pause-title")->SetInnerRML(paused ? "Paused" : "Menu");
        auto* mode = document_.GetElementById("pause-mode");
        mode->SetClass("active", paused);
        mode->SetAttribute("value", paused ? "Pause game: On" : "Pause game: Off");
        document_.GetElementById("resume")->SetAttribute("value", paused ? "Resume" : "Close menu");
    }

    Rml::ElementFormControl& control(const char* id) {
        return static_cast<Rml::ElementFormControl&>(*document_.GetElementById(id));
    }

    void ProcessEvent(Rml::Event& event) override {
        if (controls_->process(event)) return;
        if (event.GetId() == Rml::EventId::Change) {
            if (!syncing_ && document_.IsVisible()) {
                const auto layout = parse_screen_layout(control("pause-screen-layout").GetValue());
                const bool automatic_join = control("auto-join").GetValue() != "manual";
                const auto enhancements = read_enhancements(document_);
                const auto audio = read_audio_settings(document_);
                const auto secondary = read_secondary_view(document_);
                enable_secondary_view(document_, layout, secondary);
                enable_enhancements(document_, automatic_join);
                update_audio_labels(document_, audio);
                enable_audio_controls(document_, true, audio.mic_mode);
                if (layout != launcher_.screen_layout() || automatic_join != launcher_.automatic_join() ||
                    enhancements != launcher_.screen_enhancements()) {
                    window_.set_screen_layout(layout, automatic_join, enhancements);
                    launcher_.save_screen_settings(layout, automatic_join, enhancements);
                }
                if (audio != launcher_.audio_settings()) {
                    window_.set_audio_settings(audio);
                    launcher_.save_audio_settings(audio);
                }
                if (secondary != launcher_.secondary_view()) {
                    window_.set_secondary_view(secondary);
                    launcher_.save_secondary_view(secondary);
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
        else if (id == "pause-mode") {
            const bool next = !launcher_.pause_on_menu();
            window_.set_pause_on_menu(next);
            launcher_.save_pause_on_menu(next);
            update_pause_mode_button();
        } else if (id == "display" && SDL_SetWindowFullscreen(window_.window(), !fullscreen())) {
            update_display_button();
            launcher_.save_display_mode(fullscreen());
        }
    }

    Rml::Context& context_;
    Rml::ElementDocument& document_;
    LauncherWindow& window_;
    Launcher& launcher_;
    std::unique_ptr<st::ControlsPanel> controls_;
    st::PauseAction action_ = st::PauseAction::stay;
    bool syncing_ = false;
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
                             "tab-screens", "tab-map", "tab-features", "tab-audio", "tab-controls", "tab-window", "tab-game",
                             "unify-cutscenes", "narrative-subtitles", "hide-secondary",
                             "master-volume", "master-volume-value", "mute-unfocused", "audio-interp",
                             "output-device", "mic-mode", "input-device", "mic-gain", "mic-gain-value",
                             "window-mode", "screen-layout", "auto-join", "secondary-size", "secondary-opacity", "scale", "language", "pause-on-menu",
                             "cache-summary", "rebuild", "remove-cache",
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
    if (document.GetElementById("game")->IsVisible(true) &&
        (stacked ? settings.top <= game.bottom : settings.left <= game.right)) return false;
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

bool check_controls_menu(Rml::ElementDocument& document, Launcher& launcher, Rml::Context& context,
                         const std::function<void(SDL_Event&)>& feed, const std::function<bool()>& draw,
                         const std::function<bool()>& conflict_preview = {}) {
    using A = st::input::Action;
    auto form = [&](const char* id) -> Rml::ElementFormControl& {
        return static_cast<Rml::ElementFormControl&>(*document.GetElementById(id));
    };
    auto click = [&](const std::string& id) { document.GetElementById(id)->DispatchEvent(Rml::EventId::Click,{}); };
    const std::string green = "binding-"+std::to_string(size_t(A::note1));
    const std::string orange = "binding-"+std::to_string(size_t(A::note4));
    click("tab-controls");
    form("controls-group").SetValue("instrument");
    click("controls-keyboard");
    if (!draw()) return false;
    click(green+"-0");
    SDL_Event event{}; event.type=SDL_EVENT_KEY_DOWN; event.key.scancode=SDL_SCANCODE_G;
    feed(event);
    if (launcher.controls_settings().bindings[size_t(A::note1)][0] != st::input::key(SDL_SCANCODE_G)) return false;
    click(green+"-0"); event.key.scancode=SDL_SCANCODE_J; feed(event);
    if (!draw() || !document.GetElementById("controls-conflict")->IsVisible(true) ||
        document.GetElementById("controls-conflict-description")->GetInnerRML().find("Yellow")==std::string::npos)
        return std::fprintf(stderr,"Controls smoke: conflict popup did not identify the binding\n"), false;
    if (conflict_preview && !conflict_preview()) return false;
    event.key.scancode=SDL_SCANCODE_ESCAPE; feed(event);
    if (!draw() || document.GetElementById("controls-conflict")->IsVisible(true) ||
        launcher.controls_settings().bindings[size_t(A::note1)][0] != st::input::key(SDL_SCANCODE_G))
        return std::fprintf(stderr,"Controls smoke: conflict cancel changed a binding\n"), false;
    click(green+"-0"); event.key.scancode=SDL_SCANCODE_J; feed(event);
    click("controls-conflict-replace");
    if (launcher.controls_settings().bindings[size_t(A::note1)][0] != st::input::key(SDL_SCANCODE_J) ||
        launcher.controls_settings().bindings[size_t(A::note5)][0].kind != st::input::Kind::none)
        return std::fprintf(stderr,"Controls smoke: conflict transfer failed\n"), false;
    click(green+"-0"); event.key.scancode=SDL_SCANCODE_G; feed(event);
    // Nintendo and Sony labels change without changing a physical binding.
    click("controls-gamepad");
    const auto before=launcher.controls_settings().bindings;
    if (!draw()) return false;
    document.GetElementById("controls-symbols")->Focus();
    auto nav = [&](SDL_GamepadButton button) {
        SDL_Event e{}; e.type=SDL_EVENT_GAMEPAD_BUTTON_DOWN; e.gbutton.button=button;
        st::gamepad_menu_input(context,e); e.type=SDL_EVENT_GAMEPAD_BUTTON_UP; st::gamepad_menu_input(context,e);
    };
    nav(SDL_GAMEPAD_BUTTON_SOUTH);
    if (!static_cast<Rml::ElementFormControlSelect&>(form("controls-symbols")).IsSelectBoxVisible())
        return std::fprintf(stderr,"Controls smoke: controller did not open the label selector\n"), false;
    nav(SDL_GAMEPAD_BUTTON_DPAD_DOWN);
    if (launcher.controls_settings().symbols!=st::input::Symbols::xbox)
        return std::fprintf(stderr,"Controls smoke: selector did not advance (%d)\n",int(launcher.controls_settings().symbols)), false;
    nav(SDL_GAMEPAD_BUTTON_SOUTH);
    if (static_cast<Rml::ElementFormControlSelect&>(form("controls-symbols")).IsSelectBoxVisible())
        return std::fprintf(stderr,"Controls smoke: selector did not close\n"), false;
    document.GetElementById("controls-deadzone")->Focus();
    const int deadzone=launcher.controls_settings().deadzone;
    nav(SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
    if (launcher.controls_settings().deadzone<=deadzone)
        return std::fprintf(stderr,"Controls smoke: controller did not adjust dead zone\n"), false;
    form("controls-symbols").SetValue("2");
    if (form((orange+"-2").c_str()).GetValue()!="Cross") return false;
    form("controls-symbols").SetValue("3");
    if (form((orange+"-2").c_str()).GetValue()!="B" || before!=launcher.controls_settings().bindings) return false;
    // Controller-only modal: default Cancel, then explicit focus + confirmed transfer.
    click(green+"-2"); event={}; event.type=SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    event.gbutton.button=SDL_GAMEPAD_BUTTON_SOUTH; feed(event);
    if (!draw() || !document.GetElementById("controls-conflict")->IsVisible(true)) return false;
    event.type=SDL_EVENT_GAMEPAD_BUTTON_UP; feed(event);
    event.type=SDL_EVENT_GAMEPAD_BUTTON_DOWN; feed(event); // Cancel has initial focus
    event.type=SDL_EVENT_GAMEPAD_BUTTON_UP; feed(event);
    if (launcher.controls_settings().bindings!=before) return false;
    click(green+"-2"); event.type=SDL_EVENT_GAMEPAD_BUTTON_DOWN; feed(event);
    event.type=SDL_EVENT_GAMEPAD_BUTTON_UP; feed(event);
    event.type=SDL_EVENT_GAMEPAD_BUTTON_DOWN; event.gbutton.button=SDL_GAMEPAD_BUTTON_DPAD_RIGHT; feed(event);
    event.type=SDL_EVENT_GAMEPAD_BUTTON_UP; feed(event);
    event.type=SDL_EVENT_GAMEPAD_BUTTON_DOWN; event.gbutton.button=SDL_GAMEPAD_BUTTON_SOUTH; feed(event);
    event.type=SDL_EVENT_GAMEPAD_BUTTON_UP; feed(event);
    if (launcher.controls_settings().bindings[size_t(A::note1)][2]!=st::input::button(SDL_GAMEPAD_BUTTON_SOUTH) ||
        launcher.controls_settings().bindings[size_t(A::note4)][2].kind!=st::input::Kind::none) return false;
    click(green+"-2"); event={}; event.type=SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    event.gbutton.button=SDL_GAMEPAD_BUTTON_DPAD_LEFT; feed(event);
    if (launcher.controls_settings().bindings[size_t(A::note1)][2] != st::input::button(SDL_GAMEPAD_BUTTON_DPAD_LEFT)) return false;
    document.GetElementById(green+"-2")->Focus();
    event.gbutton.button=SDL_GAMEPAD_BUTTON_NORTH; feed(event);
    if (launcher.controls_settings().bindings[size_t(A::note1)][2].kind!=st::input::Kind::none) return false;
    // Controller menu navigation must activate native controls without a pointer.
    document.GetElementById("controls-keyboard")->Focus();
    event.gbutton.button=SDL_GAMEPAD_BUTTON_SOUTH;
    st::gamepad_menu_input(context,event);
    event.type=SDL_EVENT_GAMEPAD_BUTTON_UP; st::gamepad_menu_input(context,event);
    if (!document.GetElementById("controls-keyboard")->IsClassSet("selected")) return false;
    // Tab traversal through the controller route must reach bindings inside the scroll area.
    form("controls-group").SetValue("general");
    click("controls-gamepad");
    if (!draw()) return false;
    document.GetElementById("binding-0-2")->Focus();
    const std::string last="binding-"+std::to_string(size_t(A::fast))+"-3";
    for (int step=0; step<50 && (!context.GetFocusElement() || context.GetFocusElement()->GetId()!=last); ++step) {
        event.type=SDL_EVENT_GAMEPAD_BUTTON_DOWN; event.gbutton.button=SDL_GAMEPAD_BUTTON_DPAD_DOWN;
        st::gamepad_menu_input(context,event);
        event.type=SDL_EVENT_GAMEPAD_BUTTON_UP; st::gamepad_menu_input(context,event);
    }
    if (!context.GetFocusElement() || context.GetFocusElement()->GetId()!=last) return false;
    launcher.save_controls({}); launcher.update();
    click("controls-gamepad"); click("controls-reset");
    if (launcher.controls_settings()!=st::input::Controls{} || !draw()) return false;
    click("tab-screens");
    return true;
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
    Launcher launcher(*document, window, checkout, smoke_test || !game_smoke.empty() ? fs::path{} :
                      path_from_utf8(pref.get()) / "launcher.cfg");
    window.set_input_filter([&](const SDL_Event& event) {
        return launcher.input(event) || st::gamepad_menu_input(*context,event);
    });
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
                                         std::pair{"tab-map", "secondary-size"},
                                         std::pair{"tab-features", "unify-cutscenes"},
                                         std::pair{"tab-audio", "master-volume"},
                                         std::pair{"tab-controls", "controls-group"},
                                         std::pair{"tab-screens", "screen-layout"}}) {
            document->GetElementById(tab)->DispatchEvent(Rml::EventId::Click, {});
            if (!window.draw(*context) || !document->GetElementById(field)->IsVisible(true)) return 11;
        }
        if (!check_controls_menu(*document,launcher,*context,[&](SDL_Event& e) { launcher.input(e); },
                [&] { return window.draw(*context); }, [&] {
                    return window.draw(*context,previews.empty() ? nullptr : path_to_utf8(preview("controls-conflict.bmp")).c_str());
                })) return 19;
        document->GetElementById("tab-controls")->DispatchEvent(Rml::EventId::Click,{});
        static_cast<Rml::ElementFormControl&>(*document->GetElementById("controls-group")).SetValue("instrument");
        if (!window.draw(*context,previews.empty() ? nullptr : path_to_utf8(preview("controls-wide.bmp")).c_str())) return 19;
        if (!check_layout(window,*context,*document,420,780,true,true,preview("controls-compact.bmp"))) return 19;
        document->GetElementById("tab-screens")->DispatchEvent(Rml::EventId::Click,{});
        if (!check_layout(window, *context, *document, 420, 780, true, false, preview("launcher-compact.bmp"))) return 5;
        if (!check_layout(window, *context, *document, 390, 300, true, true, {})) return 6;
        document->GetElementById("tab-map")->DispatchEvent(Rml::EventId::Click, {});
        if (!check_layout(window, *context, *document, kWidth, kHeight, false, false, preview("map-wide.bmp")) ||
            !check_layout(window, *context, *document, 390, 300, true, true, preview("map-short.bmp"))) return 18;
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
        document->GetElementById("tab-audio")->DispatchEvent(Rml::EventId::Click, {});
        if (!check_layout(window, *context, *document, kWidth, kHeight, false, false, preview("audio-wide.bmp")) ||
            !check_layout(window, *context, *document, 390, 300, true, true, preview("audio-short.bmp"))) return 15;
        static_cast<Rml::ElementFormControl&>(*document->GetElementById("master-volume")).SetValue("45");
        static_cast<Rml::ElementFormControl&>(*document->GetElementById("mute-unfocused")).SetValue("on");
        static_cast<Rml::ElementFormControl&>(*document->GetElementById("audio-interp")).SetValue("cubic");
        static_cast<Rml::ElementFormControl&>(*document->GetElementById("mic-mode")).SetValue("virtual");
        if (launcher.audio_settings().volume != 45 || !launcher.audio_settings().mute_unfocused ||
            launcher.audio_settings().interpolation != st::AudioInterpolation::cubic ||
            document->GetElementById("master-volume-value")->GetInnerRML() != "45%" ||
            !static_cast<Rml::ElementFormControl&>(*document->GetElementById("input-device")).IsDisabled() ||
            static_cast<Rml::ElementFormControl&>(*document->GetElementById("mic-gain")).IsDisabled()) return 16;
        static_cast<Rml::ElementFormControl&>(*document->GetElementById("mic-mode")).SetValue("off");
        if (!static_cast<Rml::ElementFormControl&>(*document->GetElementById("input-device")).IsDisabled() ||
            !static_cast<Rml::ElementFormControl&>(*document->GetElementById("mic-gain")).IsDisabled()) return 16;
        static_cast<Rml::ElementFormControl&>(*document->GetElementById("mic-mode")).SetValue("automatic");
        static_cast<Rml::ElementFormControl&>(*document->GetElementById("master-volume")).SetValue("100");
        static_cast<Rml::ElementFormControl&>(*document->GetElementById("mute-unfocused")).SetValue("off");
        static_cast<Rml::ElementFormControl&>(*document->GetElementById("audio-interp")).SetValue("none");
        document->GetElementById("tab-game")->DispatchEvent(Rml::EventId::Click, {});
        if (!check_layout(window, *context, *document, kWidth, kHeight, false, false, {}) ||
            !check_layout(window, *context, *document, 390, 300, true, true, {})) return 17;
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
        std::fill(tiles.begin() + 2 * st::ds_width * st::ds_screen_height, tiles.end(), 0xff9f7350);
        for (int plane = 0; plane < 3; ++plane)
            for (int y = 0; y < 8; ++y)
                for (int x = 0; x < 8; ++x) {
                    tiles[(plane * st::ds_screen_height + y) * st::ds_width + x] = 0xffdd0022;
                    tiles[((plane + 1) * st::ds_screen_height - 1 - y) * st::ds_width + x] = 0xff22dd00;
                }
        document->Hide();
        // Synthetic colors only: exercise the actual GL compositor, hit testing and persistent menu controls.
        window.set_screen_layout(st::ScreenLayout::single, true);
        for (const int opacity : {50, 75, 100}) {
            window.set_secondary_view({20, opacity});
            if (!window.present_game(tiles.data(), false, false, false)) return 18;
            if (!previews.empty()) {
                const auto shot = previews / ("single-overlay-" + std::to_string(opacity) + ".bmp");
                if (!window.save_game_preview(path_to_utf8(shot).c_str())) return 18;
            }
            int w = 0, h = 0;
            SDL_GetWindowSize(window.window(), &w, &h);
            const auto views = st::screen_views(st::ScreenLayout::single, w, h, {20, opacity});
            float x = views.top.x + views.top.width / 2.f, y = views.top.y + views.top.height / 2.f;
            if (window.game_coordinates(x, y, false)) return 18;
        }
        if (!window.present_game(tiles.data(), true, false, true)) return 18;
        if (!previews.empty() && !window.save_game_preview(path_to_utf8(previews / "single-cinematic.bmp").c_str())) return 18;
        window.peek_secondary(true);
        if (!window.present_game(tiles.data(), true, false, true)) return 18;
        if (!previews.empty() && !window.save_game_preview(path_to_utf8(previews / "single-peek.bmp").c_str())) return 18;
        float peek_x = 100.f, peek_y = 100.f;
        if (window.game_coordinates(peek_x, peek_y, false)) return 18;
        window.peek_secondary(false);
        window.set_secondary_view({});
        if (!window.present_game(tiles.data(), false, false, false)) return 18;
        if (!previews.empty() && !window.save_game_preview(path_to_utf8(previews / "single-hidden.bmp").c_str())) return 18;
        pause.open();
        if (!check_controls_menu(*pause_document,launcher,*context,[&](SDL_Event& e) { pause.input(e); },
                [&] { return pause.draw(); }, [&] {
                    return window.draw_pause(*context,previews.empty() ? nullptr : path_to_utf8(preview("controls-conflict-pause.bmp")).c_str());
                })) return 19;
        pause_document->GetElementById("tab-controls")->DispatchEvent(Rml::EventId::Click,{});
        static_cast<Rml::ElementFormControl&>(*pause_document->GetElementById("controls-group")).SetValue("instrument");
        if (!pause.draw()) return 19;
        if (!previews.empty() && !window.draw_pause(*context,path_to_utf8(preview("controls-pause.bmp")).c_str())) return 19;
        pause_document->GetElementById("tab-screens")->DispatchEvent(Rml::EventId::Click,{});
        if (window.pause_on_menu() || launcher.pause_on_menu() ||
            pause_document->GetElementById("pause-title")->GetInnerRML() != "Menu") return 17;
        pause_document->GetElementById("pause-mode")->DispatchEvent(Rml::EventId::Click, {});
        if (!window.pause_on_menu() || !launcher.pause_on_menu() ||
            static_cast<Rml::ElementFormControl&>(*document->GetElementById("pause-on-menu")).GetValue() != "on" ||
            pause_document->GetElementById("pause-title")->GetInnerRML() != "Paused" || !pause.draw()) return 17;
        pause_document->GetElementById("pause-mode")->DispatchEvent(Rml::EventId::Click, {});
        if (window.pause_on_menu() || launcher.pause_on_menu() ||
            static_cast<Rml::ElementFormControl&>(*document->GetElementById("pause-on-menu")).GetValue() != "off" ||
            pause_document->GetElementById("pause-title")->GetInnerRML() != "Menu" || !pause.draw()) return 17;
        pause_document->GetElementById("tab-features")->DispatchEvent(Rml::EventId::Click, {});
        for (unsigned mask = 0; mask < 8; ++mask) {
            show_enhancements(*pause_document, {bool(mask & 1), bool(mask & 2), bool(mask & 4)});
            if (window.screen_features() != mask || launcher.screen_enhancements().mask() != mask ||
                read_enhancements(*document).mask() != mask || !pause.draw()) return 14;
        }
        pause_document->GetElementById("tab-audio")->DispatchEvent(Rml::EventId::Click, {});
        static_cast<Rml::ElementFormControl&>(*pause_document->GetElementById("master-volume")).SetValue("35");
        static_cast<Rml::ElementFormControl&>(*pause_document->GetElementById("audio-interp")).SetValue("gaussian");
        static_cast<Rml::ElementFormControl&>(*pause_document->GetElementById("mic-mode")).SetValue("hybrid");
        static_cast<Rml::ElementFormControl&>(*pause_document->GetElementById("mic-gain")).SetValue("150");
        if (window.audio_settings().volume != 35 ||
            window.audio_settings().interpolation != st::AudioInterpolation::gaussian ||
            window.audio_settings().mic_mode != st::MicMode::hybrid ||
            window.audio_settings().mic_gain != 150 ||
            launcher.audio_settings() != window.audio_settings() ||
            read_audio_settings(*document) != window.audio_settings() || !pause.draw()) return 16;
        pause_document->GetElementById("tab-screens")->DispatchEvent(Rml::EventId::Click, {});
        for (const auto& [layout, key] : kLayoutOptions) {
            layout_control.SetValue(key);
            if (launcher.screen_layout() != layout || !window.present_game(tiles.data(), false, false, false) ||
                !pause.draw()) return 10;
        }
        layout_control.SetValue("single");
        auto& size_control = static_cast<Rml::ElementFormControl&>(*pause_document->GetElementById("secondary-size"));
        auto& opacity_control = static_cast<Rml::ElementFormControl&>(*pause_document->GetElementById("secondary-opacity"));
        for (const int size : {0, 20, 30}) {
            size_control.SetValue(std::to_string(size));
            if (opacity_control.IsDisabled() != (size == 0)) return 18;
            for (const int opacity : {50, 75, 100}) {
                opacity_control.SetValue(std::to_string(opacity));
                if (launcher.secondary_view() != st::SecondaryViewSettings{size, opacity} ||
                    read_secondary_view(*document) != launcher.secondary_view() || !pause.draw()) return 18;
            }
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
        bool audio_devices_changed = false;
        const bool open = window.process_events(*context, &audio_devices_changed);
        if (audio_devices_changed) launcher.refresh_audio_devices();
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
