// SPDX-License-Identifier: GPL-3.0-or-later
// Headless sector-colour test (A3): a sector with UDMF lightcolor = red should tint its
// (untextured) surfaces red — a colour the default warm/cool/grey tints never produce.
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

static map::MapModel room(bool redLight) {
    map::MapModel m;
    m.addVertex({0, 0});
    m.addVertex({256, 0});
    m.addVertex({256, 256});
    m.addVertex({0, 256});
    map::Sector sec;
    sec.floorHeight = 0;
    sec.ceilHeight = 128;
    sec.lightLevel = 220;
    if (redLight)
        sec.extra.push_back({"lightcolor", "16711680"}); // 0xFF0000
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

static int countRed(const map::MapModel& m) {
    render::GLDevice dev;
    render::GLContext ctx;
    render::OffscreenTarget fbo;
    std::string err;
    if (!fbo.init(160, 120, &err))
        return -1;
    fbo.bind();
    ctx.beginFrame(fbo.viewport());
    {
        view::MapRenderer3D renderer(dev); // no materials -> flat surfaces take the tint directly
        const view::Camera3D cam = view::autoCamera3D(m, 160, 120);
        renderer.render(ctx, m, cam);
    }
    ctx.endFrame();
    const gfx::Image img = fbo.readback(true);
    int red = 0;
    for (size_t i = 0; i + 3 < img.rgba.size(); i += 4) {
        const int r = img.rgba[i], g = img.rgba[i + 1], b = img.rgba[i + 2];
        if (r > 90 && r > g + 50 && r > b + 50)
            ++red;
    }
    return red;
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

    const int redlit = countRed(room(true));
    const int plain = countRed(room(false));
    std::printf("sectorcolor red-lit=%d plain=%d  GL: %s\n", redlit, plain, gl.glRenderer().c_str());
    CHECK(redlit > 500);          // the red-lit room is dominated by red pixels
    CHECK(plain < redlit / 4);    // the neutral room has far fewer (warm floor aside)

    if (eladstest::failures() == 0)
        std::printf("OK (gl_sectorcolor)\n");
    return eladstest::failures() ? 1 : 0;
}
