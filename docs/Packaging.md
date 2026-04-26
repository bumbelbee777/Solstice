# Packaging executables (FST + `package_executables.py`)

This repo uses a small **.fst** (file structure table) format to list which files to copy from a build tree into a **staging layout**, then produce **`.zip` / `.tar.gz`**, optional **MSI (WiX)**, and optional **`.deb`**. The script lives at [`tools/package_executables.py`](../tools/package_executables.py); example manifests are under [`tools/packaging/`](../tools/packaging/).

**Utilities covered:** [Sharpon](Utilities.md#sharpon-moonwalk--json) ([`Sharpon.fst`](../tools/packaging/Sharpon.fst)), **Jackhammer** ([`LevelEditor.fst`](../tools/packaging/LevelEditor.fst) — target `LevelEditor`), and **SMM** ([`MovieMaker.fst`](../tools/packaging/MovieMaker.fst) — target `MovieMaker`).

**Redistribution note:** on Windows, CMake post-build already places **`SDL3`**, **`SolsticeEngine`**, compiled **`shaders/`**, and UI **`fonts/`** next to each utility in `${CMAKE_BINARY_DIR}/bin` (SMM may also copy **`ffmpeg.exe`** if `SOLSTICE_FFMPEG_EXECUTABLE` is set at configure). **LibUI** is linked **statically** into the tools — there is no `LibUI.dll` to ship.

## FST v1 format

- **Encoding:** UTF-8.
- **Lines:** empty lines and `#` comment lines are ignored.
- **Directives** (optional): `name = value` (whitespace around `=` allowed). Keys are case-insensitive and normalized to lower snake-case (`source_root`, `lib_root`, `package_id`, `display_name`, `version`, `maintainer`, `description`, `deb_depends`). Values can contain **`${VAR}`** placeholders, resolved from the process environment after **`${SOLSTICE_BUILD_DIR}`** is set from the **`--build-dir`** CLI option when provided.
- **Table:** a single header row, **tab-separated** (TSV), then one row per file rule.

### Table columns (tab-separated, in order)

| Column   | Values | Notes |
| -------- | ------ | ----- |
| `kind`   | `file`, `dir`, `glob` | `file` = one file; `dir` = copy directory tree; `glob` = glob for files (see `**` in pattern for recursive). |
| `src`    | path | Relative to the **base** for this row, unless absolute. Supports `${VAR}`. |
| `dst`    | path in package | If empty, `file` / `glob` use the **basename** of the source; `dir` uses the source directory’s name. |
| `os`     | `all`, `windows`, `linux`, `darwin` | Rows that do not match the host OS are skipped. |
| `base`   | `root` or `lib` | **`root`:** resolve `src` under the package **source root** (see below). **`lib`:** resolve under **lib root** (for Linux/macOS shared libraries not next to the exe). If `lib_root` is empty, `lib` falls back to the same as `root`. |
| `optional` | `yes` or `no` (default) | If `yes`, a missing `file` / empty `glob` / missing `dir` only **warns**; otherwise it is an error. |

**Source root resolution (CLI and defaults):** use **`--root`** to point at your CMake `bin` directory (this **wins** over a `source_root` line in the FST so a bad or unset `${SOLSTICE_BUILD_DIR}` does not break explicit paths), or set **`--build-dir`** and rely on the default `bin` under that tree, or set the environment variable **`SOLSTICE_BUILD_DIR`**, or set **`source_root = ${SOLSTICE_BUILD_DIR}/bin`** in the FST. **Lib root** for `base=lib` rows: directive **`lib_root`**, or **`--lib-root`**, or **`SOLSTICE_PACKAGE_LIB_DIR`**.

**Linux / macOS:** the utilities do not always copy `.so` / `.dylib` next to the executable the way Windows copies DLLs. Put those libraries in a directory and point **`lib_root`** at it, or add matching **`glob`** rows under `base=lib` once the files are present.

## `package_executables.py` usage

```text
python tools/package_executables.py --fst tools/packaging/Sharpon.fst --build-dir <path-to-cmake-build> --format zip
```

| Flag | Meaning |
| ---- | -------- |
| `--fst` | Path to a `.fst` file (required). |
| `--root` | Override the staging **source** directory (e.g. `<build>/bin`). |
| `--lib-root` | Directory used for `base=lib` rows. |
| `--build-dir` | CMake build tree; sets `${SOLSTICE_BUILD_DIR}` for expansion. |
| `--out` | Output file (`.zip`, `.tar.gz`, `.msi`, `.deb`) or, with `--format none`, a **directory** to copy the staged tree into. |
| `--format` | `zip` (default), `tar` / `tar.gz`, `msi`, `deb`, or `none` (stage only / copy to `--out`). |
| `--dry-run` | Do not create archives or copy files; still validates required inputs and prints warnings for optional ones. |
| `--wix-upgrade-code` | Stable **GUID** for WiX `MajorUpgrade` (use one per product). |
| `--deb-depends` | Overrides the **`deb_depends`** directive for `.deb` `Depends:`. |
| `--quiet` | Suppress success lines and (when not `--dry-run`) optional warnings; useful with CMake. |

## CMake: automatic zips after each utility build

Configure with **`SOLSTICE_PACKAGE_UTILITIES_POSTBUILD=ON`**. On each link of **Sharpon**, **LevelEditor**, or **MovieMaker**, CMake runs the packager (after DLL/shader/font post-build steps) and writes, for the active configuration (e.g. `Debug` / `Release` with Visual Studio, or the single `CMAKE_BUILD_TYPE` with single-config generators):

- `${CMAKE_BINARY_DIR}/packages/Sharpon-<Config>.zip`
- `${CMAKE_BINARY_DIR}/packages/LevelEditor-<Config>.zip`
- `${CMAKE_BINARY_DIR}/packages/MovieMaker-<Config>.zip`

The implementation is [`cmake/PackageUtilities.cmake`](../cmake/PackageUtilities.cmake) (`solstice_utility_postbuild_package`). **Default is OFF** so normal edit–build cycles are not slowed by Python zipping on every link.

**CTest:** the **`package_executables_selftest`** test (`-L quick` and `-L tools`) runs [`tools/test_package_executables.py`](../tools/test_package_executables.py) — it loads the Sharpon FST, runs a small end-to-end zip, and needs no full engine build.

## Output formats and external tools

- **ZIP / tar.gz:** Python stdlib only; no extra tools.
- **MSI:** requires **WiX Toolset v3** on `PATH` (`candle` and `light`), or set the **`WIX`** environment variable to the WiX install root so that `%WIX%\bin` can be used. The script emits a small `.wxs` and runs `candle` + `light`. If WiX is missing, you can still use **`--format zip`** and wrap the archive with your own signing or setup pipeline.
- **`.deb`:** requires **`dpkg-deb`** (typical on Debian/Ubuntu). The script arranges files under `/opt/solstice/<short-name>/` and adds a **`/usr/bin/<short-name>`** wrapper that `exec`s the main binary, where **short name** is derived from `package_id` (last dot-separated segment, e.g. `com.solstice.sharpon` → `sharpon`). Prefers `fakeroot` if available. Adjust **`deb_depends`** to match your actual runtime (SDL3 system packages vs bundled `.so`).

**Not in scope for v1:** code signing, notarization, RPM, AppImage (future).

## See also

- [Utilities.md](Utilities.md) — build flags and what each tool loads at runtime.
- [Jackhammer.md](Jackhammer.md), [SMM.md](SMM.md) — product names vs CMake target names.
