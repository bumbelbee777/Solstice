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
    log "Build completed for preset: $preset"
}

main "$@"
