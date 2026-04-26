#!/usr/bin/env python3
"""
Download FFmpeg + libav (shared) for development (include/, lib/) and the ffmpeg CLI.

Uses BtbN FFmpeg-Builds (*-gpl-shared) on Windows and Linux, where a stable
"latest" URL is published. On macOS there is no BtbN desktop build: this script
stages a symlink (or a full copy) from `brew --prefix ffmpeg` so
CMAKE_PREFIX_PATH works the same way.

GPL *-gpl-shared: redistribution can impose copyleft; use --lgpl with *-lgpl-shared
and LGPL-appropriate use if you ship to customers.

Usage:
  python tools/fetch_ffmpeg_libav.py [--dest DIR] [--url URL] [--expected-sha256 HEX]
  python tools/fetch_ffmpeg_libav.py --static-cli
      # Windows only: smaller static zip (ffmpeg CLI, not a dev prefix for linking)

Environment:
  SOLSTICE_FFMPEG_DEST         - default extract root (default: <repo>/tools/_ffmpeg)
  SOLSTICE_FFMPEG_ZIP_SHA256  - expected SHA-256 of the download when not passed on CLI
  SOLSTICE_FFMPEG_BTB_TRIPLET - override, e.g. linux64-gpl-shared (see BtbN "latest" assets)

On success, stderr includes "FFmpeg prefix (CMAKE): ..." and the last line of stdout
is the absolute path to the ffmpeg binary (or Homebrew ffmpeg for symlinked macOS).
"""

from __future__ import annotations

import argparse
import hashlib
import os
import platform
import shutil
import subprocess
import sys
import tarfile
import urllib.error
import urllib.request
import zipfile
from pathlib import Path

_BTBN_LATEST = "https://github.com/BtbN/FFmpeg-Builds/releases/download/latest"
_DEFAULT_WIN64_STATIC_ZIP = f"{_BTBN_LATEST}/ffmpeg-master-latest-win64-gpl.zip"


def _btbn_url_for_triplet(triplet: str) -> str:
    if triplet.startswith("win"):
        return f"{_BTBN_LATEST}/ffmpeg-master-latest-{triplet}.zip"
    return f"{_BTBN_LATEST}/ffmpeg-master-latest-{triplet}.tar.xz"


def _to_lgpl(triplet: str) -> str:
    if "-lgpl" in triplet:
        return triplet
    return triplet.replace("-gpl-", "-lgpl-").replace("-gpl", "-lgpl", 1)


def _triplet() -> str:
    env = (os.environ.get("SOLSTICE_FFMPEG_BTB_TRIPLET") or "").strip()
    if env:
        return env
    s = sys.platform
    m = platform.machine().lower()
    if s == "win32":
        if m in ("arm64", "aarch64") or os.environ.get("PROCESSOR_ARCHITECTURE", "").upper() == "ARM64":
            return "winarm64-gpl-shared"
        if m in ("i386", "i686", "x86"):
            raise SystemExit("fetch: BtbN does not auto-build win32/32-bit; set --url or SOLSTICE_FFMPEG_BTB_TRIPLET.")
        return "win64-gpl-shared"
    if s == "linux":
        if m in ("aarch64", "arm64"):
            return "linuxarm64-gpl-shared"
        if m in ("i386", "i686"):
            return "linux32-gpl-shared"
        if m in ("x86_64", "amd64", ""):
            return "linux64-gpl-shared"
        return "linux64-gpl-shared"
    if s == "darwin":
        return ""
    raise SystemExit(
        f"fetch: unsupported platform {s!r} / {m!r}. Set SOLSTICE_FFMPEG_BTB_TRIPLET or use --url."
    )


def _homebrew_ffmpeg_prefix() -> Path | None:
    brew = shutil.which("brew")
    if not brew:
        return None
    try:
        r = subprocess.run(
            [brew, "--prefix", "ffmpeg"],
            capture_output=True,
            text=True,
            check=False,
            timeout=30,
        )
    except (OSError, subprocess.TimeoutExpired):
        return None
    if r.returncode != 0 or not (r.stdout or "").strip():
        return None
    p = Path((r.stdout or "").strip().splitlines()[-1])
    h = p / "include" / "libavformat" / "avformat.h"
    ldir = p / "lib"
    if h.is_file() and ldir.is_dir():
        return p
    return None


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


def _is_libav_prefix(p: Path) -> bool:
    if not p.is_dir():
        return False
    return (p / "include" / "libavformat" / "avformat.h").is_file() and (p / "lib").is_dir()


def _inner_or_self(root: Path) -> Path:
    ch = [p for p in root.iterdir() if p.name not in (".", "..")]
    if len(ch) == 1 and ch[0].is_dir():
        return ch[0]
    return root


