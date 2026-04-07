#!/usr/bin/env bash
# Install macOS build prerequisites for Solstice.
# Usage: bash tools/setup_env_macos.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

log() {
    printf '[setup-macos] %s\n' "$1"
}

tool_exists() {
    command -v "$1" >/dev/null 2>&1
}

ensure_homebrew() {
    if tool_exists brew; then
        return
    fi

    log "ERROR: Homebrew is required but not installed."
    log "Install Homebrew from: https://brew.sh/"
    exit 1
}

ensure_xcode_cli_tools() {
    if xcode-select -p >/dev/null 2>&1; then
        log "OK: Xcode Command Line Tools installed."
        return
    fi

    log "Installing Xcode Command Line Tools..."
    xcode-select --install || true
    log "Finish the GUI install, then re-run this script."
    exit 1
}

install_brew_packages() {
    local packages=(
        git
        cmake
        ninja
        ccache
        python
        llvm
    )

    log "Updating Homebrew metadata."
    brew update
    log "Installing packages: ${packages[*]}"
    brew install "${packages[@]}"
}

verify_tools() {
    local required_tools=(git cmake ninja python3)
    local missing=0

    for tool in "${required_tools[@]}"; do
        if tool_exists "$tool"; then
            log "OK: $tool found."
        else
            log "MISSING: $tool not found after install."
            missing=1
        fi
    done

    if tool_exists clang; then
        log "OK: clang found."
    else
        log "MISSING: clang not found."
        missing=1
    fi

    if tool_exists ccache; then
        log "OK: ccache found (speeds up repeat C/C++ builds)."
    else
        log "NOTE: ccache not found; repeat builds will be slower."
    fi

    if [[ "$missing" -ne 0 ]]; then
        log "ERROR: environment setup incomplete."
        exit 1
    fi
}

main() {
    log "Project root: $PROJECT_ROOT"
    ensure_homebrew
    ensure_xcode_cli_tools
    install_brew_packages
    verify_tools
    log "Environment setup complete."
    log "Tip: bash tools/build_macos.sh uses ~/.cache/solstice/cpm and ~/.cache/solstice/ccache by default (override with CPM_SOURCE_CACHE / CCACHE_DIR)."
    log "Next step: bash tools/build_macos.sh"
}

main "$@"
