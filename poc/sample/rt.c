/* SPDX-License-Identifier: GPL-3.0-or-later
 * Compiler runtime helpers for the sample (our own code). Thumb-1 code calls these for 64-bit arithmetic, as DS
 * games call their compiler's runtime library. Built in ARM mode, where the operations are single instructions. */
typedef long long s64;
typedef unsigned long long u64;
s64 __aeabi_lmul(s64 a, s64 b) { return a * b; }
s64 __aeabi_llsl(s64 a, int s) { return a << s; }
s64 __aeabi_lasr(s64 a, int s) { return a >> s; }
u64 __aeabi_llsr(u64 a, int s) { return a >> s; }
