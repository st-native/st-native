# SPDX-License-Identifier: GPL-3.0-or-later
"""Function discovery for ARMv5TE (ARM + Thumb) modules without symbols.

Strategy (each source of functions is recorded in `found_by`):
  entry    the module's entry point
  call     BL/BLX targets (BLX switches mode)
  tail     unconditional B to a known function start or before the current function (tail call)
  pointer  words in non-code areas that point into code (odd = Thumb), validated by decoding
  gap      prologue patterns (ARM stmfd sp!,{..,lr} / Thumb push {..,lr}) in unclaimed space
  table    a leaf function (no prologue) whose pointer sits next to a pointer to a known function (vtables)
  next     the first unclaimed address after a known function (other mode only with a matching epilogue)
Recursive descent follows branches inside a function, switch tables, and literal-pool loads (pools are marked as data,
their values feed the pointer pass). Switch tables, each after cmp rI,#N:
  ARM address table    ldr<c> pc,[pc,rI,lsl #2] / add rB,pc,#imm; ldr pc,[rB,rI,lsl #2]        (clang)
  ARM branch table     add<c> pc,pc,rI,lsl #2; b default; b case0 ... b caseN                  (Metrowerks)
  Thumb offset table   add rB,pc; ldrh rX,[rB,#imm]; lsl/asr #16; add pc,rX; .short offsets    (Metrowerks)

  python -m tools.re.discover <extract_dir> [--out functions.json]
"""
from __future__ import annotations

import argparse
import bisect
import json
import re
import struct
import sys
from collections.abc import Callable
from dataclasses import asdict, dataclass, field
from pathlib import Path

import capstone
from capstone import arm as A

CS = {False: capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_ARM),
      True: capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)}
for _cs in CS.values():
    _cs.detail = True


@dataclass
class Module:
    name: str
    base: int
    data: bytes

    def contains(self, a: int) -> bool:
        return self.base <= a < self.base + len(self.data)

    def word(self, a: int) -> int:
        return struct.unpack_from("<I", self.data, a - self.base)[0]


class Marks:
    """Per-byte flags of one module: 0 free, 1 marked, 2 marked by the analysis in progress (tentative, so a rejected
    speculative function can be rolled back without copying the whole set)."""

    def __init__(self, base: int, size: int):
        self.base, self.flags, self.tentative = base, bytearray(size), None

    def __contains__(self, a: int) -> bool:
        o = a - self.base
        return 0 <= o < len(self.flags) and self.flags[o] != 0

    def committed(self, a: int) -> bool:
        o = a - self.base
        return 0 <= o < len(self.flags) and self.flags[o] == 1

    def add_range(self, a: int, b: int) -> None:
        lo, hi = max(a - self.base, 0), min(b - self.base, len(self.flags))
        if hi <= lo:
            return
        if self.tentative is None:
            self.flags[lo:hi] = b"\1" * (hi - lo)
            return
        for o in range(lo, hi):
            if not self.flags[o]:
                self.flags[o] = 2
                self.tentative.append(o)

    def begin(self) -> None:
        self.tentative = []

    def commit(self) -> None:
        for o in self.tentative:
            self.flags[o] = 1
        self.tentative = None

    def rollback(self) -> None:
        for o in self.tentative:
            self.flags[o] = 0
        self.tentative = None


@dataclass
class Function:
    module: str
    addr: int
    thumb: bool
    size: int = 0
    found_by: str = ""
    calls: list[int] = field(default_factory=list)       # callee addresses (bit 0 = Thumb)
    data_refs: list[int] = field(default_factory=list)   # values loaded from literal pools
    jump_tables: int = 0
    ok: bool = True
    insns: list[int] = field(default_factory=list)       # addresses of the instructions found


