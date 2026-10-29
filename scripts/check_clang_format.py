#!/usr/bin/env python3
"""Check (or fix) the formatting of BDK's first-party C++ against .clang-format.

The file set is every tracked or untracked-but-not-ignored C/C++ source and
header under core/, module/ and test/, minus the upstream-derived files that
cmake/BDKBuildSetting.cmake lists in BDK_UPSTREAM_DERIVED_SOURCES: those are
modified copies of node or Bitcoin Core files and keep their upstream layout so
they stay diffable against it. The node sources BDK compiles live outside the
checkout (or under the untracked bitcoin-sv/) and are never touched.

CI runs this with the clang-format of the LLVM major pinned in
.github/workflows/build_bdk.yaml. Use the same major locally: clang-format's
output drifts between majors, and the root .clang-format uses options that
need clang-format 20 or later.

    python3 scripts/check_clang_format.py            # report, exit 1 on a diff
    python3 scripts/check_clang_format.py --fix      # rewrite files in place
    python3 scripts/check_clang_format.py --clang-format clang-format-23
"""
from __future__ import annotations

import argparse
import difflib
import os
import re
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIRS = ("core", "module", "test")
C_EXTENSIONS = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx"}
BUILD_SETTING = REPO_ROOT / "cmake" / "BDKBuildSetting.cmake"
MIN_MAJOR = 20


def upstream_derived() -> set[str]:
    """Paths quoted inside set(BDK_UPSTREAM_DERIVED_SOURCES ...) in the cmake file."""
    text = BUILD_SETTING.read_text(encoding="utf-8")
    # Drop comments first: a ')' inside one would end the set() match early.
    # The CACHE docstring is quoted, so no stray ')' survives this.
    text = re.sub(r"#[^\n]*", "", text)
    match = re.search(r"set\(BDK_UPSTREAM_DERIVED_SOURCES\s+(.*?)\s+CACHE\b", text, re.S)
    if not match:
        sys.exit(f"{BUILD_SETTING}: set(BDK_UPSTREAM_DERIVED_SOURCES ... CACHE ...) not found")
    paths = set(re.findall(r'"([^"]+)"', match.group(1)))
    if not paths:
        sys.exit(f"{BUILD_SETTING}: BDK_UPSTREAM_DERIVED_SOURCES lists no files")
    return paths


def first_party_sources() -> list[Path]:
    out = subprocess.run(
        ["git", "ls-files", "--cached", "--others", "--exclude-standard", "-z", "--", *SOURCE_DIRS],
        cwd=REPO_ROOT, check=True, capture_output=True,
    ).stdout.decode()
    excluded = upstream_derived()
    files = []
    for rel in filter(None, out.split("\0")):
        path = Path(rel)
        if path.suffix in C_EXTENSIONS and rel not in excluded and (REPO_ROOT / path).is_file():
            files.append(path)
    return sorted(files)


def run_clang_format(exe: str, path: Path, text: str) -> str:
    """clang-format's output for `text` as if it were the file at `path`."""
    return subprocess.run(
        [exe, "--style=file", f"--assume-filename={path}"],
        input=text, check=True, capture_output=True, text=True, cwd=REPO_ROOT,
    ).stdout


def clang_format_version(exe: str) -> str:
    try:
        banner = subprocess.run([exe, "--version"], check=True, capture_output=True, text=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError) as error:
        sys.exit(f"cannot run {exe}: {error}")
    match = re.search(r"version (\d+)", banner)
    if not match or int(match.group(1)) < MIN_MAJOR:
        sys.exit(f"{banner}\nclang-format {MIN_MAJOR} or later is required by the root .clang-format")
    return banner


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--fix", action="store_true", help="rewrite misformatted files in place")
    parser.add_argument("--clang-format", default=os.environ.get("CLANG_FORMAT", "clang-format"),
                        help="clang-format executable (default: $CLANG_FORMAT or clang-format)")
    parser.add_argument("--quiet", action="store_true", help="list misformatted files without their diffs")
    args = parser.parse_args()

    print(clang_format_version(args.clang_format))
    files = first_party_sources()
    misformatted = []
    unsettled = []  # --fix could not reach a fixed point; CI's single pass would still fail
    for rel in files:
        path = REPO_ROOT / rel
        original = path.read_text(encoding="utf-8")
        formatted = run_clang_format(args.clang_format, path, original)
        if formatted == original:
            continue
        misformatted.append(rel)
        if args.fix:
            # One clang-format pass is not always a fixed point (comment reflow
            # can shift a wrap); write until a pass changes nothing.
            for _ in range(5):
                path.write_text(formatted, encoding="utf-8")
                again = run_clang_format(args.clang_format, path, formatted)
                if again == formatted:
                    break
                formatted = again
            else:
                unsettled.append(rel)
        elif not args.quiet:
            sys.stdout.writelines(difflib.unified_diff(
                original.splitlines(keepends=True), formatted.splitlines(keepends=True),
                fromfile=f"a/{rel}", tofile=f"b/{rel}"))

    verb = "reformatted" if args.fix else "misformatted"
    print(f"{len(files)} first-party C++ files checked, {len(misformatted)} {verb}")
    for rel in misformatted:
        print(f"  {rel}")
    if misformatted and not args.fix:
        print("\nRun: python3 scripts/check_clang_format.py --fix")
        return 1
    if unsettled:
        print("\nclang-format did not settle on these files after 5 passes; each pass still "
              "changes them, so the check will fail. Rewrite the construct it keeps "
              "reflowing (usually a comment) or wrap it in // clang-format off/on:")
        for rel in unsettled:
            print(f"  {rel}")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
