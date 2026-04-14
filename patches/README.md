# Patches for vendored and CPM-fetched code

- **CPM packages:** listed `PATCHES` in `CMakeLists.txt` are applied when dependencies are fetched.
- **Vendored CMake glue** (e.g. `3rdparty/cmake/bimg/`): keep the tree aligned with upstream, record Solstice deltas as unified diffs here, and apply with `git apply -p1 patches/<name>.patch` after updating the file from upstream (or commit the post-patch result and keep the `.patch` as the audit trail).

## bimg-cmake-miniz-include-dirs.patch

**Path:** `3rdparty/cmake/bimg/3rdparty/miniz.cmake` (bgfx/bimg CMake integration bundled in this repo)

**Issue:** Newer bimg `tinyexr.h` uses `#include <miniz/miniz.h>`, which only resolves if the compile include root includes `3rdparty/tinyexr/deps` (not only `deps/miniz/`). Older snapshots used `#include <miniz.h>`.

**Fix:** Set `MINIZ_INCLUDE_DIR` to both `.../deps` and `.../deps/miniz`.

**Apply / refresh:**

```bash
git apply -p1 patches/bimg-cmake-miniz-include-dirs.patch
```

**Regenerating** after editing `miniz.cmake`:

```bash
git diff 3rdparty/cmake/bimg/3rdparty/miniz.cmake > patches/bimg-cmake-miniz-include-dirs.patch
```

(Use an unpatched upstream `miniz.cmake` as the left side if you want a clean forward-only patch.)

## bx-thread-msvc-setThreadName.patch

**Upstream:** bkaradzic/bx (master)

**Issue:** On MSVC, `thread.cpp` uses `__try`/`__except` inside `Thread::setThreadName()`, which also uses C++ objects (e.g. `StringView`). That triggers **C2712** (cannot use `__try` in functions that require object unwinding).

**Fix:** Move the RaiseException-based thread naming into a separate function `setThreadNameException(DWORD threadId, LPCSTR name)` that only uses POD types, so the compiler does not need to unwind C++ objects in the same function as the `__try` block.

**Regenerating:** Re-download `src/thread.cpp` from bkaradzic/bx, apply the same logic (add helper, replace inline block with call), then:

```bash
diff -u a/src/thread.cpp b/src/thread.cpp > patches/bx-thread-msvc-setThreadName.patch
```

Use paths relative to the bx repo root so `patch -p1` applies correctly.

## reactphysics3d-DefaultLogger-chrono.patch

**Upstream:** DanielChappuis/reactphysics3d (v0.9.0)

**Issue:** `DefaultLogger.h` uses `std::chrono::system_clock::now()` and `std::chrono::system_clock::to_time_t()` but does not include `<chrono>` (or `<ctime>` for `std::localtime`/`std::put_time`). On MSVC this causes **C3083** / **C2039** (symbol to the left of `::` must be a type; `now`/`to_time_t` not a member of `std::chrono`).

**Fix:** Add `#include <ctime>` and `#include <chrono>` after the existing standard includes so the logger compiles on all platforms.
