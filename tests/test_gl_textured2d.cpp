// SPDX-License-Identifier: GPL-3.0-or-later
// Headless textured-2D test (D5): the top-down view rendered with a MaterialSet (a checker flat)
// must differ from the flat-shaded view, and the textured fill must contain more than one
// brightness (the checker's light + dark squares).
#include <cstdio>
#include <cstdlib>
#include <cstdint>

#include "check.h"
#include "graphics/material_set.h"
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
    sec.lightLevel = 255;
    sec.floorTex = "CHK";
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

static gfx::MaterialSet checkerSet() {
    gfx::MaterialSet mats;
    gfx::Image chk(64, 64);
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x) {
            const bool a = ((x / 8 + y / 8) % 2) == 0;
            chk.set(x, y, a ? 200 : 60, a ? 190 : 55, a ? 150 : 45, 255);
        }
    mats.add("CHK", std::move(chk));
    return mats;
}

static gfx::Image renderView(const map::MapModel& m, render::GLDevice& dev, const gfx::MaterialSet* mats) {
    render::GLContext ctx;
    render::OffscreenTarget fbo;
    std::string err;
    fbo.init(160, 160, &err);
    fbo.bind();
    ctx.beginFrame(fbo.viewport());
    {
        view::MapRenderer2D r(dev, mats);
        const view::Camera2D cam = view::fitCamera(m, 160, 160);
        r.render(ctx, m, cam);
    }
    ctx.endFrame();
    return fbo.readback(true);
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
    const gfx::MaterialSet mats = checkerSet();
    const map::MapModel m = square();

    const gfx::Image tex = renderView(m, dev, &mats);
    const gfx::Image flat = renderView(m, dev, nullptr);

    // Textured vs flat differ substantially.
    int diff = 0, light = 0, dark = 0;
    for (size_t i = 0; i + 3 < tex.rgba.size(); i += 4) {
        const int r = tex.rgba[i], g = tex.rgba[i + 1], b = tex.rgba[i + 2];
        const int fr = flat.rgba[i];
        if (std::abs(r - fr) > 25)
            ++diff;
        // Checker light square (bright warm) and dark square, inside the fill.
        if (r > 150 && g > 130 && b > 100)
            ++light;
        if (r > 40 && r < 110 && b < 90 && g > 30)
            ++dark;
    }
    std::printf("textured2d diff=%d light=%d dark=%d  GL: %s\n", diff, light, dark,
                gl.glRenderer().c_str());
    CHECK(diff > 500);  // textured fill clearly differs from the flat one
    CHECK(light > 200); // checker has bright squares
    CHECK(dark > 200);  // ...and dark squares

    if (eladstest::failures() == 0)
        std::printf("OK (textured2d)\n");
    return eladstest::failures() ? 1 : 0;
}
