#!/usr/bin/env python3
"""
Download a pinned FFmpeg CLI build for offline MovieMaker export.

Default (Windows x64): BtbN FFmpeg-Builds — GPL build. Redistribution of
GPL-linked ffmpeg may impose obligations; use an LGPL build if you ship binaries.

Usage:
  python tools/fetch_ffmpeg.py [--dest DIR] [--url URL] [--expected-sha256 HEX] [--skip-hash-check]

Environment:
  SOLSTICE_FFMPEG_ZIP_SHA256 — if set, used as expected SHA-256 when --expected-sha256 is omitted.

On success, prints the absolute path to ffmpeg(.exe) on stdout (last line).
"""

from __future__ import annotations

import argparse
import hashlib
import os
import shutil
import sys
import urllib.request
import zipfile
from pathlib import Path

# BtbN "latest" win64 GPL zip (updates periodically — update URL + hash together).
DEFAULT_WIN64_GPL_ZIP = (
    "https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-win64-gpl.zip"
)
# When empty, verification is skipped unless --expected-sha256 or env is set.
DEFAULT_EXPECTED_SHA256 = ""


def _sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def _find_ffmpeg_in_tree(root: Path) -> Path | None:
    for name in ("ffmpeg.exe", "ffmpeg"):
        for p in root.rglob(name):
            if p.is_file():
                return p
    return None


def main() -> int:
    ap = argparse.ArgumentParser(description="Fetch FFmpeg CLI into a local directory.")
    ap.add_argument(
        "--dest",
        type=Path,
        default=None,
        help="Extract root directory (default: <repo>/tools/_ffmpeg or SOLSTICE_FFMPEG_DEST)",
    )
    ap.add_argument("--url", default=DEFAULT_WIN64_GPL_ZIP, help="Zip URL to download (Windows build).")
    ap.add_argument("--expected-sha256", default="", help="Expected SHA-256 of the zip (hex).")
    ap.add_argument(
        "--skip-hash-check",
        action="store_true",
        help="Do not verify zip hash (not recommended for CI).",
    )
    args = ap.parse_args()

    if sys.platform != "win32":
        print(
            "fetch_ffmpeg.py: non-Windows — install ffmpeg via your package manager "
            "(e.g. apt install ffmpeg, brew install ffmpeg).",
            file=sys.stderr,
        )
        which = shutil.which("ffmpeg")
        if which:
            print(which)
            return 0
        return 1

    repo_root = Path(__file__).resolve().parent.parent
    dest_root = args.dest
    if dest_root is None:
        dest_root = Path(os.environ.get("SOLSTICE_FFMPEG_DEST", str(repo_root / "tools" / "_ffmpeg")))

    dest_root = dest_root.resolve()
    dest_root.mkdir(parents=True, exist_ok=True)

    expected = args.expected_sha256.strip() or os.environ.get("SOLSTICE_FFMPEG_ZIP_SHA256", "").strip()
    if not expected:
        expected = DEFAULT_EXPECTED_SHA256

    zip_path = dest_root / "_download_ffmpeg.zip"

    print(f"Downloading:\n  {args.url}", file=sys.stderr)
    urllib.request.urlretrieve(args.url, str(zip_path))

    if not args.skip_hash_check and expected:
        got = _sha256_file(zip_path)
        if got.lower() != expected.lower():
            print(
                f"SHA256 mismatch.\n  expected: {expected}\n  actual:   {got}\n"
                "Update DEFAULT_EXPECTED_SHA256 / --expected-sha256 or set SOLSTICE_FFMPEG_ZIP_SHA256.",
                file=sys.stderr,
            )
            zip_path.unlink(missing_ok=True)
            return 2
    elif not args.skip_hash_check and not expected:
        got = _sha256_file(zip_path)
        print(
            f"Downloaded zip SHA256 (pin this in the script or CI env): {got}",
            file=sys.stderr,
        )

    extract_dir = dest_root / "_extract"
    if extract_dir.exists():
        shutil.rmtree(extract_dir)
    extract_dir.mkdir(parents=True, exist_ok=True)

    with zipfile.ZipFile(zip_path, "r") as z:
        z.extractall(extract_dir)

    # Single top-level folder in BtbN zips
    children = [p for p in extract_dir.iterdir() if p.name != "."]
    inner = extract_dir
    if len(children) == 1 and children[0].is_dir():
        inner = children[0]

    # Move bin directory next to dest_root for stable layout: DEST/bin/ffmpeg.exe
    install_root = dest_root / "ffmpeg"
    if install_root.exists():
        shutil.rmtree(install_root)
    shutil.move(str(inner), str(install_root))

    shutil.rmtree(extract_dir, ignore_errors=True)
    zip_path.unlink(missing_ok=True)

    ff = _find_ffmpeg_in_tree(install_root)
    if not ff:
        print("Could not find ffmpeg executable after extract.", file=sys.stderr)
        return 3

    print(str(ff.resolve()))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
