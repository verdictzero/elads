# Raspberry Pi 5 target platform

> The hardware, graphics stack, OS image, build, CI, and packaging facts that make the Raspberry Pi 5
> the **first-class** runtime target for elads. Everything the renderer must respect lives here; the
> RAL design that consumes these facts is in [render-abstraction](02-render-abstraction.md).

---

## 1. Why the Pi 5 comes first

elads targets the **Raspberry Pi 5 (8 GB, aarch64) FIRST**, then Linux desktop generally (see
[overview](00-overview.md)). Every architectural constraint that looks unusual — GLES 3.1 instead of
desktop GL, EGL instead of GLX, wx 3.2.x instead of the 3.3 dev series — traces back to this board.
Getting a Doom editor to run *well* on a fanless-by-default ARM SBC is the forcing function that
keeps the whole codebase honest about GPU feature use, thermal budget, and dependency portability.

The Pi 5 is also *why the renderer is the one replaced subsystem*: SLADE `master` needs desktop
GL 3.3 and UDB's OpenTK renderer needs desktop GL 3.2 core, both above this board's desktop-GL
ceiling of 3.1 (see [architecture](01-architecture.md), [ADR-0002](../decisions/ADR-0002-gles31-render-backend.md)).

---

## 2. Hardware

| Component | Spec | Implication for elads |
|-----------|------|-----------------------|
| SoC | **Broadcom BCM2712** | 16 nm, ARMv8.2-A. |
| CPU | Quad **Cortex-A76 @ ~2.4 GHz**, aarch64 | `-mcpu=cortex-a76`; 4 cores → parallel node-build / triangulation. |
| ISA | **ARMv8.2-A** (AArch64), NEON, fp16, dotprod, LSE atomics | No x86 SSE; scalar/NEON paths only (ZDBSP SSE files auto-excluded, §8). |
| RAM | **8 GB LPDDR4X-4267** | Large WADs + GPU staging fit in RAM; the 8 GB SKU is required, not 4 GB. |
| GPU | **VideoCore VII** (V3D 7.1.x), Mesa **V3D 7.x** | Tile-based deferred renderer; GLES 3.1 conformant (§3). |
| Video out | Dual 4Kp60 HDMI, `vc4-kms-v3d` KMS driver | HiDPI panels common → GL scaling matters (§5). |
| Storage | microSD, USB 3.0, **PCIe 2.0 x1 → NVMe** | NVMe massively speeds link steps of native builds (§6). |
| Power | USB-C PD | **27 W official PSU required** for sustained GPU+NVMe (§4 BOM). |

The GPU is the binding constraint. The VideoCore VII is a **tile-based (TBDR-style) renderer**: it
bins geometry into screen tiles and shades per-tile from fast on-chip memory. This rewards batching
and *hates* mid-frame framebuffer read-modify-write and depth/stencil thrash — the performance model
that drives the RAL's draw-call and FBO rules ([render-abstraction](02-render-abstraction.md) §7).

---

## 3. Graphics stack: Mesa V3D + V3DV

Two Mesa drivers cover the VideoCore VII (source: `docs.mesa3d.org` V3D/V3DV; `raspberrypi.com`):

- **V3D** — the OpenGL / OpenGL ES Gallium driver.
- **V3DV** — the Vulkan driver.

### 3.1 Capability table (native, conformant)

These are the **native** caps on this GPU. Desktop GL 3.3 / 4.x will **never** appear here — the
hardware simply lacks the features. Use this table as the single source of truth:

| API surface | Version on Pi 5 | Shading language | What it implies |
|-------------|-----------------|------------------|-----------------|
| Desktop GL **core** | **3.1** | GLSL **1.40** | No compute, no SSBO, no explicit `layout(location=)` on varyings. Fallback-only. |
| Desktop GL **compat** | **2.1** | GLSL 1.20 | Legacy fixed-function; irrelevant to elads. |
| **GLES** | **3.1** | **GLSL ES 3.10** | **CONFORMANT.** Compute shaders, SSBO, image load/store, explicit `layout(location=)`, UBO std140, MRT, instancing. |
| **Vulkan** | **1.3** (V3DV) | SPIR-V | **CONFORMANT** as of Mesa **24.3+** — *not* "experimental". Not used by RAL v1; reserved for a future backend. |

