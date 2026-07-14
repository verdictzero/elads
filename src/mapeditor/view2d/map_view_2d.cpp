// SPDX-License-Identifier: GPL-3.0-or-later
#include "mapeditor/view2d/map_view_2d.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "mapeditor/model/sector_tri.h"

namespace elads::view {
namespace {

// Standard 2D map vertex: position (vec2) + color (vec4). Matches the layout the GL
// backend binds (see src/render/gl/gl_backend.cpp).
struct MV {
    float x, y, r, g, b, a;
};
void push(std::vector<MV>& v, double x, double y, float r, float g, float b, float a = 1.f) {
    v.push_back({static_cast<float>(x), static_cast<float>(y), r, g, b, a});
}

// GLSL (desktop GL 3.3 core). The GLES port swaps the version/precision directives.
const char* kVert = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aColor;
uniform mat4 uMvp;
out vec4 vColor;
void main() {
    gl_Position = uMvp * vec4(aPos, 0.0, 1.0);
    gl_PointSize = 5.0;
    vColor = aColor;
}
)";

const char* kFrag = R"(#version 330 core
in vec4 vColor;
out vec4 fragColor;
void main() { fragColor = vColor; }
)";

render::BufferHandle upload(render::IRenderDevice& dev, const std::vector<MV>& verts) {
    return dev.createBuffer(render::BufferType::Vertex, render::BufferUsage::Stream,
                            verts.data(), verts.size() * sizeof(MV));
}

// vec2 position @0, vec4 color @8, stride 24.
constexpr render::VertexAttrib kAttribs[] = {{0, render::AttribType::Float2, 0},
                                             {1, render::AttribType::Float4, 8}};
constexpr render::VertexLayout kLayout{kAttribs, 2, sizeof(MV)};

} // namespace

Camera2D fitCamera(const map::MapModel& m, int width, int height, double marginFrac) {
    Camera2D cam;
    cam.width = width;
    cam.height = height;
    const util::BBox b = m.bounds();
    if (!b.valid() || width <= 0 || height <= 0) {
        cam.pixelsPerUnit = 1.0;
        return cam;
    }
    cam.centerX = b.center().x;
    cam.centerY = b.center().y;
    const double mw = std::max(1.0, b.width() * (1.0 + 2.0 * marginFrac));
    const double mh = std::max(1.0, b.height() * (1.0 + 2.0 * marginFrac));
    cam.pixelsPerUnit = std::min(width / mw, height / mh);
    return cam;
}

void cameraOrtho(const Camera2D& c, float m[16]) {
    const double halfW = (c.width / 2.0) / c.pixelsPerUnit;
    const double halfH = (c.height / 2.0) / c.pixelsPerUnit;
    const double l = c.centerX - halfW, r = c.centerX + halfW;
    const double bot = c.centerY - halfH, top = c.centerY + halfH;
    const double n = -1.0, f = 1.0;
    for (int i = 0; i < 16; ++i)
        m[i] = 0.f;
    m[0] = static_cast<float>(2.0 / (r - l));
    m[5] = static_cast<float>(2.0 / (top - bot));
    m[10] = static_cast<float>(-2.0 / (f - n));
    m[12] = static_cast<float>(-(r + l) / (r - l));
    m[13] = static_cast<float>(-(top + bot) / (top - bot));
    m[14] = static_cast<float>(-(f + n) / (f - n));
    m[15] = 1.f;
}

MapRenderer2D::MapRenderer2D(render::IRenderDevice& device) : dev_(device) {
    program_ = dev_.createProgram(render::ShaderSources{kVert, kFrag});
}

MapRenderer2D::~MapRenderer2D() {
    if (program_ != render::ShaderHandle::Invalid)
        dev_.destroyProgram(program_);
}

void MapRenderer2D::render(render::IRenderContext& ctx, const map::MapModel& m, const Camera2D& cam) {
    float mvp[16];
    cameraOrtho(cam, mvp);

    ctx.setDepthTest(false); // 2D layers draw back-to-front
    ctx.clear(render::Color{0.09f, 0.09f, 0.11f, 1.f});
    ctx.bindProgram(program_);
    ctx.setUniformMat4("uMvp", mvp);

    auto drawBatch = [&](const std::vector<MV>& verts, render::Topology topo) {
        if (verts.empty())
            return;
        const render::BufferHandle buf = upload(dev_, verts);
        ctx.bindVertexBuffer(buf, kLayout);
        ctx.draw(topo, 0, static_cast<uint32_t>(verts.size()));
        dev_.destroyBuffer(buf); // immediate-mode backend: draw already issued
    };

    // 1) Grid — lines every 64 units across the bounds.
    {
        const util::BBox b = m.bounds();
        if (b.valid()) {
            std::vector<MV> grid;
            const double step = 64.0;
            const float gc = 0.17f;
            const auto snap = [step](double v) { return std::floor(v / step) * step; };
            for (double x = snap(b.minX); x <= b.maxX; x += step) {
                push(grid, x, b.minY, gc, gc, gc);
                push(grid, x, b.maxY, gc, gc, gc);
            }
            for (double y = snap(b.minY); y <= b.maxY; y += step) {
                push(grid, b.minX, y, gc, gc, gc);
                push(grid, b.maxX, y, gc, gc, gc);
            }
            drawBatch(grid, render::Topology::Lines);
        }
    }

    // 2) Sector fills — earcut triangles, tinted by sector light.
    {
        std::vector<MV> fills;
        for (int s = 0; s < static_cast<int>(m.sectorCount()); ++s) {
            const map::Triangulation t = map::triangulateSector(m, s);
            const float light = static_cast<float>(m.sector(s).lightLevel) / 255.f;
            const float g = 0.10f + light * 0.22f;
            for (size_t i = 0; i + 2 < t.indices.size() + 1 && i + 3 <= t.indices.size(); i += 3)
                for (int k = 0; k < 3; ++k) {
                    const util::Vec2 p = t.points[t.indices[i + k]];
                    push(fills, p.x, p.y, g * 0.75f, g * 0.85f, g);
                }
        }
        drawBatch(fills, render::Topology::Triangles);
    }

    // 3) Linedefs — white for one-sided, muted blue-grey for two-sided.
    {
        std::vector<MV> lines;
        for (int i = 0; i < static_cast<int>(m.linedefCount()); ++i) {
            const map::Linedef& l = m.linedef(i);
            if (l.v1 == map::kNoRef || l.v2 == map::kNoRef)
                continue;
            const util::Vec2 a = m.vertex(l.v1).pos;
            const util::Vec2 b = m.vertex(l.v2).pos;
            float r, g, bl;
            if (l.twoSided()) {
                r = 0.42f;
                g = 0.47f;
                bl = 0.55f;
            } else {
                r = g = bl = 0.92f;
            }
            push(lines, a.x, a.y, r, g, bl);
            push(lines, b.x, b.y, r, g, bl);
        }
        drawBatch(lines, render::Topology::Lines);
    }

    // 4) Vertices — small cyan points.
    {
        std::vector<MV> pts;
        for (int i = 0; i < static_cast<int>(m.vertexCount()); ++i) {
            const util::Vec2 p = m.vertex(i).pos;
            push(pts, p.x, p.y, 0.20f, 0.90f, 0.95f);
        }
        drawBatch(pts, render::Topology::Points);
    }
}

} // namespace elads::view
