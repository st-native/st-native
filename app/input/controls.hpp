// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace st::input {

// The first twelve actions follow the public DS KEYINPUT bit order. Instrument actions
// are generic input only: game/st must explicitly enable their context before using them.
enum class Action : uint8_t {
    a, b, select, start, right, left, up, down, r, l, x, y,
    touch, blow, map, menu, original_view, fast,
    note1, note2, note3, note4, note5, note6,
    menu_accept, menu_back, name_erase, name_clear, skip_cutscene, count
};
inline constexpr size_t action_count = size_t(Action::count);
inline constexpr uint32_t bit(Action a) { return 1u << unsigned(a); }
enum class Context { gameplay, instrument, game_menu, name_keyboard, cutscene };
enum class Kind : uint8_t { none, key, mouse, button, trigger };
enum class Symbols { automatic, xbox, playstation, nintendo };
struct Binding {
    Kind kind = Kind::none;
    int code = 0;
    constexpr bool operator==(const Binding&) const = default;
};
inline constexpr Binding key(SDL_Scancode code) { return {Kind::key, int(code)}; }
inline constexpr Binding button(SDL_GamepadButton code) { return {Kind::button, int(code)}; }
inline constexpr std::array<std::string_view, action_count> action_ids{
    "ds.a","ds.b","ds.select","ds.start","ds.right","ds.left","ds.up","ds.down","ds.r","ds.l","ds.x","ds.y",
    "pointer.down","mic.blow","screen.peek","ui.menu","screen.original","speed.fast",
    "instrument.note1","instrument.note2","instrument.note3","instrument.note4","instrument.note5","instrument.note6",
    "game-menu.accept","game-menu.back","name.erase","name.clear","cutscene.skip"
};
inline constexpr std::array<std::string_view, action_count> action_titles{
    "DS A","DS B","DS Select","DS Start","Right","Left","Up","Down","DS R","DS L","DS X","DS Y",
    "Touch / draw","Virtual blow","Peek upper screen","Settings menu","Original view","Fast forward",
    "Note 1","Note 2","Note 3","Note 4","Note 5","Note 6",
    "Menu: confirm","Menu: back","Name: erase letter","Name: clear","Skip opening story"
};
inline constexpr bool instrument(Action a) { return a >= Action::note1 && a <= Action::note6; }
inline constexpr bool menu_action(Action a) { return a >= Action::menu_accept && a < Action::count; }
inline constexpr bool shared(Action a) { return a >= Action::touch && a < Action::note1; }
inline constexpr bool enabled(Action a, Context context) {
    if (shared(a)) return true;
    if (instrument(a)) return context == Context::instrument;
    if (a == Action::name_erase || a == Action::name_clear) return context == Context::name_keyboard;
    if (a == Action::skip_cutscene) return context == Context::cutscene;
    if (menu_action(a)) return context == Context::game_menu || context == Context::name_keyboard;
    if (a == Action::start || (a >= Action::right && a <= Action::down)) return context != Context::instrument && context != Context::cutscene;
    return context == Context::gameplay;
}
inline constexpr bool overlaps(Action a, Action b) {
    for (auto c : {Context::gameplay,Context::instrument,Context::game_menu,Context::name_keyboard,Context::cutscene})
        if (enabled(a,c) && enabled(b,c)) return true;
    return false;
}
inline bool valid(Binding b) {
    switch (b.kind) {
    case Kind::none: return b.code == 0;
    case Kind::key: return b.code > SDL_SCANCODE_UNKNOWN && b.code < SDL_SCANCODE_COUNT;
    case Kind::mouse: return b.code >= 1 && b.code <= 5;
    case Kind::button: return b.code >= 0 && b.code < SDL_GAMEPAD_BUTTON_COUNT;
    case Kind::trigger: return b.code == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || b.code == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
    }
    return false;
}
inline constexpr bool keyboard_slot(size_t slot) { return slot < 2; }
inline bool valid_slot(size_t slot, Binding b) {
    return slot < 4 && valid(b) && (b.kind == Kind::none ||
        (keyboard_slot(slot) ? b.kind == Kind::key || b.kind == Kind::mouse : b.kind == Kind::button || b.kind == Kind::trigger));
}
inline bool reserved(Action a, Binding b) {
    if (b.kind != Kind::key) return false;
    // Recovery keys always work, even after an accidental rebind. Escape also cancels capture.
    return (b.code == SDL_SCANCODE_ESCAPE && a != Action::menu) || b.code == SDL_SCANCODE_F1 ||
        b.code == SDL_SCANCODE_F11 || (b.code == SDL_SCANCODE_F2 && a != Action::original_view);
}
inline constexpr std::array<std::string_view, 4> symbol_ids{"automatic","xbox","playstation","nintendo"};
struct Controls {
    // primary/alternate keyboard+mouse, primary/alternate controller
    std::array<std::array<Binding, 4>, action_count> bindings{};
    Symbols symbols = Symbols::automatic;
    bool game_menus = true;
    int deadzone = 49, trigger_threshold = 24;
    bool operator==(const Controls&) const = default;