### 3.2 Why GLES 3.1 is the target — and richer than desktop GL 3.1 here

Counter-intuitively, on *this* GPU the **GLES 3.1 profile is a strict superset of the useful desktop
GL 3.1 feature set**. GLES 3.1 exposes compute shaders, SSBOs, image load/store, and explicit
attribute/uniform `layout(location=)` qualifiers that the desktop GL 3.1 (GLSL 1.40) profile on V3D
does *not*. So elads authors to **GLES 3.1 (`#version 310 es`)** as the primary target and
*down-lowers* to desktop GL 3.1 for the portability fallback — never the reverse. Rationale and the
lowering rules are in [render-abstraction](02-render-abstraction.md) §3.

```glsl
// illustrative — primary shader header used across the RAL GLES backend
#version 310 es
precision highp float;      // GLES requires explicit precision; desktop GL ignores it
layout(location = 0) in vec3 a_pos;   // explicit locations: present in GLES 3.1, absent in GL 3.1/GLSL 1.40
```

> **Design rule (restated from render doc).** The RAL exposes only features present in **both**
> GLES 3.1 and desktop GL 3.1; compute/SSBO are *optional* capabilities probed at runtime and used
> only where a CPU fallback exists. The Pi's *native* GLES 3.1 is the reference profile.

---

## 4. Thermal budget and required BOM

The BCM2712 throttles under sustained 3D load. Governing thresholds (source: `raspberrypi.com`
documentation, firmware defaults):

| Event | Temp | Behaviour |
|-------|------|-----------|
| Soft throttle | **~80 °C** | Clocks begin stepping down; frame times climb, jitter appears. |
| Hard throttle | **~85 °C** | Aggressive clock cut to protect silicon; large FPS drop. |

A bare board under a fanless case will hit soft-throttle within minutes of continuous 3D map-view
rendering. To keep the *editor* usable during long sessions the **BOM mandates active cooling and
adequate power**:

| Item | Requirement | Why |
|------|-------------|-----|
| **Official Active Cooler** | Required | Keeps SoC well under 80 °C at sustained GPU load; PWM-controlled off the fan header. |
| **27 W USB-C PD PSU** | Required | Under-powering causes brownout throttle + NVMe dropouts independent of temperature. |
| NVMe HAT + SSD | Recommended | Link-time I/O for native builds (§6); not thermally required. |

**Benchmark policy.** Performance numbers for the 3D map view are only meaningful **sustained** —
measure after ≥10 min of continuous rendering with the Active Cooler fitted, and report both the
first-minute and steady-state frame times. A benchmark that ignores throttle overstates the board.
The headless EGL smoke test (§7) does *not* stress thermals; a separate `bench/sustained-3d`
scene (dense sector geometry, textured, depth-tested) is the throttle canary.

---

## 5. Wayland-first display (labwc)

Raspberry Pi OS is **Wayland-first**, defaulting to the **labwc** compositor. Consequences that the
app must handle:

- **No GLX under native Wayland.** GL contexts are created via **EGL**. elads uses the wxWidgets
  **wxGLCanvas EGL path** for on-screen contexts and **EGL + GBM** for offscreen/headless contexts
  (CI smoke test, thumbnail generation) — see [render-abstraction](02-render-abstraction.md) §5 and
  §7 below.
- **Test matrix: labwc (native Wayland) *and* X11 / Xwayland.** Some users run the legacy X11
  session or launch under Xwayland; the EGL path must work in both. CI runs headless EGL (§7);
  manual on-device verification covers labwc-native and Xwayland windowed.
- **HiDPI GL scaling.** Wayland reports a fractional/integer output scale; the GL canvas must render
  at physical pixels and map input in logical coordinates. This needs wx's Wayland HiDPI GL fixes
  (see §6.3 wx-version note). Mishandling scale gives a half-size or blurry viewport on 4K panels.

