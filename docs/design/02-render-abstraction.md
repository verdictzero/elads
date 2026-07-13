# Render Abstraction Layer (GLES 3.1)

> The one subsystem elads *replaces* rather than reuses. This document defines the backend
> interface, the legacy-GL → GLES 3.1 lowering rules, shader conventions, context creation, and the
> V3D tile-GPU performance model. Read [architecture](01-architecture.md) first; the Pi hardware
> facts live in [rpi5-target](09-rpi5-target.md).

---

## 1. Why this layer exists

elads forks SLADE3 and **reuses ~80%** of it (Archive core, Scintilla text editing, Lua/sol2,
graphics/TEXTUREx, wxAUI shell — see [overview](00-overview.md)) but **replaces exactly one
subsystem: the renderer.** SLADE `master` requires **desktop GL 3.3** and its map editor has *no*
software fallback; UDB's OpenTK renderer needs **desktop GL 3.2 core**. Both sit *above* the Pi 5's
native desktop-GL ceiling and would drop to `llvmpipe` (software) on V3D.

The Render Abstraction Layer (RAL) is a thin, explicit interface that all elads rendering
(2D map view, 3D map view, graphics previews, texture composition) is written against **once**, and
which is then satisfied by a **GLES 3.1 primary backend** plus **desktop-GL / Zink fallback
backends**. Nothing above the RAL calls OpenGL directly.

### The Pi GL ceiling (recap)

VideoCore VII, Mesa **V3D 7.x**. These are the *native, conformant* caps — desktop GL 3.3/4.x will
**never** exist on this GPU (source: Mesa V3D driver, `docs.mesa3d.org`; Raspberry Pi 5 docs,
`raspberrypi.com`):

| API surface        | Version on Pi 5 | Shading language | Notes |
|--------------------|-----------------|------------------|-------|
| Desktop GL core    | **3.1**         | GLSL **1.40**    | No `layout(location)` on varyings, no compute. |
| Desktop GL compat  | 2.1             | GLSL 1.20        | Legacy fixed-function; irrelevant to us. |
| **GLES**           | **3.1**         | **GLSL ES 3.10** | **CONFORMANT.** Compute shaders, SSBO, explicit `layout(location=)`, UBO std140, MRT. |
| Vulkan             | 1.3 (V3DV)      | SPIR-V           | Conformant, Mesa 24.3+. Not used by the RAL v1; see §9. |

**Primary render target = GLES 3.1 (`#version 310 es`).** It is a strict *superset* of the useful
feature set of desktop GL 3.1 on this hardware (compute, SSBO, explicit attribute locations), so we
author to GLES 3.1 and *down-lower* to desktop GL 3.1 for the fallback rather than the reverse.

> **Design rule.** The RAL exposes only features present in **both** GLES 3.1 and desktop GL 3.1.
> That common denominator is large enough for a Doom editor: VBO/VAO, UBO, textures + arrays, FBO
> with MRT, GLSL with UBOs. Compute/SSBO are *optional* capabilities probed at runtime (§8) and used
> only where a CPU fallback exists.

---

## 2. Module layout

Lives in `src/render/` (module 3 of 9; see [architecture](01-architecture.md)):

```
src/render/
  backend/          # RAL interface — pure virtual, zero GL includes leak upward
    IRenderDevice.h
    IRenderContext.h
    Resources.h     # ShaderProgram, GpuBuffer, Texture, Framebuffer handles
    Capabilities.h  # probed feature flags
    Enums.h         # formats, primitive topology, blend/depth state
  gles/             # PRIMARY backend — GLES 3.1 via EGL
  desktopgl/        # FALLBACK backend — desktop GL 3.1 (and Zink shim, §8)
  common/           # shared: mat4 math, shader preprocessor, atlas packer
```

Callers (`mapeditor/view2d`, `mapeditor/view3d`, `graphics`) `#include "render/backend/…"` only.
GL headers (`<GLES3/gl31.h>`, `<epoxy/gl.h>`) appear **only** under `gles/` and `desktopgl/`.

---

