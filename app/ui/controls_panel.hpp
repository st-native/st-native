// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <RmlUi/Core.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>
#include <RmlUi/Core/Elements/ElementFormControlSelect.h>
#include <RmlUi/Core/StringUtilities.h>
#include <functional>
#include <cstdio>
#include "../input/controls.hpp"
#include "../../game/st/hooks/control_profile.hpp"

namespace st {

inline std::string control_title(input::Action action) {
    return std::string(input::instrument(action) ? pipe_control_titles[size_t(action)-size_t(input::Action::note1)] : input::action_titles[size_t(action)]);
}

class ControlsPanel {
public:
    using Get = std::function<input::Controls()>;
    using Set = std::function<void(const input::Controls&)>;
    ControlsPanel(Rml::ElementDocument& doc, Get get, Set set, std::function<SDL_JoystickID()> device)
        : doc_(doc), get_(std::move(get)), set_(std::move(set)), device_(std::move(device)) {
        doc_.GetElementById("settings-controls")->SetInnerRML(R"(
          <div class="control-pages"><input type="submit" id="controls-keyboard" value="Keyboard &amp; mouse" />
            <input type="submit" id="controls-gamepad" class="selected" value="Controller" /></div>
          <p id="controls-device" class="hint"></p>
          <div class="controls-columns"><div class="controls-setup">
          <div class="field"><label for="controls-group">Actions</label><select id="controls-group">
            <option value="general">General</option><option value="menus">Game menus</option><option value="instrument">Flute</option></select></div>
          <div class="field"><label for="controls-game-menus">PC game menus</label><select id="controls-game-menus">
            <option value="on">On</option><option value="off">Off (original)</option></select></div>
          <div id="controls-pad-options">
            <div class="field"><label for="controls-symbols">Button labels</label><select id="controls-symbols">
              <option value="0">Automatic</option><option value="1">Xbox</option>
              <option value="2">PlayStation</option><option value="3">Nintendo</option></select></div>
            <div class="field"><label for="controls-deadzone">Stick dead zone</label><div class="range-group">
              <input type="range" class="slider" id="controls-deadzone" min="10" max="90" step="1" value="49" />
              <span id="controls-deadzone-value" class="range-value">49%</span></div></div>
            <div class="field"><label for="controls-threshold">Trigger threshold</label><div class="range-group">
              <input type="range" class="slider" id="controls-threshold" min="5" max="95" step="1" value="24" />
              <span id="controls-threshold-value" class="range-value">24%</span></div></div>
          </div>
          <p id="controls-scope" class="hint"></p>
          </div><div class="controls-editor">
          <div class="binding-heading"><span>Action</span><span>Primary</span><span>Alternate</span></div>
          <div id="controls-bindings"></div>
          <p id="controls-message" class="hint">Choose a binding, then press a key, mouse button or controller input.</p>
          <input type="submit" id="controls-reset" value="Reset controller bindings" />
          </div></div>
          <p id="controls-menu-help" class="hint"></p>
        )");
        auto modal = doc_.CreateElement("div");
        modal->SetId("controls-conflict");
        modal->SetInnerRML(R"(<div id="controls-conflict-panel">
            <h2>Input already assigned</h2><p id="controls-conflict-description"></p>
            <p>Remove the conflicting assignment and use it here?</p>
            <div class="conflict-actions"><input type="submit" id="controls-conflict-cancel" value="Cancel" />
            <input type="submit" id="controls-conflict-replace" value="Replace assignment" /></div></div>)");
        doc_.AppendChild(std::move(modal));
        refresh(true);
    }
    bool capturing() const { return capture_.has_value(); }
    void cancel() { dismiss_conflict(); capture_.reset(); refresh(true); }
    void enabled(bool value) { enabled_ = value; refresh(true); }
    void refresh(bool force = false) {
        const auto device_id = device_();
        auto* pad = device_id ? SDL_GetGamepadFromID(device_id) : nullptr;
        const auto type = pad ? SDL_GetGamepadType(pad) : SDL_GAMEPAD_TYPE_UNKNOWN;
        const auto settings = get_();
        const char* name = pad ? SDL_GetGamepadName(pad) : nullptr;
        const std::string device = name ? name : "No controller connected";
        if (!force && settings == shown_ && type == type_ && device == device_name_) return;
        shown_ = settings; type_ = type; device_name_ = device;
        syncing_ = true;
        form("controls-symbols").SetValue(std::to_string(int(settings.symbols)));
        form("controls-game-menus").SetValue(settings.game_menus ? "on" : "off");
        form("controls-deadzone").SetValue(std::to_string(settings.deadzone));
        form("controls-threshold").SetValue(std::to_string(settings.trigger_threshold));
        doc_.GetElementById("controls-deadzone-value")->SetInnerRML(std::to_string(settings.deadzone)+"%");
        doc_.GetElementById("controls-threshold-value")->SetInnerRML(std::to_string(settings.trigger_threshold)+"%");
        doc_.GetElementById("controls-pad-options")->SetProperty("display", keyboard_ ? "none" : "block");
        doc_.GetElementById("controls-device")->SetInnerRML(keyboard_ ? "Keyboard and mouse" : encode(device));
        doc_.GetElementById("controls-keyboard")->SetClass("selected",keyboard_);
        doc_.GetElementById("controls-gamepad")->SetClass("selected",!keyboard_);
        doc_.GetElementById("controls-scope")->SetInnerRML(menus_ ?
            "In supported game menus, D-pad / left stick changes the selection. Confirm activates it. Start accepts the name or skips the opening story." : instrument_ ?
            "Bindings are saved. Button-note gameplay will be enabled with the flute adaptation." :
            "DS buttons are the original game shortcuts. The left stick acts as the D-pad.");
        std::string rows;
        for (size_t a = 0; a < input::action_count; ++a) {
            if (menus_ ? !input::menu_action(input::Action(a)) :
                (input::menu_action(input::Action(a)) || input::instrument(input::Action(a)) != instrument_)) continue;
            rows += "<div class=\"binding-row\"><label>"+encode(control_title(input::Action(a)))+"</label>";
            for (size_t s = keyboard_ ? 0 : 2; s < (keyboard_ ? 2 : 4); ++s) {
                const bool waiting = capture_ && capture_->first == a && capture_->second == s;
                rows += "<input type=\"submit\" id=\"binding-"+std::to_string(a)+"-"+std::to_string(s)+
                    "\" value=\""+encode(waiting ? "Waiting…" : input::label(settings.bindings[a][s],settings.symbols,type))+"\""+
                    (enabled_ ? "" : " disabled=\"disabled\"")+" />";
            }
            rows += "</div>";
        }
        auto* bindings = doc_.GetElementById("controls-bindings");
        const float scroll = bindings->GetScrollTop();
        bindings->SetInnerRML(rows);
        bindings->SetScrollTop(scroll);
        form("controls-reset").SetValue(keyboard_ ? "Reset keyboard & mouse bindings" : "Reset controller bindings");
        const auto menu_label = input::label(settings.bindings[size_t(input::Action::menu)][2],settings.symbols,type);
        const auto choose = input::label(input::button(SDL_GAMEPAD_BUTTON_SOUTH),settings.symbols,type);
        const auto clear = input::label(input::button(SDL_GAMEPAD_BUTTON_NORTH),settings.symbols,type);
        doc_.GetElementById("controls-menu-help")->SetInnerRML(encode("D-pad up/down: focus · "+choose+": choose · "+
            clear+": clear focused binding · "+menu_label+": open/close menu. Esc/F1 always work; F11 toggles fullscreen."));
        auto shortcut = [&](input::Action action) {
            const auto& bindings = settings.bindings[size_t(action)];
            std::string result = input::label(bindings[0],settings.symbols,type);
            if (bindings[2].kind != input::Kind::none) result += " / "+input::label(bindings[2],settings.symbols,type);
            return result;
        };
        if (auto* note = doc_.GetElementById("single-screen-note")) note->SetInnerRML(encode(
            "Single screen is currently manual. Some scenes need both views. Upper-screen peek: "+shortcut(input::Action::map)+" (hold). F2 restores both."));
        if (auto* hint = doc_.GetElementById("map-shortcut")) hint->SetInnerRML(encode(
            "Upper-screen peek: "+shortcut(input::Action::map)+" (hold). F2 restores both original screens."));
        for (const char* id : {"controls-keyboard","controls-gamepad","controls-group","controls-game-menus","controls-symbols","controls-deadzone","controls-threshold","controls-reset"})
            form(id).SetDisabled(!enabled_);
        syncing_ = false;
    }
    bool process(Rml::Event& event) {
        auto* target = event.GetTargetElement();
        while (target && target->GetId().empty()) target = target->GetParentNode();
        if (!target) return false;
        const auto& id = target->GetId();
        if (pending_) {
            if (event.GetId() == Rml::EventId::Click && id == "controls-conflict-replace") resolve_conflict(true);
            else if (event.GetId() == Rml::EventId::Click && id == "controls-conflict-cancel") resolve_conflict(false);
            return true;
        }
        if (!id.starts_with("controls-") && !id.starts_with("binding-")) return false;
        if (syncing_ || !enabled_) return true;
        if (event.GetId() == Rml::EventId::Change) {
            if (id == "controls-group") {
                instrument_ = form("controls-group").GetValue() == "instrument";
                menus_ = form("controls-group").GetValue() == "menus";
                capture_.reset(); refresh(true);
                doc_.GetElementById("controls-bindings")->SetScrollTop(0);
            } else if (id == "controls-symbols" || id == "controls-deadzone" || id == "controls-threshold" || id == "controls-game-menus") {
                auto settings = get_();
                if (id == "controls-game-menus") settings.game_menus = form("controls-game-menus").GetValue() == "on";
                if (id == "controls-symbols") settings.symbols = input::Symbols(std::atoi(form(id.c_str()).GetValue().c_str()));
                if (id == "controls-deadzone") settings.deadzone = std::atoi(form(id.c_str()).GetValue().c_str());
                if (id == "controls-threshold") settings.trigger_threshold = std::atoi(form(id.c_str()).GetValue().c_str());
                if (settings.validate()) set_(settings);
                refresh(true);
            }
            return true;
        }
        if (event.GetId() != Rml::EventId::Click) return true;
        if (id == "controls-keyboard" || id == "controls-gamepad") {
            keyboard_ = id == "controls-keyboard"; capture_.reset(); refresh(true);
        } else if (id == "controls-reset") {
            auto settings = get_(); const input::Controls defaults;
            for (size_t a = 0; a < input::action_count; ++a)
                for (size_t s = keyboard_ ? 0 : 2; s < (keyboard_ ? 2 : 4); ++s) settings.bindings[a][s] = defaults.bindings[a][s];
            set_(settings); capture_.reset(); message("Default bindings restored."); refresh(true);
        } else if (id.starts_with("binding-")) {
            unsigned a = 0, s = 0;
            if (std::sscanf(id.c_str(),"binding-%u-%u",&a,&s) == 2 && a < input::action_count && s < 4) {
                capture_ = std::pair<size_t,size_t>{a,s};
                message(keyboard_ ? "Press a key or mouse button. Esc cancels; Delete clears." :
                    "Press a controller button or trigger. Your settings-menu binding cancels; Delete clears.");
                refresh(true);
            }
        }
        return true;
    }
    bool input(const SDL_Event& event) {
        if (!doc_.IsVisible()) return false;
        if (release_guard_ && ((event.type == SDL_EVENT_KEY_UP && release_guard_ == input::key(event.key.scancode)) ||
            (event.type == SDL_EVENT_GAMEPAD_BUTTON_UP && release_guard_ == input::button(SDL_GamepadButton(event.gbutton.button))))) {
            release_guard_.reset(); return true;
        }
        if (!doc_.GetElementById("settings-controls")->IsVisible(true)) {
            if (capturing()) cancel();
            return false;
        }
        if (pending_) {
            if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST || event.type == SDL_EVENT_GAMEPAD_REMOVED) { cancel(); return true; }
            const bool key = event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat;
            const bool pad = event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
            if (key) release_guard_ = input::key(event.key.scancode);
            if (pad) release_guard_ = input::button(SDL_GamepadButton(event.gbutton.button));
            if ((key && event.key.scancode == SDL_SCANCODE_ESCAPE) || (pad && event.gbutton.button == SDL_GAMEPAD_BUTTON_EAST))
                resolve_conflict(false);
            else if ((key && (event.key.scancode == SDL_SCANCODE_TAB || event.key.scancode == SDL_SCANCODE_LEFT || event.key.scancode == SDL_SCANCODE_RIGHT)) ||
                     (pad && (event.gbutton.button == SDL_GAMEPAD_BUTTON_DPAD_LEFT || event.gbutton.button == SDL_GAMEPAD_BUTTON_DPAD_RIGHT ||
                              event.gbutton.button == SDL_GAMEPAD_BUTTON_DPAD_UP || event.gbutton.button == SDL_GAMEPAD_BUTTON_DPAD_DOWN))) {
                const bool replace = doc_.GetContext()->GetFocusElement() != doc_.GetElementById("controls-conflict-replace");
                doc_.GetElementById(replace ? "controls-conflict-replace" : "controls-conflict-cancel")->Focus();
            } else if ((key && (event.key.scancode == SDL_SCANCODE_RETURN || event.key.scancode == SDL_SCANCODE_SPACE)) ||
                       (pad && event.gbutton.button == SDL_GAMEPAD_BUTTON_SOUTH))
                resolve_conflict(doc_.GetContext()->GetFocusElement() == doc_.GetElementById("controls-conflict-replace"));
            // Mouse events reach the modal's own buttons; all other input is trapped here.
            return event.type != SDL_EVENT_MOUSE_BUTTON_DOWN && event.type != SDL_EVENT_MOUSE_BUTTON_UP && event.type != SDL_EVENT_MOUSE_MOTION;
        }
        if (!capturing()) {
            const bool clear = (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN && event.gbutton.button == SDL_GAMEPAD_BUTTON_NORTH) ||
                (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_DELETE && !event.key.repeat);
            auto* focused = doc_.GetContext()->GetFocusElement();
            unsigned a = 0, s = 0;
            if (enabled_ && clear && focused && std::sscanf(focused->GetId().c_str(),"binding-%u-%u",&a,&s) == 2 && a < input::action_count && s < 4) {
                auto settings = get_(); settings.assign(input::Action(a),s,{}); set_(settings);
                message("Binding cleared."); refresh(true); return true;
            }
            return false;
        }
        if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST || event.type == SDL_EVENT_GAMEPAD_REMOVED) {
            capture_.reset(); message("Binding capture cancelled."); refresh(true); return false;
        }
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.repeat) return true;
        const auto menu = get_().bindings[size_t(input::Action::menu)];
        const bool menu_button = event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN &&
            (menu[2] == input::button(SDL_GamepadButton(event.gbutton.button)) || menu[3] == input::button(SDL_GamepadButton(event.gbutton.button)));
        if ((event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_ESCAPE) || menu_button) {
            capture_.reset(); message("Binding unchanged."); refresh(true); return true;
        }
        std::optional<input::Binding> binding;
        const bool keyboard = input::keyboard_slot(capture_->second);
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_DELETE) binding = input::Binding{};
        else if (keyboard && event.type == SDL_EVENT_KEY_DOWN) binding = input::key(event.key.scancode);
        else if (keyboard && event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) binding = input::Binding{input::Kind::mouse,event.button.button};
        else if (!keyboard && event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) binding = input::button(SDL_GamepadButton(event.gbutton.button));
        else if (!keyboard && event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION && event.gaxis.value > 24000 &&
                 (event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || event.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER))
            binding = input::Binding{input::Kind::trigger,event.gaxis.axis};
        if (!binding) return event.type == SDL_EVENT_KEY_UP || event.type == SDL_EVENT_GAMEPAD_BUTTON_UP;
        auto settings = get_();
        const auto [a,s] = *capture_;
        if (input::reserved(input::Action(a),*binding)) {
            message("This input is reserved for menu recovery."); return true;
        }
        if (const auto conflict = settings.conflict(input::Action(a),s,*binding)) {
            pending_ = *binding;
            std::string owners;
            for (size_t other = 0; other < input::action_count; ++other)
                if (input::overlaps(input::Action(a),input::Action(other)) &&
                    std::any_of(settings.bindings[other].begin(),settings.bindings[other].end(),[&](auto b) { return b == *binding; }))
                    owners += (owners.empty() ? "" : ", ")+control_title(input::Action(other));
            doc_.GetElementById("controls-conflict-description")->SetInnerRML(encode(
                input::label(*binding,settings.symbols,type_)+" is assigned to: "+owners+". New action: "+control_title(input::Action(a))+"."));
            doc_.GetElementById("controls-conflict")->SetProperty("display","flex");
            doc_.GetElementById("controls-conflict-cancel")->Focus();
            return true;
        }
        if (!settings.assign(input::Action(a),s,*binding)) {
            message("This input is reserved for menu recovery or is unsupported."); return true;
        }
        set_(settings); capture_.reset(); message("Binding saved."); refresh(true);
        return true;
    }
