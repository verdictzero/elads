// SPDX-License-Identifier: GPL-3.0-or-later
// Headless 3D-floor test (A4): a room rendered with a Sector_Set3DFloor slab must differ from the
// same room without it (the slab adds caps + side walls the plain room does not have).
#include <cstdio>
#include <cstdlib>
#include <cstdint>

#include "check.h"
#include "mapeditor/model/map_model.h"
#include "mapeditor/model/threed_floors.h"
#include "mapeditor/view3d/map_view_3d.h"
#include "render/gl/egl_headless.h"
#include "render/gl/gl_backend.h"
#include "render/gl/offscreen.h"

using namespace elads;

// A tall room (tag 7). If `slab`, add a control sector + a 160 line giving it a mid-height 3D floor.
static map::MapModel room(bool slab) {
    map::MapModel m;
    m.addVertex({0, 0});
    m.addVertex({320, 0});
    m.addVertex({320, 320});
    m.addVertex({0, 320});
    map::Sector target;
    target.floorHeight = 0;
    target.ceilHeight = 256;
    target.lightLevel = 220;
    target.tag = 7;
    m.addSector(target);
    for (int i = 0; i < 4; ++i) {
        map::Sidedef sd;
        sd.sector = 0;
        const int side = m.addSidedef(sd);
        map::Linedef l;
        l.v1 = i;
        l.v2 = (i + 1) % 4;
        l.front = side;
        m.addLinedef(l);
    }
    if (slab) {
        const int c0 = m.addVertex({600, 0});
        const int c1 = m.addVertex({664, 0});
        const int c2 = m.addVertex({664, 64});
        const int c3 = m.addVertex({600, 64});
        map::Sector control;
        control.floorHeight = 96;
        control.ceilHeight = 144;
        m.addSector(control);
        const int cv[4] = {c0, c1, c2, c3};
        for (int i = 0; i < 4; ++i) {
            map::Sidedef sd;
            sd.sector = 1;
            const int side = m.addSidedef(sd);
            map::Linedef l;
            l.v1 = cv[i];
            l.v2 = cv[(i + 1) % 4];
            l.front = side;
            if (i == 0) {
                l.special = map::kSpecialSet3DFloor;
                l.args = {7, 1, 0, 255, 0};
            }
            m.addLinedef(l);
        }
    }
    return m;
}

static gfx::Image renderRoom(const map::MapModel& m, render::GLDevice& dev) {
    render::GLContext ctx;
    render::OffscreenTarget fbo;
    std::string err;
    fbo.init(200, 150, &err);
    fbo.bind();
    ctx.beginFrame(fbo.viewport());
    {
        view::MapRenderer3D renderer(dev);
        const view::Camera3D cam = view::autoCamera3D(m, 200, 150);
        renderer.render(ctx, m, cam);
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

    // Sanity: the model resolves exactly one slab in the target sector.
    CHECK_EQ(map::compute3DFloors(room(true)).size(), static_cast<size_t>(1));

    const gfx::Image with = renderRoom(room(true), dev);
    const gfx::Image without = renderRoom(room(false), dev);
    CHECK_EQ(with.rgba.size(), without.rgba.size());

    int diff = 0;
    for (size_t i = 0; i + 3 < with.rgba.size(); i += 4)
        for (int k = 0; k < 3; ++k)
            if (std::abs(with.rgba[i + k] - without.rgba[i + k]) > 30) {
                ++diff;
                break;
            }
    std::printf("3dfloor changed pixels=%d  GL: %s\n", diff, gl.glRenderer().c_str());
    CHECK(diff > 300); // the slab visibly changes the frame

    if (eladstest::failures() == 0)
        std::printf("OK (gl_3dfloor)\n");
    return eladstest::failures() ? 1 : 0;
}
