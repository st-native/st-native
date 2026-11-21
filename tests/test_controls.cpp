// SPDX-License-Identifier: GPL-3.0-or-later
#include "../app/input/controls.hpp"
#include "../app/input/gamepads.hpp"
#include "../app/input/controller_replay.hpp"
#include "../game/st/hooks/menu_controls.hpp"
#include <cassert>
#include <stdexcept>

using namespace st::input;
static void require(bool value) { if (!value) throw std::runtime_error("Control invariant failed"); }
static SDL_Event key_event(SDL_Scancode scancode, bool down) {
    SDL_Event e{}; e.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP; e.key.scancode=scancode; return e;
}
static SDL_Event pad_event(SDL_GamepadButton button, bool down) {
    SDL_Event e{}; e.type = down ? SDL_EVENT_GAMEPAD_BUTTON_DOWN : SDL_EVENT_GAMEPAD_BUTTON_UP; e.gbutton.button=button; return e;
}
int main() {
    Controls settings;
    require(settings.validate());
    const auto replay_file = std::filesystem::temp_directory_path()/"st-controls-replay-test.txt";
    { std::ofstream out(replay_file); out << "# SDL input\n1 button south 1\n7 button south 0\n8 axis leftx 20000\n9 shot title\n"; }
    const auto replay = read_controller_replay(replay_file);
    require(replay.size()==4 && replay[0].event.gbutton.button==SDL_GAMEPAD_BUTTON_SOUTH && replay[3].shot=="title");
    { std::ofstream out(replay_file); out << "bad frame\n"; }
    bool rejected = false;
    try { (void)read_controller_replay(replay_file); } catch (const std::runtime_error&) { rejected = true; }
    require(rejected);
    std::filesystem::remove(replay_file);
    {
        Controls transfer;
        const auto original = transfer;
        require(!transfer.transfer(Action::a,0,key(SDL_SCANCODE_F1)) && transfer == original);
        require(transfer.transfer(Action::menu,0,key(SDL_SCANCODE_A)));
        require(transfer.bindings[size_t(Action::y)][0].kind == Kind::none);
        require(transfer.bindings[size_t(Action::note1)][0].kind == Kind::none);
        require(transfer.validate());
        Controls contextual;
        require(contextual.transfer(Action::note1,0,key(SDL_SCANCODE_S)));
        require(contextual.bindings[size_t(Action::note2)][0].kind == Kind::none);
        require(contextual.bindings[size_t(Action::x)][0] == key(SDL_SCANCODE_S));
    }
    // Reuse across layers is allowed; same-context/shared collisions are rejected transactionally.
    require(!settings.assign(Action::a,0,key(SDL_SCANCODE_Z)));
    require(settings.bindings[size_t(Action::a)][0] == key(SDL_SCANCODE_X));
    require(!settings.assign(Action::note1,0,key(SDL_SCANCODE_SPACE)));
    require(settings.assign(Action::a,0,key(SDL_SCANCODE_G)));
    require(settings.assign(Action::note1,0,key(SDL_SCANCODE_G)));
    require(!settings.assign(Action::note2,0,key(SDL_SCANCODE_G)));
    require(!settings.assign(Action::note2,0,key(SDL_SCANCODE_F1)));
    require(!settings.assign(Action::note2,0,key(SDL_SCANCODE_F11)));
    require(!settings.assign(Action::note2,2,button(SDL_GAMEPAD_BUTTON_LEFT_STICK)));
    require(!settings.assign(Action::a,0,{Kind::key,99999}));
    require(!settings.assign(Action::a,2,key(SDL_SCANCODE_G)));
    require(settings.validate());

    State input;
    input.event(pad_event(SDL_GAMEPAD_BUTTON_START,true));
    require(input.down(Action::start) && !input.down(Action::menu));
    input.clear();
    input.event(pad_event(SDL_GAMEPAD_BUTTON_BACK,true));
    require(input.down(Action::select) && !input.down(Action::menu));
    input.clear();
    input.event(pad_event(SDL_GAMEPAD_BUTTON_LEFT_STICK,true));
    require(input.down(Action::menu) && !input.down(Action::start));
    input.clear();
    input.context(Context::game_menu);
    input.event(pad_event(SDL_GAMEPAD_BUTTON_SOUTH,true));
    require(input.down(Action::menu_accept) && !input.down(Action::b) && input.ds_buttons()==0);
    input.event(pad_event(SDL_GAMEPAD_BUTTON_START,true)); require(input.down(Action::start));
    input.context(Context::name_keyboard);
    input.event(pad_event(SDL_GAMEPAD_BUTTON_WEST,true));
    require(input.down(Action::name_erase) && !input.down(Action::y));
    input.context(Context::gameplay);
    input.event(key_event(SDL_SCANCODE_X,true)); require(input.ds_buttons() == bit(Action::a));
    input.event(pad_event(SDL_GAMEPAD_BUTTON_EAST,true));
    input.event(key_event(SDL_SCANCODE_X,false)); require(input.down(Action::a));
    input.event(pad_event(SDL_GAMEPAD_BUTTON_EAST,false)); require(!input.down(Action::a));
    input.event(key_event(SDL_SCANCODE_SPACE,true)); input.event(key_event(SDL_SCANCODE_C,true));
    input.event(key_event(SDL_SCANCODE_C,false)); require(input.down(Action::blow));
    input.clear(); require(input.actions()==0);

    input.context(Context::instrument);
    input.event(key_event(SDL_SCANCODE_F,true)); require(input.active_note()==3 && input.ds_buttons()==0);
    input.event(key_event(SDL_SCANCODE_J,true)); require(input.active_note()==4);
    input.event(key_event(SDL_SCANCODE_J,false)); require(input.active_note()==3);
    input.event(key_event(SDL_SCANCODE_F,false)); require(!input.active_note());
    input.event(pad_event(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,true)); require(input.active_note()==0);
    input.context(Context::gameplay); require(input.actions()==0 && !input.active_note());

    SDL_Event axis{}; axis.type=SDL_EVENT_GAMEPAD_AXIS_MOTION; axis.gaxis.axis=SDL_GAMEPAD_AXIS_LEFTX;
    axis.gaxis.value=10000; input.event(axis); require(input.ds_buttons()==0);
    axis.gaxis.value=20000; input.event(axis); require(input.down(Action::right));
    input.settings.deadzone=80; require(!input.down(Action::right));
    input.clear(); axis.gaxis.axis=SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
    axis.gaxis.value=2000; input.event(axis); require(!input.down(Action::blow));
    axis.gaxis.value=10000; input.event(axis); require(input.down(Action::blow));

    const auto south=button(SDL_GAMEPAD_BUTTON_SOUTH), west=button(SDL_GAMEPAD_BUTTON_WEST);
    require(label(south,Symbols::xbox)=="A" && label(south,Symbols::playstation)=="Cross" && label(south,Symbols::nintendo)=="B");
    require(label(west,Symbols::xbox)=="X" && label(west,Symbols::playstation)=="Square" && label(west,Symbols::nintendo)=="Y");
    require(label(south,Symbols::automatic,SDL_GAMEPAD_TYPE_PS5)=="Cross");
    require(label(south,Symbols::automatic,SDL_GAMEPAD_TYPE_UNKNOWN)=="South");
    const auto before=settings.bindings; settings.symbols=Symbols::nintendo; require(settings.bindings==before);

    require(st::file_menu(2,0,false)==st::GameMenu::slots);
    require(st::file_menu(21,6,false)==st::GameMenu::confirm_name);
    require(st::file_menu(21,2,false)==st::GameMenu::none);
    st::MenuFocus focus;
    focus.context(st::GameMenu::slots);
    require(focus.item()->center_y()==67);
    require(focus.move(0,1,1));
    require(!focus.move(0,1,350)); // boundary feedback must stay silent
    require(focus.item()->center_y()==134);
    focus.context(st::GameMenu::name);
    const auto first = focus.item()->center_x();
    require(focus.move(1,0,2));
    require(focus.item()->center_x()==first+20);
    require(!focus.move(1,0,3)); // still inside the repeat delay
    require(focus.item()->center_x()==first+20);
    focus.context(st::GameMenu::none);
    require(!focus.item());

    // Exercise actual SDL lifecycle without a physical controller.
    require(SDL_InitSubSystem(SDL_INIT_GAMEPAD));
    SDL_VirtualJoystickDesc desc{}; SDL_INIT_INTERFACE(&desc); desc.type=SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes=SDL_GAMEPAD_AXIS_COUNT; desc.nbuttons=SDL_GAMEPAD_BUTTON_COUNT; desc.name="Synthetic controller";
    const auto id=SDL_AttachVirtualJoystick(&desc); require(id!=0);
    {
        Gamepads pads; require(pads.active()==id || pads.active()!=0);
        SDL_Event e{}; e.type=SDL_EVENT_GAMEPAD_BUTTON_DOWN; e.gbutton.which=id; pads.event(e); require(pads.active()==id);
        e.type=SDL_EVENT_GAMEPAD_REMOVED; e.gdevice.which=id; pads.event(e); require(pads.active()!=id);
    }
    require(SDL_DetachVirtualJoystick(id));
    SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
    return 0;
}