```
context creation on Pi OS (Wayland-first)
  on-screen   : wxGLCanvas ──EGL──▶ V3D (GLES 3.1 context)     [labwc or Xwayland]
  off-screen  : EGL + GBM  ──────▶ V3D (headless GLES 3.1)     [CI, thumbnails, no display]
  (GLX)       : ✗ not available under native Wayland
```

---

## 6. Building for the Pi

Three strategies; elads supports **native on-device** as the canonical path and **native ARM CI**
(§7) as the gate, with cross-compile available for developer speed.

### 6.1 Native on-device build

The simplest and most representative: build on the Pi itself.

```bash
# illustrative CMake preset flags for on-device builds
-DCMAKE_CXX_FLAGS="-mcpu=cortex-a76 -O2"     # tune for the A76 microarchitecture
-DCMAKE_CXX_COMPILER_LAUNCHER=ccache          # ccache: huge win on incremental rebuilds
# link on NVMe, not microSD — link step is I/O-bound and dominated by disk latency
```

- `-mcpu=cortex-a76` lets GCC/Clang schedule for the A76 and emit ARMv8.2-A + NEON.
- **ccache** turns edit-build-run cycles from minutes into seconds after the first build.
- **NVMe** matters most at **link time** (large static libs: Scintilla, wx, embedded AJBSP);
  microSD link times are painful. This is the practical reason NVMe is in the recommended BOM (§4).
- 8 GB RAM comfortably holds the compile working set at `-j4`; watch peak RSS on heavy TUs and drop
  to `-j2` for the linker if needed.

### 6.2 Cross-compile with a sysroot vs QEMU

| Approach | Speed | Fidelity | Use when |
|----------|-------|----------|----------|
| **Native on-device** | slow build, perfect fidelity | 100% | canonical; release builds; anything GPU-touching. |
| **Cross-compile + sysroot** | fast build | high (needs correct sysroot libs/ABI) | dev iteration; requires a Pi OS sysroot mirroring the target's t64 packages (§9). |
| **QEMU user-mode** | very slow | high functional, no GPU | last resort; **not** for CI (we use native ARM runners instead, §7). |

Cross-compiling requires an aarch64 sysroot populated from the *same* Pi OS release you target, so
the wx/GL/EGL headers and the **t64 ABI** package names line up with the device (§9). Mismatched
sysroot ↔ device ABI is the classic cross-build footgun here.

### 6.3 wxWidgets version note (important caveat)

elads targets **system wxWidgets 3.2.9+**, which added the **EGL 1.4** context path and the
**Wayland HiDPI GL** fixes elads depends on (see [overview](00-overview.md); SLADE `master` instead
wants the 3.3.x *dev* series, which Pi OS does not ship). **Caveat to flag:** neither Bookworm
(wx ~3.2.2) nor Trixie (wx ~3.2.6) ships 3.2.9 in `apt` today. Until a Pi OS release ships ≥3.2.9,
elads must either **vendor/build wxWidgets 3.2.9+** or consume a backport. Treat "system wx satisfies
us" as *aspirational* per-release; verify the packaged version before relying on the EGL/HiDPI
behaviour above. See [risks](../risks.md).

---

## 7. Continuous integration

CI runs on **GitHub Actions `ubuntu-24.04-arm` native runners** — real aarch64 hardware, **no QEMU**
(source: GitHub Actions Linux Arm64 runners). This keeps CI build times sane and, crucially, tests
the *actual* target ISA. Full pipeline design is in [build-test-pipeline](07-build-test-pipeline.md);
the Pi-specific piece is the **headless EGL smoke test**:

```
CI job: headless-egl-smoke  (ubuntu-24.04-arm, no display)
  1. create EGL + GBM offscreen context   (surfaceless / gbm device)
  2. bring up a GLES 3.1 context           → assert #version 310 es compiles
  3. render one RAL frame to an FBO        → readback pixels
  4. checksum / bounds-check the readback  → fail if black/garbage
```

