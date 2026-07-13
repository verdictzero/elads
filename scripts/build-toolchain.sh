#!/usr/bin/env bash
#
# build-toolchain.sh — clone + build the Doom-specific toolchain elads orchestrates.
#
# Builds, from source, into ./third_party/ :
#   * ZDBSP  — node builder (standard/GL/XGL2/XGL3/UDMF) for GZDoom            [required]
#   * acc    — the ACS script compiler                                        [required]
#   * AJBSP  — lightweight embeddable node builder (mirror; verify in Phase 0)[optional]
#   * GZDoom — the playtest engine (large; build only if BUILD_GZDOOM=1)      [optional]
#
# All URLs were confirmed clone-able via `git ls-remote`. Override any with an
# env var if upstream moves. On aarch64, ZDBSP's x86 SSE classifier files are
# excluded automatically by its own CMake (gated on 32-bit), so no patch needed.
#
# Usage:
#   scripts/build-toolchain.sh                 # ZDBSP + acc (+ AJBSP)
#   BUILD_GZDOOM=1 scripts/build-toolchain.sh  # also build GZDoom (slow)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TP="${ROOT}/third_party"
SRC="${TP}/src"
OUT="${TP}/install"
mkdir -p "${SRC}" "${OUT}/bin"

JOBS="$(nproc 2>/dev/null || echo 4)"

ZDBSP_URL="${ZDBSP_URL:-https://github.com/ZDoom/zdbsp.git}"
ACC_URL="${ACC_URL:-https://github.com/ZDoom/acc.git}"
AJBSP_URL="${AJBSP_URL:-https://github.com/RusticTroll/ajbsp.git}"   # AJBSP mirror; elf-alchemist/elfbsp is a maintained alt
GZDOOM_URL="${GZDOOM_URL:-https://github.com/ZDoom/gzdoom.git}"

clone_or_update() {
    local url="$1" dir="$2"
    if [[ -d "${dir}/.git" ]]; then
        echo "==> updating $(basename "${dir}")"
        git -C "${dir}" pull --ff-only || echo "    (pull skipped)"
    else
        echo "==> cloning ${url}"
        git clone --depth 1 "${url}" "${dir}"
    fi
}

build_cmake() {
    local dir="$1"; shift
    cmake -S "${dir}" -B "${dir}/build" -DCMAKE_BUILD_TYPE=Release "$@"
    cmake --build "${dir}/build" -j "${JOBS}"
}

# --- ZDBSP -----------------------------------------------------------------
clone_or_update "${ZDBSP_URL}" "${SRC}/zdbsp"
build_cmake "${SRC}/zdbsp"
find "${SRC}/zdbsp/build" -maxdepth 2 -type f -name 'zdbsp' -exec cp -v {} "${OUT}/bin/" \; || true

# --- acc -------------------------------------------------------------------
clone_or_update "${ACC_URL}" "${SRC}/acc"
build_cmake "${SRC}/acc"
find "${SRC}/acc/build" -maxdepth 2 -type f -name 'acc' -exec cp -v {} "${OUT}/bin/" \; || true
# acc also needs its zcommon/zdefs includes at runtime; copy the .acs headers next to it.
find "${SRC}/acc" -maxdepth 1 -name '*.acs' -exec cp -v {} "${OUT}/bin/" \; || true

# --- AJBSP (optional, best effort) -----------------------------------------
if clone_or_update "${AJBSP_URL}" "${SRC}/ajbsp" 2>/dev/null; then
    if build_cmake "${SRC}/ajbsp" 2>/dev/null; then
        find "${SRC}/ajbsp/build" -maxdepth 2 -type f -name 'ajbsp' -exec cp -v {} "${OUT}/bin/" \; || true
    else
        echo "    (AJBSP build skipped — verify the correct upstream in Phase 0)"
    fi
fi

# --- GZDoom (optional, slow) -----------------------------------------------
if [[ "${BUILD_GZDOOM:-0}" == "1" ]]; then
    clone_or_update "${GZDOOM_URL}" "${SRC}/gzdoom"
    echo "==> Building GZDoom (this is slow on a Pi; ensure Active Cooler is fitted)"
    build_cmake "${SRC}/gzdoom"
    echo "    Run on the Pi with:  gzdoom +vid_preferbackend 3   (GLES backend)"
fi

echo ""
echo "==> Toolchain built. Binaries in: ${OUT}/bin"
ls -l "${OUT}/bin" || true
echo "==> Add to PATH or point elads' pipeline config at this directory (Phase 2)."
