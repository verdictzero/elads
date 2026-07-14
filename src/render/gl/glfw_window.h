// SPDX-License-Identifier: GPL-3.0-or-later
// elads — GLFW window + GL context for the interactive viewport (elads-view).
//
// Creates an on-screen GL 3.3 core context so the SAME GLDevice/GLContext that render headless
// to an FBO also render into a real window's default framebuffer (no offscreen target). This is
// the interim standalone viewport that de-risks the GL-in-a-window path before the wxWidgets
// shell (see docs/design/02-render-abstraction.md §6, and B1/C2 in docs/implementation-plan.md).
// GLFW is contained entirely within this wrapper: the app consumes a decoded InputFrame, not GLFW.
#pragma once

#include <string>

#include "graphics/image.h"
#include "render/backend/render_backend.h"

struct GLFWwindow;

namespace elads::render {

// One frame's worth of decoded input. Movement fields are "is held now"; toggle/action fields are
// edge-triggered (true only on the frame the key goes down). Mouse deltas are since the last poll.
struct InputFrame {
    bool forward = false, back = false, left = false, right = false; // WASD
    bool riseUp = false, fallDown = false;                          // Q / E (3D height)
    bool speed = false;                                             // Shift (move faster)
    bool toggleView = false;                                        // Tab (edge)
    bool screenshot = false;                                        // F12 (edge)
    bool reset = false;                                             // R (edge)
    bool quit = false;                                             // Esc (edge)

    double lookDX = 0.0, lookDY = 0.0; // mouse-look delta (3D, cursor captured)
    double scroll = 0.0;               // wheel delta (2D zoom)
    bool dragging = false;             // left mouse button held (2D pan)
    double dragDX = 0.0, dragDY = 0.0; // cursor delta while dragging (screen px)
};

class GlfwWindow {
public:
    GlfwWindow() = default;
    ~GlfwWindow();
    GlfwWindow(const GlfwWindow&) = delete;
    GlfwWindow& operator=(const GlfwWindow&) = delete;

    // Create the window + GL 3.3 core context and make it current. Returns false and sets *error
    // on failure (e.g. no display available). glMajor/glMinor request the context version.
    bool init(int width, int height, const std::string& title, std::string* error = nullptr);

    bool shouldClose() const;
    void requestClose();

    // Poll events and return the decoded input for this frame.
    InputFrame poll();

    void swapBuffers();

    // Current framebuffer size (tracks resizes) and a viewport covering it.
    int width() const { return w_; }
    int height() const { return h_; }
    Viewport viewport() const { return {0, 0, w_, h_}; }

    // Bind the window's default framebuffer (0) and set the viewport. Call before rendering.
    void bindDefaultFramebuffer();

    // Capture (hide+grab) the cursor for mouse-look, or release it. Idempotent.
    void setCursorCaptured(bool captured);
    bool cursorCaptured() const { return cursorCaptured_; }

    // Read the default framebuffer back into an RGBA image (row 0 = top).
    gfx::Image screenshot() const;

    // Accumulate a wheel delta — called by the internal GLFW scroll callback only.
    void addScroll(double dy);

private:
    GLFWwindow* win_ = nullptr;
    int w_ = 0, h_ = 0;
    double lastCursorX_ = 0.0, lastCursorY_ = 0.0;
    bool haveCursor_ = false;
    bool cursorCaptured_ = false;
    double scrollAccum_ = 0.0; // accumulated by the GLFW scroll callback, drained each poll
    // Previous edge-key states for edge triggering.
    bool prevTab_ = false, prevF12_ = false, prevR_ = false, prevEsc_ = false;
};

} // namespace elads::render
