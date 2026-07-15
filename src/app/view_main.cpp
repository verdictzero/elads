// SPDX-License-Identifier: GPL-3.0-or-later
// elads-view — interactive standalone map viewport (desktop OpenGL).
//
//   elads-view --demo | --demo-slope | <file.wad> <MAP>   [options]
//   options: --3d | --2d           start mode (default: 3d for demos)
//            --size W H             window size (default 1024 768)
//            --auto-screenshot P    headless: render --frames then write PNG P and exit
//            --frames N             frames to render in auto mode (default 3)
//
// The same GLDevice/GLContext + 2D/3D renderers used headless now draw into a real window's
// default framebuffer (B1). Interactive: WASD + mouse-look (3D), drag-pan + wheel-zoom (2D),
// Tab toggles mode, F12 screenshots, R resets the camera, Esc quits. The --auto-screenshot path
// is the CI-able check (run under xvfb-run). See docs/implementation-plan.md B1.
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string>

#include "archive/wad.h"
#include "graphics/material_set.h"
#include "graphics/png.h"
#include "graphics/wad_materials.h"
#include "mapeditor/edit/editor.h"
#include "mapeditor/model/doom_map_io.h"
#include "mapeditor/model/map_model.h"
#include "mapeditor/model/map_save.h"
#include "mapeditor/view2d/map_view_2d.h"
#include "mapeditor/view3d/map_view_3d.h"
#include "render/gl/gl_backend.h"
#include "render/gl/glfw_window.h"

using namespace elads;

namespace {

util::Bytes readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f)
        throw std::runtime_error("cannot open '" + path + "'");
    return util::Bytes(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}
void writeFile(const std::string& path, const util::Bytes& data) {
    std::ofstream f(path, std::ios::binary);
    if (!f)
        throw std::runtime_error("cannot write '" + path + "'");
    f.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

// A square room with a floor sloped by three slope things (type 9500) — self-contained demo
// content that exercises 2D fill, 3D walls, and the slope path.
map::MapModel demoMap(bool slope) {
    map::MapModel m;
    const double s = 512;
    m.addVertex({0, 0});
    m.addVertex({s, 0});
    m.addVertex({s, s});
    m.addVertex({0, s});
    map::Sector sec;
    sec.floorHeight = 0;
    sec.ceilHeight = 192;
    sec.floorTex = "FLOOR";
    sec.ceilTex = "CEIL";
    sec.lightLevel = 200;
    m.addSector(sec);
    for (int i = 0; i < 4; ++i) {
        map::Sidedef sd;
        sd.sector = 0;
        sd.middle = "BRICK";
        const int side = m.addSidedef(sd);
        map::Linedef l;
        l.v1 = i;
        l.v2 = (i + 1) % 4;
        l.front = side;
        m.addLinedef(l);
    }
    if (slope) {
        const double z[3] = {0, 160, 64};
        const double px[3] = {32, s - 32, 32}, py[3] = {32, 32, s - 32};
        for (int i = 0; i < 3; ++i) {
            map::Thing t;
            t.pos = {px[i], py[i]};
            t.z = z[i];
            t.type = 9500;
            m.addThing(t);
        }
    }
    return m;
}

// Procedural textures so the demos are textured without an IWAD (mirrors elads-render's set).
uint8_t clamp8(int v) { return static_cast<uint8_t>(v < 0 ? 0 : v > 255 ? 255 : v); }
gfx::MaterialSet demoMaterials() {
    gfx::MaterialSet mats;
    gfx::Image brick(64, 128);
    for (int y = 0; y < 128; ++y)
        for (int x = 0; x < 64; ++x) {
            const int row = y / 16, xoff = (row % 2) ? 16 : 0;
            const int bx = (x + xoff) % 32, by = y % 16;
            const bool mortar = by < 2 || bx < 2;
            const int n = ((x * 13 + y * 7) % 17) - 8;
            if (mortar)
                brick.set(x, y, 58, 54, 50, 255);
            else
                brick.set(x, y, clamp8(150 + n), clamp8(70 + n), clamp8(54 + n), 255);
        }
    mats.add("BRICK", std::move(brick));
    gfx::Image floor(64, 64);
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x) {
            const bool a = ((x / 16 + y / 16) % 2) == 0;
            const int n = ((x * 5 + y * 11) % 13) - 6;
            floor.set(x, y, clamp8((a ? 120 : 96) + n), clamp8((a ? 112 : 90) + n),
                      clamp8((a ? 92 : 74) + n), 255);
        }
    mats.add("FLOOR", std::move(floor));
    gfx::Image ceil(64, 64);
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x) {
            const bool grid = (x % 16 == 0) || (y % 16 == 0);
            ceil.set(x, y, grid ? 48 : 66, grid ? 52 : 72, grid ? 66 : 92, 255);
        }
    mats.add("CEIL", std::move(ceil));
    return mats;
}

struct Options {
    std::string wad, mapName;
    bool demo = false, demoSlope = false;
    bool start3d = true;
    int width = 1024, height = 768;
    std::string autoShot;
    int frames = 3;
};

