// SPDX-License-Identifier: GPL-3.0-or-later
#include "render/gl/glfw_window.h"

// epoxy provides the GL symbols; keep GLFW from pulling in <GL/gl.h> (which clashes with epoxy).
#include <epoxy/gl.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <vector>

namespace elads::render {
namespace {

int g_glfwRefs = 0; // process-wide GLFW init refcount (one window today, but keep it honest)

void scrollCallback(GLFWwindow* w, double /*xoff*/, double yoff) {
    auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(w));
    if (self)
        self->addScroll(yoff);
}

} // namespace

bool GlfwWindow::init(int width, int height, const std::string& title, std::string* error) {
    auto fail = [&](const char* m) {
        if (error)
            *error = m;
        return false;
    };
    if (g_glfwRefs == 0 && !glfwInit())
        return fail("glfwInit failed (no display?)");
    ++g_glfwRefs;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    win_ = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!win_) {
        if (--g_glfwRefs == 0)
            glfwTerminate();
        return fail("glfwCreateWindow failed (no GL 3.3 context?)");
    }
    glfwMakeContextCurrent(win_);
    glfwSwapInterval(1);
    glfwSetWindowUserPointer(win_, this);
    glfwSetScrollCallback(win_, scrollCallback);

    glfwGetFramebufferSize(win_, &w_, &h_);
    return true;
}

GlfwWindow::~GlfwWindow() {
    if (win_) {
        glfwDestroyWindow(win_);
        win_ = nullptr;
        if (--g_glfwRefs == 0)
            glfwTerminate();
    }
}

void GlfwWindow::addScroll(double dy) { scrollAccum_ += dy; }

bool GlfwWindow::shouldClose() const { return !win_ || glfwWindowShouldClose(win_); }
void GlfwWindow::requestClose() {
    if (win_)
        glfwSetWindowShouldClose(win_, GLFW_TRUE);
}

InputFrame GlfwWindow::poll() {
    InputFrame in;
    if (!win_)
        return in;

    glfwPollEvents();
    glfwGetFramebufferSize(win_, &w_, &h_);

    auto held = [&](int key) { return glfwGetKey(win_, key) == GLFW_PRESS; };
    in.forward = held(GLFW_KEY_W);
    in.back = held(GLFW_KEY_S);
    in.left = held(GLFW_KEY_A);
    in.right = held(GLFW_KEY_D);
    in.riseUp = held(GLFW_KEY_Q);
    in.fallDown = held(GLFW_KEY_E);
    in.speed = held(GLFW_KEY_LEFT_SHIFT) || held(GLFW_KEY_RIGHT_SHIFT);

    // Edge-triggered toggles/actions.
    const bool tab = held(GLFW_KEY_TAB), f12 = held(GLFW_KEY_F12), r = held(GLFW_KEY_R),
               esc = held(GLFW_KEY_ESCAPE);
    in.toggleView = tab && !prevTab_;
    in.screenshot = f12 && !prevF12_;
    in.reset = r && !prevR_;
    in.quit = esc && !prevEsc_;
    prevTab_ = tab;
    prevF12_ = f12;
    prevR_ = r;
    prevEsc_ = esc;

    // Cursor delta (used for 3D look when captured, 2D pan when dragging).
    double cx = 0.0, cy = 0.0;
    glfwGetCursorPos(win_, &cx, &cy);
    double dx = 0.0, dy = 0.0;
    if (haveCursor_) {
        dx = cx - lastCursorX_;
        dy = cy - lastCursorY_;
    }
    lastCursorX_ = cx;
    lastCursorY_ = cy;
    haveCursor_ = true;

    if (cursorCaptured_) {
        in.lookDX = dx;
        in.lookDY = dy;
    }
    in.dragging = glfwGetMouseButton(win_, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    if (in.dragging && !cursorCaptured_) {
        in.dragDX = dx;
        in.dragDY = dy;
    }

    in.scroll = scrollAccum_;
    scrollAccum_ = 0.0;
    return in;
}

void GlfwWindow::swapBuffers() {
    if (win_)
        glfwSwapBuffers(win_);
}

void GlfwWindow::bindDefaultFramebuffer() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, w_, h_);
}

void GlfwWindow::setCursorCaptured(bool captured) {
    if (!win_ || captured == cursorCaptured_)
        return;
    cursorCaptured_ = captured;
    glfwSetInputMode(win_, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    haveCursor_ = false; // avoid a large jump on the first frame after (un)capture
}

gfx::Image GlfwWindow::screenshot() const {
    gfx::Image img(w_, h_);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w_, h_, GL_RGBA, GL_UNSIGNED_BYTE, img.rgba.data());
    // Flip so row 0 is the top (GL origin is bottom-left).
    const size_t rowBytes = static_cast<size_t>(w_) * 4;
    std::vector<uint8_t> tmp(rowBytes);
    for (int y = 0; y < h_ / 2; ++y) {
        uint8_t* top = img.rgba.data() + static_cast<size_t>(y) * rowBytes;
        uint8_t* bot = img.rgba.data() + static_cast<size_t>(h_ - 1 - y) * rowBytes;
        std::copy(top, top + rowBytes, tmp.data());
        std::copy(bot, bot + rowBytes, top);
        std::copy(tmp.data(), tmp.data() + rowBytes, bot);
    }
    return img;
}

} // namespace elads::render
