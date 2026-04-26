#!/usr/bin/env python3
"""
FNTBE — Funny Numbers To Boost Ego

Count lines, files, and folders in the Solstice tree while skipping build
artifacts, vendored 3rd party trees, and common junk (aligned with .gitignore).
"""

from __future__ import annotations

import argparse
import os
import sys
from collections import Counter
from dataclasses import dataclass, field
from pathlib import Path

# Windows console: UTF-8
if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except (AttributeError, OSError):
        pass


def repo_root() -> Path:
    return Path(__file__).resolve().parent.parent


# --- exclusion (broadly matches .gitignore) ---

SKIP_DIR_NAMES: frozenset[str] = frozenset(
    {
        ".git",
        "3rdparty",
        "node_modules",
        "venv",
        ".venv",
        "__pycache__",
        ".vs",
        ".idea",
        ".vscode",
        "out",
        "build",
        "bin",
        "lib",
        ".cpm-cache",
        "logs",
        "build-cmakecheck2",
        ".cache",
        ".ccache",
    }
)

SKIP_PATH_CONTAINS: tuple[str, ...] = (
    f"{os.sep}tools{os.sep}_ffmpeg",
    f"{os.sep}source{os.sep}Shaders{os.sep}bin",
)

_BUILDISH: frozenset[str] = frozenset(
    {
        "Release",
        "RelWithDebInfo",
        "MinSizeRel",
        "x64",
        "x86",
        "Win32",
    }
)


def _is_core_debug_path(rel_posix: str) -> bool:
    return rel_posix == "source/Core/Debug" or rel_posix.startswith("source/Core/Debug/")


def _should_prune_dir(rel: Path) -> bool:
    name = rel.name
    s = rel.as_posix()
    for sub in SKIP_PATH_CONTAINS:
        if sub.replace("\\", "/") in s:
            return True
    if name in SKIP_DIR_NAMES:
        return True
    if name.startswith("cmake-build-"):
        return True
    if name in _BUILDISH:
        return True
    if name == "Debug" and not _is_core_debug_path(rel.as_posix()):
        return True
    return False


# Headline "lines of code" — implementation + shader + build scripts, etc.
LOC_BY_EXT: dict[str, str] = {
    ".c": "C",
    ".cc": "C++",
    ".cpp": "C++",
    ".cxx": "C++",
    ".h": "C/C++ header",
    ".hh": "C++ header",
    ".hpp": "C++ header",
    ".hxx": "C++ header",
    ".inl": "C++",
    ".inc": "C/C++",
    ".m": "ObjC",
    ".mm": "ObjC++",
    ".ixx": "C++",
    ".cppm": "C++",
    ".py": "Python",
    ".cmake": "CMake",
    ".glsl": "Shader",
    ".hlsl": "Shader",
    ".vert": "Shader",
    ".frag": "Shader",
    ".geom": "Shader",
    ".comp": "Shader",
    ".metal": "Shader",
    ".lua": "Lua",
    ".ps1": "Script",
    ".sh": "Script",
    ".bat": "Script",
    ".cmd": "Script",
}

# Docs / config: lines counted, not mixed into headline LOC
DOC_BY_EXT: dict[str, str] = {
    ".md": "Markdown",
    ".txt": "Text",
    ".yml": "YAML",
    ".yaml": "YAML",
    ".json": "JSON",
    ".toml": "TOML",
    ".xml": "XML",
    ".ini": "INI",
    ".in": "Template",
    ".rc": "Win resource",
    ".def": "DEF",
    ".editorconfig": "EditorConfig",
    ".natvis": "Natvis",
}

# Dotfiles (suffix empty or odd)
DOT_DOC: dict[str, str] = {
    ".gitignore": "Git",
    ".gitattributes": "Git",
}

