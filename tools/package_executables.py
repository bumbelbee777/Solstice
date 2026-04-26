#!/usr/bin/env python3
"""
Stage and package Solstice executables from an .fst (file structure table) manifest.

See docs/Packaging.md for the format. Supports zip / tar, optional WiX MSI, optional .deb.
"""

from __future__ import annotations

import argparse
import glob as globmod
import os
import re
import shutil
import platform
import subprocess
import sys
import tarfile
import tempfile
import zipfile
from dataclasses import dataclass, field
from pathlib import Path
# ---------------------------------------------------------------------------
# Data model
# ---------------------------------------------------------------------------


@dataclass
class FstRow:
    kind: str
    src: str
    dst: str
    os: str
    base: str
    optional: bool
    line_no: int


@dataclass
class FstDocument:
    path: Path
    directives: dict[str, str] = field(default_factory=dict)
    rows: list[FstRow] = field(default_factory=list)


# ---------------------------------------------------------------------------
# Path / env expansion
# ---------------------------------------------------------------------------

_VAR = re.compile(r"\$\{([A-Za-z_][A-Za-z0-9_]*)\}")


def expand_vars(s: str, extra: dict[str, str]) -> str:
    def repl(m: re.Match) -> str:
        k = m.group(1)
        if k in extra:
            return extra[k]
        return os.environ.get(k, "")

    return _VAR.sub(repl, s)


def current_os() -> str:
    if sys.platform == "win32":
        return "windows"
    if sys.platform == "darwin":
        return "darwin"
    return "linux"


def os_matches(row_os: str) -> bool:
    row_os = row_os.lower().strip()
    if row_os in ("", "all"):
        return True
    return row_os == current_os()


# ---------------------------------------------------------------------------
# FST parse
# ---------------------------------------------------------------------------


def parse_fst(path: Path) -> FstDocument:
    text = path.read_text(encoding="utf-8")
    doc = FstDocument(path=path)
    in_table = False

    for i, line in enumerate(text.splitlines(), start=1):
        raw = line.rstrip()
        s = raw.strip()
        if not s or s.startswith("#"):
            continue
        if not in_table and "=" in s and "\t" not in s and not s.lower().startswith("kind"):
            k, _, v = s.partition("=")
            k = k.strip().lower().replace(" ", "_")
            v = v.strip()
            doc.directives[k] = v
            continue
        if not in_table and "\t" in raw:
            parts = [p.strip().lower() for p in raw.split("\t")]
            if parts and parts[0] == "kind":
                in_table = True
            continue
        if not in_table:
            continue
        if "\t" not in raw:
            continue
        row_parts = raw.split("\t")
        while len(row_parts) < 6:
            row_parts.append("")

        kind = row_parts[0].strip().lower()
        src = row_parts[1].strip()
        dst = row_parts[2].strip()
        row_os = row_parts[3].strip() or "all"
        b = (row_parts[4].strip().lower() or "root")
        opt_s = (row_parts[5].strip().lower() or "no")
        optional = opt_s in ("yes", "1", "true", "y", "optional")

        if not kind or not src:
            continue
        if kind not in ("file", "dir", "glob"):
            print(f"Warning: {path}:{i}: unknown kind {kind!r}, skipping", file=sys.stderr)
            continue
        if b not in ("root", "lib"):
            print(f"Warning: {path}:{i}: base {b!r} -> root", file=sys.stderr)
            b = "root"

        doc.rows.append(
            FstRow(
                kind=kind,
                src=src,
                dst=dst,
                os=row_os,
                base=b,
                optional=optional,
                line_no=i,
            )
        )
    return doc


def _resolve_base(
    base: str,
    root: Path,
    lib_root: Path | None,
) -> Path:
    if base == "lib":
        if lib_root and str(lib_root).strip():
            return lib_root
    return root


def _dst_for_file(src: Path, dst: str) -> str:
    if dst:
        return dst
    return src.name


def _dst_dir_path(dst: str, src_name: str) -> str:
    if dst:
        return dst.rstrip("/")
    return src_name


# ---------------------------------------------------------------------------
# Stage
# ---------------------------------------------------------------------------


def _collect_expand_env(doc: FstDocument, build_dir: str | None) -> dict[str, str]:
    ex: dict[str, str] = {k.upper(): v for k, v in doc.directives.items()}
    for k, v in os.environ.items():
        ex.setdefault(k, v)
    if build_dir:
        ex["SOLSTICE_BUILD_DIR"] = str(Path(build_dir).resolve())
    for k, v in list(ex.items()):
        if isinstance(v, str) and "${" in v:
            ex[k] = expand_vars(v, ex)
    for k, v in list(ex.items()):
        ex[k] = expand_vars(v, ex)
    return ex


