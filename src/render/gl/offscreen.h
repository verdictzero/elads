// SPDX-License-Identifier: GPL-3.0-or-later
// elads — offscreen render target (FBO) for headless rendering / screenshots.
#pragma once

#include <string>

#include "graphics/image.h"
#include "render/backend/render_backend.h"

namespace elads::render {

class OffscreenTarget {
public:
    OffscreenTarget() = default;
    ~OffscreenTarget();
    OffscreenTarget(const OffscreenTarget&) = delete;
    OffscreenTarget& operator=(const OffscreenTarget&) = delete;

    bool init(int width, int height, std::string* error = nullptr);
    void bind(); // make this FBO current and set the viewport
    Viewport viewport() const { return {0, 0, w_, h_}; }

    // Read the color attachment back into an RGBA image (flipped so row 0 is the top).
    gfx::Image readback(bool flipY = true) const;

private:
    unsigned fbo_ = 0;
    unsigned colorTex_ = 0;
    unsigned depthRb_ = 0;
    int w_ = 0;
    int h_ = 0;
};

} // namespace elads::render