class Discovery:
    def __init__(self, modules: list[Module], progress: Callable[[int, str, int, int], None] | None = None):
        self.modules = {m.name: m for m in modules}
        # An overlay can refer to itself or static code. Preserve module order when static ranges overlap.
        self.static_modules = tuple(m for m in self.modules.values() if m.name in ("arm9", "itcm"))
        self.progress = progress
        self.round = 0
        self.funcs: dict[tuple[str, int], Function] = {}
        self.code = {m.name: Marks(m.base, len(m.data)) for m in modules}   # bytes decoded as instructions
        self.pools = {m.name: Marks(m.base, len(m.data)) for m in modules}  # bytes known to be data
        self.queue: list[tuple[str, int, bool, str]] = []
        self.rejected: set[tuple[str, int, bool]] = set()  # speculative starts that failed (claims only grow)
        spans = sorted((m.base, m.base + len(m.data)) for m in modules)
        self.span_lo = [a for a, _ in spans]
        self.span_hi = [b for _, b in spans]
        for k in range(1, len(self.span_hi)):  # running max, so one bisect answers "inside any module?"
            self.span_hi[k] = max(self.span_hi[k], self.span_hi[k - 1])

    def report(self, phase: str, done: int = 0, total: int = 0) -> None:
        if self.progress is not None:
            self.progress(self.round, phase, done, total)

    def in_any_module(self, a: int) -> bool:
        k = bisect.bisect_right(self.span_lo, a) - 1
        return k >= 0 and a < self.span_hi[k]

    def module_of(self, addr: int, prefer: str) -> Module | None:
        m = self.modules[prefer]
        if m.contains(addr):
            return m
        for m in self.static_modules:
            if m.contains(addr):
                return m
        return None

    def seed(self, module: str, addr: int, thumb: bool, how: str) -> None:
        if (module, addr) not in self.funcs and (how in self.STRONG or (module, addr, thumb) not in self.rejected):
            self.queue.append((module, addr, thumb, how))

    # ---- one function --------------------------------------------------------------------------------------------
    # Strong sources (entry, call, tail) are always accepted; speculative ones (pointer, gap, next) only when the
    # function decodes cleanly, ends every path, and does not run into code or data already claimed.
    STRONG = ("entry", "call", "tail")

    def analyze(self, mod: Module, start: int, thumb: bool, how: str) -> Function | None:
        if not mod.contains(start):
            return None
        f = Function(mod.name, start, thumb, found_by=how)
        cs = CS[thumb]
        todo, seen = [start], {}
        end = start
        pools, code = self.pools[mod.name], self.code[mod.name]
        pools.begin()
        new_seeds = len(self.queue)
        while todo:
            pc = todo.pop()
            while pc not in seen:
                if not mod.contains(pc):
                    f.ok = False
                    break
                off = pc - mod.base
                insn = next(cs.disasm(mod.data[off:off + 4], pc, 1), None)
                if insn is None or (how not in self.STRONG and (pc in code or pools.committed(pc))):
                    f.ok = False
                    break
                seen[pc] = insn.size
                end = max(end, pc + insn.size)
                stop = self.step(mod, f, insn, thumb, start, todo)
                pc += insn.size
                if stop:
                    break
        if not f.ok and how not in self.STRONG:
            pools.rollback()
            del self.queue[new_seeds:]
            self.rejected.add((mod.name, start, thumb))
            return None
        pools.commit()
        for a, n in seen.items():
            code.add_range(a, a + n)
        f.insns = sorted(seen)
        # the function's literal pools: data words right after its last instruction
        pool_end = end
        while True:
            if pool_end in pools:
                pool_end += 1
            elif thumb and pool_end % 4 == 2 and pool_end + 2 in pools:  # alignment padding before a pool
                pool_end += 2
            else:
                break
        f.size = pool_end - start
        return f

    def step(self, mod: Module, f: Function, insn, thumb: bool, start: int, todo: list[int]) -> bool:
        """Handles one instruction; returns True if the path ends after it."""
        i = insn.id
        ops = insn.operands
        cond = insn.cc not in (A.ARM_CC_AL, A.ARM_CC_INVALID)
        _, writes = insn.regs_access()
        writes_pc = A.ARM_REG_PC in writes

        if i in (A.ARM_INS_BL, A.ARM_INS_BLX) and ops and ops[0].type == A.ARM_OP_IMM:
            target = ops[0].imm
            to_thumb = thumb if i == A.ARM_INS_BL else not thumb
            f.calls.append(target | int(to_thumb))
            m = self.module_of(target, mod.name)
            if m:
                self.seed(m.name, target, to_thumb, "call")
            return False
        if i == A.ARM_INS_BLX:  # blx reg: indirect call
            return False
        if i == A.ARM_INS_B and ops and ops[0].type == A.ARM_OP_IMM:
            target = ops[0].imm
            if not cond and ((mod.name, target) in self.funcs or target < start or not mod.contains(target)):
                f.calls.append(target | int(thumb))
                m = self.module_of(target, mod.name)  # a tail call may leave the module (overlay -> static code)
                # a backward branch into straight-line code is a loop of a function we have split, not a tail call
                if m and ((m.name, target) in self.funcs or m is not mod or
                          self.looks_like_function(m, target, thumb, False)):
                    self.seed(m.name, target, thumb, "tail")
                return True
            todo.append(target)
            return not cond
        if i == A.ARM_INS_LDR and ops and ops[1].type == A.ARM_OP_MEM and ops[1].mem.base == A.ARM_REG_PC:
            m = ops[1].mem
            if m.index == 0:  # literal: [pc, #imm]
                pc_val = (insn.address + 4) & ~3 if thumb else insn.address + 8
                lit = pc_val + m.disp
                if mod.contains(lit) and lit + 4 <= mod.base + len(mod.data):
                    self.pools[mod.name].add_range(lit, lit + 4)
                    f.data_refs.append(mod.word(lit))
                if ops[0].reg == A.ARM_REG_PC:
                    return not cond
                return False
            if writes_pc and not thumb:  # ldr(cond) pc, [pc, rN, lsl #2]: inline jump table
                return self.jump_table(mod, f, insn, todo) and not cond
        if i == A.ARM_INS_LDR and writes_pc and not thumb and ops[1].type == A.ARM_OP_MEM and ops[1].mem.index:
            # add rB, pc, #imm ; ldr pc, [rB, rI, lsl #2]  (clang's form)
            return self.jump_table(mod, f, insn, todo) and not cond
        if i == A.ARM_INS_ADD and writes_pc and ops[-1].type == A.ARM_OP_REG:
            if not thumb and len(ops) == 3 and ops[1].type == A.ARM_OP_REG and ops[1].reg == A.ARM_REG_PC:
                return self.branch_table(mod, f, insn, todo)   # add<c> pc, pc, rI, lsl #2
            if thumb and len(ops) == 2:
                return self.thumb_offset_table(mod, f, insn, todo)  # add pc, rX
        if i == A.ARM_INS_BX:
            return not cond
        if writes_pc:  # pop {..pc}, ldm ..{pc}, mov pc, add pc...
            return not cond
        return False

    def jump_table(self, mod: Module, f: Function, insn, todo: list[int]) -> bool:
        # count from the nearest preceding cmp rI, #imm on the index register; table base is pc+8 for
        # ldr pc,[pc,rI,lsl #2], or the value of a preceding add rB, pc, #imm for ldr pc,[rB,rI,lsl #2]
        mem = insn.operands[1].mem
        idx_reg, base_reg = mem.index, mem.base
        count, table = None, (insn.address + 8 if base_reg == A.ARM_REG_PC else None)
        for back in range(1, 8):
            a = insn.address - 4 * back
            if not mod.contains(a):
                break
            prev = next(CS[False].disasm(mod.data[a - mod.base:a - mod.base + 4], a, 1), None)
            if prev is None:
                continue
            po = prev.operands
            if table is None and prev.id == A.ARM_INS_ADD and len(po) == 3 and po[0].reg == base_reg and \
                    po[1].type == A.ARM_OP_REG and po[1].reg == A.ARM_REG_PC and po[2].type == A.ARM_OP_IMM:
                table = prev.address + 8 + po[2].imm
            if count is None and prev.id == A.ARM_INS_CMP and po[0].reg == idx_reg and po[1].type == A.ARM_OP_IMM:
                count = po[1].imm + 1
        if count is None or table is None:
            return True
        f.jump_tables += 1
        for k in range(count):
            a = table + 4 * k
            if not mod.contains(a):
                break
            self.pools[mod.name].add_range(a, a + 4)
            todo.append(mod.word(a))
        if insn.cc not in (A.ARM_CC_AL, A.ARM_CC_INVALID):
            todo.append(insn.address + 4)  # the out-of-range branch after the ldrls
        return True

    def _cmp_count(self, mod: Module, addr: int, thumb: bool, reg: int | None, back: int = 8) -> int | None:
        """Number of cases from the nearest preceding cmp reg, #imm (any register when reg is None)."""
        step = 2 if thumb else 4
        for k in range(1, back + 1):
            a = addr - step * k
            if not mod.contains(a):
                break
            p = next(CS[thumb].disasm(mod.data[a - mod.base:a - mod.base + step], a, 1), None)
            if p is not None and p.id == A.ARM_INS_CMP and len(p.operands) == 2 and \
                    p.operands[1].type == A.ARM_OP_IMM and (reg is None or p.operands[0].reg == reg):
                return p.operands[1].imm + 1
        return None

    def branch_table(self, mod: Module, f: Function, insn, todo: list[int]) -> bool:
        """Metrowerks ARM switch: add<c> pc, pc, rI, lsl #2 jumps into a table of branch instructions at +8; the
        instruction at +4 is the out-of-range (default) branch."""
        count = self._cmp_count(mod, insn.address, False, insn.operands[2].reg)
        if count is None:
            return True
        f.jump_tables += 1
        todo.append(insn.address + 4)
        todo.extend(insn.address + 8 + 4 * k for k in range(count))
        return True

    def thumb_offset_table(self, mod: Module, f: Function, insn, todo: list[int]) -> bool:
        """Metrowerks Thumb switch: add rB, pc ; ldrh rX, [rB, #imm] ; lsl/asr #16 ; add pc, rX. The table of signed
        halfword offsets is at (add rB,pc) + 4 + imm; case k is at (add pc) + 4 + offset[k]."""
        rx = insn.operands[1].reg
        imm = rb = add_pc = None
        for k in range(1, 6):
            a = insn.address - 2 * k
            if not mod.contains(a):
                break
            p = next(CS[True].disasm(mod.data[a - mod.base:a - mod.base + 2], a, 1), None)
            if p is None:
                break
            po = p.operands
            if imm is None and p.id == A.ARM_INS_LDRH and po[0].reg == rx and po[1].type == A.ARM_OP_MEM:
                imm, rb = po[1].mem.disp, po[1].mem.base
            elif imm is not None and p.id == A.ARM_INS_ADD and len(po) == 2 and po[0].reg == rb and \
                    po[1].type == A.ARM_OP_REG and po[1].reg == A.ARM_REG_PC:
                add_pc = p.address
                break
        count = self._cmp_count(mod, add_pc, True, None) if add_pc is not None else None
        if count is None:
            return True
        table = add_pc + 4 + imm
        if not (mod.contains(table) and mod.contains(table + 2 * count - 1)):
            return True
        f.jump_tables += 1
        self.pools[mod.name].add_range(table, table + 2 * count)
        for k in range(count):
            off = struct.unpack_from("<h", mod.data, table - mod.base + 2 * k)[0]
            todo.append(insn.address + 4 + off)
        return True

    # ---- passes ------------------------------------------------------------------------------------------------
    def drain(self) -> None:
        checked = 0
        if self.queue:
            self.report("candidates")
        while self.queue:
            if self.progress is not None:
                checked += 1
                if checked % 256 == 0:
                    self.report("candidates", checked)
            name, addr, thumb, how = self.queue.pop()
            if (name, addr) in self.funcs:
                continue
            f = self.analyze(self.modules[name], addr, thumb, how)
            if f is not None:
                self.funcs[(name, addr)] = f
        if checked % 256:
            self.report("candidates", checked)

    def follows_flow_end(self, mod: Module, addr: int, thumb: bool) -> bool:
        """A function starts after code that cannot fall through (return, branch, jump), after data, or at the
        module start; a prologue in the middle of straight-line code (shrink-wrapping) is not a start."""
        if addr == mod.base:
            return True
        if addr - 1 in self.pools[mod.name]:
            return True
        prev4 = mod.data[addr - mod.base - 4:addr - mod.base] if addr - 4 >= mod.base else b""
        if prev4 == b"\0\0\0\0" or (thumb and prev4[2:] == b"\0\0"):
            return True  # alignment padding
        # decode the instruction before (whether claimed yet or not) and require that it cannot fall through;
        # a Thumb function aligned to 4 may sit after one halfword of padding
        ends = [addr] + ([addr - 2] if thumb and addr % 4 == 0 else [])
        for e in ends:
            for size in ((2, 4) if thumb else (4,)):
                p = e - size
                if p < mod.base:
                    continue
                insn = next(CS[thumb].disasm(mod.data[p - mod.base:e - mod.base], p, 1), None)
                if insn is None or insn.size != size:
                    continue
                cond = insn.cc not in (A.ARM_CC_AL, A.ARM_CC_INVALID)
                _, w = insn.regs_access()
                if not cond and (insn.id in (A.ARM_INS_B, A.ARM_INS_BX) or A.ARM_REG_PC in w):
                    return True
        # after a literal pool of a function not found yet: the word before is an address in a module
        if addr % 4 == 0 and len(prev4) == 4:
            if self.in_any_module(struct.unpack("<I", prev4)[0] & ~1):
                return True
        return False

    def looks_like_function(self, mod: Module, addr: int, thumb: bool, need_prologue: bool = True) -> bool:
        cs = CS[thumb]
        off = addr - mod.base
        insns = list(cs.disasm(mod.data[off:off + 32], addr, 8))
        if len(insns) < 2 or all(b == 0 for b in mod.data[off:off + 8]):
            return False
        if not self.follows_flow_end(mod, addr, thumb):
            return False
        first = insns[0]
        if first.id in (A.ARM_INS_PUSH, A.ARM_INS_STMDB) and "lr" in first.op_str:
            return True
        return not need_prologue

    def next_pass(self) -> int:
        """Functions of a module follow each other: try the first unclaimed address after each known function."""
        added = 0
        known = list(self.funcs.items())
        self.report("adjacent", 0, len(known))
        for index, ((name, a), f) in enumerate(known):
            if index and index % 512 == 0:
                self.report("adjacent", index, len(known))
            mod = self.modules[name]
            nxt = a + f.size
            nxt += (-nxt) % (2 if f.thumb else 4)
            # Thumb functions are normally 4-aligned with a halfword of padding before them: try that first
            for cand in ((nxt + 2, nxt) if f.thumb and nxt % 4 == 2 else (nxt, nxt + 2)):
                if not mod.contains(cand) or (name, cand) in self.funcs or cand in self.code[name] or \
                        cand in self.pools[name]:
                    continue
                if (not f.thumb and cand % 4) or not self.looks_like_function(mod, cand, f.thumb, False):
                    continue
                self.seed(name, cand, f.thumb, "next")
                added += 1
                break
            else:
                # a function of the other mode (an ARM file after a Thumb file or the reverse): only with a
                # prologue whose matching epilogue follows
                other = not f.thumb
                cand = nxt + (-nxt) % (2 if other else 4)
                if other and cand % 4 == 2:
                    cand += 2  # Thumb after ARM: normally 4-aligned too
                if mod.contains(cand) and (name, cand) not in self.funcs and cand not in self.code[name] and \
                        cand not in self.pools[name] and self.looks_like_function(mod, cand, other) and \
                        self.has_matching_epilogue(mod, cand, other):
                    self.seed(name, cand, other, "next")
                    added += 1
        self.report("adjacent", len(known), len(known))
        return added

    def has_matching_epilogue(self, mod: Module, a: int, thumb: bool, limit: int = 4096) -> bool:
        """The prologue at a (push {regs, lr} / stmdb sp!, {regs, lr}) has its mirror (pop {regs, pc} /
        ldmia sp!, {regs, pc}) within limit bytes: strong evidence of a function start in that mode."""
        off = a - mod.base
        if thumb:
            push = struct.unpack_from("<H", mod.data, off)[0]
            if push & 0xFF00 != 0xB500:
                return False
            want, align = struct.pack("<H", 0xBD00 | (push & 0xFF)), 2
        else:
            push = mod.word(a)
            if push & 0xFFFF4000 != 0xE92D4000:
                return False
            want, align = struct.pack("<I", 0xE8BD8000 | (push & 0x3FFF)), 4
        o = mod.data.find(want, off + align, off + limit)
        while o >= 0:
            if o % align == 0:
                return True
            o = mod.data.find(want, o + 1, off + limit)
        return False

    def in_pointer_table(self, mod: Module, a: int) -> bool:
        """The word at a has a neighbour that points to a known function (with the right mode bit): a table of
        function pointers such as a C++ vtable."""
        for nb in (a - 4, a + 4):
            if not mod.contains(nb) or nb + 4 > mod.base + len(mod.data) or nb in self.code[mod.name]:
                continue
            v = mod.word(nb)
            m = self.module_of(v & ~1, mod.name)
            f = self.funcs.get((m.name, v & ~1)) if m else None
            if f is not None and f.thumb == bool(v & 1):
                return True
        return False

    def pointer_pass(self) -> int:
        added = 0
        total = len(self.modules)
        self.report("pointers", 0, total)
        for index, mod in enumerate(self.modules.values(), 1):
            for off in range(0, len(mod.data) - 3, 4):
                a = mod.base + off
                if a in self.code[mod.name]:
                    continue
                v = mod.word(a)
                target, thumb = v & ~1, bool(v & 1)
                tm = self.module_of(target, mod.name)
                if not tm or (tm.name, target) in self.funcs or target in self.code[tm.name]:
                    continue
                if not thumb and target % 4:
                    continue
                if self.looks_like_function(tm, target, thumb):
                    self.seed(tm.name, target, thumb, "pointer")
                    added += 1
                elif self.in_pointer_table(mod, a) and self.looks_like_function(tm, target, thumb, False):
                    self.seed(tm.name, target, thumb, "table")  # leaf function (no prologue) in a pointer table
                    added += 1
            if index % 4 == 0 or index == total:
                self.report("pointers", index, total)
        return added

    # prologues: ARM stmdb sp!, {.., lr} (word & 0xFFFF4000 == 0xE92D4000); Thumb push {.., lr} (0xB5xx)
    _ARM_PRO = re.compile(rb"(?=[\x00-\xff][\x40-\x7f\xc0-\xff]\x2d\xe9)", re.S)
    _THUMB_PRO = re.compile(rb"(?=[\x00-\xff]\xb5)", re.S)

    def gap_pass(self) -> int:
        added = 0
        total = len(self.modules)
        self.report("gaps", 0, total)
        for index, mod in enumerate(self.modules.values(), 1):
            code, pools = self.code[mod.name], self.pools[mod.name]
            # mode of a gap: the mode of the closest discovered function before it
            starts = sorted((a, f.thumb) for (n, a), f in self.funcs.items() if n == mod.name)
            keys = [a for a, _ in starts]
            cands = [(o, False) for o in (m.start() for m in self._ARM_PRO.finditer(mod.data)) if o % 4 == 0]
            cands += [(o, True) for o in (m.start() for m in self._THUMB_PRO.finditer(mod.data)) if o % 2 == 0]
            for off, thumb in sorted(cands):
                a = mod.base + off
                if a + 4 > mod.base + len(mod.data):
                    continue
                if a in code or a in pools or (mod.name, a) in self.funcs or (mod.name, a, thumb) in self.rejected:
                    continue
                k = bisect.bisect_right(keys, a)
                if k and starts[k - 1][1] != thumb and not self.has_matching_epilogue(mod, a, thumb):
                    continue
                if self.looks_like_function(mod, a, thumb):
                    self.seed(mod.name, a, thumb, "gap")
                    added += 1
            if index % 4 == 0 or index == total:
                self.report("gaps", index, total)
        return added

    def run(self, entries: list[tuple[str, int, bool]]) -> list[Function]:
        for name, addr, thumb in entries:
            self.seed(name, addr, thumb, "entry")
        self.round = 0
        while True:
            self.round += 1
            self.drain()
            before = len(self.funcs)
            for p in (self.next_pass, self.pointer_pass, self.gap_pass):
                if p():
                    self.drain()
            if len(self.funcs) == before:
                break
        return sorted(self.funcs.values(), key=lambda f: (f.module, f.addr))


