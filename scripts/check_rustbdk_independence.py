#!/usr/bin/env python3
"""Guard rustbdk against machine-meaningful GoBDK coupling.

This deliberately ignores prose and comments. Documentation and comments may
mention GoBDK while explaining the boundary; only include, link, build-time
file access, symlink, and vendored-archive coupling is rejected.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
RUSTBDK_DIR = REPO_ROOT / "module" / "rustbdk"
GOBDK_DIR = REPO_ROOT / "module" / "gobdk"
BDK_SYS_BUILD_RS = RUSTBDK_DIR / "bdk-sys" / "build.rs"
BDK_SYS_LIB_DIR = RUSTBDK_DIR / "bdk-sys" / "lib"

C_EXTENSIONS = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx"}
GOBDK_LINK_TOKENS = ("GoBDK", "static-GoBDK", "MergeGoBDK", "libGoBDK")
LINK_CMAKE_COMMANDS = {
    "add_custom_command",
    "add_custom_target",
    "add_dependencies",
    "link_libraries",
    "set",
    "target_link_libraries",
    "target_link_options",
}
BUILD_PATH_CMAKE_COMMANDS = {
    "add_custom_command",
    "add_custom_target",
    "add_subdirectory",
    "configure_file",
    "execute_process",
    "file",
    "install",
}
INCLUDE_PATH_CMAKE_COMMANDS = {
    "include_directories",
    "target_include_directories",
}
BUILD_RS_FILE_OP_RE = re.compile(
    r"\b(?:std::)?fs::(?:canonicalize|copy|hard_link|metadata|read|read_dir|read_to_string|symlink_metadata)\b"
    r"|(?:std::fs::)?File::open\b"
    r"|include_bytes!\s*\("
    r"|include_str!\s*\("
    r"|(?:std::process::)?Command::new\s*\("
    r"|std::os::unix::fs::symlink"
)


class Failure:
    def __init__(self, category: str, path: Path, line: int | None, detail: str) -> None:
        self.category = category
        self.path = path
        self.line = line
        self.detail = detail

    def __str__(self) -> str:
        location = rel(self.path)
        if self.line is not None:
            location = f"{location}:{self.line}"
        return f"[{self.category}] {location}: {self.detail}"


def rel(path: Path) -> str:
    try:
        return path.relative_to(REPO_ROOT).as_posix()
    except ValueError:
        return path.as_posix()


def is_under(path: Path, parent: Path) -> bool:
    try:
        path.resolve(strict=False).relative_to(parent.resolve(strict=False))
        return True
    except ValueError:
        return False


def line_number(text: str, offset: int) -> int:
    return text.count("\n", 0, offset) + 1


def strip_c_like_comments(text: str) -> str:
    out: list[str] = []
    i = 0
    state = "normal"
    quote = ""
    while i < len(text):
        ch = text[i]
        nxt = text[i + 1] if i + 1 < len(text) else ""

        if state == "normal":
            if ch == "/" and nxt == "/":
                out.extend((" ", " "))
                i += 2
                while i < len(text) and text[i] != "\n":
                    out.append(" ")
                    i += 1
                continue
            if ch == "/" and nxt == "*":
                out.extend((" ", " "))
                i += 2
                state = "block"
                continue
            if ch in ("'", '"'):
                quote = ch
                state = "string"
            out.append(ch)
            i += 1
            continue

        if state == "block":
            if ch == "\n":
                out.append("\n")
                i += 1
                continue
            if ch == "*" and nxt == "/":
                out.extend((" ", " "))
                i += 2
                state = "normal"
                continue
            out.append(" ")
            i += 1
            continue

        out.append(ch)
        if ch == "\\" and i + 1 < len(text):
            out.append(text[i + 1])
            i += 2
            continue
        if ch == quote:
            state = "normal"
        i += 1

    return "".join(out)


def _spaces_preserving_newlines(text: str) -> str:
    return "".join("\n" if ch == "\n" else " " for ch in text)


def _raw_string_end(text: str, start: int) -> int | None:
    if text.startswith(("br", "cr"), start):
        start += 1
    if start >= len(text) or text[start] != "r":
        return None

    marker = start + 1
    hashes = 0
    while marker < len(text) and text[marker] == "#":
        hashes += 1
        marker += 1
    if marker >= len(text) or text[marker] != '"':
        return None

    end_marker = '"' + ("#" * hashes)
    end = text.find(end_marker, marker + 1)
    if end == -1:
        return len(text)
    return end + len(end_marker)


def strip_c_like_comments_and_string_literals(text: str) -> str:
    text = strip_c_like_comments(text)
    out: list[str] = []
    i = 0
    while i < len(text):
        raw_end = _raw_string_end(text, i)
        if raw_end is not None:
            out.append(_spaces_preserving_newlines(text[i:raw_end]))
            i = raw_end
            continue

        if text[i] == '"':
            start = i
            i += 1
            while i < len(text):
                if text[i] == "\\" and i + 1 < len(text):
                    i += 2
                    continue
                if text[i] == '"':
                    i += 1
                    break
                i += 1
            out.append(_spaces_preserving_newlines(text[start:i]))
            continue

        out.append(text[i])
        i += 1

    return "".join(out)


def strip_cmake_comments(text: str) -> str:
    cleaned: list[str] = []
    for line in text.splitlines(keepends=True):
        in_quote = False
        escaped = False
        cut_at = len(line)
        for idx, ch in enumerate(line):
            if escaped:
                escaped = False
                continue
            if ch == "\\":
                escaped = True
                continue
            if ch == '"':
                in_quote = not in_quote
                continue
            if ch == "#" and not in_quote:
                cut_at = idx
                break
        cleaned.append(line[:cut_at] + ("\n" if line.endswith("\n") else ""))
    return "".join(cleaned)


def iter_cmake_statements(text: str) -> list[tuple[int, str]]:
    statements: list[tuple[int, str]] = []
    current: list[str] = []
    start_line = 1
    depth = 0
    for no, line in enumerate(text.splitlines(), 1):
        if not current and not line.strip():
            continue
        if not current:
            start_line = no
        current.append(line)
        depth += line.count("(") - line.count(")")
        if depth <= 0 and current:
            statements.append((start_line, "\n".join(current)))
            current = []
            depth = 0
    if current:
        statements.append((start_line, "\n".join(current)))
    return statements


def cmake_command(statement: str) -> str | None:
    match = re.match(r"\s*([A-Za-z_][A-Za-z0-9_]*)\s*\(", statement)
    if not match:
        return None
    return match.group(1).lower()


def token_resolves_under_gobdk(token: str, base: Path) -> bool:
    token = token.strip().strip("\"'")
    if not token or "$" in token or "<" in token or ">" in token:
        return False
    candidate = Path(token)
    if not candidate.is_absolute():
        candidate = base / candidate
    return is_under(candidate, GOBDK_DIR)


def references_gobdk_path(text: str, base: Path) -> bool:
    lower = text.replace("\\", "/").lower()
    if "module/gobdk" in lower or re.search(r"(^|[\s\"'/])(?:\.\./)+gobdk([/\s\"']|$)", lower):
        return True
    for token in re.findall(r"[A-Za-z0-9_./\\:-]+", text):
        normalized = token.replace("\\", "/").lower()
        if normalized == "gobdk" or normalized.startswith("gobdk/") or "/gobdk/" in normalized:
            return True
        if token_resolves_under_gobdk(token, base):
            return True
    return False


def check_cpp_includes(failures: list[Failure]) -> None:
    include_re = re.compile(r"^\s*#\s*include\s*([<\"])([^>\"]+)[>\"]", re.MULTILINE)
    for path in sorted(RUSTBDK_DIR.rglob("*")):
        if not path.is_file() or path.suffix not in C_EXTENSIONS:
            continue
        text = strip_c_like_comments(path.read_text(encoding="utf-8", errors="replace"))
        for match in include_re.finditer(text):
            header = match.group(2).strip()
            lower = header.replace("\\", "/").lower()
            bad_name = (
                "gobdk" in lower
                or "bdkcgo" in lower
                or lower.endswith("_cgo.h")
                or lower.endswith("gobdk.h")
            )
            bad_path = match.group(1) == '"' and token_resolves_under_gobdk(header, path.parent)
            if bad_name or bad_path:
                failures.append(
                    Failure(
                        "include",
                        path,
                        line_number(text, match.start()),
                        f"forbidden include target {header!r}",
                    )
                )


def check_cmake_link_and_build_paths(failures: list[Failure]) -> None:
    for path in sorted(RUSTBDK_DIR.rglob("CMakeLists.txt")):
        text = strip_cmake_comments(path.read_text(encoding="utf-8", errors="replace"))
        for start_line, statement in iter_cmake_statements(text):
            command = cmake_command(statement)
            if not command:
                continue

            if command in LINK_CMAKE_COMMANDS:
                for token in GOBDK_LINK_TOKENS:
                    if token in statement:
                        failures.append(
                            Failure(
                                "link",
                                path,
                                start_line,
                                f"forbidden GoBDK link/archive token {token!r}",
                            )
                        )
                if references_gobdk_path(statement, path.parent):
                    failures.append(
                        Failure("link", path, start_line, "link statement references module/gobdk")
                    )

            if command in BUILD_PATH_CMAKE_COMMANDS and references_gobdk_path(statement, path.parent):
                failures.append(
                    Failure(
                        "build-file-access",
                        path,
                        start_line,
                        f"{command} statement references module/gobdk",
                    )
                )

            if command in INCLUDE_PATH_CMAKE_COMMANDS and references_gobdk_path(statement, path.parent):
                failures.append(
                    Failure(
                        "include-path",
                        path,
                        start_line,
                        f"{command} statement references module/gobdk",
                    )
                )


def check_build_rs(failures: list[Failure]) -> None:
    if not BDK_SYS_BUILD_RS.exists():
        return
    raw_text = BDK_SYS_BUILD_RS.read_text(encoding="utf-8", errors="replace")

    # build.rs uses two tiers:
    # 1. operation-tied scans keep string literals so real fs/Command/include/link
    #    arguments pointing at module/gobdk are rejected;
    # 2. the broad backstop strips strings and comments so educational prose in
    #    panic/error messages is allowed.
    text = strip_c_like_comments(raw_text)
    code_text = strip_c_like_comments_and_string_literals(raw_text)

    if references_gobdk_path(code_text, BDK_SYS_BUILD_RS.parent):
        failures.append(
            Failure(
                "build-file-access",
                BDK_SYS_BUILD_RS,
                None,
                "build.rs code references module/gobdk outside comments and string literals",
            )
        )

    for token in GOBDK_LINK_TOKENS:
        for match in re.finditer(re.escape(token), text):
            failures.append(
                Failure(
                    "link",
                    BDK_SYS_BUILD_RS,
                    line_number(text, match.start()),
                    f"forbidden GoBDK link/archive token {token!r}",
                )
            )

    for match in re.finditer(r"cargo:rustc-link-(?:lib|search)[^\"\n;]*", text):
        directive = match.group(0)
        if any(token in directive for token in GOBDK_LINK_TOKENS) or references_gobdk_path(
            directive, BDK_SYS_BUILD_RS.parent
        ):
            failures.append(
                Failure(
                    "link",
                    BDK_SYS_BUILD_RS,
                    line_number(text, match.start()),
                    f"forbidden rustc link directive {directive!r}",
                )
            )

    lines = text.splitlines()
    for idx, line in enumerate(lines):
        if not BUILD_RS_FILE_OP_RE.search(line):
            continue
        statement = rust_statement_around(lines, idx)
        if references_gobdk_path(statement, BDK_SYS_BUILD_RS.parent):
            failures.append(
                Failure(
                    "build-file-access",
                    BDK_SYS_BUILD_RS,
                    idx + 1,
                    "build.rs file operation references module/gobdk",
                )
            )


def rust_statement_around(lines: list[str], idx: int) -> str:
    start = idx
    while start > 0 and ";" not in lines[start - 1]:
        start -= 1

    end = idx
    while end + 1 < len(lines) and ";" not in lines[end]:
        end += 1

    return "\n".join(lines[start : end + 1])


def check_symlinks(failures: list[Failure]) -> None:
    for path in sorted(RUSTBDK_DIR.rglob("*")):
        if not path.is_symlink():
            continue
        if is_under(path.resolve(strict=False), GOBDK_DIR):
            failures.append(
                Failure("build-file-access", path, None, "symlink resolves under module/gobdk")
            )


def check_vendored_archives(failures: list[Failure]) -> None:
    if not BDK_SYS_LIB_DIR.exists():
        return
    for path in sorted(BDK_SYS_LIB_DIR.rglob("libGoBDK_*.a")):
        failures.append(
            Failure(
                "vendored-archive",
                path,
                None,
                "rustbdk may vendor only libbdkffi_*.a archives, never libGoBDK_*.a",
            )
        )


def main() -> int:
    if not RUSTBDK_DIR.is_dir():
        print(f"rustbdk independence guard failed: missing {rel(RUSTBDK_DIR)}")
        return 2

    failures: list[Failure] = []
    check_cpp_includes(failures)
    check_cmake_link_and_build_paths(failures)
    check_build_rs(failures)
    check_symlinks(failures)
    check_vendored_archives(failures)

    if failures:
        print("rustbdk independence guard failed:")
        for failure in failures:
            print(f"  - {failure}")
        return 1

    print(
        "rustbdk independence guard passed: checked includes, link/archive names, "
        "build-time file access/symlinks, and vendored GoBDK archives; prose/comment "
        "mentions are ignored."
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
