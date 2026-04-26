#!/usr/bin/env python3
"""
Remove common CMake / build / CPM cache artifacts without touching Git state.
Preserves untracked source files (does not run git clean).
Cross-platform: Windows, macOS, Linux.
"""

from __future__ import annotations

import argparse
import shutil
import sys
from pathlib import Path


def repo_root() -> Path:
    here = Path(__file__).resolve()
    return here.parent.parent


def rm_tree(path: Path, dry_run: bool) -> None:
    if not path.exists():
        return
    print(f"  REMOVE {path}")
    if not dry_run:
        if path.is_dir():
            shutil.rmtree(path, ignore_errors=True)
        else:
            try:
                path.unlink()
            except OSError:
                pass


def scorch(dry_run: bool) -> int:
    root = repo_root()
    candidates = [
        root / "out",
        root / "build",
        root / "cmake-build-debug",
        root / "cmake-build-release",
        root / ".cpm-cache",
        root / "CMakeFiles",
        root / "CMakeCache.txt",
        root / "CMakeScripts",
        root / "Testing",
        root / "compile_commands.json",
    ]
    # Ninja / VS may create build dirs at repo root
    for name in ("Debug", "Release", "RelWithDebInfo", "MinSizeRel", "x64", "Win32"):
        candidates.append(root / name)

    print(f"Scorch: cleaning build-like paths under {root}")
    for p in candidates:
        rm_tree(p, dry_run)
    print("Done (Git working tree unchanged; untracked files kept).")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--dry-run", action="store_true", help="Print paths only, do not delete.")
    args = ap.parse_args()
    return scorch(dry_run=args.dry_run)


if __name__ == "__main__":
    sys.exit(main())
