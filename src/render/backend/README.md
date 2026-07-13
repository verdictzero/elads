# render/backend — abstract render interface

Toolkit- and API-agnostic interfaces: `IRenderDevice`, `IRenderContext`,
`Viewport`, `ShaderProgram`, `GpuBuffer`, `Texture`, `Framebuffer`, plus the
context-creation hooks (wxGLCanvas EGL on-screen; EGL/GBM offscreen for headless
CI/thumbnails). Concrete backends live in sibling directories.
