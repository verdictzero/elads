# render/gles — OpenGL ES 3.1 backend (primary)

The Pi-appropriate implementation of `render/backend`. Shaders are authored as
`#version 310 es` under `res/shaders/gles/`. Uses VAO/VBO/UBO, instancing, and
(optionally) compute. This is the default `ELADS_RENDER_BACKEND=gles`.
