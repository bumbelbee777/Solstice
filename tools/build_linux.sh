#!/usr/bin/env bash
# Configure and build Solstice on Linux using CMake presets.
# Usage: bash tools/build_linux.sh [--release] [--clean] [--refresh-deps] [--cmake4-compat]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
# shellcheck source=solstice_unix_build_env.sh
source "$SCRIPT_DIR/solstice_unix_build_env.sh"

BUILD_MODE="debug"
CLEAN_BUILD="false"
REFRESH_DEPS="false"
FORCE_CMAKE4_COMPAT="false"

usage() {
    cat <<'EOF'
Usage: bash tools/build_linux.sh [options]

Options:
  --release   Build with linux-release preset.
  --clean     Remove preset build directory before configuring.
  --refresh-deps
              Remove cached CPM sources for reactphysics3d and SDL3
              before configuring.
  --cmake4-compat
              Force CMake policy compatibility flag used by some
              older dependency CMake files (for example zstd).
  -h, --help  Show this help message.
EOF
}

log() {
    printf '[build-linux] %s\n' "$1"
}

parse_args() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --release)
                BUILD_MODE="release"
                ;;
            --clean)
                CLEAN_BUILD="true"
                ;;
            --refresh-deps)
                REFRESH_DEPS="true"
                ;;
            --cmake4-compat)
                FORCE_CMAKE4_COMPAT="true"
                ;;
            -h|--help)
                usage
                exit 0
                ;;
            *)
                log "ERROR: unknown option: $1"
                usage
                exit 1
                ;;
        esac
        shift
    done
}

cmake_major_version() {
    local ver
    ver="$(cmake --version | awk 'NR==1 {print $3}')"
    echo "${ver%%.*}"
}

check_prereqs() {
    local required_tools=(cmake)
    for tool in "${required_tools[@]}"; do
        if ! command -v "$tool" >/dev/null 2>&1; then
            log "ERROR: $tool not found."
            log "Run: bash tools/setup_env_linux.sh"
            exit 1
        fi
    done
}

copy_tree_contents() {
    local src_dir="$1"
    local dst_dir="$2"

    mkdir -p "$dst_dir"

    if command -v rsync >/dev/null 2>&1; then
        rsync -a "$src_dir"/ "$dst_dir"/
    else
        cp -R "$src_dir"/. "$dst_dir"/
    fi
}

setup_runtime_layout() {
    local build_dir="$1"
    local bin_dir="$build_dir/bin"
    local assets_src="$PROJECT_ROOT/assets"
    local shaders_src="$PROJECT_ROOT/source/Shaders/bin"
    local assets_dst="$bin_dir/assets"
    local shaders_dst="$bin_dir/shaders"

    # Common runtime directories used by examples/tools.
    mkdir -p "$bin_dir" "$assets_dst" "$assets_dst/fonts" "$PROJECT_ROOT/cache"

    # Stage project assets (fonts, music, textures, etc.) if present.
    if [[ -d "$assets_src" ]]; then
        log "Staging assets from $assets_src to $assets_dst"
        copy_tree_contents "$assets_src" "$assets_dst"
    else
        log "No root assets directory found at $assets_src (skipping asset staging)"
    fi

    # Stage compiled shaders for runtime loading.
    if [[ -d "$shaders_src" ]]; then
        log "Staging shaders from $shaders_src to $shaders_dst"
        copy_tree_contents "$shaders_src" "$shaders_dst"
    else
        log "No compiled shaders found at $shaders_src (skipping shader staging)"
    fi
}

main() {
    parse_args "$@"
    check_prereqs

    solstice_export_build_caches
    solstice_set_default_parallelism

    local preset="linux-debug"
    if [[ "$BUILD_MODE" == "release" ]]; then
        preset="linux-release"
    fi

    local build_dir="$PROJECT_ROOT/out/build/$preset"
    local rp3d_cache="${CPM_SOURCE_CACHE}/reactphysics3d"
    local sdl3_cache="${CPM_SOURCE_CACHE}/sdl3"
    local -a configure_args=()
    local cmake_major
    log "Project root: $PROJECT_ROOT"
    log "Using preset: $preset"
    solstice_log_build_caches log
    if [[ -n "${CMAKE_BUILD_PARALLEL_LEVEL:-}" ]]; then
        log "CMAKE_BUILD_PARALLEL_LEVEL=$CMAKE_BUILD_PARALLEL_LEVEL"
    fi

    if [[ "$CLEAN_BUILD" == "true" ]]; then
        log "Cleaning build directory: $build_dir"
        rm -rf "$build_dir"
    fi

    if [[ "$REFRESH_DEPS" == "true" ]]; then
        log "Refreshing CPM dependency cache for reactphysics3d and SDL3 under $CPM_SOURCE_CACHE"
        rm -rf "$rp3d_cache" "$sdl3_cache"
    fi

    cmake_major="$(cmake_major_version)"
    if [[ "$FORCE_CMAKE4_COMPAT" == "true" || "$cmake_major" -ge 4 ]]; then
        # CMake 4 removed compatibility defaults some third-party CMake files rely on.
        # This keeps legacy dependency projects (such as zstd build/cmake) configurable.
        configure_args+=("-DCMAKE_POLICY_VERSION_MINIMUM=3.5")
        log "Enabling CMake compatibility flag: CMAKE_POLICY_VERSION_MINIMUM=3.5"
    fi

    cmake --preset "$preset" "${configure_args[@]}"
    cmake --build --preset "$preset"
    setup_runtime_layout "$build_dir"
    log "Build completed for preset: $preset"
}

main "$@"
