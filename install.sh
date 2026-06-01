#!/usr/bin/env bash
# install.sh — Smart Downloads Organizer installer
# Detects the Linux distribution, installs build dependencies,
# compiles from source, and installs system-wide.
#
# Usage:
#   sudo ./install.sh           # Full install
#   ./install.sh --user         # Install to ~/.local (no sudo required)
#   ./install.sh --uninstall    # Remove installed files
#   ./install.sh --help         # Show help

set -euo pipefail

# ─── Constants ─────────────────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_NAME="Smart Downloads Organizer"
APP_BIN="sdo"
APP_ID="com.sdo.organizer"
VERSION="2.0.0"

# ANSI colours
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
BOLD='\033[1m'
NC='\033[0m'

# ─── Helpers ───────────────────────────────────────────────────────────────────
info()    { echo -e "${BLUE}[INFO]${NC}  $*"; }
ok()      { echo -e "${GREEN}[OK]${NC}    $*"; }
warn()    { echo -e "${YELLOW}[WARN]${NC}  $*"; }
error()   { echo -e "${RED}[ERROR]${NC} $*" >&2; }
die()     { error "$*"; exit 1; }
require() { command -v "$1" &>/dev/null || die "Required tool not found: $1"; }

banner() {
    echo -e "${BOLD}"
    echo "  ╔══════════════════════════════════════════╗"
    echo "  ║   Smart Downloads Organizer  v${VERSION}     ║"
    echo "  ║   Installer                              ║"
    echo "  ╚══════════════════════════════════════════╝"
    echo -e "${NC}"
}

# ─── Argument parsing ──────────────────────────────────────────────────────────
USER_INSTALL=false
UNINSTALL=false
BUILD_TYPE="Release"
BUILD_TESTS="OFF"
JOBS="$(nproc 2>/dev/null || echo 4)"

for arg in "$@"; do
    case "$arg" in
        --user)        USER_INSTALL=true ;;
        --uninstall)   UNINSTALL=true ;;
        --debug)       BUILD_TYPE="Debug"; BUILD_TESTS="ON" ;;
        --with-tests)  BUILD_TESTS="ON" ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --user         Install to ~/.local (no root required)"
            echo "  --uninstall    Remove installed files"
            echo "  --debug        Build with debug symbols and tests"
            echo "  --with-tests   Build and run tests"
            echo "  --help         Show this help"
            exit 0
            ;;
        *) warn "Unknown argument: $arg" ;;
    esac
done

# ─── Uninstall ─────────────────────────────────────────────────────────────────
do_uninstall() {
    info "Uninstalling ${APP_NAME}..."

    local prefix="/usr"
    if $USER_INSTALL; then prefix="$HOME/.local"; fi

    local files=(
        "$prefix/bin/${APP_BIN}"
        "$prefix/share/applications/${APP_ID}.desktop"
        "$prefix/share/icons/hicolor/48x48/apps/${APP_ID}.png"
        "$prefix/share/icons/hicolor/128x128/apps/${APP_ID}.png"
        "$prefix/share/icons/hicolor/256x256/apps/${APP_ID}.png"
        "$prefix/share/metainfo/${APP_ID}.appdata.xml"
        "$prefix/share/doc/smart-downloads-organizer"
    )

    for f in "${files[@]}"; do
        if [ -e "$f" ]; then
            rm -rf "$f" && ok "Removed: $f" || warn "Could not remove: $f"
        fi
    done

    # Update caches
    gtk-update-icon-cache "$prefix/share/icons/hicolor" 2>/dev/null || true
    update-desktop-database "$prefix/share/applications" 2>/dev/null || true

    ok "Uninstall complete."
    echo ""
    echo "Note: User data is preserved at:"
    echo "  ~/.config/smart-downloads-organizer/"
    echo "  ~/.local/share/smart-downloads-organizer/"
    echo "Remove these manually if desired."
    exit 0
}

