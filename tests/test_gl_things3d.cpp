// SPDX-License-Identifier: GPL-3.0-or-later
// Headless 3D things test (A5): render a room with a player-start thing and confirm the thing's
// green billboard appears (a colour no wall/floor/ceiling surface produces).
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

static map::MapModel roomWithThing() {
    map::MapModel m;
    m.addVertex({0, 0});
    m.addVertex({256, 0});
    m.addVertex({256, 256});
    m.addVertex({0, 256});
    map::Sector sec;
    sec.floorHeight = 0;
    sec.ceilHeight = 128;
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
    map::Thing t; // player 1 start -> green billboard
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
        std::fprintf(stderr, "FAIL: GL init: %s\n", err.c_str());
        return 1;
    }
    render::GLDevice dev;
    render::GLContext ctx;
    render::OffscreenTarget fbo;
    if (!fbo.init(200, 150, &err)) {
        std::fprintf(stderr, "FAIL: offscreen: %s\n", err.c_str());
        return 1;
    }

    const map::MapModel m = roomWithThing();
    fbo.bind();
    ctx.beginFrame(fbo.viewport());
    {
        view::MapRenderer3D renderer(dev); // no materials -> flat surfaces, green billboard stands out
        const view::Camera3D cam = view::autoCamera3D(m, 200, 150);
        renderer.render(ctx, m, cam);
    }
    ctx.endFrame();

    const gfx::Image img = fbo.readback(true);
    // The green player-start billboard: g clearly dominant. No wall/floor/ceiling tint is green.
    int green = 0;
    for (size_t i = 0; i + 3 < img.rgba.size(); i += 4) {
        const int r = img.rgba[i], g = img.rgba[i + 1], b = img.rgba[i + 2];
        if (g > 140 && g > r + 40 && g > b + 40)
            ++green;
    }
    std::printf("things3d green=%d  GL: %s\n", green, gl.glRenderer().c_str());
    CHECK(green > 15); // the billboard is a visible cluster of green pixels

    if (eladstest::failures() == 0)
        std::printf("OK (gl_things3d)\n");
    return eladstest::failures() ? 1 : 0;
}
