// SPDX-License-Identifier: GPL-3.0-or-later
#include "render/gl/egl_headless.h"

#include <epoxy/gl.h>
#include <epoxy/egl.h>

#include <cstdlib>

namespace elads::render {

bool HeadlessGL::init(int glMajor, int glMinor, std::string* error) {
    auto fail = [&](const char* m) {
        if (error)
            *error = m;
        return false;
    };

    // Select Mesa's surfaceless platform (no window/display needed). We use eglGetDisplay
    // (EGL 1.0) rather than eglGetPlatformDisplay because epoxy aborts resolving the latter
    // before any display fixes the EGL version. Both are set with overwrite=0 so a caller may
    // override (e.g. to use a GPU instead of llvmpipe).
    setenv("EGL_PLATFORM", "surfaceless", 0);

    EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (dpy == EGL_NO_DISPLAY)
        return fail("eglGetDisplay failed");

    EGLint vmaj = 0, vmin = 0;
    if (!eglInitialize(dpy, &vmaj, &vmin))
        return fail("eglInitialize failed");
    if (!eglBindAPI(EGL_OPENGL_API))
        return fail("eglBindAPI(OpenGL) failed");

    const EGLint cfgAttribs[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
                                 EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
                                 EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8,
                                 EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE};
    EGLConfig cfg;
    EGLint n = 0;
    if (!eglChooseConfig(dpy, cfgAttribs, &cfg, 1, &n) || n < 1)
        return fail("eglChooseConfig found no config");

    const EGLint ctxAttribs[] = {EGL_CONTEXT_MAJOR_VERSION, glMajor,
                                 EGL_CONTEXT_MINOR_VERSION, glMinor,
                                 EGL_CONTEXT_OPENGL_PROFILE_MASK,
                                 EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE};
    EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctxAttribs);
    if (ctx == EGL_NO_CONTEXT)
        return fail("eglCreateContext failed");
    if (!eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx)) {
        eglDestroyContext(dpy, ctx);
        return fail("eglMakeCurrent (surfaceless) failed");
    }

    dpy_ = dpy;
    ctx_ = ctx;
    return true;
}

HeadlessGL::~HeadlessGL() {
    if (dpy_) {
        eglMakeCurrent(dpy_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (ctx_)
            eglDestroyContext(dpy_, ctx_);
        eglTerminate(dpy_);
    }
}

std::string HeadlessGL::glVersion() const {
    const GLubyte* v = glGetString(GL_VERSION);
    return v ? reinterpret_cast<const char*>(v) : "";
}
std::string HeadlessGL::glRenderer() const {
    const GLubyte* v = glGetString(GL_RENDERER);
    return v ? reinterpret_cast<const char*>(v) : "";
}

} // namespace elads::render
