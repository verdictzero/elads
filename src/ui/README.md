# ui — wxAUI docking shell

Reuses SLADE's wxWidgets/wxAUI docking UI, hosting the central GL canvases from
`src/render`. Includes the explicit idle-handling fix that reconciles wxAUI's
idle-driven docking with an always-rendering GL viewport (a documented SLADE-class
friction point). Context creation uses the wxGLCanvas **EGL** path (required under
Wayland/labwc on Pi OS).
