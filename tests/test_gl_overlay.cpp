// SPDX-License-Identifier: GPL-3.0-or-later
// Headless overlay render test: render the 2D map with an editor overlay (a selected vertex + a
// thing) to an FBO and confirm the selection colour (orange) and thing colour (green) appear.
// Uses the EGL headless path (no window), so it runs in CI without Xvfb.
#include <cstdio>
#include <cstdlib>
#include <cstdint>

#include "check.h"
#include "mapeditor/edit/selection.h"
#include "mapeditor/model/map_model.h"
#include "mapeditor/view2d/map_view_2d.h"
#include "render/gl/egl_headless.h"
#include "render/gl/gl_backend.h"
#include "render/gl/offscreen.h"

using namespace elads;

static map::MapModel square() {
    map::MapModel m;
    m.addVertex({0, 0});
    m.addVertex({256, 0});
    m.addVertex({256, 256});
    m.addVertex({0, 256});
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
    map::Thing t;
    t.pos = {128, 128};
    t.type = 1;
    m.addThing(t);
    return m;
}

int main() {
    setenv("LIBGL_ALWAYS_SOFTWARE", "1", 0);
    setenv("EGL_PLATFORM", "surfaceless", 0);

    render::HeadlessGL gl;
    std::string err;
    if (!gl.init(3, 3, &err)) {
        std::fprintf(stderr, "FAIL: GL init failed: %s\n", err.c_str());
        return 1;
    }
    render::GLDevice dev;
    render::GLContext ctx;
    render::OffscreenTarget fbo;
    if (!fbo.init(256, 256, &err)) {
        std::fprintf(stderr, "FAIL: offscreen init: %s\n", err.c_str());
        return 1;
    }

    const map::MapModel m = square();
    view::MapOverlay ov;
    ov.selection = edit::Selection{edit::ObjType::Vertex, 2}; // a corner vertex -> orange

    fbo.bind();
    ctx.beginFrame(fbo.viewport());
    {
        view::MapRenderer2D renderer(dev);
        const view::Camera2D cam = view::fitCamera(m, 256, 256);
        renderer.render(ctx, m, cam, ov);
    }
    ctx.endFrame();

    const gfx::Image img = fbo.readback(true);

    // Orange selection marker (r high, g mid, b low) and green thing marker (g high, r/b lower).
    int orange = 0, green = 0;
    for (size_t i = 0; i + 3 < img.rgba.size(); i += 4) {
        const int r = img.rgba[i], g = img.rgba[i + 1], b = img.rgba[i + 2];
        if (r > 180 && g > 70 && g < 170 && b < 90)
            ++orange;
        if (g > 150 && r < 160 && b < 160 && g > r + 40 && g > b + 40)
            ++green;
    }
    std::printf("overlay orange=%d green=%d  GL: %s\n", orange, green, gl.glRenderer().c_str());
    CHECK(orange > 4); // the enlarged selected vertex is a cluster of orange pixels
    CHECK(green > 2);  // the thing marker

    if (eladstest::failures() == 0)
        std::printf("OK (gl_overlay)\n");
    return eladstest::failures() ? 1 : 0;
}
