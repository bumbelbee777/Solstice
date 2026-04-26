#!/usr/bin/env python3
"""
Reset the repository to the latest commit on the current branch and remove all
untracked and ignored files (git clean -fdx). Destructive.

Requires git on PATH. Run from the repository root (or any subdirectory).
Cross-platform: Windows, macOS, Linux.
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path


def find_git_root(start: Path) -> Path | None:
    cur = start.resolve()
    for _ in range(64):
        if (cur / ".git").exists():
            return cur
        if cur.parent == cur:
            return None
        cur = cur.parent
    return None


def run(cmd: list[str], cwd: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(cmd, cwd=cwd, check=False, text=True, capture_output=True)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--i-really-mean-it",
        action="store_true",
        help="Required. Without this flag, the script exits without changing anything.",
    )
    ap.add_argument(
        "--use-origin",
        action="store_true",
        help="After fetch, reset --hard to origin/<current-branch> if it exists; else HEAD.",
    )
    ap.add_argument("--dry-run", action="store_true", help="Show git commands only; do not run them.")
    args = ap.parse_args()

    if not args.i_really_mean_it:
        print("Refusing to run: pass --i-really-mean-it to confirm destructive reset.", file=sys.stderr)
        return 2

    root = find_git_root(Path(__file__).resolve().parent.parent)
    if root is None:
        print("Could not find .git above this script.", file=sys.stderr)
        return 1

    branch = run(["git", "rev-parse", "--abbrev-ref", "HEAD"], root)
    if branch.returncode != 0:
        print(branch.stderr or branch.stdout, file=sys.stderr)
        return 1
    bname = branch.stdout.strip()
    if bname == "HEAD":
        print("Detached HEAD; reset will use HEAD only (no origin/<branch>).", file=sys.stderr)

    fetch_cmd = ["git", "fetch", "--all", "--prune"]
    reset_target = "HEAD"
    if args.use_origin and bname != "HEAD":
        reset_target = f"origin/{bname}"

    cmds = [
        fetch_cmd,
        ["git", "reset", "--hard", reset_target],
        ["git", "clean", "-fdx"],
    ]

    print(f"Repo root: {root}")
    for c in cmds:
        print(" ", " ".join(c))

    if args.dry_run:
        print("Dry run: no commands executed.")
        return 0

    fr = run(fetch_cmd, root)
    if fr.returncode != 0:
        print(fr.stderr or fr.stdout, file=sys.stderr)
        return 1

    rr = run(["git", "reset", "--hard", reset_target], root)
    if rr.returncode != 0:
        print(rr.stderr or rr.stdout, file=sys.stderr)
        return 1

    cr = run(["git", "clean", "-fdx"], root)
    if cr.returncode != 0:
        print(cr.stderr or cr.stdout, file=sys.stderr)
        return 1

    print("Nuke complete.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
