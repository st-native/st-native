// SPDX-License-Identifier: GPL-3.0-or-later
// Spirit Tracks glue for the recompiled game module. The hooks themselves are the engine's generic NitroSDK
// replacements (ntr/nitro.h); this file supplies the addresses they need for this game, and the lookup by name.
// The structure follows the engine's game/src/cfie_hooks.cpp (FFCCEOT-Recomp, JonathanDC64, commit 934c743).
#include "st_hooks.hpp"

#include <cstdint>
#include <cstring>

#include "ntr/nitro.h"

namespace {

// Addresses, per release. How each was found: game/st/config/<code>/hooks.txt.
struct Addresses {
    uint32_t os_irq_table;                  // OS_IRQTable (DTCM)
    uint32_t os_irq_handler_thread_switch;  // OS_IrqHandler_ThreadSwitch (ITCM)
};
constexpr Addresses kUsaRev0 = {
    0x027E0198,  // os_irq_table
    0x01FFBDC8,  // os_irq_handler_thread_switch
};
const Addresses* A = &kUsaRev0;

}  // namespace

// OS_IrqHandler: the runtime's port, given this game's IRQ table and thread-switch routine.
extern "C" void st_os_irq_handler(NtrCpu* c) {
    ntr::nitro::irq_handler(c, A->os_irq_table, A->os_irq_handler_thread_switch);
}

bool st::select_release(const std::string& game_code) {
    if (game_code == "BKIE") {
        A = &kUsaRev0;
        return true;
    }
    return false;
}

void* st::find_hook(const char* name) {
    static const struct {
        const char* name;
        void* fn;
    } kHooks[] = {
        {"st_os_irq_handler", reinterpret_cast<void*>(&st_os_irq_handler)},
        {"ntr_nitro_save_context", reinterpret_cast<void*>(&ntr_nitro_save_context)},
        {"ntr_nitro_load_context", reinterpret_cast<void*>(&ntr_nitro_load_context)},
        {"ntr_nitro_init_context", reinterpret_cast<void*>(&ntr_nitro_init_context)},
        {"ntr_dsprot_not_detected", reinterpret_cast<void*>(&ntr_dsprot_not_detected)},
    };
    for (const auto& h : kHooks)
        if (std::strcmp(h.name, name) == 0) return h.fn;
    return nullptr;
}