def _tar_extract(archive: Path, dest: Path) -> None:
    dest.mkdir(parents=True, exist_ok=True)
    with tarfile.open(archive, "r:*") as tf:
        if sys.version_info >= (3, 12):
            tf.extractall(dest, filter="data")
        else:
            tf.extractall(dest)


def _http_download(url: str, out: Path) -> None:
    out.parent.mkdir(parents=True, exist_ok=True)
    req = urllib.request.Request(
        url,
        headers={"User-Agent": "Solstice/fetch-ffmpeg-libav (open-source build helper)"},
    )
    with urllib.request.urlopen(req, timeout=600) as resp, out.open("wb") as w:
        shutil.copyfileobj(resp, w, length=256 * 1024)


def _macos_stage(install_root: Path, hermetic: bool) -> tuple[Path, Path]:
    src = _homebrew_ffmpeg_prefix()
    assert src is not None
    if install_root.exists() or install_root.is_symlink():
        if install_root.is_symlink():
            install_root.unlink()
        else:
            shutil.rmtree(install_root)
    install_root.parent.mkdir(parents=True, exist_ok=True)
    if hermetic:
        shutil.copytree(src, install_root, symlinks=True, dirs_exist_ok=True)
    else:
        try:
            install_root.symlink_to(src, target_is_directory=True)
        except OSError:
            shutil.copytree(src, install_root, symlinks=True, dirs_exist_ok=True)
    pfx = install_root
    if not _is_libav_prefix(pfx) and pfx.is_symlink():
        pfx = pfx.resolve()
    if not _is_libav_prefix(pfx):
        pfx = _inner_or_self(pfx) if pfx.is_dir() else pfx
    ff = pfx / "bin" / "ffmpeg"
    if not ff.is_file():
        ff = _find_ffmpeg_in_tree(install_root) or (src / "bin" / "ffmpeg")
    return (ff, install_root)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument(
        "--dest",
        type=Path,
        default=None,
        help="Extract / stage root (default: <repo>/tools/_ffmpeg or SOLSTICE_FFMPEG_DEST)",
    )
    ap.add_argument("--url", default="", help="Override download URL (omit for BtbN auto-triplet / Homebrew on macOS).")
    ap.add_argument(
        "--expected-sha256", default="", help="Optional SHA-256 of the download (for pinned CI)."
    )
    ap.add_argument("--skip-hash-check", action="store_true", help="Do not verify a pinned hash.")
    ap.add_argument(
        "--static-cli",
        action="store_true",
        help="Windows: fetch static win64 BtbN .zip (CLI only). Ignored on Linux; not used for linking.",
    )
    ap.add_argument(
        "--lgpl",
        action="store_true",
        help="With default BtbN triplet, use *-lgpl-shared instead of *-gpl-shared.",
    )
    ap.add_argument(
        "--hermetic-macos",
        action="store_true",
        help="macOS: copy the Homebrew prefix (large) instead of symlinking into --dest/libav",
    )
    args = ap.parse_args()

    repo_root = Path(__file__).resolve().parent.parent
    dest_root = args.dest
    if dest_root is None:
        dest_root = Path(os.environ.get("SOLSTICE_FFMPEG_DEST", str(repo_root / "tools" / "_ffmpeg")))
    dest_root = dest_root.resolve()
    dest_root.mkdir(parents=True, exist_ok=True)

    libav = dest_root / "libav"
    expected = (args.expected_sha256 or os.environ.get("SOLSTICE_FFMPEG_ZIP_SHA256", "")).strip()

    u_in = (args.url or "").strip()
    if sys.platform == "darwin" and not u_in and not args.static_cli:
        if _homebrew_ffmpeg_prefix() is not None:
            ff, pfx = _macos_stage(libav, hermetic=args.hermetic_macos)
            print(f"FFmpeg prefix (CMAKE): {pfx}", file=sys.stderr)
            if not ff.is_file():
                print("fetch: could not find bin/ffmpeg in Homebrew tree.", file=sys.stderr)
                return 3
            print(str(ff.resolve()))
            return 0
        print(
            "fetch: no Homebrew ffmpeg. Install: `brew install ffmpeg`, or pass a BtbN *-shared archive: --url ...",
            file=sys.stderr,
        )
        return 1

    u = u_in
    if args.static_cli:
        if sys.platform != "win32":
            print("fetch: --static-cli is only for Windows (static zip).", file=sys.stderr)
            return 1
        u = _DEFAULT_WIN64_STATIC_ZIP
    elif not u:
        t = _triplet()
        if not t:
            print("fetch: set --url, or SOLSTICE_FFMPEG_BTB_TRIPLET, or (macOS) install Homebrew ffmpeg.", file=sys.stderr)
            return 1
        if args.lgpl:
            t = _to_lgpl(t)
        u = _btbn_url_for_triplet(t)
    elif u == "default":
        t = (os.environ.get("SOLSTICE_FFMPEG_BTB_TRIPLET") or "").strip() or _triplet()
        if not t:
            print("fetch: could not resolve BtbN triplet for \"default\" URL.", file=sys.stderr)
            return 1
        if args.lgpl:
            t = _to_lgpl(t)
        u = _btbn_url_for_triplet(t)

    if not u or u == "default":
        print("fetch: could not determine URL (set --url or BtbN triplet / Homebrew).", file=sys.stderr)
        return 1

    return _download_and_extract(
        dest_root=dest_root,
        url=u,
        expect_sha=expected,
        skip_hash=args.skip_hash_check,
        static_cli_win=args.static_cli,
    )