Notes and caveats:
- The `ubuntu-24.04-arm` runner has **no VideoCore GPU** — the headless EGL context is served by a
  software GLES driver (Mesa `llvmpipe`/`softpipe`/`swrast`). The smoke test therefore validates
  **API correctness, shader compilation, and RAL wiring on aarch64**, *not* V3D-specific behaviour
  or performance. Real V3D validation is manual on-device (§4 benchmark, labwc/Xwayland matrix §5).
- GZDoom validation needs a **live GL context** (no headless compile-validate mode) and on the Pi
  renders only via its **GLES backend** (`+vid_preferbackend 3`); that is an on-device concern, not
  a CI job — see [build-test-pipeline](07-build-test-pipeline.md) §8 and [risks](../risks.md) R7.

---

## 8. apt dependency availability

SLADE's mandatory dependency set (see [overview](00-overview.md)) is all present in Pi OS `apt`,
which is why the fork is viable without vendoring most libraries. Availability by concern:

| Concern | Packages (Debian/Pi OS) | Notes |
|---------|-------------------------|-------|
| GUI toolkit | `libwxgtk3.2-dev` (+ `-gl`) | Version caveat §6.3 (3.2.2/3.2.6 vs needed 3.2.9). |
| GL / EGL / GLES | `libgl1-mesa-dev`, `libegl1-mesa-dev`, `libgles2-mesa-dev`, `libgbm-dev` | V3D driver via `mesa`; GBM for headless. |
| Vulkan (future) | `libvulkan-dev`, `mesa-vulkan-drivers` (V3DV) | Not used by RAL v1. |
| Scripting | `liblua5.4-dev` | Lua 5.4 for sol2 ([scripting](00-overview.md)). |
| Audio (SLADE dep) | `libsfml-dev` (pulls `libopenal`), `libmpg123-dev` | SFML mandatory in SLADE for audio preview. |
| Text render | `libfreetype-dev`, `libftgl-dev` | ftgl for GL text; freetype underneath. |
| Image codecs | `libpng-dev`, `libwebp-dev` | libwebp mandatory in SLADE. |
| Compression | `zlib1g-dev`, `libbz2-dev`, `liblzma-dev` | Archive core codecs ([data-model](03-data-model.md)). |
| Optional | `libfluidsynth-dev` | MIDI synth; optional feature. |

**t64 ABI caveat.** On the `time_t` 64-bit transition, many runtime packages were **renamed** (e.g.
`libfoo1` → `libfoo1t64`). `-dev` package *names* are largely stable, but **binary runtime deps and
`Depends:` in our own `.deb` differ across releases** — this is the single biggest per-release
packaging hazard (§9).

---

## 9. OS image: Bookworm vs Trixie — pin one

elads **pins a single Pi OS release per build** rather than trying to be simultaneously correct
across both. The two candidates and their material differences:

| Aspect | **Bookworm** (Debian 12) | **Trixie** (Debian 13) |
|--------|--------------------------|------------------------|
| Mesa (V3D/V3DV) | **~23.2** | **~25.0** |
| wxWidgets | **~3.2.2** | **~3.2.6** |
| SDL | SDL2 | **SDL3** available |
| GLES/Vulkan ext sets | older V3D ext set; **Vulkan 1.2-era** on 23.2 | newer V3D ext set; **Vulkan 1.3 conformant** (needs Mesa 24.3+) |
| ABI / package names | pre-/early-t64 | **t64 ABI** fully in effect → renamed runtime libs |
| SDL / package names | `libsdl2-*` | `libsdl3-*`, various `*t64` runtime libs |

Implications for elads:
- **Vulkan 1.3 conformant V3DV requires Mesa 24.3+**, i.e. **Trixie**, not Bookworm. The RAL v1 does
  not use Vulkan, so this does not gate v1 — but a future Vulkan backend would pin Trixie.
- **GLES 3.1 is conformant on both** (V3D 7.x); the *extension* sets differ, so any optional
  extension probe (§3.2, compute/SSBO paths) must be runtime-detected, never assumed by release.
