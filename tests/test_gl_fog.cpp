// SPDX-License-Identifier: GPL-3.0-or-later
// Headless fog test (A3): a deep room with UDMF fadecolor = green should fade distant surfaces
// toward green; the same room without fadecolor should not. Camera looks down the room's length.
#include <cmath>
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

// A long room: 256 wide (x), 4000 deep (y), 160 tall. Optional green fadecolor.
static map::MapModel corridor(bool fog) {
    map::MapModel m;
    m.addVertex({0, 0});
    m.addVertex({256, 0});
    m.addVertex({256, 4000});
    m.addVertex({0, 4000});
    map::Sector sec;
    sec.floorHeight = 0;
    sec.ceilHeight = 160;
    sec.lightLevel = 235;
    if (fog)
        sec.extra.push_back({"fadecolor", "65280"}); // 0x00FF00 green
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

static int greenPixels(const map::MapModel& m) {
    render::GLDevice dev;
    render::GLContext ctx;
    render::OffscreenTarget fbo;
    std::string err;
    if (!fbo.init(160, 120, &err))
        return -1;

    // Camera at the near end, looking down +Y (yaw = pi) so the far wall is straight ahead.
    view::Camera3D cam;
    cam.x = 128;
    cam.y = 80;
    cam.z = 40;
    cam.yaw = static_cast<float>(M_PI);
    cam.pitch = 0.f;
    cam.width = 160;
    cam.height = 120;

    fbo.bind();
    ctx.beginFrame(fbo.viewport());
    {
        view::MapRenderer3D renderer(dev); // untextured -> tints/fog show directly
        renderer.render(ctx, m, cam);
    }
    ctx.endFrame();
    const gfx::Image img = fbo.readback(true);
    int green = 0;
    for (size_t i = 0; i + 3 < img.rgba.size(); i += 4) {
        const int r = img.rgba[i], g = img.rgba[i + 1], b = img.rgba[i + 2];
        if (g > 90 && g > r + 40 && g > b + 40)
            ++green;
    }
    return green;
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

    const int fog = greenPixels(corridor(true));
    const int plain = greenPixels(corridor(false));
    std::printf("fog green=%d plain=%d  GL: %s\n", fog, plain, gl.glRenderer().c_str());
    CHECK(fog > 300);          // distant surfaces fade to green
    CHECK(plain < fog / 4);    // without fadecolor there is essentially no green

    if (eladstest::failures() == 0)
        std::printf("OK (gl_fog)\n");
    return eladstest::failures() ? 1 : 0;
}