## 3. The backend interface

Illustrative C++17 signatures — **shape and intent, not a frozen ABI.** Handles are opaque,
move-only RAII wrappers over backend-native names (GL object ids today, could be Vulkan handles
later). Errors surface via `expected`-style returns for creation and asserts/logs for hot-path
misuse.

### 3.1 Device and context

`IRenderDevice` owns GPU-lifetime state and creates resources. `IRenderContext` is the per-frame
command recorder. On single-threaded GL they are backed by one live context, but the split keeps the
door open for a future command-buffer backend.

```cpp
// ILLUSTRATIVE — src/render/backend/IRenderDevice.h
namespace render {

struct DeviceInfo {
    std::string vendor, renderer, versionStr;   // GL_VENDOR / GL_RENDERER / GL_VERSION
    Backend     backend;                         // Gles | DesktopGl | Zink
    Capabilities caps;                           // see §8
};

class IRenderDevice {
public:
    virtual ~IRenderDevice() = default;
    virtual const DeviceInfo& info() const = 0;

    // Resource creation (return null handle on failure; log reason).
    virtual ShaderProgram createProgram(const ProgramDesc&)      = 0;
    virtual GpuBuffer     createBuffer (const BufferDesc&)       = 0;
    virtual Texture       createTexture(const TextureDesc&)      = 0;
    virtual Framebuffer   createFramebuffer(const FramebufferDesc&) = 0;

    // Per-frame context (one per canvas). Makes the underlying GL/EGL context current.
    virtual IRenderContext& acquireContext() = 0;
    virtual void            present()        = 0;   // eglSwapBuffers / SwapBuffers
};

} // namespace render
```

```cpp
// ILLUSTRATIVE — src/render/backend/IRenderContext.h
class IRenderContext {
public:
    virtual void setViewport(const Viewport&) = 0;
    virtual void clear(const ClearDesc&)      = 0;   // color/depth/stencil + tile hint (§7)

    virtual void bindFramebuffer(const Framebuffer&) = 0;  // null handle => default FBO
    virtual void bindProgram(const ShaderProgram&)   = 0;
    virtual void bindUniformBuffer(uint32_t binding, const GpuBuffer&,
                                   size_t offset, size_t size) = 0;
    virtual void bindTexture(uint32_t unit, const Texture&, const Sampler&) = 0;
    virtual void bindVertexBuffer(const GpuBuffer&, const VertexLayout&)    = 0;
    virtual void bindIndexBuffer (const GpuBuffer&, IndexType)              = 0;

    virtual void setPipelineState(const PipelineState&) = 0;  // blend/depth/cull/lineWidth intent

    virtual void draw       (Topology, uint32_t first, uint32_t count) = 0;
    virtual void drawIndexed(Topology, uint32_t indexCount, uint32_t firstIndex) = 0;
};
```

### 3.2 Viewport and pipeline state

```cpp
// ILLUSTRATIVE — src/render/backend/Enums.h / Resources.h
struct Viewport { int x, y, w, h; float minDepth = 0.f, maxDepth = 1.f; };

enum class Topology { Triangles, TriangleStrip, Lines, LineStrip, Points };
enum class IndexType { U16, U32 };

struct PipelineState {
    bool     depthTest = true, depthWrite = true;
    CompareOp depthOp  = CompareOp::LessEqual;
    BlendMode blend    = BlendMode::None;      // None | AlphaOver | Additive
    CullMode  cull     = CullMode::Back;
    float     lineWidth = 1.0f;                // clamped to GL_ALIASED_LINE_WIDTH_RANGE (§6, L-9)
};
```

### 3.3 GPU buffers (VBO / UBO / IBO)

One buffer type, tagged by usage. UBOs are laid out **std140** on the C++ side (see §5). `Dynamic`
buffers are the map editor's per-frame geometry; `Persistent` are display-list replacements (§6,
L-3).

