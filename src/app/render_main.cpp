// SPDX-License-Identifier: GPL-3.0-or-later
// elads-render — headless 2D map renderer (desktop OpenGL variant).
//
//   elads-render render-demo <out.png> [w h]
//   elads-render render-map  <file.wad> <MAPNAME> <out.png> [w h]
//
// Renders a map's top-down 2D view to a PNG using the EGL/GL backend. Software rendering
// (llvmpipe) is forced so it runs headless in CI/containers without a GPU.
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string>

#include "archive/wad.h"
#include "graphics/material_set.h"
#include "graphics/png.h"
#include "graphics/wad_materials.h"
#include "mapeditor/model/doom_map_io.h"
#include "mapeditor/model/udmf.h"
#include "mapeditor/view2d/map_view_2d.h"
#include "mapeditor/view3d/map_view_3d.h"
#include "render/gl/egl_headless.h"
#include "render/gl/gl_backend.h"
#include "render/gl/offscreen.h"

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

// A non-convex L-shaped one-sector room (guaranteed-valid demo geometry).
map::MapModel demoMap() {
    map::MapModel m;
    const double pts[6][2] = {{0, 0}, {384, 0}, {384, 128}, {192, 128}, {192, 256}, {0, 256}};
    for (auto& p : pts)
        m.addVertex({p[0], p[1]});
    map::Sector sec;
    sec.ceilHeight = 128;
    sec.lightLevel = 200;
    sec.floorTex = "FLOOR";
    sec.ceilTex = "CEIL";
    m.addSector(sec);
    for (int i = 0; i < 6; ++i) {
        map::Sidedef sd;
        sd.sector = 0;
        sd.middle = "BRICK";
        const int side = m.addSidedef(sd);
        map::Linedef l;
        l.v1 = i;
        l.v2 = (i + 1) % 6;
        l.front = side;
        m.addLinedef(l);
    }
    map::Thing p1;
    p1.pos = {64, 64};
    p1.type = 1;
    m.addThing(p1);
    return m;
}

int renderToPng(const map::MapModel& model, int w, int h, const std::string& out) {
    std::string err;
    render::HeadlessGL gl;
    if (!gl.init(3, 3, &err)) {
        std::fprintf(stderr, "GL init failed: %s\n", err.c_str());
        return 1;
    }
    render::GLDevice dev;
    render::GLContext ctx;
    render::OffscreenTarget fbo;
    if (!fbo.init(w, h, &err)) {
        std::fprintf(stderr, "offscreen init failed: %s\n", err.c_str());
        return 1;
    }

    fbo.bind();
    ctx.beginFrame(fbo.viewport());
    {
        view::MapRenderer2D renderer(dev);
        const view::Camera2D cam = view::fitCamera(model, w, h);
        renderer.render(ctx, model, cam);
    }
    ctx.endFrame();

    const gfx::Image img = fbo.readback(true);
    writeFile(out, gfx::encodePng(img));
    std::printf("wrote %s (%dx%d)  GL: %s / %s\n", out.c_str(), w, h, gl.glVersion().c_str(),
                gl.glRenderer().c_str());
    return 0;
}

// --- Procedural demo textures (so render-demo3d is textured without needing an IWAD) ---
uint8_t clamp8(int v) { return static_cast<uint8_t>(v < 0 ? 0 : v > 255 ? 255 : v); }

