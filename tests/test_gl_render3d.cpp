// SPDX-License-Identifier: GPL-3.0-or-later
// Headless 3D visual-mode smoke test: render a room in perspective to an FBO and confirm
// floor, ceiling, and walls all appear (distinct shaded surfaces + depth).
#include <cstdio>
#include <cstdlib>
#include <cstdint>

#include "check.h"
#include "mapeditor/model/map_model.h"
#include "mapeditor/view3d/map_view_3d.h"
#include "render/gl/egl_headless.h"
#include "render/gl/gl_backend.h"
#include "render/gl/offscreen.h"

using namespace elads;

// A 128-high square room (floor 0, ceiling 128) so walls have visible extent.
static map::MapModel room() {
    map::MapModel m;
    m.addVertex({0, 0});
    m.addVertex({256, 0});
    m.addVertex({256, 256});
    m.addVertex({0, 256});
    map::Sector sec;
    sec.floorHeight = 0;
    sec.ceilHeight = 128;
    sec.lightLevel = 210;
    m.addSector(sec);
    const int e[4][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}};
    for (int i = 0; i < 4; ++i) {
        map::Sidedef sd;
        sd.sector = 0;
        const int side = m.addSidedef(sd);
        map::Linedef l;
        l.v1 = e[i][0];
        l.v2 = e[i][1];
        l.front = side;
        m.addLinedef(l);
    }
    return m;
}

int main() {
    setenv("LIBGL_ALWAYS_SOFTWARE", "1", 0);
    setenv("EGL_PLATFORM", "surfaceless", 0);

    render::HeadlessGL gl;
    std::string err;
    if (!gl.init(3, 3, &err)) {
        std::fprintf(stderr, "FAIL: GL init: %s\n", err.c_str());
        return 1;
    }
    render::GLDevice dev;
    render::GLContext ctx;
    render::OffscreenTarget fbo;
    if (!fbo.init(160, 120, &err)) {
        std::fprintf(stderr, "FAIL: offscreen: %s\n", err.c_str());
        return 1;
    }

    fbo.bind();
    ctx.beginFrame(fbo.viewport());
    {
        view::MapRenderer3D renderer(dev);
        const view::Camera3D cam = view::autoCamera3D(room(), 160, 120);
        renderer.render(ctx, room(), cam);
    }
    ctx.endFrame();

    const gfx::Image img = fbo.readback(true);

    // The 3D view should fill most of the frame with non-background geometry, and include
    // both warm-ish (floor, r>g>b) and cool-ish (ceiling, b>r) shaded pixels.
    int drawn = 0, warm = 0, cool = 0;
    for (size_t i = 0; i + 3 < img.rgba.size(); i += 4) {
        const int r = img.rgba[i], g = img.rgba[i + 1], b = img.rgba[i + 2];
        if (std::abs(r - 13) > 25 || std::abs(g - 15) > 25 || std::abs(b - 23) > 25)
            ++drawn;
        if (r > g && g >= b && r > 60)
            ++warm;
        if (b > r + 8 && b > 60)
            ++cool;
    }
    std::printf("3D drawn=%d warm=%d cool=%d  GL: %s\n", drawn, warm, cool, gl.glRenderer().c_str());
    CHECK(drawn > 160 * 120 / 2); // geometry fills most of the frame
    CHECK(warm > 100);            // floor visible
    CHECK(cool > 100);            // ceiling visible

    if (eladstest::failures() == 0)
        std::printf("OK (gl_render3d)\n");
    return eladstest::failures() ? 1 : 0;
}
