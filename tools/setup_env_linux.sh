#!/usr/bin/env bash
# Install Linux build prerequisites for Solstice.
# Usage: bash tools/setup_env_linux.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

log() {
    printf '[setup-linux] %s\n' "$1"
}

require_sudo() {
    if [[ "${EUID:-$(id -u)}" -ne 0 ]] && ! command -v sudo >/dev/null 2>&1; then
        log "ERROR: sudo is required when not running as root."
        exit 1
    fi
}

run_as_root() {
    if [[ "${EUID:-$(id -u)}" -eq 0 ]]; then
        "$@"
    else
        sudo "$@"
    fi
}

detect_package_manager() {
    if command -v apt-get >/dev/null 2>&1; then
        echo "apt"
        return
    fi

    if command -v dnf >/dev/null 2>&1; then
        echo "dnf"
        return
    fi

    if command -v pacman >/dev/null 2>&1; then
        echo "pacman"
        return
    fi

    echo "unsupported"
}

tool_exists() {
    command -v "$1" >/dev/null 2>&1
}

install_with_apt() {
    local packages=(
        git
        cmake
        ninja-build
        python3
        python3-pip
        build-essential
        clang
        pkg-config
        # SDL3 desktop dependencies (X11/Wayland and common runtime backends)
        libx11-dev
        libxext-dev
        libxrandr-dev
        libxfixes-dev
        libxcursor-dev
        libxi-dev
        libxss-dev
        libxinerama-dev
        libxxf86vm-dev
        libxtst-dev
        libwayland-dev
        wayland-protocols
        libxkbcommon-dev
        libdrm-dev
        libgbm-dev
        libegl1-mesa-dev
        libgl1-mesa-dev
        libasound2-dev
        libpulse-dev
        libjack-jackd2-dev
        libsndio-dev
        libsamplerate0-dev
        libudev-dev
        libdbus-1-dev
        libibus-1.0-dev
        libdecor-0-dev
    )

    log "Using apt-get package manager."
    run_as_root apt-get update
    run_as_root apt-get install -y "${packages[@]}"
}

install_with_dnf() {
    local packages=(
        git
        cmake
        ninja-build
        python3
        python3-pip
        gcc
        gcc-c++
        make
        clang
        pkgconf-pkg-config
        # SDL3 desktop dependencies (X11/Wayland and common runtime backends)
        libX11-devel
        libXext-devel
        libXrandr-devel
        libXfixes-devel
        libXcursor-devel
        libXi-devel
        libXScrnSaver-devel
        libXinerama-devel
        libXxf86vm-devel
        libXtst-devel
        wayland-devel
        wayland-protocols-devel
        libxkbcommon-devel
        mesa-libEGL-devel
        mesa-libGL-devel
        libdrm-devel
        libgbm-devel
        alsa-lib-devel
        pulseaudio-libs-devel
        jack-audio-connection-kit-devel
        libsamplerate-devel
        systemd-devel
        dbus-devel
        libdecor-devel
    )

    log "Using dnf package manager."
    run_as_root dnf install -y "${packages[@]}"
}

install_with_pacman() {
    local packages=(
        git
        cmake
        ninja
        python
        base-devel
        clang
        pkgconf
        # SDL3 desktop dependencies (X11/Wayland and common runtime backends)
        libx11
        libxext
        libxrandr
        libxfixes
        libxcursor
        libxi
        libxss
        libxinerama
        libxxf86vm
        libxtst
        wayland
        wayland-protocols
        libxkbcommon
        libdrm
        mesa
        libglvnd
        alsa-lib
        libpulse
        libsamplerate
        systemd
        dbus
        libdecor
        libibus
        sndio
    )

    log "Using pacman package manager."
    run_as_root pacman -Sy --needed --noconfirm "${packages[@]}"

    # JACK provider conflict note:
    # - jack2 and pipewire-jack conflict on Arch.
    # - Many systems already have pipewire-jack installed.
    # - SDL can still build without JACK; this backend is optional.
    # Install one manually only if you specifically need JACK support.
    if ! pacman -Q jack2 >/dev/null 2>&1 && ! pacman -Q pipewire-jack >/dev/null 2>&1; then
        log "NOTE: JACK provider not installed (jack2/pipewire-jack)."
        log "      Optional for SDL; install manually if needed:"
        log "      sudo pacman -S jack2    # or: sudo pacman -S pipewire-jack"
    fi
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

    if tool_exists gcc || tool_exists clang; then
        log "OK: C/C++ compiler found."
    else
        log "MISSING: Neither gcc nor clang was found."
        missing=1
    fi

    if [[ "$missing" -ne 0 ]]; then
        log "ERROR: environment setup incomplete."
        exit 1
    fi
}

main() {
    log "Project root: $PROJECT_ROOT"
    local pkg_manager
    pkg_manager="$(detect_package_manager)"
    require_sudo

    case "$pkg_manager" in
        apt)
            install_with_apt
            ;;
        dnf)
            install_with_dnf
            ;;
        pacman)
            install_with_pacman
            ;;
        *)
            log "ERROR: unsupported Linux package manager."
            log "Supported managers: apt-get, dnf, pacman."
            exit 1
            ;;
    esac

    verify_tools
    log "Environment setup complete."
    log "Next step: bash tools/build_linux.sh"
}

main "$@"
