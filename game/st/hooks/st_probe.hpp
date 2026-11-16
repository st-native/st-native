// SPDX-License-Identifier: GPL-3.0-or-later
// Instrumentation for hooks and replays: counters for calls into recompiled or replaced functions, by hook name.
//   - `before st_count_<addr>` in hooks.txt counts the calls of the recompiled function at <addr> (its body still runs);
//   - a replacement registered with probe::count_replacement() counts its own calls.
// Counters record the number of calls and the first and last frame with a call. All guest code runs on the runtime's
// game thread, so the counters need no locking; report() is called after the runtime has stopped.
#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

struct NtrCpu;  // ntr/recomp.h

namespace st::probe {

// The frame the program is on (set by the host from Host::on_frame).
extern uint64_t current_frame;

// A hook named st_count_<8 hex digits>: a counting function for that address, or null for any other name.
void* count_hook(const char* name);

// A counter by name (created on first use); returns its slot for record().
int counter(const std::string& name);
void record(int slot);

// A replacement of a recompiled function (a hook), wrapped so that its calls are counted
// under its hook name. Returns the function to give the game module, or null when every slot is taken.
using GuestFn = void (*)(NtrCpu*);
void* replacement_hook(const char* name, GuestFn fn);

// One line per counter: "<name> calls <n> first <frame> last <frame>", in order of registration.
void report(FILE* out);

}  // namespace st::probe