BINARY_EXT: frozenset[str] = frozenset(
    {
        ".o",
        ".obj",
        ".exe",
        ".dll",
        ".pdb",
        ".ilk",
        ".lib",
        ".a",
        ".so",
        ".dylib",
        ".elf",
        ".class",
        ".pyc",
        ".pyo",
        ".whl",
        ".egg",
        ".zip",
        ".7z",
        ".rar",
        ".tar",
        ".gz",
        ".bz2",
        ".xz",
        ".parquet",
        ".bin",
        ".spv",
        ".ninja_deps",
        ".ninja_log",
        ".png",
        ".jpg",
        ".jpeg",
        ".gif",
        ".webp",
        ".ico",
        ".cur",
        ".ttf",
        ".otf",
        ".woff",
        ".woff2",
        ".mp3",
        ".wav",
        ".ogg",
        ".flac",
        ".mp4",
        ".webm",
        ".avi",
        ".fbx",
        ".gltf",
        ".glb",
        ".blend",
        ".psd",
        ".exr",
        ".hdr",
    }
)

SKIP_BASENAMES: frozenset[str] = frozenset(
    {
        "CMakeCache.txt",
        "compile_commands.json",
    }
)


@dataclass
class ScanState:
    total_text_files: int = 0
    total_binary_files: int = 0
    total_bytes: int = 0
    dirs_walked: int = 0  # directories entered (incl. root)
    line_total_loc: int = 0
    line_total_other: int = 0
    loc_by_lang: Counter[str] = field(default_factory=Counter)
    loc_files_by_lang: Counter[str] = field(default_factory=Counter)
    other_by_label: Counter[str] = field(default_factory=Counter)
    other_file_count: int = 0
    lines_by_top: Counter[str] = field(default_factory=Counter)


def _top_bucket(rel: Path) -> str:
    if len(rel.parent.parts) == 0:
        return "(root)"
    return rel.parts[0]


def _line_count_text(path: Path) -> int | None:
    try:
        with path.open("rb") as f:
            chunk = f.read(1 << 17)
    except OSError:
        return None
    if b"\0" in chunk:
        return None
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return None
    if not text:
        return 0
    n = text.count("\n")
    if not text.endswith("\n") and n > 0:
        return n + 1
    if not text.endswith("\n") and n == 0 and len(text) > 0:
        return 1
    return n


def _classify(
    name_lower: str, ext: str
) -> tuple[str, str] | None:
    """
    Return (category, label) with category in ('loc', 'doc', 'other'),
    or None = treat as unclassified (probe as text/binary).
    """
    if name_lower == "cmakelists.txt":
        return ("loc", "CMake (CMakeLists.txt)")
    if name_lower in ("makefile", "gnumakefile"):
        return ("loc", "Make")
    if name_lower in DOT_DOC:
        return ("doc", DOT_DOC[name_lower])
    if ext in LOC_BY_EXT:
        return ("loc", LOC_BY_EXT[ext])
    if ext in DOC_BY_EXT:
        return ("doc", DOC_BY_EXT[ext])
    return None


def scan(root: Path) -> ScanState:
    state = ScanState()
    root = root.resolve()
    for dirpath, dirnames, filenames in os.walk(root, topdown=True):
        dpath = Path(dirpath)
        rel_dir = dpath.relative_to(root) if dpath != root else Path()
        state.dirs_walked += 1
        dirnames[:] = [d for d in dirnames if not _should_prune_dir((rel_dir / d))]

        for name in filenames:
            path = dpath / name
            rel = path.relative_to(root)
            rel_norm = rel.as_posix()
            if any(
                p.replace("\\", "/") in rel_norm.replace("\\", "/")
                for p in SKIP_PATH_CONTAINS
            ):
                continue
            if name in SKIP_BASENAMES:
                continue

            ext = path.suffix.lower()
            low = name.lower()
            st = _classify(low, ext)

            try:
                size = path.stat().st_size
            except OSError:
                continue

            if ext in BINARY_EXT:
                state.total_binary_files += 1
                state.total_bytes += size
                continue

            nlines = _line_count_text(path)
            if nlines is None:
                state.total_binary_files += 1
                state.total_bytes += size
                continue

            state.total_text_files += 1
            state.total_bytes += size

            if st is not None:
                cat, label = st
            else:
                cat, label = "other", "other (text)"

            if cat == "loc":
                state.line_total_loc += nlines
                state.loc_by_lang[label] += nlines
                state.loc_files_by_lang[label] += 1
                state.lines_by_top[_top_bucket(rel)] += nlines
            elif cat == "doc":
                state.line_total_other += nlines
                state.other_by_label[label] += nlines
                state.other_file_count += 1
            else:
                state.line_total_other += nlines
                state.other_by_label[label] += nlines
                state.other_file_count += 1

    return state


