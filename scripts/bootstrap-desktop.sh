#!/usr/bin/env bash
#
# bootstrap-desktop.sh — install dependencies for the Linux x86-64 desktop OpenGL variant.
#
# Installs the GL/EGL dev headers, libepoxy, and the Mesa software renderer (llvmpipe) so the
# headless renderer + GL render test run even without a GPU. On a real desktop the GPU driver
# is used automatically. Debian/Ubuntu.
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
    libgl1-mesa-dri

echo ""
echo "==> Done. Build + render:"
echo "     cmake --preset desktop && cmake --build --preset desktop"
echo "     ctest --preset desktop"
echo "     ./build/desktop/src/elads-render render-demo demo.png"