```cpp
// ILLUSTRATIVE — src/render/backend/Resources.h
enum class BufferKind  { Vertex, Index, Uniform };
enum class BufferUsage { Static, Dynamic, Persistent };

struct BufferDesc {
    BufferKind  kind;
    BufferUsage usage;
    size_t      bytes;
    const void* initialData = nullptr;
};

class GpuBuffer {                       // move-only RAII over a GL name
public:
    void  update(size_t offset, size_t bytes, const void* src);  // glBufferSubData / map
    void* mapRange(size_t offset, size_t bytes, MapFlags);       // GLES3 glMapBufferRange
    void  unmap();
    bool  valid() const;
};
```

### 3.4 Shader programs

```cpp
// ILLUSTRATIVE
struct ProgramDesc {
    std::string_view vertexSrc;      // GLSL ES 3.10 source (see §5)
    std::string_view fragmentSrc;
    std::string_view computeSrc = {}; // optional; only if caps.compute
    std::string      debugName;
};

class ShaderProgram {
public:
    bool valid() const;
    // Uniform *blocks* are bound by index (std140). Loose uniforms are avoided in v1;
    // where used (e.g. a single mat4), looked up + cached by name at build time.
    int  uniformBlockIndex(std::string_view) const;
    int  uniformLocation  (std::string_view) const;
};
```

### 3.5 Textures and samplers

```cpp
// ILLUSTRATIVE
enum class TexTarget { Tex2D, Tex2DArray };     // atlases/arrays are first-class (§7)
enum class TexFormat { RGBA8, RGB8, R8, SRGB8_A8, Depth24Stencil8 };

struct TextureDesc {
    TexTarget target = TexTarget::Tex2D;
    TexFormat format = TexFormat::RGBA8;
    int width, height, layers = 1, mipLevels = 1;
    const void* initialData = nullptr;
};

struct Sampler { Filter min = Filter::Nearest, mag = Filter::Nearest;
                 Wrap wrapU = Wrap::Repeat, wrapV = Wrap::Repeat; };

class Texture { public: bool valid() const; void upload(int level, int layer, Rect, const void*); };
```

Doom graphics are paletted/nearest by default (crisp texels); `Nearest` is the sane default.

### 3.6 Framebuffers

```cpp
// ILLUSTRATIVE
struct FramebufferDesc {
    std::vector<Texture> colorAttachments;   // MRT supported on GLES 3.1
    Texture              depthStencil;       // Depth24Stencil8
    bool                 forHeadless = false; // offscreen render for CI/thumbnails (§7)
};
class Framebuffer { public: bool valid() const; int width() const, height() const; };
```

---

## 4. Legacy-GL → GLES 3.1 lowering rules

SLADE's older render paths and the general Doom-editor idiom lean on **immediate mode and
fixed-function GL**. None of that exists in GLES 3.1 (or in a GL core profile). Every legacy pattern
maps to an explicit modern equivalent. **The canonical reference implementation for these lowerings
is GZDoom's GLES backend, `src/common/rendering/gles/` (`GZDoom gles`)** — it already solved the
"legacy OpenGL engine → GLES" problem for a Doom renderer, and we mirror its choices.

