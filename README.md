# elads

**A unified Doom / ZDoom / GZDoom development environment for Linux — Raspberry Pi 5 first.**

`elads` (SLADE spelled backwards) aims to bring the two tools every Doom modder relies on
into a **single native GPLv3 application** that runs **hardware-accelerated on a Raspberry Pi 5**:

- the **resource / archive / graphics / code editing** of [SLADE3](https://github.com/sirjuddington/SLADE), and
- the **2D map editing + 3D visual-mode** authoring of [Ultimate Doom Builder](https://github.com/UltimateDoomBuilder/UltimateDoomBuilder).

> **Status: pre-alpha.** A working **GUI/GL-free core** (19 tests) plus the first **desktop
> OpenGL renderer** (Linux x86-64; the Pi/GLES port comes later behind the same abstraction).
> The core covers **WAD** + **PK3/zip** archives + **entry-type detection**; the map data
> model with **classic Doom binary** + **UDMF** I/O (lossless), **earcut triangulation**,
> **validation**, **sector slopes** (slope things + `Plane_Align`), and **save-back** into a
> WAD; an **editing layer** — undoable operations (move/split/flip, properties, things, sector
> authoring) + **picking/selection**; palette, **Doom picture**/**flat** codecs,
> **TEXTUREx/PNAMES** and **composite-texture assembly**; PNG output; and an **`elads` CLI**.
> The desktop variant adds an **EGL/OpenGL backend** implementing the render abstraction,
> **`elads-render`** (draws **2D top-down** and **textured, slope-aware 3D visual-mode** views
> to a PNG headlessly — real wall textures + flats via the archive→TEXTUREx→GL path, proven on
> Mesa software GL), and **`elads-view`**, an interactive GLFW window running the same renderers
> (with a headless `--auto-screenshot` mode). See [`docs/`](docs/).

---

## Why this project exists

On the Raspberry Pi 5 there is no good single answer for Doom development:

- **Ultimate Doom Builder** is C#/.NET on Mono (slow, dev-unsupported on Linux) and its
  renderer needs **desktop OpenGL 3.2 core** — above what the Pi's GPU exposes natively.
- **SLADE3** fits the Pi far better (C++/wxWidgets, builds on aarch64) but its current
  `master` renderer now targets **desktop GL 3.3**, and its map editor is secondary to UDB's.

The Raspberry Pi 5's **VideoCore VII** GPU (Mesa V3D) caps **desktop OpenGL at 3.1**, but
offers **conformant OpenGL ES 3.1** and **conformant Vulkan 1.3**. So the winning move is:

> **Fork SLADE3, replace exactly one subsystem — the renderer — with a GLES 3.1 backend,
> and grow its map editor toward UDB parity.**

This reuses ~80% of a mature codebase and confines the hard, Pi-specific work to a single
render-abstraction layer. See [`docs/design/00-overview.md`](docs/design/00-overview.md).

## The environment (planned capabilities)

| Area | Capability | Inspired by |
|------|-----------|-------------|
| Archives | WAD / PK3 / PKE / PAK / GRP / RFF read+write, namespaced VFS | SLADE |
| Graphics | Doom gfx, flats, PNG/WebP, TEXTUREx & ZDoom `TEXTURES`, palette/COLORMAP | SLADE |
| Code | ZScript / DECORATE / ACS / MAPINFO editor with syntax highlighting | SLADE |
| Scripting | Lua automation + plugin API | SLADE |
| Maps (2D) | UDMF / Doom / Hexen geometry editing, sector draw, snapping, error checks | both |
| Maps (3D) | in-editor "visual mode" walkthrough with real textures, light, fog, slopes | UDB |
| Build/test | AJBSP + ZDBSP node building, `acc` ACS compile, one-click GZDoom playtest | both |

## Target hardware & platform

- **Primary:** Raspberry Pi 5, 8 GB, aarch64, Raspberry Pi OS (Wayland/labwc).
  Official **Active Cooler + 27 W USB-C PD** recommended.
- **Also:** general Linux desktop (x86-64 / arm64). A Zink-over-Vulkan / desktop-GL
  backend is planned for GL-3.3-class parity on capable GPUs.
- **Graphics target:** OpenGL **ES 3.1** primary; desktop GL 3.1 and Zink fallbacks.

See [`docs/design/09-rpi5-target.md`](docs/design/09-rpi5-target.md).

## Repository layout

```
docs/            Design documentation (start at docs/README.md)
  design/        Numbered design docs (architecture, renderer, formats, RPi5, …)
  decisions/     Architecture Decision Records (ADRs)
src/             Source tree skeleton, one dir per architecture module (README stubs)
  render/        Render Abstraction Layer + GLES / desktop-GL backends (the core investment)
  archive/       Archive / data core (reuse SLADE src/Archive)
  mapeditor/     2D + 3D map editor
  texteditor/    Scintilla code editor
  graphics/      Graphics & texture editor
  pipeline/      Node build + ACS compile + playtest pipeline
  scripting/     Lua/sol2 scripting + plugin system
  ui/            wxAUI docking shell
res/             Runtime resources (data-driven lexer configs, GLES shaders, icons)
cmake/           CMake helper modules
scripts/         Pi bootstrap, third-party toolchain build, GL probe
.github/         CI (arm64 GitHub Actions)
```

## Building

**Build + test the GUI/GL-free core today** (no wxWidgets/OpenGL needed — any C++17 toolchain):

```sh
cmake --preset core          # configure the core library + tests
cmake --build --preset core  # build (also builds the `elads` CLI)
ctest --preset core          # run the test suite
```

**Build the desktop OpenGL variant + render a map to PNG** (needs `libepoxy-dev libegl-dev
libgl-dev libgles-dev libgbm-dev libgl1-mesa-dri` — see `scripts/bootstrap-desktop.sh`):

```sh
cmake --preset desktop && cmake --build --preset desktop
ctest --preset desktop                                   # incl. headless GL render test
./build/desktop/src/elads-render render-demo   demo.png          # 2D top-down -> PNG
./build/desktop/src/elads-render render-demo3d demo3d.png        # 3D visual mode -> PNG
./build/desktop/src/elads-render render-demo-slope3d slope.png   # 3D with a sloped floor
./build/desktop/src/elads-render render-map   DOOM.wad E1M1 e1m1.png 1200 900
./build/desktop/src/elads-render render-map3d DOOM.wad E1M1 e1m1_3d.png 1200 900
```

**Run the interactive viewport** (`elads-view`, needs `libglfw3-dev`; WASD + mouse-look in 3D,
drag-pan + wheel-zoom in 2D, `Tab` toggles, `F12` screenshots):

```sh
./build/desktop/src/elads-view --demo-slope                      # a window on a demo map
./build/desktop/src/elads-view DOOM.wad E1M1                     # a window on a real map
xvfb-run -a ./build/desktop/src/elads-view --demo --auto-screenshot shot.png  # headless
```

Try the CLI (writes a sample WAD with a binary and a UDMF map, then inspects it):

```sh
./build/core/src/elads demo-wad /tmp/demo.wad
./build/core/src/elads wad-info /tmp/demo.wad
./build/core/src/elads lump-types /tmp/demo.wad       # detected entry types
./build/core/src/elads map-info /tmp/demo.wad MAP01   # binary
./build/core/src/elads map-info /tmp/demo.wad MAP02   # UDMF
./build/core/src/elads demo-pk3 /tmp/demo.pk3         # write a sample PK3 (zip)
./build/core/src/elads pk3-info /tmp/demo.pk3         # list entries + namespaces
```

Validate just the scaffold (no code compiled):

```sh
cmake --preset stub && cmake --build --preset stub   # prints "elads scaffold OK"
```

When the GUI/OpenGL layers land (Phase 1), the flow on a Pi 5 will be:

```sh
scripts/bootstrap-pi.sh          # install apt dependencies (wx 3.2.x, GL/EGL, Lua, …)
scripts/build-toolchain.sh       # build AJBSP / ZDBSP / acc from source (aarch64)
cmake --preset pi-native         # configure
cmake --build --preset pi-native # build
```

## License

**GPLv3.** `elads` derives from SLADE3 (GPLv2-or-later), so the combined work ships under
the [GNU GPL v3](LICENSE). See [`docs/design/10-licensing.md`](docs/design/10-licensing.md)
for the full reuse/licensing analysis. `elads` is an independent community project and is not
affiliated with or endorsed by the SLADE, Ultimate Doom Builder, or GZDoom projects, or with
id Software / ZeniMax.

## Acknowledgements

Built on the shoulders of SLADE3 (Simon Judd & contributors), Ultimate Doom Builder, GZDoom,
AJBSP, ZDBSP, `acc`, earcut.hpp, sol2, and the wider Doom modding community.