private:
    void dismiss_conflict() {
        pending_.reset();
        doc_.GetElementById("controls-conflict")->SetProperty("display","none");
    }
    void resolve_conflict(bool replace) {
        if (!capture_ || !pending_) return;
        const auto [a,s] = *capture_;
        auto settings = get_();
        const bool saved = replace && settings.transfer(input::Action(a),s,*pending_);
        dismiss_conflict(); capture_.reset();
        if (saved) set_(settings);
        message(saved ? "Binding transferred." : "Bindings unchanged."); refresh(true);
        doc_.GetElementById("binding-"+std::to_string(a)+"-"+std::to_string(s))->Focus();
    }
    static std::string encode(std::string_view text) { return Rml::StringUtilities::EncodeRml(std::string(text)); }
    Rml::ElementFormControl& form(const char* id) { return static_cast<Rml::ElementFormControl&>(*doc_.GetElementById(id)); }
    void message(const std::string& text) { doc_.GetElementById("controls-message")->SetInnerRML(encode(text)); }
    Rml::ElementDocument& doc_;
    Get get_; Set set_;
    std::function<SDL_JoystickID()> device_;
    input::Controls shown_;
    SDL_GamepadType type_ = SDL_GAMEPAD_TYPE_UNKNOWN;
    std::string device_name_;
    std::optional<std::pair<size_t,size_t>> capture_;
    std::optional<input::Binding> pending_;
    std::optional<input::Binding> release_guard_;
    bool keyboard_ = false, instrument_ = false, menus_ = false, syncing_ = false, enabled_ = true;
};

