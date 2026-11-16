// SPDX-License-Identifier: GPL-3.0-or-later
// M1a: a generic headless host for the engine's runtime (ntr-runtime), without any game layer: loads a ROM and a
// recompiled game module, runs N emulated frames and writes, per frame, the runtime's state hash (main RAM, both work
// RAMs and the CPU clock) and how many instructions the fallback interpreter ran. The engine's own app refuses every
// ROM but its game's dumps; this host is the smallest way to run another program, e.g. our synthetic cartridge.
//   ntr_run <rom.nds> <module.dll | -> <frames> <hashes.txt>
//   ("-": no module, every ARM9 instruction runs on the interpreter)
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "ntr/recomp.h"
#include "ntr/runtime.hpp"

#ifdef _WIN32
#include <windows.h>
static void* open_module(const char* p) { return (void*)LoadLibraryA(p); }
static void* module_symbol(void* m, const char* s) { return (void*)GetProcAddress((HMODULE)m, s); }
#else
#include <dlfcn.h>
static void* open_module(const char* p) { return dlopen(p, RTLD_NOW | RTLD_LOCAL); }
static void* module_symbol(void* m, const char* s) { return dlsym(m, s); }
#endif

namespace {

void* no_hooks(const char*) { return nullptr; }  // this host has no game-specific hooks

class HashHost : public ntr::Host {
public:
    HashHost(uint64_t frames, FILE* out) : frames_(frames), out_(out) {}
    bool on_frame(const ntr::Frame& f) override {
        std::fprintf(out_, "%llu %016llx %llu\n", (unsigned long long)n_, (unsigned long long)f.state_hash,
                     (unsigned long long)f.interp_steps);
        interp_total_ += f.interp_steps;
        return ++n_ < frames_;
    }
    uint64_t frames() const { return n_; }
    uint64_t interp_total() const { return interp_total_; }

private:
    uint64_t frames_, n_ = 0, interp_total_ = 0;
    FILE* out_;
};

}  // namespace

int main(int argc, char** argv) {
    if (argc != 5) {
        std::fprintf(stderr, "usage: ntr_run <rom.nds> <module.dll | -> <frames> <hashes.txt>\n");
        return 2;
    }
    std::ifstream in(argv[1], std::ios::binary);
    std::vector<uint8_t> rom((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (rom.empty()) {
        std::fprintf(stderr, "cannot read %s\n", argv[1]);
        return 2;
    }
    ntr::GameCode code;
    if (std::string(argv[2]) != "-") {
        void* m = open_module(argv[2]);
        auto load = m ? reinterpret_cast<NtrGameLoadFn>(module_symbol(m, NTR_GAME_LOAD_SYMBOL)) : nullptr;
        if (!load) {
            std::fprintf(stderr, "%s is not a game module\n", argv[2]);
            return 2;
        }
        NtrHostApi api;
        ntr_fill_host_api(&api);
        api.find_hook = no_hooks;
        const NtrGameInfo* info = load(&api);
        if (!info || info->abi != NTR_ABI_VERSION) {
            std::fprintf(stderr, "%s was not built for this runtime (ABI %u)\n", argv[2], NTR_ABI_VERSION);
            return 2;
        }
        code = {info->functions, info->function_count, info->overlays, info->overlay_count};
    }
    FILE* out = std::fopen(argv[4], "w");
    if (!out) return 2;
    HashHost host(std::stoull(argv[3]), out);
    ntr::RuntimeOptions options;
    options.frame_hash = true;
    auto rt = ntr::Runtime::create(std::move(rom), code, host, options);
    int rc = rt->run();
    std::fclose(out);
    std::printf("ran %llu frames, %zu recompiled functions, %llu instructions on the interpreter, exit %d%s%s\n",
                (unsigned long long)host.frames(), code.function_count, (unsigned long long)host.interp_total(), rc,
                *ntr::last_fatal() ? ", fatal: " : "", ntr::last_fatal());
    return rc;
}
