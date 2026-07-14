// SPDX-License-Identifier: GPL-3.0-or-later
// Headless render smoke test: create a GL context, render a map to an FBO, and confirm
// the frame actually contains drawn geometry (and encodes to PNG).
#include <cstdio>
#include <cstdlib>
#include <cstdint>

#include "check.h"
#include "graphics/png.h"
#include "mapeditor/model/map_model.h"
#include "mapeditor/view2d/map_view_2d.h"
#include "render/gl/egl_headless.h"
#include "render/gl/gl_backend.h"
#include "render/gl/offscreen.h"

using namespace elads;

static map::MapModel square() {
    map::MapModel m;
    m.addVertex({0, 0});
    m.addVertex({64, 0});
    m.addVertex({64, 64});
    m.addVertex({0, 64});
    map::Sector sec;
    sec.lightLevel = 200;
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
        std::fprintf(stderr, "SKIP/FAIL: GL init failed: %s\n", err.c_str());
        return 1;
    }

    render::GLDevice dev;
    render::GLContext ctx;
    render::OffscreenTarget fbo;
    if (!fbo.init(128, 128, &err)) {
        std::fprintf(stderr, "FAIL: offscreen init: %s\n", err.c_str());
        return 1;
    }

    fbo.bind();
    ctx.beginFrame(fbo.viewport());
    {
        view::MapRenderer2D renderer(dev);
        const view::Camera2D cam = view::fitCamera(square(), 128, 128);
        renderer.render(ctx, square(), cam);
    }
    ctx.endFrame();

    const gfx::Image img = fbo.readback(true);

    // Count pixels notably different from the (23,23,28) background — the walls/fills/verts.
    int drawn = 0;
    for (size_t i = 0; i + 3 < img.rgba.size(); i += 4) {
        const int r = img.rgba[i], g = img.rgba[i + 1], b = img.rgba[i + 2];
        if (std::abs(r - 23) > 25 || std::abs(g - 23) > 25 || std::abs(b - 28) > 25)
            ++drawn;
    }
    std::printf("drawn pixels = %d   GL: %s\n", drawn, gl.glRenderer().c_str());
    CHECK(drawn > 50);

    const util::Bytes png = gfx::encodePng(img);
    CHECK(png.size() > 8);
    CHECK(png[1] == 'P' && png[2] == 'N' && png[3] == 'G');

    if (eladstest::failures() == 0)
        std::printf("OK (gl_render)\n");
    return eladstest::failures() ? 1 : 0;
}