    Controls() {
        const std::array keys{SDL_SCANCODE_X,SDL_SCANCODE_Z,SDL_SCANCODE_BACKSPACE,SDL_SCANCODE_RETURN,
            SDL_SCANCODE_RIGHT,SDL_SCANCODE_LEFT,SDL_SCANCODE_UP,SDL_SCANCODE_DOWN,SDL_SCANCODE_W,SDL_SCANCODE_Q,
            SDL_SCANCODE_S,SDL_SCANCODE_A};
        const std::array pads{SDL_GAMEPAD_BUTTON_EAST,SDL_GAMEPAD_BUTTON_SOUTH,SDL_GAMEPAD_BUTTON_BACK,
            SDL_GAMEPAD_BUTTON_START,SDL_GAMEPAD_BUTTON_DPAD_RIGHT,SDL_GAMEPAD_BUTTON_DPAD_LEFT,
            SDL_GAMEPAD_BUTTON_DPAD_UP,SDL_GAMEPAD_BUTTON_DPAD_DOWN,SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
            SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,SDL_GAMEPAD_BUTTON_NORTH,SDL_GAMEPAD_BUTTON_WEST};
        for (size_t i = 0; i < keys.size(); ++i) {
            bindings[i][0] = key(keys[i]);
            if (pads[i] != SDL_GAMEPAD_BUTTON_INVALID) bindings[i][2] = button(pads[i]);
        }
        bindings[size_t(Action::touch)][0] = {Kind::mouse, SDL_BUTTON_LEFT};
        bindings[size_t(Action::blow)] = {key(SDL_SCANCODE_SPACE),key(SDL_SCANCODE_C),
            button(SDL_GAMEPAD_BUTTON_RIGHT_STICK),Binding{Kind::trigger,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER}};
        bindings[size_t(Action::map)][0] = key(SDL_SCANCODE_M);
        bindings[size_t(Action::menu)][0] = key(SDL_SCANCODE_ESCAPE);
        bindings[size_t(Action::menu)][2] = button(SDL_GAMEPAD_BUTTON_LEFT_STICK);
        bindings[size_t(Action::original_view)][0] = key(SDL_SCANCODE_F2);
        bindings[size_t(Action::fast)][0] = key(SDL_SCANCODE_TAB);
        const std::array note_keys{SDL_SCANCODE_A,SDL_SCANCODE_S,SDL_SCANCODE_D,SDL_SCANCODE_F,SDL_SCANCODE_J,SDL_SCANCODE_K};
        const std::array note_pads{SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,SDL_GAMEPAD_BUTTON_WEST,SDL_GAMEPAD_BUTTON_NORTH,
            SDL_GAMEPAD_BUTTON_SOUTH,SDL_GAMEPAD_BUTTON_EAST,SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER};
        for (size_t i = 0; i < note_keys.size(); ++i) {
            bindings[size_t(Action::note1)+i][0] = key(note_keys[i]);
            bindings[size_t(Action::note1)+i][2] = button(note_pads[i]);
        }
        bindings[size_t(Action::menu_accept)] = {key(SDL_SCANCODE_X),{},button(SDL_GAMEPAD_BUTTON_SOUTH),{}};
        bindings[size_t(Action::menu_back)] = {key(SDL_SCANCODE_Z),{},button(SDL_GAMEPAD_BUTTON_EAST),{}};
        bindings[size_t(Action::name_erase)] = {key(SDL_SCANCODE_BACKSPACE),{},button(SDL_GAMEPAD_BUTTON_WEST),{}};
        bindings[size_t(Action::skip_cutscene)] = {key(SDL_SCANCODE_RETURN),{},button(SDL_GAMEPAD_BUTTON_START),{}};
        bindings[size_t(Action::name_clear)] = {key(SDL_SCANCODE_DELETE),{},button(SDL_GAMEPAD_BUTTON_NORTH),{}};
    }
    std::optional<Action> conflict(Action action, size_t slot, Binding value) const {
        if (value.kind == Kind::none) return {};
        for (size_t a = 0; a < action_count; ++a)
            if (overlaps(action, Action(a)))
                for (size_t s = 0; s < 4; ++s)
                    if ((a != size_t(action) || s != slot) && bindings[a][s] == value) return Action(a);
        return {};
    }
    bool assign(Action action, size_t slot, Binding value) {
        if (size_t(action) >= action_count || !valid_slot(slot,value) || reserved(action,value) || conflict(action,slot,value)) return false;
        bindings[size_t(action)][slot] = value;
        return true;
    }
    // Apply a user-confirmed transfer atomically. Bindings in disjoint contexts survive.
    bool transfer(Action action, size_t slot, Binding value) {
        if (size_t(action) >= action_count || !valid_slot(slot,value) || reserved(action,value)) return false;
        auto changed = *this;
        if (value.kind != Kind::none)
            for (size_t a = 0; a < action_count; ++a)
                if (overlaps(action,Action(a)))
                    for (size_t s = 0; s < 4; ++s)
                        if ((a != size_t(action) || s != slot) && changed.bindings[a][s] == value)
                            changed.bindings[a][s] = {};
        if (!changed.assign(action,slot,value) || !changed.validate()) return false;
        *this = changed;
        return true;
    }
    bool validate() const {
        if (unsigned(symbols) >= symbol_ids.size() || deadzone < 10 || deadzone > 90 || trigger_threshold < 5 || trigger_threshold > 95) return false;
        for (size_t a = 0; a < action_count; ++a)
            for (size_t s = 0; s < 4; ++s)
                if (!valid_slot(s,bindings[a][s]) || reserved(Action(a),bindings[a][s]) || conflict(Action(a),s,bindings[a][s])) return false;
        return true;
    }
};

