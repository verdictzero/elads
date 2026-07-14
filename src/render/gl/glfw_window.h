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
#include <unordered_map>

#include "graphics/image.h"
#include "render/backend/render_backend.h"

struct GLFWwindow;

namespace elads::render {

// One frame's worth of decoded input. Movement fields are "is held now"; toggle/action fields are
// edge-triggered (true only on the frame the key/button goes down). Mouse deltas are since the
// last poll. Pointer fields are meaningful only when the cursor is not captured (2D mode).
struct InputFrame {
    // 3D fly (held).
    bool forward = false, back = false, left = false, right = false; // WASD
    bool riseUp = false, fallDown = false;                          // Q / E (3D height)
    bool speed = false;                                             // Shift (move faster)

    // Edge-triggered actions.
    bool toggleView = false; // Tab
    bool screenshot = false; // F12
    bool reset = false;      // R (reset camera)
    bool quit = false;       // Esc
    bool mode1 = false, mode2 = false, mode3 = false, mode4 = false, mode5 = false; // 1..5 edit modes
    bool del = false;        // Delete / X
    bool undo = false;       // Z
    bool redo = false;       // Y
    bool save = false;       // F2
    bool snapToggle = false; // G

    // 3D look (cursor captured).
    double lookDX = 0.0, lookDY = 0.0;
    double scroll = 0.0; // wheel delta (zoom)

    // 2D pointer (cursor not captured).
    double cursorX = 0.0, cursorY = 0.0;   // absolute position (px, origin top-left)
    double cursorDX = 0.0, cursorDY = 0.0; // delta since last poll
    bool leftDown = false, rightDown = false;
    bool leftClick = false; // left button went down this frame (edge)
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
    bool prevLeft_ = false;    // previous left-mouse state (for click edge)
    std::unordered_map<int, bool> prevKey_; // previous key states, keyed by GLFW key (edge trigger)
};

} // namespace elads::render
