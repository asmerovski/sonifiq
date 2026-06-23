#!/bin/bash
# SonifiQ build script
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

echo "==> Checking dependencies..."
if ! command -v ffmpeg &>/dev/null; then
    echo "  [!] ffmpeg not found. Install with: sudo pacman -S ffmpeg"
    exit 1
fi
if ! command -v cmake &>/dev/null; then
    echo "  [!] cmake not found. Install with: sudo pacman -S cmake"
    exit 1
fi
if ! pkg-config --exists Qt6Widgets 2>/dev/null; then
    echo "  [!] Qt6 not found. Install with: sudo pacman -S qt6-base"
    exit 1
fi

echo "==> Configuring..."
mkdir -p "$BUILD_DIR"
cmake -B "$BUILD_DIR" -S "$SCRIPT_DIR" -DCMAKE_BUILD_TYPE=Release

echo "==> Building..."
cmake --build "$BUILD_DIR" --parallel "$(nproc)"

echo ""
echo "==> Build complete!"
echo "    Binary: $BUILD_DIR/sonifiq"
echo "    Run:    $BUILD_DIR/sonifiq"
echo ""
echo "    To install system-wide:"
echo "    sudo cmake --install $BUILD_DIR --prefix /usr/local"

install_icon() {
    local ICON_SRC="$SCRIPT_DIR/sonifiq.svg"
    local ICON_DST="$HOME/.local/share/icons/hicolor/scalable/apps/sonifiq.svg"
    local DESKTOP_DST="$HOME/.local/share/applications/sonifiq.desktop"
    mkdir -p "$(dirname "$ICON_DST")" "$(dirname "$DESKTOP_DST")"
    cp "$ICON_SRC" "$ICON_DST"
    sed "s|Exec=sonifiq|Exec=$BUILD_DIR/sonifiq|g" "$SCRIPT_DIR/sonifiq.desktop" > "$DESKTOP_DST"
    gtk-update-icon-cache ~/.local/share/icons/hicolor 2>/dev/null || true
    echo "    Icon installed → $ICON_DST"
    echo "    Desktop entry → $DESKTOP_DST"
}

read -rp "Install icon + desktop entry to ~/.local/share? [y/N] " ans
[[ "$ans" =~ ^[Yy]$ ]] && install_icon