def _download_and_extract(
    dest_root: Path,
    url: str,
    expect_sha: str,
    skip_hash: bool,
    static_cli_win: bool,
) -> int:
    extname = url.split("?")[0]
    pth = Path(extname)
    is_url = "://" in url or url.lower().startswith("http://") or url.lower().startswith("https://")
    dl: Path
    if not is_url:
        local = Path(url).expanduser()
        if not local.is_file():
            print(f"fetch: not a local file: {url}", file=sys.stderr)
            return 1
        el = local.name.lower()
        if el.endswith(".tar.xz") or el.endswith(".txz"):
            dl = dest_root / "_download_ffmpeg.tar.xz"
        elif local.suffix.lower() == ".zip":
            dl = dest_root / "_download_ffmpeg.zip"
        else:
            dl = dest_root / f"_download_ffmpeg{local.suffix or '.bin'}"
        print(f"Using local archive:\n  {local}", file=sys.stderr)
        try:
            shutil.copy2(local, dl)
        except OSError as e:
            print(f"Copy failed: {e}", file=sys.stderr)
            return 1
    else:
        if ".tar.xz" in extname or extname.lower().endswith(".txz"):
            dl = dest_root / "_download_ffmpeg.tar.xz"
        elif extname.lower().endswith(".zip"):
            dl = dest_root / "_download_ffmpeg.zip"
        else:
            dl = dest_root / f"_download_ffmpeg{pth.suffix or '.bin'}"

        print(f"Downloading:\n  {url}", file=sys.stderr)
        try:
            _http_download(url, dl)
        except urllib.error.HTTPError as e:
            print(
                f"HTTP {e.code} for {url}\n  Try a different --url or SOLSTICE_FFMPEG_BTB_TRIPLET "
                f"(BtbN 'latest' assets list: {_BTBN_LATEST}).",
                file=sys.stderr,
            )
            return 1
        except (urllib.error.URLError, OSError) as e:
            print(f"Download failed: {e}", file=sys.stderr)
            return 1

    if not skip_hash and expect_sha:
        got = _sha256_file(dl)
        if got.lower() != expect_sha.lower():
            print(f"SHA256 mismatch.\n  expected: {expect_sha}\n  actual:   {got}", file=sys.stderr)
            dl.unlink(missing_ok=True)
            return 2
    elif not skip_hash and not expect_sha:
        got = _sha256_file(dl)
        print(
            f"Download SHA256: {got}\n  Pin: --expected-sha256 {got}  or  SOLSTICE_FFMPEG_ZIP_SHA256",
            file=sys.stderr,
        )

    ex = dest_root / "_extract"
    if ex.exists():
        shutil.rmtree(ex)
    ex.mkdir(parents=True, exist_ok=True)
    if dl.suffix.lower() == ".zip":
        with zipfile.ZipFile(dl, "r") as zf:
            zf.extractall(ex)
    else:
        _tar_extract(dl, ex)

    inner = _inner_or_self(ex)
    if static_cli_win:
        out = dest_root / "ffmpeg"
    else:
        out = dest_root / "libav"
    if out.exists():
        shutil.rmtree(out)
    shutil.move(str(inner), str(out))
    shutil.rmtree(ex, ignore_errors=True)
    dl.unlink(missing_ok=True)

    if not _is_libav_prefix(out) and not static_cli_win:
        print(
            f"fetch: warning: {out} is missing include/libavformat (not a gpl-shared layout?).",
            file=sys.stderr,
        )

    ff = _find_ffmpeg_in_tree(out)
    if not ff:
        print("Could not find ffmpeg in extracted tree.", file=sys.stderr)
        return 3

    print(f"FFmpeg prefix (CMAKE): {out}", file=sys.stderr)
    print(str(ff.resolve()))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
