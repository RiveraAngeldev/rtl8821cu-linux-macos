
#!/usr/bin/env bash
# RTL8821CU Linux/macOS Research
# Cross-platform development environment setup.
# Does not modify Wi-Fi settings or kernel drivers.

set -Eeuo pipefail

readonly LINUX_PACKAGES=(
  usb-modeswitch usb-modeswitch-data usbutils
  iw kmod build-essential pkg-config
  libusb-1.0-0-dev ripgrep
)

readonly MACOS_PACKAGES=(libusb pkgconf)

info() { printf '[INFO] %s\n' "$*"; }
warn() { printf '[WARN] %s\n' "$*" >&2; }
fail() { printf '[ERROR] %s\n' "$*" >&2; exit 1; }

usage() {
  cat <<'HELP'
Usage: bash scripts/setup.sh [OPTION]

  --install    Install missing dependencies (default)
  --check      Verify dependencies without modifying anything
  --dry-run    Preview installation commands
  --help       Show this message

Supported:
  Linux: Debian/Ubuntu using APT
  macOS: Homebrew + Xcode Command Line Tools

Does not install drivers or modify Wi-Fi settings.
HELP
}

mode="${1:---install}"

if (( $# > 1 )); then
  usage
  exit 2
fi

case "$mode" in
  --install|--check|--dry-run) ;;
  --help|-h) usage; exit 0 ;;
  *) usage; exit 2 ;;
esac

find_brew() {
  if command -v brew >/dev/null 2>&1; then
    command -v brew
  elif [[ -x /opt/homebrew/bin/brew ]]; then
    echo /opt/homebrew/bin/brew
  elif [[ -x /usr/local/bin/brew ]]; then
    echo /usr/local/bin/brew
  else
    return 1
  fi
}

configure_macos_pkg_path() {
  local brew_cmd="$1"
  local prefix pc_dir

  prefix="$("$brew_cmd" --prefix libusb)" || return 1
  pc_dir="$prefix/lib/pkgconfig"

  if [[ ! -f "$pc_dir/libusb-1.0.pc" ]]; then
    warn "libusb metadata not found: $pc_dir"
    return 1
  fi

  export PKG_CONFIG_PATH="$pc_dir${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"

  info "Configured Homebrew libusb search path."
  info "PKG_CONFIG_PATH configured for this script only."
}

verify_build_tools() {
  local failed=0

  if command -v cc >/dev/null 2>&1; then
    info "C compiler: $(command -v cc)"
  else
    warn "C compiler not found."
    failed=1
  fi

  if command -v pkg-config >/dev/null 2>&1; then
    info "pkg-config: $(command -v pkg-config)"

    if pkg-config --exists libusb-1.0; then
      info "libusb: $(pkg-config --modversion libusb-1.0)"
    else
      warn "libusb not found by pkg-config."
      failed=1
    fi
  else
    warn "pkg-config not found."
    failed=1
  fi

  return "$failed"
}

setup_linux() {
  command -v apt-get >/dev/null 2>&1 ||
    fail "APT is not available."

  command -v dpkg-query >/dev/null 2>&1 ||
    fail "dpkg-query is required."

  local missing=() pkg

  for pkg in "${LINUX_PACKAGES[@]}"; do
    if [[ "$(dpkg-query -W -f='${Status}' "$pkg" 2>/dev/null || true)" != "install ok installed" ]]; then
      missing+=("$pkg")
    fi
  done

  if [[ "$mode" == "--dry-run" ]]; then
    if (( ${#missing[@]} )); then
      echo "Would run: sudo apt-get update"
      printf "Would run: sudo apt-get install"
      printf " %q" "${missing[@]}"
      printf "\n"
    else
      info "All APT packages are installed."
    fi
    return
  fi

  if [[ "$mode" == "--check" ]]; then
    if (( ${#missing[@]} )); then
      warn "Missing packages: ${missing[*]}"
      return 1
    fi
    info "All Linux packages installed."
    verify_build_tools
    return
  fi

  if (( ${#missing[@]} )); then
    local elevate=()

    if (( EUID != 0 )); then
      command -v sudo >/dev/null 2>&1 ||
        fail "sudo is required."
      elevate=(sudo)
    fi

    info "Installing: ${missing[*]}"
    "${elevate[@]}" apt-get update
    "${elevate[@]}" apt-get install "${missing[@]}"
  else
    info "All Linux packages already installed."
  fi

  verify_build_tools || fail "Build environment incomplete."
}

setup_macos() {
  local brew_cmd missing=() pkg

  brew_cmd="$(find_brew || true)"

  if [[ -z "$brew_cmd" ]]; then
    if [[ "$mode" == "--dry-run" ]]; then
      warn "Homebrew is not installed."
      printf "Would run after installing Homebrew: brew install %s\n" "${MACOS_PACKAGES[*]}"
      return
    fi
    fail "Install Homebrew manually from https://brew.sh/"
  fi

  for pkg in "${MACOS_PACKAGES[@]}"; do
    if ! "$brew_cmd" list --formula "$pkg" >/dev/null 2>&1; then
      missing+=("$pkg")
    fi
  done

  if [[ "$mode" == "--dry-run" ]]; then
    if (( ${#missing[@]} )); then
      printf "Would run: brew install"
      printf " %q" "${missing[@]}"
      printf "\n"
    else
      info "All Homebrew packages installed."
    fi
    info "Would configure libusb pkg-config lookup."
    return
  fi

  if ! xcode-select -p >/dev/null 2>&1; then
    fail "Run xcode-select --install first."
  fi

  if [[ "$mode" == "--check" ]]; then
    if (( ${#missing[@]} )); then
      warn "Missing Homebrew packages: ${missing[*]}"
      return 1
    fi
  elif (( ${#missing[@]} )); then
    info "Installing: ${missing[*]}"
    "$brew_cmd" install "${missing[@]}"
  else
    info "All Homebrew packages already installed."
  fi

  configure_macos_pkg_path "$brew_cmd" ||
    fail "Could not configure libusb."

  verify_build_tools ||
    fail "C/libusb build environment incomplete."

  info "macOS setup verified successfully."
}

os="$(uname -s)"
info "Detected operating system: $os"

case "$os" in
  Linux) setup_linux ;;
  Darwin) setup_macos ;;
  *) fail "Unsupported operating system: $os" ;;
esac

info "Setup task finished."

