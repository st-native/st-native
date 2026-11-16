// SPDX-License-Identifier: GPL-3.0-or-later
#include "st_probe.hpp"

#include <array>
#include <cstring>
#include <utility>
#include <vector>

#include "ntr/recomp.h"

namespace st::probe {

uint64_t current_frame = 0;

namespace {

struct Counter {
    std::string name;
    uint64_t calls = 0, first = 0, last = 0;
};
std::vector<Counter>& counters() {
    static std::vector<Counter> c;
    return c;
}

// Hooks get only the CPU, so each counting hook is its own function: one instantiation per slot.
constexpr int kSlots = 128;
std::array<int, kSlots> slot_counter{};  // counting-hook slot -> counter index
int slots_used = 0;

template <int I> void count_stub(NtrCpu*) { record(slot_counter[I]); }

template <int... I> constexpr std::array<void (*)(NtrCpu*), kSlots> make_stubs(std::integer_sequence<int, I...>) {
    return {&count_stub<I>...};
}
constexpr auto kStubs = make_stubs(std::make_integer_sequence<int, kSlots>{});

// Replacements: the same, calling the replacement after counting.
std::array<int, kSlots> repl_counter{};
std::array<GuestFn, kSlots> repl_fn{};
int repl_used = 0;

template <int I> void repl_stub(NtrCpu* c) {
    record(repl_counter[I]);
    repl_fn[I](c);
}

template <int... I> constexpr std::array<void (*)(NtrCpu*), kSlots> make_repl_stubs(std::integer_sequence<int, I...>) {
    return {&repl_stub<I>...};
}
constexpr auto kReplStubs = make_repl_stubs(std::make_integer_sequence<int, kSlots>{});

}  // namespace

int counter(const std::string& name) {
    auto& c = counters();
    for (size_t i = 0; i < c.size(); i++)
        if (c[i].name == name) return int(i);
    c.push_back({name});
    return int(c.size() - 1);
}

void record(int slot) {
    Counter& k = counters()[size_t(slot)];
    if (!k.calls++) k.first = current_frame;
    k.last = current_frame;
}

void* count_hook(const char* name) {
    if (std::strncmp(name, "st_count_", 9) != 0 || std::strlen(name) != 17) return nullptr;
    // The module may ask for the same hook more than once: one slot per name.
    const int idx = counter(name);
    for (int s = 0; s < slots_used; s++)
        if (slot_counter[size_t(s)] == idx) return reinterpret_cast<void*>(kStubs[size_t(s)]);
    if (slots_used == kSlots) return nullptr;
    slot_counter[size_t(slots_used)] = idx;
    return reinterpret_cast<void*>(kStubs[size_t(slots_used++)]);
}

void* replacement_hook(const char* name, GuestFn fn) {
    const int idx = counter(name);
    for (int s = 0; s < repl_used; s++)
        if (repl_counter[size_t(s)] == idx) return reinterpret_cast<void*>(kReplStubs[size_t(s)]);
    if (repl_used == kSlots) return nullptr;
    repl_counter[size_t(repl_used)] = idx;
    repl_fn[size_t(repl_used)] = fn;
    return reinterpret_cast<void*>(kReplStubs[size_t(repl_used++)]);
}

void report(FILE* out) {
    for (const Counter& k : counters())
        std::fprintf(out, "%s calls %llu first %llu last %llu\n", k.name.c_str(), (unsigned long long)k.calls,
                     (unsigned long long)k.first, (unsigned long long)k.last);
}

}  // namespace st::probe
