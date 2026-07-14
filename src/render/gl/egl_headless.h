// SPDX-License-Identifier: GPL-3.0-or-later
// elads — headless OpenGL context via EGL surfaceless (desktop variant).
//
// Creates a windowless GL core context (Mesa surfaceless; llvmpipe when no GPU), used by the
// batch renderer and CI render smoke tests. The interactive desktop app will instead supply a
// window-backed context; both feed the same GL backend. See docs/design/02-render-abstraction.md §6.
#pragma once

#include <string>

namespace elads::render {

class HeadlessGL {
public:
    HeadlessGL() = default;
    ~HeadlessGL();
    HeadlessGL(const HeadlessGL&) = delete;
    HeadlessGL& operator=(const HeadlessGL&) = delete;

    // Create a GL core context (>= glMajor.glMinor) and make it current. Returns false and
    // sets *error on failure.
    bool init(int glMajor = 3, int glMinor = 3, std::string* error = nullptr);
    bool valid() const { return ctx_ != nullptr; }

    std::string glVersion() const;   // requires a current context
    std::string glRenderer() const;

private:
    void* dpy_ = nullptr; // EGLDisplay
    void* ctx_ = nullptr; // EGLContext
};

} // namespace elads::render