// Held state is resolved from all bindings, so releasing an alternate or another device
// cannot cancel a still-held primary binding. Reset on context/focus/device/menu changes.
class State {
public:
    Controls settings;
    void clear() { keys_.fill(false); mouse_.fill(false); buttons_.fill(false); axes_.fill(0); note_order_.fill(0); serial_ = 0; }
    void context(Context value) { if (context_ != value) { clear(); context_ = value; } }
    void event(const SDL_Event& e) {
        const auto before = actions();
        if ((e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP) && e.key.scancode > 0 && e.key.scancode < SDL_SCANCODE_COUNT)
            keys_[e.key.scancode] = e.type == SDL_EVENT_KEY_DOWN;
        else if ((e.type == SDL_EVENT_MOUSE_BUTTON_DOWN || e.type == SDL_EVENT_MOUSE_BUTTON_UP) && e.button.button < mouse_.size())
            mouse_[e.button.button] = e.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
        else if ((e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || e.type == SDL_EVENT_GAMEPAD_BUTTON_UP) && e.gbutton.button < buttons_.size())
            buttons_[e.gbutton.button] = e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
        else if (e.type == SDL_EVENT_GAMEPAD_AXIS_MOTION && e.gaxis.axis < axes_.size()) axes_[e.gaxis.axis] = e.gaxis.value;
        const auto rising = actions() & ~before;
        for (unsigned i = 0; i < 6; ++i)
            if (rising & bit(Action(unsigned(Action::note1)+i))) note_order_[i] = ++serial_;
    }
    bool held(Binding b) const {
        if (!valid(b)) return false;
        switch (b.kind) {
        case Kind::key: return keys_[b.code];
        case Kind::mouse: return mouse_[b.code];
        case Kind::button: return buttons_[b.code];
        case Kind::trigger: return int(axes_[b.code]) > settings.trigger_threshold * 32767 / 100;
        default: return false;
        }
    }
    uint32_t actions() const {
        uint32_t result = 0;
        for (size_t a = 0; a < action_count; ++a)
            if (enabled(Action(a),context_) && std::any_of(settings.bindings[a].begin(),settings.bindings[a].end(),
                    [this](Binding b) { return held(b); })) result |= bit(Action(a));
        if (context_ != Context::instrument) {
            const int dz = settings.deadzone * 32767 / 100;
            if (axes_[SDL_GAMEPAD_AXIS_LEFTX] > dz) result |= bit(Action::right);
            if (axes_[SDL_GAMEPAD_AXIS_LEFTX] < -dz) result |= bit(Action::left);
            if (axes_[SDL_GAMEPAD_AXIS_LEFTY] > dz) result |= bit(Action::down);
            if (axes_[SDL_GAMEPAD_AXIS_LEFTY] < -dz) result |= bit(Action::up);
        }
        return result;
    }
    bool down(Action a) const { return (actions() & bit(a)) != 0; }
    uint32_t ds_buttons() const { return actions() & 0xfff; }
    std::optional<unsigned> active_note() const {
        std::optional<unsigned> result;
        const auto held = actions();
        for (unsigned i = 0; i < 6; ++i)
            if ((held & bit(Action(unsigned(Action::note1)+i))) && (!result || note_order_[i] > note_order_[*result])) result = i;
        return result;
    }
private:
    Context context_ = Context::gameplay;
    std::array<bool, SDL_SCANCODE_COUNT> keys_{};
    std::array<bool, 6> mouse_{};
    std::array<bool, SDL_GAMEPAD_BUTTON_COUNT> buttons_{};
    std::array<Sint16, SDL_GAMEPAD_AXIS_COUNT> axes_{};
    std::array<uint64_t,6> note_order_{};
    uint64_t serial_ = 0;
};

