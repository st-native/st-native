#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Generate a synthetic, game-like ARM program: the stand-in for game code in the proof of concept.

The output is our own code (GPL-3.0-or-later) and contains nothing from any game. Function bodies, constants, struct
layouts and the call graph are drawn from a seeded random generator so that whoever reconstructs the compiled binary
does not know its source: the reconstruction test stays blind. Patterns follow what DS game code is made of:
fixed-point (4.12) arithmetic, structs with mixed field sizes, per-object state machines, linked lists, lookup tables,
button masks, checksum loops, and a table of function pointers (indirect calls).

  python gen_sample.py --seed 1 --out <dir>      # writes main_arm.c, main_thumb.c, ovl_thumb.c, data.c, sample.h

Do not read the generated C if you are going to do the blind reconstruction.
"""
from __future__ import annotations

import argparse
import random
from pathlib import Path

SCALARS = [("s32", 4), ("u32", 4), ("s16", 2), ("u16", 2), ("u8", 1), ("s8", 1)]

HEADER_COMMON = """\
typedef signed int s32; typedef unsigned int u32; typedef signed short s16; typedef unsigned short u16;
typedef signed char s8; typedef unsigned char u8; typedef signed long long s64; typedef s32 fx32;
#define NOINLINE __attribute__((noinline))
typedef struct { fx32 x, y, z; } Vec3;
"""


class Gen:
    def __init__(self, seed: int):
        self.r = random.Random(seed)
        self.funcs: list[dict] = []  # {name, kind, sig, file}
        self.struct_fields: list[tuple[str, str]] = []  # (ctype, name), scalars only (plus special)

    # ---- struct layout ----------------------------------------------------------------------------------------
    def make_struct(self) -> str:
        r = self.r
        fields = [(t, f"f{i:02d}") for i, (t, _) in enumerate(r.choice(SCALARS) for _ in range(r.randint(6, 10)))]
        fields.append(("s32", "state"))
        fields.append(("Vec3", "pos"))
        fields.append(("struct Obj *", "next"))
        r.shuffle(fields)
        # natural alignment: sort is not applied on purpose (padding is part of what must be recovered)
        self.struct_fields = [(t, n) for t, n in fields if t in dict(SCALARS)]
        body = "\n".join(f"    {t} {n};" for t, n in fields)
        return f"typedef struct Obj {{\n{body}\n}} Obj;\n"

    # ---- expressions ------------------------------------------------------------------------------------------
    def const(self, small=False) -> str:
        r = self.r
        if small:
            return str(r.randint(1, 31))
        return r.choice([str(r.randint(0, 255)), hex(r.randint(0x100, 0xFFFF)), str(r.randint(-64, 64)),
                         hex(r.choice([0x1000, 0x800, 0x2000, 0x400]))])

    def expr(self, leaves: list[str], depth: int = 0) -> str:
        r = self.r
        if depth > 2 or r.random() < 0.3:
            return r.choice(leaves + [self.const()])
        op = r.choice(["+", "-", "*", "&", "|", "^", "<<", ">>", "fx", "?:"])
        a, b = self.expr(leaves, depth + 1), self.expr(leaves, depth + 1)
        if op in ("<<", ">>"):
            return f"(({a}) {op} {self.const(small=True)})"
        if op == "fx":
            return f"(s32)(((s64)({a}) * (s64)({b})) >> 12)"
        if op == "?:":
            c = r.choice(["<", ">", "==", "!=", "<=", ">="])
            return f"(({a}) {c} ({b}) ? ({self.expr(leaves, depth + 1)}) : ({self.expr(leaves, depth + 1)}))"
        return f"(({a}) {op} ({b}))"

    def callable(self, kind: str) -> list[dict]:
        return [f for f in self.funcs if f["kind"] == kind]

    def call_arith(self, leaves: list[str]) -> str:
        fs = self.callable("arith")
        if not fs or self.r.random() < 0.5:
            return self.expr(leaves)
        f = self.r.choice(fs)
        return f"{f['name']}({self.expr(leaves, 1)}, {self.expr(leaves, 1)})"

    # ---- function kinds ---------------------------------------------------------------------------------------
    def f_arith(self, name: str) -> str:
        r = self.r
        lines = [f"NOINLINE s32 {name}(s32 a, s32 b) {{", "    s32 t;"]
        lines.append(f"    t = {self.expr(['a', 'b'])};")
        for _ in range(r.randint(0, 2)):
            lines.append(f"    if (t {r.choice(['<', '>', '=='])} {self.const()}) t = {self.call_arith(['a', 'b', 't'])};")
        lines.append(f"    return {self.expr(['a', 'b', 't'])};")
        lines.append("}")
        return "\n".join(lines)

    def field(self) -> str:
        return self.r.choice(self.struct_fields)[1]

    def f_update(self, name: str) -> str:
        r = self.r
        lines = [f"NOINLINE void {name}(Obj *o) {{"]
        for _ in range(r.randint(2, 5)):
            leaves = [f"o->{self.field()}" for _ in range(2)] + ["o->pos.x", "o->state"]
            stmt = f"o->{self.field()} = {self.call_arith(leaves)};"
            if r.random() < 0.4:
                stmt = f"if (o->{self.field()} & {hex(1 << r.randint(0, 7))}) {{ {stmt} }}"
            lines.append("    " + stmt)
        if r.random() < 0.5:
            lines.append(f"    o->pos.{r.choice('xyz')} += {self.expr(['o->pos.x', 'o->pos.y', 'o->pos.z'])};")
        ups = self.callable("update")
        if ups and r.random() < 0.5:
            lines.append(f"    {r.choice(ups)['name']}(o);")
        lines.append("}")
        return "\n".join(lines)

    def f_state(self, name: str) -> str:
        r = self.r
        n = r.randint(3, 6)
        lines = [f"NOINLINE s32 {name}(Obj *o) {{", "    switch (o->state) {"]
        ups = self.callable("update")
        for c in range(n):
            lines.append(f"    case {c}:")
            lines.append(f"        o->{self.field()} = {self.expr(['o->' + self.field(), 'o->state'])};")
            if ups and r.random() < 0.4:
                lines.append(f"        {r.choice(ups)['name']}(o);")
            if r.random() < 0.6:
                lines.append(f"        if (o->{self.field()} {r.choice(['>', '<', '=='])} {self.const()}) "
                             f"o->state = {r.randint(0, n - 1)};")
            lines.append("        break;")
        lines.append("    default:")
        lines.append("        o->state = 0;")
        lines.append("        return -1;")
        lines.append("    }")
        lines.append(f"    return o->{self.field()};")
        lines.append("}")
        return "\n".join(lines)

    def f_walk(self, name: str) -> str:
        r = self.r
        fld = self.field()
        mode = r.choice(["count", "sum", "find"])
        lines = [f"NOINLINE s32 {name}(s32 k) {{", "    Obj *o;", "    s32 acc = 0;"]
        lines.append("    for (o = g_list; o != 0; o = o->next) {")
        cond = f"(o->{fld} & {hex(r.randint(1, 0xFF))}) {r.choice(['!=', '=='])} 0"
        if mode == "count":
            lines.append(f"        if ({cond}) acc++;")
        elif mode == "sum":
            lines.append(f"        if ({cond}) acc += o->{self.field()} + k;")
        else:
            lines.append(f"        if (o->{fld} == k) return o->{self.field()};")
            lines.append("        acc--;")
        lines.append("    }")
        lines.append("    return acc;")
        lines.append("}")
        return "\n".join(lines)

    def f_vec(self, name: str) -> str:
        r = self.r
        lines = [f"NOINLINE void {name}(Vec3 *out, const Vec3 *a, s32 s) {{"]
        for c in "xyz":
            lines.append(f"    out->{c} = {r.choice(['a->' + c, 'out->' + c])} "
                         f"{r.choice(['+', '-'])} (s32)(((s64)a->{r.choice('xyz')} * s) >> 12);")
        lines.append("}")
        return "\n".join(lines)

    def f_checksum(self, name: str) -> str:
        r = self.r
        lines = [f"NOINLINE u32 {name}(const u8 *buf, s32 n) {{", f"    u32 h = {hex(r.randint(0, 0xFFFFFFFF))}u;",
                 "    s32 i;", "    for (i = 0; i < n; i++) {"]
        for _ in range(r.randint(1, 3)):
            lines.append("        " + r.choice([f"h = (h << {r.randint(1, 7)}) | (h >> {32 - r.randint(1, 7)});",
                                               "h ^= buf[i];", f"h += buf[i] * {r.randint(3, 255)}u;",
                                               f"h = h * {r.choice([31, 33, 0x1003F, 65599])}u + buf[i];"]))
        lines.append("    }")
        lines.append("    return h;")
        lines.append("}")
        return "\n".join(lines)

    def f_table(self, name: str) -> str:
        r = self.r
        lo, hi = -r.randint(0x100, 0x2000), r.randint(0x100, 0x2000)
        return "\n".join([
            f"NOINLINE s32 {name}(s32 v) {{",
            f"    s32 t = g_table[(v >> {r.randint(0, 8)}) & 0xFF];",
            f"    t = {self.expr(['t', 'v'])};",
            f"    if (t < {lo}) t = {lo};",
            f"    if (t > {hi}) t = {hi};",
            "    return t;", "}"])

    def f_input(self, name: str) -> str:
        r = self.r
        lines = [f"NOINLINE void {name}(void) {{"]
        for _ in range(r.randint(2, 4)):
            m = hex(1 << r.randint(0, 11))
            src = r.choice(["g_keys", "g_keys_trg"])
            lines.append(f"    if ({src} & {m}) g_counters[{r.randint(0, 15)}] {r.choice(['+=', '-=', '^='])} "
                         f"{self.const()};")
        lines.append(f"    g_frame += {r.randint(1, 2)};")
        lines.append("}")
        return "\n".join(lines)

    KINDS = {"arith": f_arith, "update": f_update, "state": f_state, "walk": f_walk, "vec": f_vec,
             "checksum": f_checksum, "table": f_table, "input": f_input}
    SIGS = {"arith": "s32 {}(s32 a, s32 b)", "update": "void {}(Obj *o)", "state": "s32 {}(Obj *o)",
            "walk": "s32 {}(s32 k)", "vec": "void {}(Vec3 *out, const Vec3 *a, s32 s)",
            "checksum": "u32 {}(const u8 *buf, s32 n)", "table": "s32 {}(s32 v)", "input": "void {}(void)"}

    def run(self, n_arm: int, n_thumb: int, n_ovl: int) -> dict[str, str]:
        r = self.r
        struct = self.make_struct()
        files: dict[str, list[str]] = {"main_arm.c": [], "main_thumb.c": [], "ovl_thumb.c": []}
        plan = [("main_arm.c", n_arm), ("main_thumb.c", n_thumb), ("ovl_thumb.c", n_ovl)]
        weights = {"arith": 5, "update": 4, "state": 3, "walk": 2, "vec": 2, "checksum": 2, "table": 2, "input": 2}
        idx = 0
        for fname, count in plan:
            for _ in range(count):
                kind = r.choices(list(weights), list(weights.values()))[0]
                name = f"fn_{idx:03d}"
                idx += 1
                # overlay code may call main code, never the reverse (main cannot see the overlay)
                visible_before = list(self.funcs)
                if fname != "ovl_thumb.c":
                    self.funcs = [f for f in self.funcs if f["file"] != "ovl_thumb.c"]
                body = self.KINDS[kind](self, name)
                self.funcs = visible_before
                self.funcs.append({"name": name, "kind": kind, "file": fname})
                files[fname].append(body)

        protos = "\n".join("NOINLINE " + self.SIGS[f["kind"]].format(f["name"]) + ";" for f in self.funcs)
        updaters = [f["name"] for f in self.funcs if f["kind"] == "update" and f["file"] != "ovl_thumb.c"]
        header = (HEADER_COMMON + struct +
                  "extern Obj *g_list; extern s32 g_frame; extern u16 g_keys, g_keys_trg; extern s32 g_counters[16];\n"
                  "extern const s16 g_table[256];\nextern void (*const g_dispatch[])(Obj *);\n" + protos + "\n")
        table = ", ".join(str(r.randint(-0x1000, 0x1000)) for _ in range(256))
        disp = updaters[: max(2, len(updaters) // 2)]
        data = ('#include "sample.h"\n'
                "Obj *g_list; s32 g_frame; u16 g_keys, g_keys_trg; s32 g_counters[16];\n"
                f"const s16 g_table[256] = {{{table}}};\n"
                f"void (*const g_dispatch[])(Obj *) = {{{', '.join(disp)}}};\n")
        # The entry point: a small "game loop" calling some functions directly and the rest through g_dispatch, as
        # real code reaches many functions only through tables (the discovery has to find those too).
        direct = [f["name"] for f in self.funcs if f["file"] != "ovl_thumb.c" and f["kind"] in ("input", "walk")]
        start = ['#include "sample.h"', "void start_loop(void) {", "    Obj *o; u32 i;", "    for (;;) {"]
        for n in direct[:6]:
            start.append(f"        {n}{'()' if n in [f['name'] for f in self.funcs if f['kind'] == 'input'] else '(0)'};")
        start += [f"        for (o = g_list, i = 0; o != 0; o = o->next, i++) g_dispatch[i % {len(disp)}](o);",
                  "    }", "}"]
        out = {"sample.h": header, "data.c": data, "start.c": "\n".join(start) + "\n"}
        for k, v in files.items():
            out[k] = '#include "sample.h"\n\n' + "\n\n".join(v) + "\n"
        return out


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--arm", type=int, default=60)
    ap.add_argument("--thumb", type=int, default=60)
    ap.add_argument("--ovl", type=int, default=30)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    for name, text in Gen(a.seed).run(a.arm, a.thumb, a.ovl).items():
        (out / name).write_text(text)
    print(f"generated {a.arm + a.thumb + a.ovl} functions (seed {a.seed}) in {out}")


if __name__ == "__main__":
    main()
