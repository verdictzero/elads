# render — Render Abstraction Layer (the core investment)

**The one subsystem elads writes new.** A thin device/context/viewport/shader/
buffer/texture interface that every visual module draws through, so nothing
hard-depends on a specific GL version.

- `backend/` — the abstract interface (headers) + shared helpers.
- `gles/` — **primary** backend: OpenGL ES 3.1 / GLSL ES 3.10. The only conformant
  hardware-accelerated path on the Pi 5's VideoCore VII.
- `desktopgl/` — fallback backend: desktop GL 3.1 core (and a Zink-over-Vulkan variant).

All legacy GL idioms from SLADE (`glBegin`/`GL_QUADS`, display lists, fixed-function
fog, `GL_POINT_SPRITE`, double-precision verts) are **lowered** here to shader
triangles / `gl_PointCoord` / UBO fog / floats. Reference: GZDoom
`src/common/rendering/gles`. See `docs/design/02-render-abstraction.md`.