// Menu navigation is independent of gameplay remapping and provides a recovery route.
inline bool gamepad_menu_input(Rml::Context& context, const SDL_Event& e) {
    if (e.type != SDL_EVENT_GAMEPAD_BUTTON_DOWN && e.type != SDL_EVENT_GAMEPAD_BUTTON_UP) return false;
    auto* focused = context.GetFocusElement();
    while (focused && focused->GetTagName() != "select") focused = focused->GetParentNode();
    auto* select = focused ? static_cast<Rml::ElementFormControlSelect*>(focused) : nullptr;
    const bool selecting = select && select->IsSelectBoxVisible();
    if (select && e.gbutton.button == SDL_GAMEPAD_BUTTON_SOUTH) {
        if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
            if (selecting) select->HideSelectBox(); else select->ShowSelectBox();
        }
        return true;
    }
    if (selecting && e.gbutton.button == SDL_GAMEPAD_BUTTON_EAST) {
        if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) select->CancelSelectBox();
        return true;
    }
    Rml::Input::KeyIdentifier key = Rml::Input::KI_UNKNOWN;
    int modifiers = 0;
    switch (e.gbutton.button) {
    case SDL_GAMEPAD_BUTTON_DPAD_UP: key = selecting ? Rml::Input::KI_UP : Rml::Input::KI_TAB; modifiers = selecting ? 0 : Rml::Input::KM_SHIFT; break;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: key = selecting ? Rml::Input::KI_DOWN : Rml::Input::KI_TAB; break;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: key = Rml::Input::KI_LEFT; break;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: key = Rml::Input::KI_RIGHT; break;
    case SDL_GAMEPAD_BUTTON_SOUTH: key = Rml::Input::KI_RETURN; break;
    case SDL_GAMEPAD_BUTTON_EAST: key = Rml::Input::KI_ESCAPE; break;
    default: return false;
    }
    if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) context.ProcessKeyDown(key,modifiers);
    else context.ProcessKeyUp(key,modifiers);
    return true;
}
} // namespace st
