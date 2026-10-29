#!/usr/bin/env python3
"""Check that the per-byte script loop GCC inlines into the CTxIn/CTxOut vector
destructors starts on a 32-byte boundary and never crosses a 64-byte line.

bitcoin-sv's prevector::erase pops a script one byte per iteration. Where that
loop lands relative to a 64-byte line changes large-script validation time by
11-16% on some x86-64 CPUs, so BDK builds with GCC's
`-falign-loops=32 --param=align-loop-iterations=1` (cmake/BDKBuildSetting.cmake,
bdkSetLoopAlignment). This script disassembles every copy of both destructors in
the given archives or objects and fails if the loop is not 32-byte aligned, or
if it would cross a 64-byte line at any start the section alignment permits.

Usage: check_loop_alignment.py [--objdump PATH] FILE...  (ELF, x86-64 or aarch64)
"""

import argparse
import re
import subprocess
import sys

DESTRUCTORS = {
    "_ZNSt6vectorI5CTxInSaIS0_EED2Ev": "std::vector<CTxIn>::~vector",
    "_ZNSt6vectorI6CTxOutSaIS0_EED2Ev": "std::vector<CTxOut>::~vector",
}
LINE = 64
ALIGN = 32
MAX_LOOP_INSNS = 8

MEMBER_RE = re.compile(r"^(?:\S+\()?([^()\s]+)\)?:\s+file format\s+(\S+)")
INSN_RE = re.compile(r"^\s*([0-9a-f]+):\s+(\S.*)$")
# conditional backward branch: x86-64 jcc (not jmp), aarch64 b.cond/cbz/cbnz/tbz/tbnz
BRANCH_RE = re.compile(r"^(j(?!mp)[a-z]+|b\.[a-z]+|cbn?z|tbn?z)\s.*?\b([0-9a-f]+)\s+<")
CALL_RE = re.compile(r"^(call|bl|blr)\b")
# the per-byte decrement of prevector's size: x86-64 `sub $0x1,%reg` / `dec`,
# aarch64 `sub(s) xN, xN, #0x1`
DEC_RE = re.compile(r"^(sub[a-z]*\s+\$0x1,|dec[a-z]*\s|subs?\s+[xw]\d+,\s*[xw]\d+,\s*#0x1\b)")


def objdump(tool, args):
    res = subprocess.run([tool] + args, capture_output=True, text=True)
    # any failure, even with partial output: an unreadable member would drop
    # its destructor copies from both listings and leave the rest to pass
    if res.returncode != 0:
        sys.exit(f"error: {tool} {' '.join(args)} failed (exit {res.returncode}):\n{res.stderr}")
    return res.stdout


def section_aligns(tool, path, section):
    """Alignment of `section` in each member that has it, in order."""
    aligns, member = [], None
    for line in objdump(tool, ["-h", "-w", path]).splitlines():
        m = MEMBER_RE.match(line)
        if m:
            member = m.group(1)
            continue
        f = line.split()
        if len(f) > 6 and f[1] == section:
            aligns.append((member, 1 << int(f[6].split("**")[1]), int(f[2], 16)))
    return aligns


def disassemble(tool, path, section):
    """Instruction lists of `section`, one per member that has it, in order."""
    out, cur = [], None
    for line in objdump(tool, ["-d", "--no-show-raw-insn", "-j", section, path]).splitlines():
        if MEMBER_RE.match(line):
            cur = None
        elif line.startswith("Disassembly of section"):
            cur = []
            out.append(cur)
        elif cur is not None:
            m = INSN_RE.match(line)
            if m:
                cur.append((int(m.group(1), 16), m.group(2).strip()))
    return out


def byte_loop(insns, size):
    """(start, end) of the innermost backward-branch loop whose first
    instruction decrements by one and that makes no call, else None."""
    best = None
    for i, (addr, text) in enumerate(insns):
        m = BRANCH_RE.match(text)
        if not m:
            continue
        target = int(m.group(2), 16)
        body = [t for a, t in insns if target <= a <= addr]
        if target > addr or not body or len(body) > MAX_LOOP_INSNS:
            continue
        if not DEC_RE.match(body[0]) or any(CALL_RE.match(t) for t in body):
            continue
        end = insns[i + 1][0] if i + 1 < len(insns) else size
        if best is None or end - target < best[1] - best[0]:
            best = (target, end)
    return best


def crossing_starts(align, start, end):
    """Function starts (mod 64) the section alignment permits at which
    [start, end) crosses a 64-byte line."""
    return [p for p in range(0, LINE, align) if (p + start) // LINE != (p + end - 1) // LINE]


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--objdump", default="objdump")
    ap.add_argument("files", nargs="+")
    args = ap.parse_args()

    failures, checked = [], 0
    for path in args.files:
        for section, name in DESTRUCTORS.items():
            sec = ".text." + section
            aligns = section_aligns(args.objdump, path, sec)
            listings = disassemble(args.objdump, path, sec)
            if not aligns:
                failures.append(f"{path}: no {name} ({sec}) found")
                continue
            if len(aligns) != len(listings):
                failures.append(f"{path}: {name}: {len(aligns)} sections but {len(listings)} disassemblies")
                continue
            for (member, align, size), insns in zip(aligns, listings):
                loop = byte_loop(insns, size)
                where = f"{path}({member}) {name}" if member != path else f"{path} {name}"
                if loop is None:
                    failures.append(f"{where}: no per-byte loop found. If bitcoin-sv's prevector::erase "
                                    "no longer loops, drop BDK_ALIGN_LOOPS and this check")
                    continue
                start, end = loop
                cross = crossing_starts(align, start, end)
                checked += 1
                print(f"{where}: loop +{start:#x}..+{end:#x} ({end - start} B), section align {align}, "
                      f"start mod {ALIGN} = {start % ALIGN}, crosses a {LINE}-byte line at function "
                      f"start(s) mod {LINE}: {cross or 'none'}")
                if start % ALIGN or align < ALIGN:
                    failures.append(f"{where}: loop at +{start:#x} in a {align}-byte aligned section is not "
                                    f"{ALIGN}-byte aligned")
                if cross:
                    failures.append(f"{where}: loop crosses a {LINE}-byte line when the function starts at "
                                    f"{', '.join(map(str, cross))} mod {LINE}")
    if failures:
        print(f"\nFAIL: {len(failures)} problem(s). The per-byte script loop is not kept inside one "
              f"{LINE}-byte line; check that bdkSetLoopAlignment() (cmake/BDKBuildSetting.cmake) applied "
              "-falign-loops=32 --param=align-loop-iterations=1 to this GCC build.")
        for f in failures:
            print("  " + f)
        return 1
    print(f"OK: {checked} destructor copies, loop {ALIGN}-byte aligned and inside one {LINE}-byte line")
    return 0


if __name__ == "__main__":
    sys.exit(main())
