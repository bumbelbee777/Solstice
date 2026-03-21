#!/usr/bin/env bash
# Configure and build Solstice on macOS using CMake presets.
# Usage: bash tools/build_macos.sh [--release] [--clean]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

BUILD_MODE="debug"
CLEAN_BUILD="false"

usage() {
    cat <<'EOF'
Usage: bash tools/build_macos.sh [options]

Options:
  --release   Build with macos-release preset.
  --clean     Remove preset build directory before configuring.
  -h, --help  Show this help message.
EOF
}

log() {
    printf '[build-macos] %s\n' "$1"
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

check_prereqs() {
    local required_tools=(cmake)
    for tool in "${required_tools[@]}"; do
        if ! command -v "$tool" >/dev/null 2>&1; then
            log "ERROR: $tool not found."
            log "Run: bash tools/setup_env_macos.sh"
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

    local preset="macos-debug"
    if [[ "$BUILD_MODE" == "release" ]]; then
        preset="macos-release"
    fi

    local build_dir="$PROJECT_ROOT/out/build/$preset"
    log "Project root: $PROJECT_ROOT"
    log "Using preset: $preset"

    if [[ "$CLEAN_BUILD" == "true" ]]; then
        log "Cleaning build directory: $build_dir"
        rm -rf "$build_dir"
    fi

    cmake --preset "$preset"
    cmake --build --preset "$preset"
    setup_runtime_layout "$build_dir"
    log "Build completed for preset: $preset"
}

main "$@"
