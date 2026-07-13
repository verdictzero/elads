#!/usr/bin/env bash
#
# bootstrap-pi.sh — install elads build dependencies on Raspberry Pi OS / Debian.
#
# Targets Debian "bookworm" and "trixie" (and derivatives). Package names differ
# slightly between releases (the t64 ABI transition); this script prefers the
# generic libglvnd dev packages and system wxWidgets 3.2.x.
#
# Usage:
#   scripts/bootstrap-pi.sh            # install required deps (uses sudo)
#   WITH_OPTIONAL=1 scripts/bootstrap-pi.sh   # also install optional deps (gzdoom, tools)
#
# This installs system libraries only. The Doom-specific toolchain (ZDBSP, acc,
# AJBSP) is built from source by scripts/build-toolchain.sh.
set -euo pipefail

SUDO=""
if [[ "${EUID}" -ne 0 ]]; then
    SUDO="sudo"
fi

echo "==> elads: installing build dependencies via apt"
echo "    (Debian bookworm/trixie / Raspberry Pi OS)"

# Core toolchain
CORE=(
    build-essential cmake ninja-build ccache pkg-config git
)

# GUI toolkit: system wxWidgets 3.2.x (std/aui/gl/stc/richtext/propgrid come with it)
WX=(
    libwxgtk3.2-dev libgtk-3-dev
)

# Graphics: desktop GL + EGL + GLES headers (glvnd-based, release-stable)
GL=(
    libgl-dev libegl-dev libgles-dev libglvnd-dev
)

# Audio / fonts (SLADE deps): SFML pulls OpenAL; FreeType/FTGL for text
MEDIA=(
    libsfml-dev libopenal-dev libfreetype-dev libftgl-dev
    libmpg123-dev libfluidsynth-dev
)

# Image + compression
IMG=(
    libpng-dev libwebp-dev libjpeg-dev zlib1g-dev libbz2-dev liblzma-dev
)

# Scripting
SCRIPT=(
    liblua5.4-dev
)

REQUIRED=( "${CORE[@]}" "${WX[@]}" "${GL[@]}" "${MEDIA[@]}" "${IMG[@]}" "${SCRIPT[@]}" )

# Optional: on-device GL/Vulkan probe tools + a playtest engine
OPTIONAL=(
    mesa-utils vulkan-tools
    gzdoom
)

$SUDO apt-get update

echo "==> Installing required packages"
$SUDO apt-get install -y --no-install-recommends "${REQUIRED[@]}"

if [[ "${WITH_OPTIONAL:-0}" == "1" ]]; then
    echo "==> Installing optional packages (best effort)"
    for pkg in "${OPTIONAL[@]}"; do
        if ! $SUDO apt-get install -y --no-install-recommends "$pkg"; then
            echo "    (optional) could not install '$pkg' — skipping"
        fi
    done
fi

echo ""
echo "==> Done. Verify wxWidgets:"
echo "      apt-cache policy libwxgtk3.2-dev"
echo "      wx-config --version"
echo "==> Next: scripts/build-toolchain.sh   (builds ZDBSP / acc / AJBSP)"
echo "==> Then: scripts/probe-gl.sh          (confirms GLES 3.1 on this GPU)"
