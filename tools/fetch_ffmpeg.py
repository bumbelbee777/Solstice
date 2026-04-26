#!/usr/bin/env python3
"""Compatibility entry point for ``tools/fetch_ffmpeg_libav.py`` (same CLI)."""

from __future__ import annotations

import runpy
from pathlib import Path

if __name__ == "__main__":
    runpy.run_path(
        str(Path(__file__).resolve().parent / "fetch_ffmpeg_libav.py"),
        run_name="__main__",
    )