| # | Legacy / fixed-function GL | GLES 3.1 lowering | Notes |
|---|----------------------------|-------------------|-------|
| **L-1** | `glBegin`/`glEnd` immediate mode | Build a vertex array in a `Dynamic` VBO, `draw()` | Batch per-material; never one draw per primitive. |
| **L-2** | `GL_QUADS` | Emit **2 triangles** per quad → `GL_TRIANGLES` (indices `0-1-2, 0-2-3`) | GLES has no `GL_QUADS`. Do it at buffer-build time. |
| **L-3** | Display lists (`glNewList`/`glCallList`) | **Persistent VBOs** built once, re-`draw()`n each frame | Static level geometry & UI glyph runs become persistent buffers. |
| **L-4** | Fixed-function fog (`glFog*`) | Fog params in a **UBO** + compute in the **fragment shader** | `FogParams{ color, start, end/density, mode }`; see §5 std140 block. |
| **L-5** | `GL_POINT_SPRITE` + `gl_PointSize` | Small handles: native `GL_POINTS` + `gl_PointCoord` in FS. Larger / arbitrary-size sprites: **textured quads** (2 tris) | The 2D editor draws vertex handles as native `GL_POINTS` (small, within V3D's point-size range) — see [map-editor](04-map-editor.md) §2.2.1. Thing sprites and any point exceeding that range use quads (§2.2.4). |
| **L-6** | `GL_ALPHA_TEST` / `glAlphaFunc` | `if (color.a < uThreshold) discard;` in fragment shader | Doom masked textures. Note: `discard` disables early-Z — batch alpha-tested geometry separately (§7). |
| **L-7** | Matrix stack (`glRotate/glTranslate/glScale`, `glPushMatrix`) | **App-side `mat4`** (our own math, `render/common/mat4`) → uniform/UBO | We own model/view/proj; no GL matrix state exists. Column-major, GL convention. |
| **L-8** | Double-precision vertices | **`float` (fp32)** vertices; keep world origin near geometry | V3D has no fp64 in shaders. For huge Doom maps, translate to a local origin to preserve precision. |
| **L-9** | `GL_LINE_SMOOTH` / wide lines (`glLineWidth > 1`) | Thin 1px lines: native `GL_LINES`. Wide / AA lines: shader-based AA as **quads/triangle-strips** with a falloff in FS (or MSAA) | GLES aliased line width is often capped at `[1,1]`, so the 2D grid/linedef pass draws native 1px `GL_LINES` ([map-editor](04-map-editor.md) §2.2.2/§2.3); only width>1 highlights or explicitly AA'd lines expand to quads. |
| **L-10** | `glColor*` (current color) | Per-vertex **color attribute** (`layout(location=…)`) or a **uniform** | Flat fills → uniform; gradient/selection tints → vertex attribute. |
| **L-11** | `glEnableClientState`/`glVertexPointer` (client arrays) | **VAO + VBO** with `glVertexAttribPointer` | No client-side arrays in core/ES. |
| **L-12** | `glTexEnv` combiners | Do the math in the **fragment shader** | Modulate/replace/add become explicit GLSL. |
| **L-13** | Built-in `gl_FragColor`, `gl_Vertex`, `ftransform()` | Explicit `out vec4` + `layout(location)` inputs + own MVP | Required by `#version 310 es`. |

### 4.1 Worked example — L-1 + L-2 + L-10 together

A legacy filled quad with per-corner color:

```cpp
// LEGACY (does not exist on GLES):
// glBegin(GL_QUADS);
//   glColor4f(r,g,b,a); glVertex2f(x0,y0); ... 4 verts ... glEnd();
```

lowers to: build `{pos.xy, color.rgba}` for 4 vertices in a `Dynamic` VBO, index them as two
triangles, bind a program whose VS reads `layout(location=0) in vec2 aPos;
layout(location=1) in vec4 aColor;`, and `drawIndexed(Topology::Triangles, 6, …)`.

---

## 5. Shader authoring conventions

All elads shaders are **GLSL ES 3.10** authored to compile *unchanged* on the GLES backend and with
a one-line version-header rewrite on the desktop-GL-3.1 fallback (which uses GLSL 1.40 — the common
preprocessor in `render/common` swaps `#version 310 es` for `#version 140` and injects
`precision`-stripping + `in/out` aliases as needed).

Conventions (enforced by review + the shared preprocessor):

- **Version first line:** `#version 310 es`.
- **Precision qualifiers are mandatory** in ES. Default to:
  - Vertex shader: `precision highp float;` (positions/matrices need fp32 range).
  - Fragment shader: `precision mediump float;` for color work; `highp` for UV/world-space math
    (fog distance, depth reconstruction) to avoid banding on V3D.
  - Integer samplers/indices: `precision highp int;` where used.
- **Explicit `layout(location=)`** on every vertex attribute and fragment output. GLES 3.1 supports
  this natively; it makes VAO setup match the shader without name lookups.
- **UBOs, std140, explicit binding points.** No loose per-draw uniforms in hot paths. Binding-point
  registry is fixed project-wide so C++ `struct`s and GLSL blocks never drift:

  | binding | block name | contents |
  |---------|-----------|----------|
  | 0 | `CameraUBO`   | `mat4 viewProj; vec4 camPos;` |
  | 1 | `ObjectUBO`   | `mat4 model; vec4 tint;` |
  | 2 | `FogUBO`      | `vec4 fogColor; float fogStart; float fogEnd; int fogMode; float _pad;` |
  | 3 | `MaterialUBO` | `vec4 params; float alphaThreshold; ...` |

- **std140 discipline.** `vec3` is padded to 16 bytes; arrays' element stride is rounded to
  `vec4`. C++ mirror structs use explicit `_pad` members and a `static_assert(sizeof(...) % 16 == 0)`.
- **No `gl_FragColor`/`gl_Vertex`/`ftransform`** (L-13). Compute clip-space explicitly:

```glsl
#version 310 es
precision highp float;
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aColor;
layout(std140, binding = 0) uniform CameraUBO { mat4 viewProj; vec4 camPos; };
out vec4 vColor;
void main() {
    vColor = aColor;
    gl_Position = viewProj * vec4(aPos, 0.0, 1.0);
}
```

```glsl
#version 310 es
precision mediump float;
in vec4 vColor;
layout(location = 0) out vec4 oColor;
layout(std140, binding = 3) uniform MaterialUBO { vec4 params; float alphaThreshold; };
uniform sampler2D uTex;
in highp vec2 vUV;
void main() {
    vec4 c = texture(uTex, vUV) * vColor;
    if (c.a < alphaThreshold) discard;   // L-6 alpha test
    oColor = c;
}
```

The desktop-GL-3.1 fallback compiles the same source after the preprocessor rewrites the header;
GLSL 1.40 accepts `in`/`out`, UBOs, and `texture()`, and lacks only the `precision` keyword (which
the preprocessor strips) and layout-on-varyings (not used).

---

## 6. Context creation

The Pi runs **Wayland-first (labwc)**; under native Wayland **there is no GLX**, so all contexts come
via **EGL** (see [rpi5-target](09-rpi5-target.md)). Two paths:

### 6.1 On-screen: `wxGLCanvas` EGL path

wxWidgets **3.2.9+** added the EGL 1.4 backend + Wayland HiDPI GL fixes we depend on (older wx used
GLX and broke under Wayland). We request a **GLES 3.1** context explicitly.

```cpp
// ILLUSTRATIVE — src/render/gles/GlesCanvas.cpp
wxGLAttributes attrs;
attrs.PlatformDefaults().RGBA().DoubleBuffer().Depth(24).Stencil(8).EndList();

wxGLContextAttrs ctxAttrs;
ctxAttrs.PlatformDefaults()
        .OGLVersion(3, 1)                 // 3.1 …
        .ES()                             // …as OpenGL ES  → EGL_OPENGL_ES_API, ES 3.1
        .EndList();

auto* canvas  = new wxGLCanvas(parent, attrs);
auto* context = new wxGLContext(canvas, /*other=*/nullptr, &ctxAttrs);
// context->IsOK() must be true; else fall through to backend selection (§8).
```

Notes:

- wx 3.2.9 routes this through EGL on GTK3/Wayland automatically; on X11/desktop it may still get an
  EGL or GLX context — we don't care which, only that `ES 3.1` is honored.
- Function loading uses **libepoxy** (already a SLADE dependency), which resolves ES + desktop
  entrypoints transparently, so `gles/` and `desktopgl/` share the same loader.

### 6.2 Off-screen: EGL + GBM for headless render

Needed for **CI frame-hash smoke tests** and **thumbnail generation** (see
[build-test-pipeline](07-build-test-pipeline.md)) — no window, no compositor. We create a
surfaceless (or GBM-backed) EGL context and render into an offscreen `Framebuffer`
(`forHeadless = true`), then `glReadPixels` for hashing/PNG export.

```cpp
// ILLUSTRATIVE — src/render/gles/HeadlessEgl.cpp
int fd = open("/dev/dri/renderD128", O_RDWR);          // V3D render node
gbm_device* gbm = gbm_create_device(fd);
EGLDisplay dpy  = eglGetPlatformDisplay(EGL_PLATFORM_GBM_KHR, gbm, nullptr);
eglInitialize(dpy, nullptr, nullptr);
eglBindAPI(EGL_OPENGL_ES_API);
// choose config, then:
const EGLint ctxAttr[] = { EGL_CONTEXT_MAJOR_VERSION, 3,
                           EGL_CONTEXT_MINOR_VERSION, 1, EGL_NONE };
EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctxAttr);
eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx);   // surfaceless; render to FBO
```

On CI runners (`ubuntu-24.04-arm`, no V3D) the same path runs against **`llvmpipe` via EGL surfaceless
+ `LIBGL_ALWAYS_SOFTWARE=1`** — correctness (frame-hash) does not require the real GPU, only a
conformant GLES 3.1 implementation. Mesa's software GLES is conformant, so hashes are stable.

---

## 7. V3D tile-GPU performance guidance

VideoCore VII / V3D is a **tile-based deferred renderer (TBDR)** and is **fill-rate limited**, not
geometry limited (source: Broadcom V3D architecture; Mesa V3D driver docs, `docs.mesa3d.org`). The
GPU bins geometry into on-chip tiles, then shades each tile from fast tile memory. This inverts some
desktop-GL instincts. Design the RAL usage accordingly:

- **Minimize render-target / FBO switches.** Every `bindFramebuffer` to a new target forces a **tile
  flush** (store the current tile buffer to memory, load the next). Batch all draws for one target
  together; do not ping-pong between FBOs mid-frame. This is the single biggest V3D win.
- **`clear` beats `load`.** Starting a render pass with a full `clear()` lets the driver mark tiles
  as "don't load previous contents," avoiding a memory read per tile. Prefer clearing over drawing
  over stale contents. The `ClearDesc` carries a hint the backend maps to `glClear` at pass start
  (and, where available, `glInvalidateFramebuffer` for depth/stencil that needn't be stored).
- **Avoid overdraw.** Fill-rate is the bottleneck. Draw **opaque front-to-back** so early-Z rejects
  occluded fragments; keep the 3D map view's opaque pass depth-tested and roughly sorted. Every
  fragment shaded twice is wasted fill.
- **Be careful with blending and large transparent overdraw.** Transparent/alpha-blended geometry
  can't early-Z and must be shaded in order; a full-screen stack of translucent surfaces (Doom fake
  contrast, translucent midtextures, big additive sprites) is exactly the worst case. Draw
  transparents **back-to-front, after** opaques, and keep the count down.
- **`discard` (alpha test, L-6) disables early-Z** for that draw. Group masked textures into their
  own pass; don't interleave them with cheap opaque geometry.
- **Prefer texture atlases / `Tex2DArray`.** Doom composites hundreds of small patches/flats. Fewer,
  larger textures mean fewer bind/state changes and better tile-cache locality. The `common/` atlas
  packer builds a per-map texture atlas; wall/flat variants that share dimensions go into a
  `Tex2DArray` layer instead of separate textures.
- **Keep MSAA modest.** MSAA multiplies tile-memory pressure. If used for the 2D grid/line AA
  (alternative to the L-9 quad-expansion approach), cap at 4x and confine it to the pass that needs
  it.
- **Small varyings, `mediump` where safe.** Interpolators consume tile bandwidth; use `mediump` for
  colors (§5) and keep the varying count low.

```
V3D frame shape (3D map view) — one pass, minimal target switches:

  [clear default FBO]
     └─ opaque geometry pass   (depth test on, front-to-back, no discard)   ← early-Z friendly
     └─ masked/alpha-test pass (discard; grouped)                            ← early-Z off
     └─ translucent pass       (blend on, back-to-front, depth-write off)    ← fill-rate risk
  [present / swap]

  Offscreen thumbnails (§6.2) render into ONE headless FBO, then glReadPixels once.
```

---

## 8. Backend selection, probing, and fallback

The RAL supports three backends, chosen at startup and overridable by the env var
**`ELADS_RENDER_BACKEND`**:

| value       | backend            | when |
|-------------|--------------------|------|
| `gles`      | GLES 3.1 via EGL   | **Default / primary.** The Pi 5 and any conformant GLES 3.1. |
| `desktopgl` | Desktop GL 3.1     | Fallback for desktops where a native GLES context is awkward but GL 3.1 exists. |
| `zink`      | GL-on-Vulkan (Mesa Zink → V3DV) | Experimental: routes GL/GLES through Vulkan 1.3. Opt-in for testing the Vulkan path. |

### Selection algorithm

```
1. Read ELADS_RENDER_BACKEND.
     - If set to a valid value → try ONLY that backend; if it fails, hard-error
       (explicit override must not silently fall back — surfaces misconfiguration).
     - If unset → auto order below.
2. Auto order (unset):
     a. Try GLES 3.1  (wxGLCanvas .ES().OGLVersion(3,1), §6.1)   → use if context OK + caps pass.
     b. Try desktop GL 3.1 core                                   → use if OK.
     c. Refuse to start the map editor on software GL; show a
        diagnostic (never silently run on llvmpipe — the whole
        project exists to avoid that). Non-GPU views may degrade.
3. After a context is live, PROBE capabilities and store in DeviceInfo.caps.
```

### Capability probe (`Capabilities`)

Queried once post-context via `glGetString`/`glGetIntegerv`/extension list:

```cpp
// ILLUSTRATIVE — src/render/backend/Capabilities.h
struct Capabilities {
    int  glslVersion;        // 310 (es) or 140 (desktop)
    bool compute;            // GLES 3.1 core yes; desktop-GL-3.1 core NO → CPU fallback
    bool ssbo;               // pairs with compute
    bool textureArray;       // Tex2DArray
    int  maxTextureSize;     // GL_MAX_TEXTURE_SIZE
    int  maxUboBindings;     // GL_MAX_UNIFORM_BUFFER_BINDINGS
    float maxLineWidth;      // GL_ALIASED_LINE_WIDTH_RANGE[1]  (usually 1 on V3D → L-9)
    bool invalidateFbo;      // glInvalidateFramebuffer for tile store-avoidance (§7)
};
```

**Important asymmetry:** compute/SSBO exist in **GLES 3.1 core** but **not** in desktop-GL-3.1 core.
Any RAL feature that uses compute (e.g. a GPU sector-triangulation experiment) **must** have a CPU
fallback (we already use `earcut.hpp` on the CPU for sector triangulation — see
[map-editor](04-map-editor.md)). The RAL never *requires* a capability the fallback backend lacks.

---

## 9. Relationship to Vulkan / future work

The Pi 5 has **conformant Vulkan 1.3 (V3DV)**, and the RAL split (device vs. context, §3.1) is
deliberately Vulkan-shaped so a native `vulkan/` backend could be added later. For **v1 we do not**
write a Vulkan backend — GLES 3.1 is enough for a Doom editor and is far less code. The `zink`
option (§8) already lets us exercise the *Vulkan driver* (V3DV) underneath a GL API for testing,
without committing to a second hand-written backend. Revisit only if profiling shows GLES driver
overhead dominating (see [risks](../risks.md)).

---

## 10. Cross-references

- Hardware ceiling, Wayland/EGL specifics, V3D details → [rpi5-target](09-rpi5-target.md).
- Where the RAL sits among the 9 modules → [architecture](01-architecture.md).
- Who calls the RAL: 2D/3D map views → [map-editor](04-map-editor.md); graphics/texture previews →
  [graphics-texture-editor](06-graphics-texture-editor.md).
- Headless render in CI (frame-hash, thumbnails) → [build-test-pipeline](07-build-test-pipeline.md).
- **Lowering reference implementation:** GZDoom `src/common/rendering/gles/` (`GZDoom gles`) — the
  authoritative example of a Doom renderer lowered from legacy OpenGL to GLES.
```