def materialize(
    doc: FstDocument,
    root: Path,
    lib_root: Path | None,
    out_root: Path,
    build_dir: str | None,
    dry_run: bool,
    apply_fst_source_root: bool = True,
) -> tuple[list[str], list[str], str | None]:
    ex = _collect_expand_env(doc, build_dir)

    if apply_fst_source_root and "source_root" in doc.directives and doc.directives["source_root"]:
        p = expand_vars(doc.directives["source_root"], ex)
        if p:
            root = Path(p).resolve()
    if "lib_root" in doc.directives and doc.directives.get("lib_root", "").strip():
        lp = expand_vars(doc.directives["lib_root"], ex)
        if lp:
            lib_root = Path(lp).resolve()
    if lib_root is not None and not str(lib_root).strip():
        lib_root = None

    warnings: list[str] = []
    errors: list[str] = []
    main_exe: str | None = None

    for row in doc.rows:
        if not os_matches(row.os):
            continue
        base_path = _resolve_base(row.base, root, lib_root)
        ex_row = {**ex, **{k: v for k, v in ex.items() if k == "SOLSTICE_BUILD_DIR"}}

        if row.kind == "file":
            src_e = expand_vars(row.src, ex_row)
            if os.path.isabs(src_e) or (len(src_e) > 1 and src_e[1] == ":"):
                sp = Path(src_e)
            else:
                sp = (base_path / src_e).resolve()
            if not sp.exists() or not sp.is_file():
                if row.optional:
                    warnings.append(f"optional missing: {row.src!r} (line {row.line_no})")
                    continue
                errors.append(f"missing file: {sp} (line {row.line_no})")
                continue
            rel_dst = _dst_for_file(sp, row.dst)
            target = (out_root / rel_dst).resolve()
            if not str(target).startswith(str(out_root.resolve())):
                errors.append(f"dst escapes staging root: {rel_dst!r} (line {row.line_no})")
                continue
            if main_exe is None:
                main_exe = rel_dst
            if not dry_run:
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(sp, target)
            continue

        if row.kind == "dir":
            src_e = expand_vars(row.src, ex_row)
            sd = (base_path / src_e) if not os.path.isabs(src_e) else Path(src_e)
            sd = sd.resolve()
            if not sd.is_dir():
                if row.optional:
                    warnings.append(f"optional missing directory: {row.src!r} (line {row.line_no})")
                    continue
                errors.append(f"missing directory: {sd} (line {row.line_no})")
                continue
            dname = _dst_dir_path(row.dst, sd.name)
            target = (out_root / dname).resolve()
            if not str(target).startswith(str(out_root.resolve())):
                errors.append(f"dst escapes staging root: {dname!r} (line {row.line_no})")
                continue
            if not dry_run:
                shutil.copytree(sd, target, dirs_exist_ok=True)
            continue

        if row.kind == "glob":
            pat = expand_vars(row.src, ex_row)
            gpat = str((base_path / pat).as_posix()) if not os.path.isabs(pat) else pat
            gpat = expand_vars(gpat, ex_row) if "{" in gpat or "${" in gpat else gpat
            rec = "**" in pat
            found = [Path(p) for p in sorted(globmod.glob(gpat, recursive=rec)) if Path(p).is_file()]
            if not found:
                if row.optional:
                    warnings.append(f"optional glob empty: {gpat!r} (line {row.line_no})")
                    continue
                errors.append(f"glob match empty: {gpat!r} (line {row.line_no})")
                continue
            for f in found:
                rel_dst = _dst_for_file(f, row.dst) if row.dst else f.name
                target = (out_root / rel_dst).resolve()
                if not str(target).startswith(str(out_root.resolve())):
                    errors.append(f"dst escapes staging root: {rel_dst!r} (line {row.line_no})")
                    continue
                if not dry_run:
                    target.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(f, target)
            continue

    mname = str(Path(main_exe).name) if main_exe else None
    return warnings, errors, mname


# ---------------------------------------------------------------------------
# zip / tar
# ---------------------------------------------------------------------------


def write_zip(staged: Path, out: Path) -> None:
    out.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as zf:
        for f in sorted(staged.rglob("*")):
            if f.is_file():
                zf.write(f, arcname=f.relative_to(staged).as_posix())


