# Patches for CPM-fetched dependencies

Applied at configure time when dependencies are fetched via CPM.

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
