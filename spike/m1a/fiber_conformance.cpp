// SPDX-License-Identifier: GPL-3.0-or-later
// M1a V2: conformance test for the engine's host fibers (runtime/src/fiber.hpp, ntr-runtime at the pinned commit).
// Guest NitroSDK threads run on these fibers, so every target must create, switch, nest and destroy them identically
// and preserve floating-point state. Prints a deterministic transcript and its FNV-1a hash: the hash must be the same
// on every platform. Exit code 0 only if every check passes.
//   build (Windows): clang-cl /O2 /EHsc /I<engine>/runtime/src fiber_conformance.cpp <engine>/runtime/src/fiber.cpp
#include "fiber.hpp"

#include <cfenv>
#include <cstdarg>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace ntr;

static std::string g_log;
static int g_failures = 0;

static void log(const char* fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    g_log += buf;
    g_log += '\n';
    std::printf("%s\n", buf);
}

static void check(bool ok, const char* what) {
    if (!ok) {
        ++g_failures;
        log("FAIL %s", what);
    }
}

static Fiber* g_main = nullptr;

// ---- T1: ping-pong between the main fiber and one fiber --------------------------------------------------------
static Fiber* t1_fiber;
static int t1_counter = 0;
static void t1_entry(void*) {
    for (;;) {
        t1_counter += 3;
        fiber_switch(t1_fiber, g_main);
    }
}

// ---- T2: round robin over 16 fibers, each passing control to the next ------------------------------------------
static const int T2_N = 16, T2_ROUNDS = 100;
static Fiber* t2_fibers[T2_N];
static uint32_t t2_hash = 2166136261u;
static void t2_entry(void* arg) {
    int id = int(intptr_t(arg));
    for (int round = 0;; ++round) {
        t2_hash = (t2_hash ^ uint32_t(id * 1000 + round)) * 16777619u;
        Fiber* next = (id == T2_N - 1) ? g_main : t2_fibers[id + 1];
        fiber_switch(t2_fibers[id], next);
    }
}

// ---- T3: deep nested calls on a fiber stack, switching out at the deepest point ---------------------------------
static Fiber* t3_fiber;
static uint64_t t3_result = 0;
static uint64_t t3_recurse(int depth, uint64_t acc) {
    volatile uint8_t frame[64];  // real stack usage per level
    for (int i = 0; i < 64; ++i) frame[i] = uint8_t(acc + i);
    if (depth == 0) {
        fiber_switch(t3_fiber, g_main);  // suspend with ~3000 frames live
        return acc;
    }
    uint64_t r = t3_recurse(depth - 1, acc * 31 + uint64_t(depth));
    return r + frame[depth & 63];
}
static void t3_entry(void*) {
    t3_result = t3_recurse(3000, 7);
    fiber_switch(t3_fiber, g_main);
    for (;;) fiber_switch(t3_fiber, g_main);
}

// ---- T4: floating-point values live across switches (callee-saved FP/SIMD registers) ----------------------------
static Fiber* t4_a;
static Fiber* t4_b;
static double t4_out_a = 0, t4_out_b = 0;
static double t4_work(double seed, Fiber* self, Fiber* other) {  // self == nullptr: no switching (reference)
    double v0 = seed, v1 = seed * 1.5, v2 = seed + 2.25, v3 = std::sqrt(seed + 1.0), v4 = seed / 3.0, v5 = -seed,
           v6 = seed * seed, v7 = seed + 0.125;
    for (int i = 0; i < 50; ++i) {
        if (self) fiber_switch(self, other);  // the other fiber clobbers FP registers with its own values
        v0 = v0 * 1.0001 + v1;
        v1 = v1 - v2 * 0.5;
        v2 = v2 + v3;
        v3 = v3 * 0.999 + v4;
        v4 = v4 + v5 * 0.01;
        v5 = v5 - v6 * 0.001;
        v6 = v6 * 0.98 + v7;
        v7 = v7 + v0 * 1e-6;
    }
    return v0 + v1 + v2 + v3 + v4 + v5 + v6 + v7;
}
static void t4_entry_a(void*) {
    t4_out_a = t4_work(1.25, t4_a, t4_b);
    fiber_switch(t4_a, g_main);
    for (;;) fiber_switch(t4_a, g_main);
}
static void t4_entry_b(void*) {
    t4_out_b = t4_work(7.5, t4_b, t4_a);
    for (;;) fiber_switch(t4_b, t4_a);
}