def write_tar_gz(staged: Path, out: Path) -> None:
    out.parent.mkdir(parents=True, exist_ok=True)
    with tarfile.open(out, "w:gz") as tf:
        for f in sorted(staged.rglob("*")):
            if f.is_file():
                tf.add(f, arcname=f.relative_to(staged).as_posix())


# ---------------------------------------------------------------------------
# WiX 3: candle + light
# ---------------------------------------------------------------------------


def _xml_escape(s: str) -> str:
    return (
        s.replace("&", "&amp;")
        .replace("<", "&lt;")
        .replace(">", "&gt;")
        .replace('"', "&quot;")
    )


def build_msi(
    staged: Path,
    out_msi: Path,
    app_name: str,
    version: str,
    manufacturer: str,
    upgrade_code: str,
) -> None:
    wix = os.environ.get("WIX", "")
    if wix and (Path(wix) / "bin" / "candle.exe").exists():
        os.environ["PATH"] = str(Path(wix) / "bin") + os.pathsep + os.environ.get("PATH", "")

    candle = shutil.which("candle") or shutil.which("candle.exe")
    light = shutil.which("light") or shutil.which("light.exe")
    if not candle or not light:
        raise RuntimeError("WiX (candle, light) not on PATH. Install WiX 3+ or set WIX to the install root.")

    v = version if re.match(r"^\d+(\.\d+){0,3}$", version) else "1.0.0.0"
    files: list[Path] = sorted([f for f in staged.rglob("*") if f.is_file()])
    if not files:
        raise ValueError("No staged files; cannot build MSI")

    def safe_arc_name(f: Path) -> str:
        s = f.relative_to(staged).as_posix().replace("/", "_").replace(" ", "_")
        if len(s) > 180:
            s = s[:160] + "_" + f"{f.stat().st_mtime_ns:x}"
        return s

    comp_xml: list[str] = []
    feat_refs: list[str] = []
    for i, f in enumerate(files):
        ssrc = str(f).replace("\\", "\\\\")
        sname = safe_arc_name(f)
        cid = f"C{i}"
        comp_xml.append(
            f'         <Component Id="{cid}" Guid="*">'
            f' <File Id="F{i}" KeyPath="yes" Name="{_xml_escape(sname)}" Source="{ssrc}"/> </Component>'
        )
        feat_refs.append(f'    <ComponentRef Id="{cid}"/>')

    wxs = f"""<?xml version="1.0" encoding="UTF-8"?>
<Wix xmlns="http://schemas.microsoft.com/wix/2006/wi">
  <Product
      Id="*"
      Name="{_xml_escape(app_name)}"
      Language="1033"
      Version="{_xml_escape(v)}"
      Manufacturer="{_xml_escape(manufacturer)}"
      UpgradeCode="{upgrade_code.upper()}">
    <Package InstallerVersion="200" Compressed="yes" InstallScope="perMachine" />
    <MajorUpgrade DowngradeErrorMessage="A newer version is already installed." />
    <MediaTemplate />
    <Feature Id="FMain" Level="1" Title="{_xml_escape(app_name)}">
{chr(10).join(feat_refs)}
    </Feature>
  </Product>
  <Fragment>
    <Directory Id="TARGETDIR" Name="SourceDir">
      <Directory Id="ProgramFiles64Folder">
        <Directory Id="INSTALLFOLDER" Name="{_xml_escape(app_name)}">
{chr(10).join(comp_xml)}
        </Directory>
      </Directory>
    </Directory>
  </Fragment>
</Wix>
"""
    wr = tempfile.mkdtemp(prefix="solstice_wix_")
    try:
        wxs_p = Path(wr) / "package.wxs"
        obj_p = Path(wr) / "package.wixobj"
        wxs_p.write_text(wxs, encoding="utf-8")
        for cmd in (
            [candle, str(wxs_p), "-o", str(obj_p)],
            [light, str(obj_p), "-o", str(out_msi), "-sval"],
        ):
            p = subprocess.run(cmd, capture_output=True, text=True)
            if p.returncode != 0:
                raise RuntimeError(
                    f"WiX step failed: {' '.join(cmd)}\n{p.stdout or ''}{p.stderr or ''}"
                )
    finally:
        shutil.rmtree(wr, ignore_errors=True)


# ---------------------------------------------------------------------------
# .deb: opt layout + /usr/bin wrapper
# ---------------------------------------------------------------------------