inline SDL_GamepadType symbol_type(Symbols style, SDL_GamepadType detected) {
    switch (style) {
    case Symbols::xbox: return SDL_GAMEPAD_TYPE_XBOXONE;
    case Symbols::playstation: return SDL_GAMEPAD_TYPE_PS5;
    case Symbols::nintendo: return SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO;
    default: return detected;
    }
}
inline bool sony(SDL_GamepadType t) { return t == SDL_GAMEPAD_TYPE_PS3 || t == SDL_GAMEPAD_TYPE_PS4 || t == SDL_GAMEPAD_TYPE_PS5; }
inline bool nintendo(SDL_GamepadType t) { return t == SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO || t == SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR || t == SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT || t == SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT; }
inline std::string label(Binding b, Symbols style = Symbols::automatic, SDL_GamepadType detected = SDL_GAMEPAD_TYPE_UNKNOWN) {
    if (!valid(b) || b.kind == Kind::none) return "Unbound";
    if (b.kind == Kind::key) { const char* name = SDL_GetScancodeName(SDL_Scancode(b.code)); return name && *name ? name : "Key " + std::to_string(b.code); }
    if (b.kind == Kind::mouse) {
        constexpr std::array names{"", "Mouse left", "Mouse middle", "Mouse right", "Mouse 4", "Mouse 5"};
        return names[b.code];
    }
    const auto type = symbol_type(style,detected);
    if (b.kind == Kind::button && type == SDL_GAMEPAD_TYPE_UNKNOWN && b.code <= SDL_GAMEPAD_BUTTON_NORTH) {
        constexpr std::array names{"South","East","West","North"};
        return names[b.code];
    }
    if (b.kind == Kind::trigger) return b.code == SDL_GAMEPAD_AXIS_LEFT_TRIGGER ?
        (sony(type) ? "L2" : nintendo(type) ? "ZL" : "LT") : (sony(type) ? "R2" : nintendo(type) ? "ZR" : "RT");
    const auto face = SDL_GetGamepadButtonLabelForType(type,SDL_GamepadButton(b.code));
    switch (face) {
    case SDL_GAMEPAD_BUTTON_LABEL_A: return "A";
    case SDL_GAMEPAD_BUTTON_LABEL_B: return "B";
    case SDL_GAMEPAD_BUTTON_LABEL_X: return "X";
    case SDL_GAMEPAD_BUTTON_LABEL_Y: return "Y";
    case SDL_GAMEPAD_BUTTON_LABEL_CROSS: return "Cross";
    case SDL_GAMEPAD_BUTTON_LABEL_CIRCLE: return "Circle";
    case SDL_GAMEPAD_BUTTON_LABEL_SQUARE: return "Square";
    case SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE: return "Triangle";
    default: break;
    }
    switch (b.code) {
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return sony(type) ? "L1" : nintendo(type) ? "L" : "LB";
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return sony(type) ? "R1" : nintendo(type) ? "R" : "RB";
    case SDL_GAMEPAD_BUTTON_START: return type == SDL_GAMEPAD_TYPE_PS3 ? "Start" : sony(type) ? "Options" : nintendo(type) ? "+" : "Menu";
    case SDL_GAMEPAD_BUTTON_BACK: return type == SDL_GAMEPAD_TYPE_PS3 ? "Select" : type == SDL_GAMEPAD_TYPE_PS5 ? "Create" : sony(type) ? "Share" : nintendo(type) ? "-" : "View";
    case SDL_GAMEPAD_BUTTON_LEFT_STICK: return "L-stick click";
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK: return "R-stick click";
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return "D-pad up";
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return "D-pad down";
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return "D-pad left";
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return "D-pad right";
    default: return "Button " + std::to_string(b.code);
    }
}
} // namespace st::input