# --- output ---

_ANSI = {
    "reset": "\033[0m",
    "bold": "\033[1m",
    "dim": "\033[2m",
    "accent": "\033[38;2;200;150;255m",  # soft violet
    "title": "\033[38;2;120;200;255m",  # cool blue
    "ok": "\033[38;2;130;220;180m",  # mint
    "num": "\033[38;2;255;220;150m",  # warm
    "mut": "\033[38;2;220;200;150m",  # label
}


def _fmt_num(n: int) -> str:
    return f"{n:,}"


def _bar(pct: float, width: int = 18) -> str:
    p = max(0.0, min(100.0, pct))
    filled = int(round((p / 100.0) * width))
    return "█" * filled + "░" * (width - filled)


def report(state: ScanState, root: Path, use_color: bool) -> None:
    c = _ANSI if use_color else {k: "" for k in _ANSI}

    loc_files = int(sum(state.loc_files_by_lang.values()))
    total_lines = state.line_total_loc + state.line_total_other
    all_files = state.total_text_files + state.total_binary_files

    avg_bytes = state.total_bytes / all_files if all_files else 0.0
    avg_loc_per_locfile = state.line_total_loc / loc_files if loc_files else 0.0
    avg_all_lines = total_lines / state.total_text_files if state.total_text_files else 0.0
    loc_pct_lines = 100.0 * state.line_total_loc / total_lines if total_lines else 0.0
    loc_pct_files = 100.0 * loc_files / state.total_text_files if state.total_text_files else 0.0

    w = 56
    line = f"{c['dim']}{'═' * w}{c['reset']}"

    print()
    print(
        f"  {c['title']}{c['bold']}"
        f"FNTBE{c['reset']}"
        f"{c['dim']} — Funny Numbers To Boost Ego{c['reset']}"
    )
    print(f"  {c['mut']}{root}{c['reset']}")
    print(line)

    print(
        f"  {c['accent']}{'Lines of code (headline):':<28}{c['reset']}"
        f" {c['bold']}{c['num']}{_fmt_num(state.line_total_loc):>14}{c['reset']}"
    )
    print(
        f"  {c['mut']}{'  … across LOC files:':<28}{c['reset']}"
        f" {c['num']}{_fmt_num(loc_files):>14}{c['reset']}"
    )
    print(
        f"  {c['mut']}{'Other / docs text lines:':<28}{c['reset']}"
        f" {c['num']}{_fmt_num(state.line_total_other):>14}{c['reset']}"
    )
    print(
        f"  {c['mut']}{'Total text lines:':<28}{c['reset']}"
        f" {c['bold']}{c['ok']}{_fmt_num(total_lines):>14}{c['reset']}"
    )
    print(line)

    print(
        f"  {c['accent']}{'Text files:':<28}{c['reset']}"
        f" {c['num']}{_fmt_num(state.total_text_files):>14}{c['reset']}"
    )
    print(
        f"  {c['mut']}{'Binary / skipped-as-binary:':<28}{c['reset']}"
        f" {c['num']}{_fmt_num(state.total_binary_files):>14}{c['reset']}"
    )
    print(
        f"  {c['mut']}{'Folders scanned:':<28}{c['reset']}"
        f" {c['num']}{_fmt_num(state.dirs_walked):>14}{c['reset']}"
    )
    print(
        f"  {c['mut']}{'Total size (text+binary):':<28}{c['reset']}"
        f" {c['num']}{_fmt_num(state.total_bytes):>10}{c['reset']}  bytes"
    )
    print(line)

    print(
        f"  {c['accent']}{'Avg file size:':<28}{c['reset']}"
        f" {c['num']}{avg_bytes:>14,.1f}{c['reset']}  B"
    )
    print(
        f"  {c['mut']}{'Avg lines / LOC file:':<28}{c['reset']}"
        f" {c['num']}{avg_loc_per_locfile:>14,.1f}{c['reset']}"
    )
    print(
        f"  {c['mut']}{'Avg lines / any text file:':<28}{c['reset']}"
        f" {c['num']}{avg_all_lines:>14,.1f}{c['reset']}"
    )
    print(
        f"  {c['mut']}{'LOC % of all text lines:':<28}{c['reset']}"
        f" {c['num']}{loc_pct_lines:>13.1f}%{c['reset']}"
    )
    print(
        f"  {c['mut']}{'LOC files % of text files:':<28}{c['reset']}"
        f" {c['num']}{loc_pct_files:>13.1f}%{c['reset']}"
    )
    print(line)

    if state.loc_by_lang:
        print(f"  {c['title']}{c['bold']}LOC by language{c['reset']}")
        items = sorted(
            state.loc_by_lang.items(),
            key=lambda x: (-x[1], x[0].lower()),
        )
        mlines = max(state.line_total_loc, 1)
        for name, n in items[:16]:
            pct = 100.0 * n / mlines
            print(
                f"    {c['mut']}{name[:32]:<32}{c['reset']}"
                f" {_bar(pct)} {c['num']}{_fmt_num(n):>9}{c['reset']}"
            )
        if len(items) > 16:
            print(f"    {c['dim']}(… {len(items) - 16} more rows){c['reset']}")
        print()

    if state.lines_by_top:
        print(f"  {c['title']}{c['bold']}LOC by top-level tree{c['reset']}")
        for k, n in state.lines_by_top.most_common(12):
            pct = 100.0 * n / max(state.line_total_loc, 1)
            print(
                f"    {c['mut']}{k[:24]:<24}{c['reset']}"
                f" {_bar(pct, 14)} {c['num']}{_fmt_num(n):>9}{c['reset']}"
            )
        print()

    oitems = [x for x in state.other_by_label.most_common(10) if x[1] > 0]
    if oitems:
        print(f"  {c['title']}{c['bold']}Non-LOC text (top){c['reset']}")
        for name, n in oitems:
            print(f"    {c['mut']}{name[:40]:<40}{c['reset']}{c['num']}{_fmt_num(n):>10}{c['reset']}")
        print()

    print(
        f"  {c['dim']}"
        f"Excludes: 3rdparty, build out dirs, .cpm-cache, .git, IDEs, "
        f"Shader/bin, tools/_ffmpeg, common binaries. "
        f"Debug/ is pruned except source/Core/Debug.{c['reset']}"
    )
    print()


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument(
        "root",
        nargs="?",
        type=Path,
        default=repo_root(),
        help="Repository root (default: parent of tools/)",
    )
    p.add_argument(
        "--no-color",
        action="store_true",
        help="Plain output (no ANSI colors)",
    )
    args = p.parse_args()
    root: Path = args.root.resolve()
    if not root.is_dir():
        print(f"Not a directory: {root}", file=sys.stderr)
        return 1
    use_color = (
        not args.no_color
        and sys.stdout.isatty()
        and "NO_COLOR" not in os.environ
    )
    st = scan(root)
    report(st, root, use_color=use_color)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