def _which_or_none(name: str) -> str | None:
    return shutil.which(name)


def _deb_architecture() -> str:
    dpkg = shutil.which("dpkg")
    if dpkg:
        try:
            a = subprocess.check_output([dpkg, "--print-architecture"], text=True, stderr=subprocess.DEVNULL)
            return a.strip() or "all"
        except (subprocess.CalledProcessError, OSError):
            pass
    m = platform.machine().lower()
    return {"x86_64": "amd64", "amd64": "amd64", "aarch64": "arm64", "arm64": "arm64"}.get(m, m or "all")


def _deb_short_name(pkg_id: str) -> str:
    s = pkg_id.lower().strip()
    if "." in s and not s.replace(".", "").isdigit():
        s = s.rsplit(".", 1)[-1]
    s = re.sub(r"[^a-z0-9+.-]+", "-", s).strip("-.")
    return s or "solstice"


def build_deb(
    staged: Path,
    out_deb: Path,
    pkg_name: str,
    version: str,
    maintainer: str,
    description: str,
    main_binary: str | None,
    depends: str,
) -> None:
    dpkg = _which_or_none("dpkg-deb")
    if not dpkg:
        raise RuntimeError("dpkg-deb not found. Install dpkg (Debian/Ubuntu) or use --format zip.")

    v = re.sub(r"[^0-9a-zA-Z.+~:-]", "-", version) or "0.0.0"
    safe = _deb_short_name(pkg_name)
    if main_binary and not (staged / main_binary).is_file() and (staged / Path(main_binary).name).is_file():
        main_binary = Path(main_binary).name
    mbin = main_binary
    if not mbin or not (staged / mbin).is_file():
        for f in staged.iterdir():
            if f.is_file() and os.access(f, os.X_OK):
                mbin = f.name
                break

    with tempfile.TemporaryDirectory(prefix="solstice_deb_") as wr:
        w = Path(wr)
        opt_dir = w / "opt" / f"solstice/{safe}"
        opt_dir.parent.mkdir(parents=True, exist_ok=True)
        shutil.copytree(staged, opt_dir, dirs_exist_ok=True)
        bindir = w / "usr" / "bin"
        bindir.mkdir(parents=True, exist_ok=True)
        if mbin and (opt_dir / mbin).is_file():
            wrapper = bindir / safe
            wr_text = f'#!/bin/sh\nexec "/opt/solstice/{safe}/{mbin}" "$@"\n'
            wrapper.write_text(wr_text, encoding="utf-8")
            os.chmod(wrapper, 0o755)
        ddir = w / "DEBIAN"
        ddir.mkdir(parents=True, exist_ok=True)
        ctrl = (
            f"Package: {safe}\n"
            f"Version: {v}\n"
            f"Section: devel\n"
            f"Priority: optional\n"
            f"Architecture: {_deb_architecture()}\n"
            f"Maintainer: {maintainer}\n"
            f"Depends: {depends}\n"
            f"Description: {description}\n"
        )
        (ddir / "control").write_text(ctrl, encoding="utf-8")
        if shutil.which("fakeroot"):
            subprocess.run(["fakeroot", dpkg, "-b", str(w), str(out_deb)], check=True)
        else:
            subprocess.run([dpkg, "-b", str(w), str(out_deb)], check=True)


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------