# ─── Detect OS and install dependencies ────────────────────────────────────────
install_deps() {
    info "Detecting Linux distribution..."

    if [ -f /etc/os-release ]; then
        # shellcheck disable=SC1091
        . /etc/os-release
        DISTRO_ID="${ID:-unknown}"
        DISTRO_LIKE="${ID_LIKE:-}"
    else
        DISTRO_ID="unknown"
        DISTRO_LIKE=""
    fi

    info "Distribution: ${PRETTY_NAME:-$DISTRO_ID}"

    # Determine package manager family
    local family=""
    case "$DISTRO_ID" in
        ubuntu|debian|linuxmint|pop|elementary|zorin|kali) family="apt" ;;
        fedora|rhel|centos|almalinux|rocky)                family="dnf" ;;
        opensuse*|sles)                                    family="zypper" ;;
        arch|manjaro|endeavouros|garuda)                   family="pacman" ;;
        void)                                              family="xbps" ;;
        *)
            # Check ID_LIKE
            if echo "$DISTRO_LIKE" | grep -q "debian\|ubuntu"; then family="apt"
            elif echo "$DISTRO_LIKE" | grep -q "fedora\|rhel"; then  family="dnf"
            elif echo "$DISTRO_LIKE" | grep -q "arch";          then  family="pacman"
            else
                warn "Unrecognised distribution. Please install dependencies manually:"
                echo "  - cmake >= 3.16, ninja-build"
                echo "  - libgtk-3-dev / gtk3-devel"
                echo "  - libsqlite3-dev / sqlite-devel"
                echo "  - libssl-dev / openssl-devel"
                echo "  - uuid-dev / libuuid-devel"
                family="manual"
            fi
            ;;
    esac

    if [ "$family" = "manual" ]; then
        read -rp "Continue without auto-installing dependencies? [y/N]: " ans
        [[ "$ans" =~ ^[Yy]$ ]] || die "Aborted."
        return
    fi

    info "Installing build dependencies via ${family}..."

    case "$family" in
        apt)
            apt-get update -qq
            DEBIAN_FRONTEND=noninteractive apt-get install -y \
                cmake ninja-build pkg-config \
                libgtk-3-dev \
                libsqlite3-dev \
                libssl-dev \
                uuid-dev \
                librsvg2-bin \
                desktop-file-utils
            ;;
        dnf)
            dnf install -y \
                cmake ninja-build pkgconf-pkg-config \
                gtk3-devel \
                sqlite-devel \
                openssl-devel \
                libuuid-devel \
                librsvg2-tools \
                desktop-file-utils
            ;;
        zypper)
            zypper install -y \
                cmake ninja pkg-config \
                gtk3-devel \
                sqlite3-devel \
                openssl-devel \
                libuuid-devel \
                rsvg-view \
                desktop-file-utils
            ;;
        pacman)
            pacman -Sy --noconfirm \
                cmake ninja pkgconf \
                gtk3 \
                sqlite \
                openssl \
                util-linux-libs \
                librsvg \
                desktop-file-utils
            ;;
        xbps)
            xbps-install -Sy \
                cmake ninja pkg-config \
                gtk+3-devel \
                sqlite-devel \
                openssl-devel \
                libuuid-devel \
                librsvg-devel
            ;;
    esac

    ok "Dependencies installed."
}

# ─── Build ─────────────────────────────────────────────────────────────────────
do_build() {
    info "Configuring build (${BUILD_TYPE})..."

    local prefix="/usr"
    if $USER_INSTALL; then prefix="$HOME/.local"; fi

    cmake -B "${SCRIPT_DIR}/build" \
          -S "${SCRIPT_DIR}" \
          -G Ninja \
          -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
          -DCMAKE_INSTALL_PREFIX="${prefix}" \
          -DSDO_BUILD_TESTS="${BUILD_TESTS}" \
          -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
          2>&1 | grep -v "^--" || true

    info "Compiling with ${JOBS} parallel jobs..."
    cmake --build "${SCRIPT_DIR}/build" --parallel "${JOBS}"
    ok "Build complete."
}

# ─── Test ──────────────────────────────────────────────────────────────────────
do_test() {
    if [ "$BUILD_TESTS" = "OFF" ]; then return; fi
    info "Running test suite..."
    cd "${SCRIPT_DIR}/build"
    ctest --output-on-failure \
          --exclude-regex ".*gui.*" \
          --timeout 60
    ok "All tests passed."
}

# ─── Install ───────────────────────────────────────────────────────────────────
do_install() {
    local prefix="/usr"
    if $USER_INSTALL; then prefix="$HOME/.local"; fi

    info "Installing to ${prefix}..."
    cmake --install "${SCRIPT_DIR}/build" --prefix "${prefix}"

    # Update icon cache
    if command -v gtk-update-icon-cache &>/dev/null; then
        gtk-update-icon-cache -f -t "${prefix}/share/icons/hicolor" 2>/dev/null || true
    fi
    # Update .desktop database
    if command -v update-desktop-database &>/dev/null; then
        update-desktop-database "${prefix}/share/applications" 2>/dev/null || true
    fi

    ok "Installation complete."
    echo ""
    echo -e "${BOLD}${GREEN}✓ ${APP_NAME} v${VERSION} installed successfully!${NC}"
    echo ""
    echo "  Run:       ${APP_BIN}"
    echo "  Headless:  ${APP_BIN} --headless"
    echo "  Help:      ${APP_BIN} --help"
    echo ""
    if $USER_INSTALL; then
        echo "  NOTE: Add ~/.local/bin to your PATH if not already present:"
        echo "    export PATH=\"\$HOME/.local/bin:\$PATH\""
        echo ""
    fi
}

# ─── Main ──────────────────────────────────────────────────────────────────────
main() {
    banner

    # Checks
    if $UNINSTALL; then do_uninstall; fi
    if ! $USER_INSTALL && [ "$(id -u)" -ne 0 ]; then
        die "System-wide install requires root. Run with sudo, or use --user flag."
    fi

    # Verify we're in the project root
    [ -f "${SCRIPT_DIR}/CMakeLists.txt" ] || die "Run from the project root directory."

    require cmake
    require g++

    install_deps
    do_build
    do_test
    do_install
}

main "$@"
