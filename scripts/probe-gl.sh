#!/usr/bin/env bash
#
# probe-gl.sh — capture the graphics capabilities of this machine.
#
# Run this on a real Raspberry Pi 5 and attach scripts/probe-output/ to an issue.
# We use it to confirm, per Pi OS image, the exact levels elads must target:
#   * desktop OpenGL core/compat version (expected: core 3.1, compat 2.1 on V3D)
#   * OpenGL ES version                  (expected: GLES 3.1, the primary target)
#   * Vulkan version                     (expected: 1.3 conformant via V3DV)
#
# Needs (best effort): mesa-utils (glxinfo/eglinfo), vulkan-tools (vulkaninfo).
#   sudo apt-get install mesa-utils vulkan-tools
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUTDIR="${ROOT}/scripts/probe-output"
mkdir -p "${OUTDIR}"

have() { command -v "$1" >/dev/null 2>&1; }
line() { printf -- '---- %s ----\n' "$1"; }

echo "==> elads GL probe"
echo "    session: XDG_SESSION_TYPE=${XDG_SESSION_TYPE:-unknown}  WAYLAND_DISPLAY=${WAYLAND_DISPLAY:-}  DISPLAY=${DISPLAY:-}"

# System / distro
{
    line "system"
    uname -a
    echo
    [[ -r /etc/os-release ]] && cat /etc/os-release
    echo
    line "mesa version (apt)"
    (apt-cache policy libgl1-mesa-dri 2>/dev/null || true)
} >"${OUTDIR}/system.txt" 2>&1

# Desktop GL (GLX). Under native Wayland there is no GLX; this may fail — that is itself data.
if have glxinfo; then
    glxinfo -B >"${OUTDIR}/glxinfo.txt" 2>&1 || echo "(glxinfo failed — expected under native Wayland; see EGL below)" >>"${OUTDIR}/glxinfo.txt"
else
    echo "glxinfo not installed (apt-get install mesa-utils)" >"${OUTDIR}/glxinfo.txt"
fi

# EGL + GLES — the path elads actually uses on the Pi.
if have eglinfo; then
    eglinfo >"${OUTDIR}/eglinfo.txt" 2>&1 || echo "(eglinfo failed)" >>"${OUTDIR}/eglinfo.txt"
else
    echo "eglinfo not installed (apt-get install mesa-utils / mesa-utils-bin)" >"${OUTDIR}/eglinfo.txt"
fi

# Vulkan (V3DV)
if have vulkaninfo; then
    vulkaninfo --summary >"${OUTDIR}/vulkaninfo.txt" 2>&1 || vulkaninfo >"${OUTDIR}/vulkaninfo.txt" 2>&1 || true
else
    echo "vulkaninfo not installed (apt-get install vulkan-tools)" >"${OUTDIR}/vulkaninfo.txt"
fi

echo ""
echo "==> Summary (grep of the interesting lines):"
line "renderer / GL versions"
grep -iE 'OpenGL (renderer|core profile version|version|shading)' "${OUTDIR}/glxinfo.txt" 2>/dev/null || echo "  (no GLX data)"
line "EGL / GLES"
grep -iE 'EGL version|OpenGL ES|GLSL ES|Version:' "${OUTDIR}/eglinfo.txt" 2>/dev/null | head -20 || echo "  (no EGL data)"
line "Vulkan"
grep -iE 'apiVersion|deviceName|driverName|V3D' "${OUTDIR}/vulkaninfo.txt" 2>/dev/null | head -10 || echo "  (no Vulkan data)"

echo ""
echo "==> Full output written to: ${OUTDIR}/"
echo "    Please attach these files when reporting on-device results."
