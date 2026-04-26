#!/usr/bin/env python3
"""Self-test for package_executables.py (no built utilities required). Run by CTest as package_executables_selftest."""

from __future__ import annotations

import subprocess
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SCRIPT = REPO / "tools" / "package_executables.py"
SHARPON_FST = REPO / "tools" / "packaging" / "Sharpon.fst"


def _py() -> str:
    return sys.executable


def _run(args: list[str], *, check: bool = True) -> subprocess.CompletedProcess[str]:
    r = subprocess.run(
        [_py(), str(SCRIPT)] + args,
        cwd=REPO,
        capture_output=True,
        text=True,
    )
    if check and r.returncode != 0:
        raise AssertionError(
            f"package_executables failed: {args!r}\nSTDOUT:\n{r.stdout}\nSTDERR:\n{r.stderr}"
        )
    return r


def _import_parse():
    sys.path.insert(0, str(REPO / "tools"))
    from package_executables import parse_fst  # type: ignore

    return parse_fst


def _write_minimal_fst(path: Path) -> None:
    # Cross-platform: one file row, current OS
    if sys.platform == "win32":
        src = "hello.exe"
    else:
        src = "hello"
    t = f"""# minimal
package_id = test.pkg
display_name = Hello
version = 1.0.0
maintainer = test
description = e2e

kind\tsrc\tdst\tos\tbase\toptional
file\t{src}\t\tall\troot\tno
"""
    path.write_text(t, encoding="utf-8")


class TestPackageExecutables(unittest.TestCase):
    def test_parse_sharpon_fst(self) -> None:
        parse_fst = _import_parse()
        doc = parse_fst(SHARPON_FST)
        self.assertIn("source_root", doc.directives)
        self.assertTrue(any("Sharpon" in r.src for r in doc.rows))

    def test_help_exits_zero(self) -> None:
        r = subprocess.run([_py(), str(SCRIPT), "-h"], cwd=REPO, capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn("fst", r.stdout)

    def test_end_to_end_zip(self) -> None:
        with tempfile.TemporaryDirectory() as tdir:
            tpath = Path(tdir)
            fst = tpath / "m.fst"
            if sys.platform == "win32":
                exe = "hello.exe"
            else:
                exe = "hello"
            root = tpath / "bin"
            root.mkdir()
            (root / exe).write_bytes(b"bin")

            _write_minimal_fst(fst)
            out = tpath / "out.zip"
            _run(
                [
                    "--fst",
                    str(fst),
                    "--root",
                    str(root),
                    "--format",
                    "zip",
                    "--out",
                    str(out),
                ],
            )
            self.assertTrue(out.is_file(), out)
            with zipfile.ZipFile(out) as zf:
                names = set(zf.namelist())
            self.assertIn(exe, names)

    def test_quiet_no_wrote_in_stdout(self) -> None:
        with tempfile.TemporaryDirectory() as tdir:
            tpath = Path(tdir)
            fst = tpath / "m.fst"
            if sys.platform == "win32":
                exe = "hello.exe"
            else:
                exe = "hello"
            root = tpath / "bin"
            root.mkdir()
            (root / exe).write_bytes(b"1")
            _write_minimal_fst(fst)
            r = subprocess.run(
                [
                    _py(),
                    str(SCRIPT),
                    "--fst",
                    str(fst),
                    "--root",
                    str(root),
                    "--format",
                    "zip",
                    "--out",
                    str(tpath / "q.zip"),
                    "--quiet",
                ],
                cwd=REPO,
                capture_output=True,
                text=True,
            )
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertNotIn("Wrote", r.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
