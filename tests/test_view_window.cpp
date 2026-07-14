// SPDX-License-Identifier: GPL-3.0-or-later
// Interactive-window smoke test (B1): create a real GLFW window, render one 3D frame into its
// default framebuffer, and assert the screenshot is non-empty. When no display is available
// (plain headless CI, no Xvfb) window creation fails and the test SKIPs (passes) — run it under
// `xvfb-run` to exercise the real path.
#include <cstdio>
#include <cstdlib>

#include "check.h"
#include "mapeditor/model/map_model.h"
#include "mapeditor/view3d/map_view_3d.h"
#include "render/gl/gl_backend.h"
#include "render/gl/glfw_window.h"

using namespace elads;

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
        const int s = m.addSidedef(sd);
        map::Linedef l;
        l.v1 = e[i][0];
        l.v2 = e[i][1];
        l.front = s;
        m.addLinedef(l);
    }
    return m;
}

int main() {
    setenv("LIBGL_ALWAYS_SOFTWARE", "1", 0);

    render::GlfwWindow win;
    std::string err;
    if (!win.init(200, 150, "elads-view test", &err)) {
        std::printf("SKIP (view_window): no display (%s)\n", err.c_str());
        return 0; // headless without Xvfb -> not a failure
    }

    render::GLDevice dev;
    render::GLContext ctx;
    view::MapRenderer3D r3(dev);

    const map::MapModel m = room();
    view::Camera3D cam = view::autoCamera3D(m, win.width(), win.height());

    // Render two frames into the window's default framebuffer.
    for (int f = 0; f < 2; ++f) {
        win.poll();
        win.bindDefaultFramebuffer();
        ctx.beginFrame(win.viewport());
        r3.render(ctx, m, cam);
        ctx.endFrame();
        win.swapBuffers();
    }

    const gfx::Image shot = win.screenshot();
    CHECK_EQ(static_cast<int>(shot.rgba.size()), win.width() * win.height() * 4);
    int drawn = 0;
    for (size_t i = 0; i + 3 < shot.rgba.size(); i += 4) {
        const int r = shot.rgba[i], g = shot.rgba[i + 1], b = shot.rgba[i + 2];
        if (std::abs(r - 13) > 25 || std::abs(g - 15) > 25 || std::abs(b - 23) > 25)
            ++drawn;
    }
    std::printf("view_window drawn=%d of %d\n", drawn, win.width() * win.height());
    CHECK(drawn > win.width() * win.height() / 4); // geometry fills a good part of the frame

    if (eladstest::failures() == 0)
        std::printf("OK (view_window)\n");
    return eladstest::failures() ? 1 : 0;
}