- **Packaging differs by release** because of t64 renames (§8, §10). One `.deb` does not serve both.

> **Decision:** primary target is whichever release the current Pi OS ships to end users; the CI
> matrix and `.deb` outputs are tagged per release. When in doubt, prefer the release with **wx
> closest to 3.2.9** and **Mesa new enough for the desired ext set**. Record the pin in
> [ADR](../decisions/README.md) / [roadmap](../roadmap.md).

---

## 10. Packaging

Two distribution channels, both aarch64:

### 10.1 CPack `.deb` — per Pi OS release

- Produced by **CMake/CPack** as part of the build ([build-test-pipeline](07-build-test-pipeline.md)).
- **One `.deb` per Pi OS release**, because the **t64 ABI renames** mean the `Depends:` line
  (runtime lib package names) differs between Bookworm and Trixie (§8, §9). A `.deb` built against
  Trixie's `*t64` runtime libs will not resolve dependencies cleanly on Bookworm and vice-versa.
- CPack `CPACK_DEBIAN_PACKAGE_DEPENDS` is generated per-release from the resolved runtime libs;
  do not hand-freeze it across releases.

### 10.2 Flatpak — Flathub, `org.freedesktop.Platform.GL`

- The **distro-agnostic** channel: one Flatpak runs on any Pi OS release (and desktop aarch64),
  side-stepping the t64/release skew of `.deb`.
- **GPU access** for V3D under Flatpak comes from the **`org.freedesktop.Platform.GL`** runtime
  extension — this is what lets the sandboxed app reach the host's Mesa V3D driver for a hardware
  GLES 3.1 context. Without the correct GL extension the app would fall back to software.
- App ID: `org.elads.Editor` (illustrative), runtime `org.freedesktop.Platform` + the `GL`
  extension; Wayland (labwc) + EGL socket permissions in the manifest.

---

## 11. Practical performance guidance (editor on V3D)

The VideoCore VII is a competent but modest tile GPU. Concrete guidance for the map/graphics editor;
the full V3D perf model is in [render-abstraction](02-render-abstraction.md) §7:

- **Batch aggressively.** Draw-call count is a primary cost on V3D. Merge 2D map-view primitives
  (lines, vertices, things) into few instanced/batched draws rather than per-object calls.
- **Avoid mid-frame FBO read-modify-write.** TBDR pays a full tile store/reload on framebuffer
  feedback. Do post/compositing in separate passes, not by re-reading the current target.
- **Minimise depth/stencil state churn** in the 3D view; sort by state, then by texture.
- **Respect thermals for UX.** A 60 fps 3D view that soft-throttles to 35 fps after 10 min is a
  regression users feel; profile *sustained* (§4), and cap the idle map-view redraw (redraw on
  change, not on a free-running loop) to keep the SoC cool during editing.
- **HiDPI cost.** 4K panels quadruple fragment work; offer a render-scale control for the 3D view.
- **Prefer GLES 3.1 compute for CPU-heavy GPU-friendly work** only where a CPU fallback exists
  (probe at runtime, §3.2) — e.g. optional GPU-assisted sector fill; keep earcut.hpp CPU
  triangulation as the baseline ([map-editor](04-map-editor.md)).

---

## 12. Cross-references

- [render-abstraction](02-render-abstraction.md) — the RAL, GLES 3.1 lowering, V3D perf model, EGL/GBM.
- [architecture](01-architecture.md) — the 9-module layout and why render is the replaced subsystem.
- [overview](00-overview.md) — strategy, dependency list, wx/GL version decisions.
- [build-test-pipeline](07-build-test-pipeline.md) — CMake presets, CI, CPack, toolchain build.
- [map-editor](04-map-editor.md) — 2D/3D views that stress the GPU; earcut triangulation.
- [roadmap](../roadmap.md) / [risks](../risks.md) — OS-pin decision, wx 3.2.9 availability risk, R7.
- [decisions](../decisions/README.md) — ADRs for renderer replacement and OS/toolchain pins.
