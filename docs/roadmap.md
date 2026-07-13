# elads roadmap

Living plan. Dates are relative effort estimates, not commitments. Priorities:
**MVP** → **v1** → **later**. The guiding principle is *de-risk graphics first, reuse
SLADE aggressively, and phase 3D fidelity*.

## Phase 0 — On-device bring-up spike (2–4 weeks)

**Goal:** de-risk the single biggest unknown (graphics) before committing architecture.

- [ ] Build SLADE `master` on a real Pi 5 (target image pinned) against **system wx 3.2.x**.
- [ ] Capture `glxinfo`/`eglinfo`/`vulkaninfo` via `scripts/probe-gl.sh`; record GL core/compat,
      GLES, and Vulkan versions + `GL_MAX_DRAW_BUFFERS`. Attach to an issue.
- [ ] Prove a **GLES 3.1 context** via wxGLCanvas EGL and render one textured triangle in a viewport.
- [ ] Build **AJBSP, ZDBSP, acc** from source via `scripts/build-toolchain.sh` (aarch64).
- [ ] Run one **author → nodes → ACS → GZDoom-GLES playtest** loop by hand.
- [ ] **Decide:** GLES-only vs Zink-supported; pin the Pi OS image + Mesa version.

**Exit criteria:** confirmed GLES 3.1 rendering on-device + a working manual toolchain loop.

## Phase 1 — MVP: resource + text + graphics on GLES (2–3 months)

**Goal:** a forked SLADE running hardware-accelerated on the Pi 5 for non-map work plus basic
2D map editing.

- [ ] Fork SLADE; relicense combined project **GPLv3**; strip/guard desktop-GL-3.3-only paths.
- [ ] **Render Abstraction Layer** with a working GLES 3.1 backend; port gfx/texture preview and
      the 2D map canvas onto it.
- [ ] Reuse **Archive core**, **EntryType** detection, **Scintilla editor + data-driven lexers** verbatim.
- [ ] **wxAUI shell** with the idle-handling fix; boots and edits WAD/PK3, textures, and text on-device.
- [ ] **CMake presets** + **GitHub Actions `ubuntu-24.04-arm`** CI + **headless EGL** render smoke test.

**Exit criteria:** open/edit/save WAD & PK3, edit textures and ZScript, all HW-accelerated on the Pi.

## Phase 2 — v1: full 2D map authoring + build/test pipeline (3–4 months)

**Goal:** complete author→compile→playtest loop for UDMF/Doom/Hexen maps.

- [ ] 2D UDMF map editor to **UDB-comparable UX** (sector draw, snapping, error checks,
      Thing browser from ZScript + MAPINFO).
- [ ] Embedded **AJBSP** + bundled **ZDBSP** node building; **acc/bcc** ACS compilation.
- [ ] **One-click GZDoom-GLES playtest** with stdout error surfacing.
- [ ] **Lua/sol2** scripting console and first plugin API surface.
- [ ] earcut-based sector fill with cached VBOs.

**Exit criteria:** build a small map end-to-end and play it, without leaving elads.

## Phase 3 — v1: 3D visual mode to SLADE parity, then slopes (2–3 months)

**Goal:** usable in-editor 3D walkthrough on Pi hardware.

- [ ] 3D visual mode on GLES: texture-batched walls/flats, sector light, shader fog, frustum culling.
- [ ] **Slope** support via plane evaluation in the vertex shader.
- [ ] Visual-mode **texture painting/alignment** and thing placement.
- [ ] Performance pass for the V3D tile-based GPU (minimise FBO switches/overdraw).

**Exit criteria:** navigate a real map in 3D at interactive framerates on the Pi 5.

## Phase 4 — later: UDB-parity visuals, packaging, ecosystem (ongoing)

- [ ] Stacked **3D floors**, capped forward **dynamic lights**, **MODELDEF** models, sprites.
- [ ] **Zink/desktop-GL** backend for x86 Linux desktop parity.
- [ ] **CPack `.deb`** per Pi OS release + **Flatpak** on Flathub; `.desktop`/AppStream assets.
- [ ] **DoomMake/DECOHack** build orchestration and optional **Obsidian** generation.
- [ ] Optional **ZScript semantic layer** (tree-sitter grammar → diagnostics/go-to-def).

## Open decisions (tracked)

See the plan's open-decisions list. Current defaults: fork-SLADE · docs+scaffolding first ·
GPLv3 · GZDoom UDMF-first · staged 3D (SLADE-level for v1) · certify against Pi OS Trixie.