// ---- T5: the floating-point environment (rounding mode) belongs to each fiber -----------------------------------
static Fiber* t5_fiber;
static int t5_seen_mode = -1;
#if defined(__aarch64__)
static int t5_resumed_mode = -1;
static int t5_resumed_flags = 0;
#endif
static void t5_entry(void*) {
    t5_seen_mode = std::fegetround();
    std::fesetround(FE_UPWARD);  // changed only inside this fiber
#if defined(__aarch64__)
    std::feraiseexcept(FE_INVALID);
#endif
    fiber_switch(t5_fiber, g_main);
#if defined(__aarch64__)
    t5_resumed_mode = std::fegetround();
    t5_resumed_flags = std::fetestexcept(FE_ALL_EXCEPT);
#endif
    for (;;) fiber_switch(t5_fiber, g_main);
}

// ---- T6: a chain of switches between non-main fibers -------------------------------------------------------------
static Fiber* t6_a;
static Fiber* t6_b;
static Fiber* t6_c;
static std::string t6_trace;
static void t6_entry_a(void*) { for (;;) { t6_trace += 'A'; fiber_switch(t6_a, t6_b); } }
static void t6_entry_b(void*) { for (;;) { t6_trace += 'B'; fiber_switch(t6_b, t6_c); } }
static void t6_entry_c(void*) { for (;;) { t6_trace += 'C'; fiber_switch(t6_c, g_main); } }

// ---- T7: create and destroy many fibers (each run once) ----------------------------------------------------------
static Fiber* t7_current;
static uint32_t t7_sum = 0;
static void t7_entry(void* arg) {
    t7_sum += uint32_t(intptr_t(arg));
    for (;;) fiber_switch(t7_current, g_main);
}

static std::string round_mode_name(int m) {
    return m == FE_TONEAREST ? "nearest" : m == FE_UPWARD ? "upward" : m == FE_DOWNWARD ? "downward" :
           m == FE_TOWARDZERO ? "towardzero" : "unknown";
}