int usage() {
    std::puts("elads-view — interactive map viewport");
    std::puts("  elads-view --demo | --demo-slope | <file.wad> <MAP> [options]");
    std::puts("  options: --2d | --3d  --size W H  --auto-screenshot P  --frames N");
    return 2;
}

// Advance the 3D camera from movement input. `dt` seconds.
void updateCamera3D(view::Camera3D& c, const render::InputFrame& in, double dt) {
    const double sens = 0.0025;
    c.yaw += static_cast<float>(in.lookDX * sens);
    c.pitch -= static_cast<float>(in.lookDY * sens);
    c.pitch = std::max(-1.5f, std::min(1.5f, c.pitch));

    const double speed = (in.speed ? 900.0 : 300.0) * dt;
    const double fdx = -std::sin(c.yaw), fdz = -std::cos(c.yaw); // forward on XZ plane
    const double rdx = std::cos(c.yaw), rdz = -std::sin(c.yaw);  // right on XZ plane
    if (in.forward) { c.x += fdx * speed; c.z += fdz * speed; }
    if (in.back)    { c.x -= fdx * speed; c.z -= fdz * speed; }
    if (in.right)   { c.x += rdx * speed; c.z += rdz * speed; }
    if (in.left)    { c.x -= rdx * speed; c.z -= rdz * speed; }
    if (in.riseUp)   c.y += speed;
    if (in.fallDown) c.y -= speed;
}

// Apply 2D pointer + keyboard input to the editor (select/drag/draw/pan/zoom + edits).
void handleEditor2D(edit::MapEditor& ed, const render::InputFrame& in) {
    if (ed.mode() == edit::MapEditor::Mode::Draw) {
        // Draw mode: each left click drops a loop point; clicking near the first closes it.
        if (in.leftClick)
            ed.addDrawPoint(in.cursorX, in.cursorY);
    } else {
        // Pointer: left = select/drag objects, right = pan, wheel = zoom.
        if (in.leftClick) {
            if (!ed.beginDrag(in.cursorX, in.cursorY))
                ed.clickSelect(in.cursorX, in.cursorY);
        } else if (in.leftDown && ed.dragging()) {
            ed.updateDrag(in.cursorX, in.cursorY);
        } else if (!in.leftDown && ed.dragging()) {
            ed.endDrag();
        } else {
            ed.hover(in.cursorX, in.cursorY);
        }
    }
    if (in.rightDown && (in.cursorDX != 0.0 || in.cursorDY != 0.0))
        ed.panPixels(in.cursorDX, in.cursorDY);
    if (in.scroll != 0.0)
        ed.zoomAt(std::exp(in.scroll * 0.12), in.cursorX, in.cursorY);

    // Keys: modes, delete, undo/redo, grid snap.
    if (in.mode1) ed.setMode(edit::MapEditor::Mode::Vertices);
    if (in.mode2) ed.setMode(edit::MapEditor::Mode::Linedefs);
    if (in.mode3) ed.setMode(edit::MapEditor::Mode::Sectors);
    if (in.mode4) ed.setMode(edit::MapEditor::Mode::Things);
    if (in.mode5) ed.setMode(edit::MapEditor::Mode::Draw);
    if (in.del) ed.deleteSelection();
    if (in.undo) ed.undoLast();
    if (in.redo) ed.redoLast();
    if (in.snapToggle) ed.setGridSnap(!ed.gridSnap());
}

} // namespace

