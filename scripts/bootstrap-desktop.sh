#!/usr/bin/env bash
#
# bootstrap-desktop.sh — install dependencies for the Linux x86-64 desktop OpenGL variant.
#
# Installs the GL/EGL dev headers, libepoxy, GLFW (for the interactive elads-view window), and
# the Mesa software renderer (llvmpipe) + Xvfb so the headless renderer, GL render tests, and the
# windowed viewport all run even without a GPU/display. On a real desktop the GPU driver and a
# real display are used automatically. Debian/Ubuntu.
#
# Usage: scripts/bootstrap-desktop.sh
set -euo pipefail

SUDO=""
if [[ "${EUID}" -ne 0 ]]; then
    SUDO="sudo"
fi

echo "==> elads: installing desktop OpenGL build dependencies"

$SUDO apt-get update
$SUDO apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build ccache pkg-config git \
    libepoxy-dev libegl-dev libgl-dev libgles-dev libgbm-dev \
    libglfw3-dev xvfb \
    libgl1-mesa-dri

echo ""
echo "==> Done. Build + render:"
echo "     cmake --preset desktop && cmake --build --preset desktop"
echo "     ctest --preset desktop"
echo "     ./build/desktop/src/elads-render render-demo demo.png"
