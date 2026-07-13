# render/desktopgl — desktop GL 3.1 / Zink fallback backend

Fallback implementation of `render/backend` for non-Pi desktops. Targets desktop
GL 3.1 core (no explicit attrib locations → `glBindAttribLocation`), and, when
`ELADS_RENDER_BACKEND=zink`, GL-on-Vulkan to reach GL-3.3-class features on
capable GPUs. Never required on the Pi 5.