int main(int argc, char** argv) {
    // Force Mesa software GL so it runs under Xvfb without a GPU (interactive on real hardware
    // will use the native driver instead — these are no-ops when the vars are already set).
    setenv("LIBGL_ALWAYS_SOFTWARE", "1", 0);

    Options opt;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--demo")
            opt.demo = true;
        else if (a == "--demo-slope")
            opt.demo = opt.demoSlope = true;
        else if (a == "--2d")
            opt.start3d = false;
        else if (a == "--3d")
            opt.start3d = true;
        else if (a == "--size" && i + 2 < argc) {
            opt.width = std::atoi(argv[++i]);
            opt.height = std::atoi(argv[++i]);
        } else if (a == "--auto-screenshot" && i + 1 < argc)
            opt.autoShot = argv[++i];
        else if (a == "--frames" && i + 1 < argc)
            opt.frames = std::atoi(argv[++i]);
        else if (!a.empty() && a[0] != '-' && opt.wad.empty())
            opt.wad = a;
        else if (!a.empty() && a[0] != '-' && opt.mapName.empty())
            opt.mapName = a;
    }
    if (!opt.demo && (opt.wad.empty() || opt.mapName.empty()))
        return usage();

    try {
        // Load content. For a real WAD we keep the archive + UDMF flag so edits can be saved back.
        map::MapModel model;
        gfx::MaterialSet materials;
        archive::Wad wad;
        bool haveWad = false, udmf = false;
        if (opt.demo) {
            model = demoMap(opt.demoSlope);
            materials = demoMaterials();
        } else {
            wad = archive::Wad::read(readFile(opt.wad));
            model = map::loadMapFromWad(wad, opt.mapName);
            materials = gfx::buildMaterialSetFromWad(wad);
            haveWad = true;
            for (const map::MapEntry& e : map::findMaps(wad))
                if (e.name == opt.mapName)
                    udmf = e.udmf;
        }

        render::GlfwWindow win;
        std::string err;
        if (!win.init(opt.width, opt.height, "elads-view", &err)) {
            std::fprintf(stderr, "window init failed: %s\n", err.c_str());
            return 1;
        }

        render::GLDevice dev;
        render::GLContext ctx;
        view::MapRenderer2D r2(dev, &materials); // textured top-down fills (D5)
        view::MapRenderer3D r3(dev, &materials);

        // The editor owns the (editable) 2D map state + camera; 3D fly keeps its own camera.
        edit::MapEditor editor(std::move(model));
        editor.camera() = view::fitCamera(editor.model(), win.width(), win.height());
        view::Camera3D cam3 = view::autoCamera3D(editor.model(), win.width(), win.height());
        bool mode3d = opt.start3d;

        auto saveBack = [&] {
            if (!haveWad) {
                std::printf("save: nothing to write (demo map has no file)\n");
                return;
            }
            map::saveMapToWad(wad, opt.mapName, editor.model(), udmf);
            writeFile(opt.wad, wad.write());
            std::printf("saved %s (%s)\n", opt.mapName.c_str(), opt.wad.c_str());
        };

        auto renderFrame = [&] {
            win.bindDefaultFramebuffer();
            editor.camera().width = win.width();
            editor.camera().height = win.height();
            cam3.width = win.width();
            cam3.height = win.height();
            ctx.beginFrame(win.viewport());
            if (mode3d) {
                r3.render(ctx, editor.model(), cam3);
            } else {
                view::MapOverlay ov;
                ov.highlight = editor.highlight();
                ov.selection = editor.selection();
                ov.drawLoop = editor.drawPoints();
                r2.render(ctx, editor.model(), editor.camera(), ov);
            }
            ctx.endFrame();
        };

        // --- Headless auto-screenshot: render N frames, save the last, exit. ---
        if (!opt.autoShot.empty()) {
            // Show the overlay in 2D auto-shots by selecting the first vertex (visual check).
            if (!mode3d && editor.model().vertexCount() > 0) {
                const util::Vec2 sp = view::worldToScreen(editor.camera(), editor.model().vertex(0).pos);
                editor.setMode(edit::MapEditor::Mode::Vertices);
                editor.clickSelect(sp.x, sp.y);
            }
            const int total = std::max(1, opt.frames);
            for (int f = 0; f < total; ++f) {
                win.poll(); // pump events so the platform is happy
                renderFrame();
                // Swap every frame except the last: screenshot() reads GL_BACK, so the final
                // frame must stay in the back buffer (a swap would move it to the front and
                // leave the back buffer undefined).
                if (f + 1 < total)
                    win.swapBuffers();
            }
            writeFile(opt.autoShot, gfx::encodePng(win.screenshot()));
            std::printf("wrote %s (%dx%d, %s)\n", opt.autoShot.c_str(), win.width(), win.height(),
                        mode3d ? "3D" : "2D");
            return 0;
        }

        // --- Interactive loop. ---
        if (mode3d)
            win.setCursorCaptured(true);
        std::puts("elads-view: Tab=2D/3D  (2D) 1-5=vertex/line/sector/thing/draw modes,"
                  " LMB=select/drag or drop draw point (5=draw: click near start to close),"
                  " RMB=pan, wheel=zoom, X=delete, Z/Y=undo/redo, G=grid snap, F2=save;"
                  " (3D) WASD+mouse, Q/E=up/down; R=reset, F12=shot, Esc=cancel/quit");
        using clock = std::chrono::steady_clock;
        clock::time_point last = clock::now();
        while (!win.shouldClose()) {
            const render::InputFrame in = win.poll();
            const clock::time_point now = clock::now();
            double dt = std::chrono::duration<double>(now - last).count(); // wall-clock seconds
            last = now;
            if (dt <= 0.0 || dt > 0.1)
                dt = 1.0 / 60.0; // clamp startup / stalls to a sane step

            if (in.quit) {
                // Esc cancels an in-progress sector trace first; otherwise it quits.
                if (!mode3d && editor.mode() == edit::MapEditor::Mode::Draw &&
                    !editor.drawPoints().empty())
                    editor.cancelDraw();
                else
                    win.requestClose();
            }
            if (in.reset) {
                editor.camera() = view::fitCamera(editor.model(), win.width(), win.height());
                cam3 = view::autoCamera3D(editor.model(), win.width(), win.height());
            }
            if (in.toggleView) {
                mode3d = !mode3d;
                win.setCursorCaptured(mode3d);
            }
            if (in.save)
                saveBack();
            if (mode3d)
                updateCamera3D(cam3, in, dt);
            else
                handleEditor2D(editor, in);

            renderFrame();
            if (in.screenshot)
                writeFile("elads-view-shot.png", gfx::encodePng(win.screenshot()));
            win.swapBuffers();
        }
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }
}
