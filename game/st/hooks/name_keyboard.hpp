// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>

#include "ntr/runtime.hpp"

namespace st {

class NameKeyboard {
public:
    static constexpr uint32_t backspace = 8, select_all = 0x110000;
    bool active() const { return active_.load(); }
    void queue(uint32_t key);
    void finish_frame();
    void discard_pending();
    void update(NtrCpu* cpu);

private:
    std::atomic<bool> active_{false};
    std::mutex mutex_;
    std::deque<uint32_t> pending_;
    bool seen_ = false;
    bool replace_next_ = false;
    bool edited_ = false;
};

class NameHost : public ntr::Host {
public:
    NameKeyboard name_keyboard;
};

}  // namespace st