def load_extract(d: Path) -> tuple[list[Module], list[tuple[str, int, bool]]]:
    man = json.loads((d / "manifest.json").read_text())
    mods = [Module(m["name"], m["base"], (d / f"{m['name']}.bin").read_bytes()) for m in man["modules"]
            if m["name"] != "arm7" and m.get("kind", "code") == "code"]
    entries = [("arm9", man["header"]["arm9_entry"], False)]
    return mods, entries, man.get("module_params")


def known_data(disc: "Discovery", params: dict | None) -> None:
    """Mark structures the ROM format defines as data: the NitroSDK module parameter block."""
    if params:
        disc.pools["arm9"].add_range(params["address"], params["address"] + 0x24)


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("extract_dir")
    ap.add_argument("--out", default=None)
    ap.add_argument("--progress", action="store_true", help=argparse.SUPPRESS)
    a = ap.parse_args(argv)
    mods, entries, params = load_extract(Path(a.extract_dir))

    def report(round_number: int, phase: str, done: int, total: int) -> None:
        print(f"SCAN {round_number} {phase} {done} {total}", flush=True)

    disc = Discovery(mods, report if a.progress else None)
    known_data(disc, params)
    funcs = disc.run(entries)
    out = Path(a.out or Path(a.extract_dir) / "functions.json")
    out.write_text(json.dumps([asdict(f) for f in funcs], indent=1))
    by = {}
    for f in funcs:
        by[f.found_by] = by.get(f.found_by, 0) + 1
    print(f"{len(funcs)} functions ({', '.join(f'{k}: {v}' for k, v in sorted(by.items()))}) -> {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