def _default_out(fmt: str, app_name: str) -> str:
    ext = {"zip": ".zip", "tar": ".tar.gz", "tar.gz": ".tar.gz", "msi": ".msi", "deb": ".deb"}.get(
        fmt, ".zip"
    )
    if fmt in ("tar", "tar.gz") and not ext:
        return f"{app_name}{ext}"
    return f"{app_name}{ext}"


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__,
    )
    ap.add_argument("--fst", type=Path, required=True, help="Path to .fst manifest")
    ap.add_argument(
        "--root",
        type=Path,
        help="Staged source root (default: <build-dir>/bin or FST source_root or SOLSTICE_BUILD_DIR/bin)",
    )
    ap.add_argument(
        "--lib-root",
        type=Path,
        help="Extra search root for base=lib rows (Linux .so, etc.); or set SOLSTICE_PACKAGE_LIB_DIR",
    )
    ap.add_argument(
        "--build-dir",
        type=str,
        help="CMake build directory; sets ${SOLSTICE_BUILD_DIR} in the FST",
    )
    ap.add_argument(
        "--out",
        type=Path,
        help="Output file (.zip, .tar.gz, .msi, .deb) or directory for staging-only",
    )
    ap.add_argument(
        "--format",
        choices=["zip", "tar", "tar.gz", "msi", "deb", "none"],
        default="zip",
        help="Output format (default zip). 'none' = stage only",
    )
    ap.add_argument("--dry-run", action="store_true", help="List actions only; do not copy or build installers")
    ap.add_argument(
        "--wix-upgrade-code",
        type=str,
        help="Fixed GUID for WiX MajorUpgrade (set once per app and keep stable)",
    )
    ap.add_argument(
        "--deb-depends",
        type=str,
        default="",
        help="Override Depends: line for .deb (default from FST deb_depends or sensible libc)",
    )
    ap.add_argument(
        "--quiet",
        action="store_true",
        help="Suppress non-error output (e.g. CMake post-build packages)",
    )

    args = ap.parse_args()
    doc = parse_fst(args.fst)
    ex = _collect_expand_env(doc, args.build_dir)

    root = args.root
    if root is None:
        s = ex.get("SOURCE_ROOT")
        if not s and doc.directives.get("source_root"):
            s = expand_vars(doc.directives["source_root"], ex)
        if s:
            root = Path(s).resolve()
    if root is None and args.build_dir:
        root = (Path(args.build_dir) / "bin").resolve()
    if root is None and os.environ.get("SOLSTICE_BUILD_DIR"):
        root = Path(os.environ["SOLSTICE_BUILD_DIR"]) / "bin"
    if root is None:
        ap.error("Set --root, --build-dir, or FST source_root / SOLSTICE_BUILD_DIR")

    lib_root = args.lib_root
    if lib_root is None and doc.directives.get("lib_root"):
        lv = expand_vars(doc.directives["lib_root"], ex)
        if lv.strip():
            lib_root = Path(lv).resolve()
    if lib_root is None and os.environ.get("SOLSTICE_PACKAGE_LIB_DIR"):
        lib_root = Path(os.environ["SOLSTICE_PACKAGE_LIB_DIR"]).resolve()
    if lib_root and not str(lib_root).strip():
        lib_root = None

    with tempfile.TemporaryDirectory(prefix="solstice_stage_") as tdir:
        staged = Path(tdir) / "stage"
        if not args.dry_run:
            staged.mkdir(parents=True, exist_ok=True)
        w, e, mexe = materialize(
            doc,
            root,
            lib_root,
            staged,
            args.build_dir,
            args.dry_run,
            apply_fst_source_root=(args.root is None),
        )
        for wn in w:
            if not args.quiet or args.dry_run:
                print(wn, file=sys.stderr)
        for er in e:
            print(f"Error: {er}", file=sys.stderr)
        if e:
            return 1
        if args.dry_run:
            if not args.quiet:
                print("Dry run completed (no files written).")
            return 0
        if args.format == "none":
            if args.out:
                shutil.copytree(staged, args.out, dirs_exist_ok=True)
            elif not args.quiet:
                print("Staged in temp (use --out with format none to copy).")
            return 0

        app = doc.directives.get("display_name", doc.path.stem)
        version = doc.directives.get("version", "0.0.0")
        maint = doc.directives.get("maintainer", "Unknown")
        desc = doc.directives.get("description", app)
        pkg_id = doc.directives.get("package_id", f"com.solstice.{app.lower()}")

        out = args.out
        if out is None:
            out = Path(_default_out(args.format, re.sub(r"[^A-Za-z0-9._-]+", "-", app)))

        out_written: Path
        if args.format == "zip":
            write_zip(staged, out)
            out_written = out
        elif args.format in ("tar", "tar.gz"):
            p = out
            if not str(p).endswith((".tar.gz", ".tgz", ".tar")):
                p = p.with_name(p.name + ".tar.gz")
            write_tar_gz(staged, p)
            out_written = p
        elif args.format == "msi":
            ucode = args.wix_upgrade_code or "12345678-ABCD-EF01-2345-6789ABCDEF00"
            build_msi(staged, out, app, version, maint, ucode)
            out_written = out
        elif args.format == "deb":
            dep = args.deb_depends
            if not dep:
                dep = doc.directives.get("deb_depends", "libc6, libstdc++6")
            build_deb(
                staged,
                out,
                pkg_id,
                version,
                maint,
                desc,
                mexe,
                dep,
            )
            out_written = out
        else:
            ap.error(f"Unknown format: {args.format}")

    if not args.quiet:
        print(f"Wrote {out_written.resolve()}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
