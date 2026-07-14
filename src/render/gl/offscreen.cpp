// SPDX-License-Identifier: GPL-3.0-or-later
#include "render/gl/offscreen.h"

#include <epoxy/gl.h>

#include <algorithm>
#include <vector>

namespace elads::render {

bool OffscreenTarget::init(int width, int height, std::string* error) {
    auto fail = [&](const char* m) {
        if (error)
            *error = m;
        return false;
    };
    if (width <= 0 || height <= 0)
        return fail("OffscreenTarget: invalid size");
    w_ = width;
    h_ = height;

    glGenFramebuffers(1, &fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);

    glGenTextures(1, &colorTex_);
    glBindTexture(GL_TEXTURE_2D, colorTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w_, h_, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex_, 0);

    glGenRenderbuffers(1, &depthRb_);
    glBindRenderbuffer(GL_RENDERBUFFER, depthRb_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w_, h_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRb_);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        return fail("OffscreenTarget: framebuffer incomplete");
    return true;
}

void OffscreenTarget::bind() {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, w_, h_);
}

gfx::Image OffscreenTarget::readback(bool flipY) const {
    gfx::Image img(w_, h_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w_, h_, GL_RGBA, GL_UNSIGNED_BYTE, img.rgba.data());

    if (flipY) {
        const size_t rowBytes = static_cast<size_t>(w_) * 4;
        std::vector<uint8_t> tmp(rowBytes);
        for (int y = 0; y < h_ / 2; ++y) {
            uint8_t* top = img.rgba.data() + static_cast<size_t>(y) * rowBytes;
            uint8_t* bot = img.rgba.data() + static_cast<size_t>(h_ - 1 - y) * rowBytes;
            std::copy(top, top + rowBytes, tmp.data());
            std::copy(bot, bot + rowBytes, top);
            std::copy(tmp.data(), tmp.data() + rowBytes, bot);
        }
    }
    return img;
}

OffscreenTarget::~OffscreenTarget() {
    if (colorTex_)
        glDeleteTextures(1, &colorTex_);
    if (depthRb_)
        glDeleteRenderbuffers(1, &depthRb_);
    if (fbo_)
        glDeleteFramebuffers(1, &fbo_);
}

} // namespace elads::render
