#!/usr/bin/env bash
# RTL8821CU Linux/macOS Research — Development environment setup
# Installs project dependencies only; does not alter network interfaces or drivers.
set -Eeuo pipefail

readonly LINUX_PACKAGES=(
  usb-modeswitch usb-modeswitch-data usbutils iw kmod
  build-essential pkg-config libusb-1.0-0-dev ripgrep
)
readonly MACOS_PACKAGES=(libusb pkgconf)

usage() {
  cat <<'HELP'
Usage: bash scripts/setup.sh [--install|--check|--dry-run|--help]

  --install    Install missing development dependencies (default).
  --check      Show whether required packages are installed; make no changes.
  --dry-run    Show planned installation commands; make no changes.
  --help       Show this help.

Linux: Debian/Ubuntu and derivatives with APT.
macOS: Homebrew and Xcode Command Line Tools must be installed first.
This script does not install drivers, change Wi-Fi settings, or install Homebrew.
HELP
}

info() { printf '[INFO] %s\n' "$*"; }
warn() { printf '[WARN] %s\n' "$*" >&2; }
fail() { printf '[ERROR] %s\n' "$*" >&2; exit 1; }

mode="${1:---install}"
if (( $# > 1 )); then usage; exit 2; fi
case "$mode" in
  --install|--check|--dry-run) ;;
  --help|-h) usage; exit 0 ;;
  *) usage; exit 2 ;;
esac

os="$(uname -s)"
info "Detected operating system: $os"

verify_build_tools() {
  local failed=0
  if command -v cc >/dev/null 2>&1; then
    info "C compiler: $(command -v cc)"
  else
    warn 'C compiler not found.'
    failed=1
  fi
  if command -v pkg-config >/dev/null 2>&1; then
    info "pkg-config: $(command -v pkg-config)"
    if pkg-config --exists libusb-1.0; then
      info "libusb: $(pkg-config --modversion libusb-1.0)"
    else
      warn 'libusb-1.0 development files not detected by pkg-config.'
      failed=1
    fi
  else
    warn 'pkg-config is not available.'
    failed=1
  fi
  return "$failed"
}

setup_linux() {
  command -v apt-get >/dev/null 2>&1 || fail 'APT is unavailable. This script currently supports Debian/Ubuntu-based Linux distributions.'
  command -v dpkg-query >/dev/null 2>&1 || fail 'dpkg-query is required on Linux.'
  local missing=() pkg
  for pkg in "${LINUX_PACKAGES[@]}"; do
    if [[ "$(dpkg-query -W -f='${Status}' "$pkg" 2>/dev/null || true)" != 'install ok installed' ]]; then
      missing+=("$pkg")
    fi
  done
  if [[ "$mode" == '--check' ]]; then
    if (( ${#missing[@]} )); then
      warn "Missing APT packages: ${missing[*]}"
      return 1
    fi
    info 'All required APT packages are installed.'
    verify_build_tools
    return
  fi
  if (( ${#missing[@]} == 0 )); then
    info 'All APT packages are already installed. Skipping installation.'
  elif [[ "$mode" == '--dry-run' ]]; then
    printf 'Would run: sudo apt-get update\n'
    printf 'Would run: sudo apt-get install'
    printf ' %q' "${missing[@]}"
    printf '\n'
    return
  else
    local elevate=()
    if (( EUID != 0 )); then
      command -v sudo >/dev/null 2>&1 || fail 'sudo is required for APT installation.'
      elevate=(sudo)
    fi
    info "Installing missing APT packages: ${missing[*]}"
    "${elevate[@]}" apt-get update
    "${elevate[@]}" apt-get install "${missing[@]}"
  fi
  if [[ "$mode" != '--dry-run' ]]; then
    verify_build_tools || fail 'Package installation finished, but the C/libusb build environment is incomplete.'
    if [[ -x /usr/sbin/iw ]] && ! command -v iw >/dev/null 2>&1; then
      warn 'iw is installed in /usr/sbin but not in PATH. For this session: export PATH="$PATH:/usr/sbin"'
    fi
  fi
}

find_brew() {
  if command -v brew >/dev/null 2>&1; then
    command -v brew
  elif [[ -x /opt/homebrew/bin/brew ]]; then
    printf '%s\n' /opt/homebrew/bin/brew
  elif [[ -x /usr/local/bin/brew ]]; then
    printf '%s\n' /usr/local/bin/brew
  else
    return 1
  fi
}

setup_macos() {
  local brew_cmd=''
  brew_cmd="$(find_brew || true)"
  if [[ "$mode" == '--dry-run' ]]; then
    printf 'Would run: brew install %s\n' "${MACOS_PACKAGES[*]}"
    info 'Prerequisites: Homebrew and Xcode Command Line Tools.'
    return
  fi
  if [[ "$mode" == '--install' ]] && ! xcode-select -p >/dev/null 2>&1; then
    fail 'Xcode Command Line Tools not detected. Run: xcode-select --install ; complete installation and rerun.'
  fi
  if [[ -z "$brew_cmd" ]]; then
    fail 'Homebrew not found. Install it from https://brew.sh/ and rerun. This script never downloads or executes an installer automatically.'
  fi
  local missing=() pkg
  for pkg in "${MACOS_PACKAGES[@]}"; do
    if ! "$brew_cmd" list --formula "$pkg" >/dev/null 2>&1; then
      missing+=("$pkg")
    fi
  done
  if [[ "$mode" == '--check' ]]; then
    if (( ${#missing[@]} )); then
      warn "Missing Homebrew packages: ${missing[*]}"
      return 1
    fi
    info 'All required Homebrew packages are installed.'
    verify_build_tools
    return
  fi
  if (( ${#missing[@]} )); then
    info "Installing missing Homebrew packages: ${missing[*]}"
    "$brew_cmd" install "${missing[@]}"
  else
    info 'All Homebrew packages are already installed. Skipping installation.'
  fi
  verify_build_tools || fail 'The C/libusb build environment is incomplete.'
  info 'Linux-specific tools (iw, kmod, usb-modeswitch) are not installed on macOS.'
}

case "$os" in
  Linux) setup_linux ;;
  Darwin) setup_macos ;;
  *) fail "Unsupported operating system: $os" ;;
esac

info 'Setup task finished.'