gfx::MaterialSet makeDemoMaterials() {
    gfx::MaterialSet mats;

    gfx::Image brick(64, 128);
    for (int y = 0; y < 128; ++y)
        for (int x = 0; x < 64; ++x) {
            const int row = y / 16;
            const int xoff = (row % 2) ? 16 : 0;
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
            if (a)
                floor.set(x, y, clamp8(120 + n), clamp8(112 + n), clamp8(92 + n), 255);
            else
                floor.set(x, y, clamp8(96 + n), clamp8(90 + n), clamp8(74 + n), 255);
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

int render3DToPng(const map::MapModel& model, int w, int h, const std::string& out,
                  const gfx::MaterialSet* materials) {
    std::string err;
    render::HeadlessGL gl;
    if (!gl.init(3, 3, &err)) {
        std::fprintf(stderr, "GL init failed: %s\n", err.c_str());
        return 1;
    }
    render::GLDevice dev;
    render::GLContext ctx;
    render::OffscreenTarget fbo;
    if (!fbo.init(w, h, &err)) {
        std::fprintf(stderr, "offscreen init failed: %s\n", err.c_str());
        return 1;
    }
    fbo.bind();
    ctx.beginFrame(fbo.viewport());
    {
        view::MapRenderer3D renderer(dev, materials);
        const view::Camera3D cam = view::autoCamera3D(model, w, h);
        renderer.render(ctx, model, cam);
    }
    ctx.endFrame();
    writeFile(out, gfx::encodePng(fbo.readback(true)));
    std::printf("wrote %s (%dx%d, 3D%s)  GL: %s / %s\n", out.c_str(), w, h,
                materials ? ", textured" : "", gl.glVersion().c_str(), gl.glRenderer().c_str());
    return 0;
}

map::MapModel loadMapFromWad(const archive::Wad& wad, const std::string& mapName) {
    for (const auto& e : map::findMaps(wad)) {
        if (e.name != mapName)
            continue;
        const auto lumps = map::mapLumps(wad, e.marker);
        if (e.udmf) {
            for (const auto& l : lumps)
                if (l.name == "TEXTMAP")
                    return map::parseUdmf(std::string(l.data.begin(), l.data.end())).model;
            throw std::runtime_error("UDMF map has no TEXTMAP lump");
        }
        return map::readDoomMap(lumps);
    }
    throw std::runtime_error("map '" + mapName + "' not found");
}

map::MapModel loadMap(const std::string& wadPath, const std::string& mapName) {
    return loadMapFromWad(archive::Wad::read(readFile(wadPath)), mapName);
}

int usage() {
    std::puts("elads-render — headless map renderer\n");
    std::puts("  elads-render render-demo   <out.png> [w h]        2D top-down");
    std::puts("  elads-render render-map    <file.wad> <MAP> <out.png> [w h]");
    std::puts("  elads-render render-demo3d <out.png> [w h]        3D visual mode");
    std::puts("  elads-render render-map3d  <file.wad> <MAP> <out.png> [w h]");
    return 2;
}

} // namespace

int main(int argc, char** argv) {
    // Force Mesa software rendering so this works headless (no GPU) in containers/CI.
    setenv("LIBGL_ALWAYS_SOFTWARE", "1", 0);
    setenv("EGL_PLATFORM", "surfaceless", 0);
    try {
        const std::string cmd = argc >= 2 ? argv[1] : "";
        if (cmd == "render-demo" && argc >= 3) {
            const int w = argc >= 5 ? std::atoi(argv[3]) : 800;
            const int h = argc >= 5 ? std::atoi(argv[4]) : 600;
            return renderToPng(demoMap(), w, h, argv[2]);
        }
        if (cmd == "render-map" && argc >= 5) {
            const int w = argc >= 7 ? std::atoi(argv[5]) : 800;
            const int h = argc >= 7 ? std::atoi(argv[6]) : 600;
            return renderToPng(loadMap(argv[2], argv[3]), w, h, argv[4]);
        }
        if (cmd == "render-demo3d" && argc >= 3) {
            const int w = argc >= 5 ? std::atoi(argv[3]) : 900;
            const int h = argc >= 5 ? std::atoi(argv[4]) : 600;
            const gfx::MaterialSet mats = makeDemoMaterials();
            return render3DToPng(demoMap(), w, h, argv[2], &mats);
        }
        if (cmd == "render-map3d" && argc >= 5) {
            const int w = argc >= 7 ? std::atoi(argv[5]) : 900;
            const int h = argc >= 7 ? std::atoi(argv[6]) : 600;
            const archive::Wad wad = archive::Wad::read(readFile(argv[2]));
            const gfx::MaterialSet mats = gfx::buildMaterialSetFromWad(wad);
            return render3DToPng(loadMapFromWad(wad, argv[3]), w, h, argv[4], &mats);
        }
        return usage();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }
}
