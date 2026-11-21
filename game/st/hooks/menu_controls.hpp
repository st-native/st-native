// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace st {

enum class GameMenu : uint8_t { none, notice, slots, file_actions, mode, name, confirm_name, handedness, confirm_hand, prologue, title, opening };

struct MenuItem {
    int x, y, width, height;
    int center_x() const { return x + width / 2; }
    int center_y() const { return y + height / 2; }
};

// BKIE rev. 0. Repeated RAM snapshots on the blank and existing-save replays
// identify the file-menu phase word at 0x022e560c. The setup substate is at
// 0x022610a5 and distinguishes the three choices from the save-complete notice.
// Only consult these fields inside ScreenContext's verified file-menu display state.
inline GameMenu file_menu(uint32_t phase, uint32_t dialog, bool editing) {
    switch (phase) {
    case 0: return GameMenu::notice;
    case 2: return GameMenu::slots;
    case 7: return GameMenu::file_actions;
    case 12: return GameMenu::mode;
    case 21:
        if (editing) return GameMenu::name;
        if (dialog == 4) return GameMenu::handedness;
        if (dialog == 5) return GameMenu::confirm_hand;
        if (dialog == 6) return GameMenu::confirm_name;
        if (dialog == 11) return GameMenu::notice;
        return GameMenu::none;
    default: return GameMenu::none;
    }
}

inline constexpr std::array notice_items{MenuItem{16,127,223,60}};
inline constexpr std::array slot_items{MenuItem{17,38,221,59}, MenuItem{17,105,221,58}};
inline constexpr std::array file_items{
    MenuItem{41,99,179,41}, MenuItem{0,162,30,29}, MenuItem{35,155,96,27}, MenuItem{136,155,97,27}};
inline constexpr std::array mode_items{
    MenuItem{49,70,174,45}, MenuItem{49,119,174,45},
    MenuItem{0,162,30,29}, MenuItem{35,166,140,25}, MenuItem{178,166,77,25}};
inline constexpr std::array choice_items{MenuItem{157,98,94,23}, MenuItem{157,126,94,23}};

inline constexpr auto keyboard_items = [] {
    std::array<MenuItem, 52> keys{};
    size_t n = 0;
    for (int col = 0; col < 12; ++col) keys[n++] = {6 + col * 20, 54, 18, 19};
    for (int col = 0; col < 11; ++col) keys[n++] = {16 + col * 20, 74, 18, 19};
    keys[n++] = {216,74,30,19}; // delete
    keys[n++] = {6,94,18,19}; // Caps
    for (int col = 0; col < 9; ++col) keys[n++] = {27 + col * 20, 94, 18, 19};
    keys[n++] = {6,114,30,19}; // Shift
    for (int col = 0; col < 10; ++col) keys[n++] = {37 + col * 20,114,18,19};
    for (const auto item : { MenuItem{37,134,19,19}, MenuItem{58,134,18,19},
                            MenuItem{78,134,83,19}, MenuItem{167,134,19,19}, MenuItem{188,134,19,19},
                            MenuItem{6,164,109,25}, MenuItem{135,164,113,25}}) keys[n++] = item;
    return keys;
}();

inline std::span<const MenuItem> menu_items(GameMenu menu) {
    switch (menu) {
    case GameMenu::notice: return notice_items;
    case GameMenu::slots: return slot_items;
    case GameMenu::file_actions: return file_items;
    case GameMenu::mode: return mode_items;
    case GameMenu::name: return keyboard_items;
    case GameMenu::confirm_name:
    case GameMenu::handedness:
    case GameMenu::confirm_hand: return choice_items;
    default: return {};
    }
}

class MenuFocus {
public:
    bool context(GameMenu value) {
        if (value == menu_) return false;
        menu_ = value;
        index_ = value == GameMenu::name ? 12 : 0;
        reveal_ = true;
        direction_ = 0;
        return true;
    }
    bool active() const { return !menu_items(menu_).empty(); }
    const MenuItem* item() const {
        const auto items = menu_items(menu_);
        return index_ < items.size() ? &items[index_] : nullptr;
    }
    size_t index() const { return index_; }
    const MenuItem* visible_item() const { return reveal_ ? item() : nullptr; }
    void hide() { reveal_ = false; direction_ = 0; }
    void select() { reveal_ = true; }

    // One discrete focus move per step, with delayed repeat for a held direction.
    bool move(int dx, int dy, uint64_t now_ms) {
        if (!active() || (!dx && !dy)) { direction_ = 0; return false; }
        if (dy) dx = 0;
        const int direction = dx ? dx : 2 * dy;
        if (direction == direction_ && now_ms < next_move_) return false;
        next_move_ = now_ms + (direction == direction_ ? 105 : 300);
        direction_ = direction;
        reveal_ = true;
        const auto items = menu_items(menu_);
        const auto& from = items[index_];
        int best = 100000;
        size_t next = index_;
        for (size_t i = 0; i < items.size(); ++i) {
            const auto& to = items[i];
            const int x = to.center_x() - from.center_x(), y = to.center_y() - from.center_y();
            if (dx && to.y != from.y && (to.y + to.height <= from.y || from.y + from.height <= to.y)) continue;
            const int along = dx * x + dy * y;
            if (along <= 0 || (dy && to.y == from.y)) continue;
            const int cross = dx ? y : x;
            const int distance = along * 8 + (cross < 0 ? -cross : cross);
            if (distance < best) { best = distance; next = i; }
        }
        if (index_ == next) return false;
        index_ = next;
        return true;
    }

private:
    GameMenu menu_ = GameMenu::none;
    size_t index_ = 0;
    bool reveal_ = false;
    int direction_ = 0;
    uint64_t next_move_ = 0;
};

} // namespace st
