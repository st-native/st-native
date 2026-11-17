// SPDX-License-Identifier: GPL-3.0-or-later
#include "name_keyboard.hpp"

#include <algorithm>
#include <array>
#include <iterator>

namespace {

// BKIE ov003. The editor update is called from 020b88f0 only while the keyboard is editable.
constexpr uint32_t kKeyboard = 0x020bb1ac, kEditor = 0x020bb024, kEditorCaller = 0x020b88f0;
constexpr uint32_t kName = kEditor + 0xe0, kLength = kEditor + 0x110;
constexpr uint32_t kInsert = 0x020b7d64, kInsertReturn = 0x020b8a84;
constexpr uint32_t kErase = 0x020b7e2c, kEraseReturn = 0x020b8aa4;

void edit(NtrCpu* cpu, uint32_t function, uint32_t return_address, uint32_t character = 0) {
    const NtrCpu saved = *cpu;
    cpu->r[0] = kEditor;
    cpu->r[1] = character;
    cpu->r[2] = 0;  // ordinary insertion, as passed by the stylus key callback
    cpu->r[14] = return_address;
    ntr_call(cpu, function);
    std::copy(std::begin(saved.r), std::end(saved.r), std::begin(cpu->r));
    cpu->n = saved.n;
    cpu->z = saved.z;
    cpu->c = saved.c;
    cpu->v = saved.v;
    cpu->q = saved.q;
    // The guest routines ran normally, including their cycle charges and any scheduled work.
}

bool default_name(NtrCpu* cpu) {
    constexpr char16_t link[] = u"Link";
    for (size_t i = 0; i < std::size(link); ++i)
        if (ntr_rd16(cpu, kName + uint32_t(2 * i)) != link[i]) return false;
    return true;
}

bool printable(uint32_t ch) {
    return (ch >= 0x20 && ch <= 0x7e) || (ch >= 0xc0 && ch <= 0xff && ch != 0xd7 && ch != 0xf7);
}

}  // namespace

void st::NameKeyboard::queue(uint32_t key) {
    if (key != backspace && key != select_all && !printable(key)) return;
    std::lock_guard lock(mutex_);
    if (active_ && pending_.size() < 32) pending_.push_back(key);
}

void st::NameKeyboard::discard_pending() {
    std::lock_guard lock(mutex_);
    pending_.clear();
}

void st::NameKeyboard::finish_frame() {
    std::lock_guard lock(mutex_);
    active_ = seen_;
    if (!seen_) {
        pending_.clear();
        replace_next_ = false;
        edited_ = false;
    }
    seen_ = false;
}

void st::NameKeyboard::update(NtrCpu* cpu) {
    if (cpu->r[0] != kKeyboard || cpu->r[14] != kEditorCaller) return;
    seen_ = true;

    std::array<uint32_t, 32> keys{};
    size_t count;
    {
        std::lock_guard lock(mutex_);
        count = pending_.size();
        std::copy(pending_.begin(), pending_.end(), keys.begin());
        pending_.clear();
    }
    for (size_t index = 0; index < count; ++index) {
        const uint32_t key = keys[index];
        if (key == select_all) {
            replace_next_ = true;
            continue;
        }
        if (replace_next_ || (!edited_ && printable(key) && default_name(cpu))) {
            // Select the default name on first typing, while respecting existing stylus edits.
            const uint32_t length = std::min(ntr_rd32(cpu, kLength), 8u);
            for (uint32_t i = 0; i < length; ++i) edit(cpu, kErase, kEraseReturn);
        } else if (key == backspace) {
            edit(cpu, kErase, kEraseReturn);
        }
        edited_ = true;
        replace_next_ = false;
        if (printable(key)) edit(cpu, kInsert, kInsertReturn, key);
    }
}

extern "C" void st_name_keyboard(NtrCpu* cpu) {
    if (auto* host = dynamic_cast<st::NameHost*>(&ntr::hook_host(cpu))) host->name_keyboard.update(cpu);
}
