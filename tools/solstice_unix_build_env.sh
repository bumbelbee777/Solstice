#!/usr/bin/env bash
# Shared environment for Linux/macOS CMake builds (sourced by build_linux.sh / build_macos.sh).
# Not meant to be run directly.
#
# Honours existing CPM_SOURCE_CACHE / CCACHE_* if already set.
# Defaults keep caches under XDG-style dirs so they persist across clean builds and repo clones.

solstice_default_cache_root() {
    printf '%s' "${XDG_CACHE_HOME:-$HOME/.cache}/solstice"
}

# Export CPM_SOURCE_CACHE and CCACHE_DIR; create directories.
solstice_export_build_caches() {
    local root
    root="$(solstice_default_cache_root)"
    export CPM_SOURCE_CACHE="${CPM_SOURCE_CACHE:-$root/cpm}"
    export CCACHE_DIR="${CCACHE_DIR:-$root/ccache}"
    # Reasonable default; override with CCACHE_MAXSIZE in environment if needed.
    export CCACHE_MAXSIZE="${CCACHE_MAXSIZE:-4G}"
    mkdir -p "$CPM_SOURCE_CACHE" "$CCACHE_DIR"
}

# Default parallel compile jobs when CMAKE_BUILD_PARALLEL_LEVEL is unset (helps non-Ninja generators).
solstice_set_default_parallelism() {
    if [[ -n "${CMAKE_BUILD_PARALLEL_LEVEL:-}" ]]; then
        return
    fi
    if command -v nproc >/dev/null 2>&1; then
        export CMAKE_BUILD_PARALLEL_LEVEL="$(nproc)"
    elif command -v sysctl >/dev/null 2>&1 && sysctl -n hw.ncpu >/dev/null 2>&1; then
        export CMAKE_BUILD_PARALLEL_LEVEL="$(sysctl -n hw.ncpu)"
    fi
}

solstice_log_build_caches() {
    local log_fn="${1:-echo}"
    "$log_fn" "CPM_SOURCE_CACHE=$CPM_SOURCE_CACHE"
    "$log_fn" "CCACHE_DIR=$CCACHE_DIR"
    if command -v ccache >/dev/null 2>&1; then
        "$log_fn" "ccache: $(command -v ccache) ($(ccache -V 2>/dev/null | head -n1 || true))"
    else
        "$log_fn" "ccache: not in PATH (install via setup_env script for faster rebuilds)"
    fi
}