int main() {
    g_main = fiber_init_thread();
    check(g_main != nullptr, "fiber_init_thread");

    t1_fiber = fiber_create(64 * 1024, t1_entry, nullptr);
    for (int i = 0; i < 1000; ++i) fiber_switch(g_main, t1_fiber);
    check(t1_counter == 3000, "T1 counter");
    log("T1 ping-pong 1000 switches: counter=%d", t1_counter);
    fiber_destroy(t1_fiber);

    for (int i = 0; i < T2_N; ++i) t2_fibers[i] = fiber_create(64 * 1024, t2_entry, (void*)intptr_t(i));
    for (int r = 0; r < T2_ROUNDS; ++r) fiber_switch(g_main, t2_fibers[0]);
    log("T2 round robin %d fibers x %d rounds: hash=%08x", T2_N, T2_ROUNDS, t2_hash);
    for (Fiber* f : t2_fibers) fiber_destroy(f);

    t3_fiber = fiber_create(2 * 1024 * 1024, t3_entry, nullptr);
    fiber_switch(g_main, t3_fiber);  // returns while 3000 frames are suspended
    check(t3_result == 0, "T3 suspended before returning");
    fiber_switch(g_main, t3_fiber);  // resume: unwinds and computes the result
    check(t3_result != 0, "T3 result");
    log("T3 nested calls depth 3000 across a switch: result=%016llx", (unsigned long long)t3_result);
    fiber_destroy(t3_fiber);

    t4_a = fiber_create(256 * 1024, t4_entry_a, nullptr);
    t4_b = fiber_create(256 * 1024, t4_entry_b, nullptr);
    fiber_switch(g_main, t4_a);  // A finishes first (B is suspended at its last switch)
    fiber_switch(g_main, t4_b);  // let B finish; it passes control to A, which returns to main
    double ref_a = t4_work(1.25, nullptr, nullptr), ref_b = t4_work(7.5, nullptr, nullptr);
    bool same = std::memcmp(&t4_out_a, &ref_a, 8) == 0 && std::memcmp(&t4_out_b, &ref_b, 8) == 0;  // bit for bit
    check(same, "T4 FP values differ from the computation without switches");
    log("T4 FP values across 100 switches: a=%.17g b=%.17g (bit-identical to no-switch reference: %s)", t4_out_a,
        t4_out_b, same ? "yes" : "no");
    fiber_destroy(t4_a);
    fiber_destroy(t4_b);

    std::fesetround(FE_TONEAREST);
    std::feclearexcept(FE_ALL_EXCEPT);
    t5_fiber = fiber_create(64 * 1024, t5_entry, nullptr);
    fiber_switch(g_main, t5_fiber);
    int main_mode = std::fegetround();
    log("T5 rounding mode: new fiber starts with %s; main after the fiber set upward: %s",
        round_mode_name(t5_seen_mode).c_str(), round_mode_name(main_mode).c_str());
    check(main_mode == FE_TONEAREST, "T5 rounding mode leaked from a fiber into main");
    check(t5_seen_mode == FE_TONEAREST, "T5 new fiber did not inherit the rounding mode");
#if defined(__aarch64__)
    // Exercise both FPCR and FPSR, in both switch directions. Successful transcripts stay portable.
    check((std::fetestexcept(FE_INVALID) & FE_INVALID) == 0, "T5 exception flag leaked into main");
    std::fesetround(FE_DOWNWARD);
    std::feraiseexcept(FE_DIVBYZERO);
    fiber_switch(g_main, t5_fiber);
    check(t5_resumed_mode == FE_UPWARD, "T5 resumed fiber lost its rounding mode");
    check((t5_resumed_flags & (FE_INVALID | FE_DIVBYZERO)) == FE_INVALID,
          "T5 resumed fiber lost or inherited exception flags");
    check(std::fegetround() == FE_DOWNWARD, "T5 main lost its rounding mode on the second switch");
    check((std::fetestexcept(FE_INVALID | FE_DIVBYZERO)) == FE_DIVBYZERO,
          "T5 main lost or inherited exception flags on the second switch");
#endif
    std::fesetround(FE_TONEAREST);
    std::feclearexcept(FE_ALL_EXCEPT);
    fiber_destroy(t5_fiber);

    t6_a = fiber_create(64 * 1024, t6_entry_a, nullptr);
    t6_b = fiber_create(64 * 1024, t6_entry_b, nullptr);
    t6_c = fiber_create(64 * 1024, t6_entry_c, nullptr);
    for (int i = 0; i < 5; ++i) fiber_switch(g_main, t6_a);
    check(t6_trace == "ABCABCABCABCABC", "T6 chain order");
    log("T6 chain main->A->B->C->main x5: %s", t6_trace.c_str());
    fiber_destroy(t6_a);
    fiber_destroy(t6_b);
    fiber_destroy(t6_c);

    for (int i = 1; i <= 5000; ++i) {
        t7_current = fiber_create(32 * 1024, t7_entry, (void*)intptr_t(i));
        fiber_switch(g_main, t7_current);
        fiber_destroy(t7_current);
    }
    check(t7_sum == 5000u * 5001u / 2u, "T7 sum");
    log("T7 create/run/destroy 5000 fibers: sum=%u", t7_sum);

    uint32_t h = 2166136261u;
    for (unsigned char c : g_log) h = (h ^ c) * 16777619u;
    std::printf("transcript hash %08x, failures %d\n", h, g_failures);
    return g_failures ? 1 : 0;
}
