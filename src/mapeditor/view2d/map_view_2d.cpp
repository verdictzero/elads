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
uniform vec4 uPointSize; // .x = gl_PointSize (only affects GL_POINTS draws)
out vec4 vColor;
void main() {
    gl_Position = uMvp * vec4(aPos, 0.0, 1.0);
    gl_PointSize = uPointSize.x;
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

util::Vec2 screenToWorld(const Camera2D& c, double screenX, double screenY) {
    const double halfW = (c.width / 2.0) / c.pixelsPerUnit;
    const double halfH = (c.height / 2.0) / c.pixelsPerUnit;
    const double ndcX = c.width > 0 ? (screenX / c.width) * 2.0 - 1.0 : 0.0;
    const double ndcY = c.height > 0 ? 1.0 - (screenY / c.height) * 2.0 : 0.0; // flip: +y is up
    return {c.centerX + ndcX * halfW, c.centerY + ndcY * halfH};
}

util::Vec2 worldToScreen(const Camera2D& c, util::Vec2 world) {
    const double halfW = (c.width / 2.0) / c.pixelsPerUnit;
    const double halfH = (c.height / 2.0) / c.pixelsPerUnit;
    const double ndcX = halfW > 0 ? (world.x - c.centerX) / halfW : 0.0;
    const double ndcY = halfH > 0 ? (world.y - c.centerY) / halfH : 0.0;
    return {(ndcX + 1.0) * 0.5 * c.width, (1.0 - ndcY) * 0.5 * c.height};
}

MapRenderer2D::MapRenderer2D(render::IRenderDevice& device) : dev_(device) {
    program_ = dev_.createProgram(render::ShaderSources{kVert, kFrag});
}

MapRenderer2D::~MapRenderer2D() {
    if (program_ != render::ShaderHandle::Invalid)
        dev_.destroyProgram(program_);
}

namespace {
// Editor overlay colours.
constexpr float kHl[3] = {1.00f, 0.85f, 0.25f};  // hover (yellow)
constexpr float kSel[3] = {1.00f, 0.42f, 0.16f}; // selection (orange)

bool matches(const edit::Selection& s, edit::ObjType t, int i) {
    return s.type == t && s.index == i;
}
} // namespace

void MapRenderer2D::render(render::IRenderContext& ctx, const map::MapModel& m, const Camera2D& cam) {
    render(ctx, m, cam, MapOverlay{});
}

void MapRenderer2D::render(render::IRenderContext& ctx, const map::MapModel& m, const Camera2D& cam,
                           const MapOverlay& ov) {
    float mvp[16];
    cameraOrtho(cam, mvp);

    ctx.setDepthTest(false); // 2D layers draw back-to-front
    ctx.clear(render::Color{0.09f, 0.09f, 0.11f, 1.f});
    ctx.bindProgram(program_);
    ctx.setUniformMat4("uMvp", mvp);
    ctx.setUniformVec4("uPointSize", 5.f, 0.f, 0.f, 0.f);

    auto drawBatch = [&](const std::vector<MV>& verts, render::Topology topo, float pointSize = 5.f) {
        if (verts.empty())
            return;
        if (topo == render::Topology::Points)
            ctx.setUniformVec4("uPointSize", pointSize, 0.f, 0.f, 0.f);
        const render::BufferHandle buf = upload(dev_, verts);
        ctx.bindVertexBuffer(buf, kLayout);
        ctx.draw(topo, 0, static_cast<uint32_t>(verts.size()));
        dev_.destroyBuffer(buf); // immediate-mode backend: draw already issued
    };

    // How an object should be tinted given the overlay: selection wins over hover over base.
    auto emphasis = [&](edit::ObjType t, int i, const float* base) -> const float* {
        if (matches(ov.selection, t, i))
            return kSel;
        if (matches(ov.highlight, t, i))
            return kHl;
        return base;
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

    // 2) Sector fills — earcut triangles, tinted by sector light (highlighted/selected recolour).
    {
        std::vector<MV> fills;
        for (int s = 0; s < static_cast<int>(m.sectorCount()); ++s) {
            const map::Triangulation t = map::triangulateSector(m, s);
            const float light = static_cast<float>(m.sector(s).lightLevel) / 255.f;
            const float g = 0.10f + light * 0.22f;
            const float base[3] = {g * 0.75f, g * 0.85f, g};
            const float* c = emphasis(edit::ObjType::Sector, s, base);
            // Overlaid sectors get a dimmed version of the overlay colour so lines stay readable.
            const bool over = c != base;
            const float r = over ? c[0] * 0.45f : c[0];
            const float gg = over ? c[1] * 0.45f : c[1];
            const float bb = over ? c[2] * 0.45f : c[2];
            for (size_t i = 0; i + 2 < t.indices.size() + 1 && i + 3 <= t.indices.size(); i += 3)
                for (int k = 0; k < 3; ++k) {
                    const util::Vec2 p = t.points[t.indices[i + k]];
                    push(fills, p.x, p.y, r, gg, bb);
                }
        }
        drawBatch(fills, render::Topology::Triangles);
    }

    // 3) Linedefs — white for one-sided, muted blue-grey for two-sided (overlay recolours).
    {
        std::vector<MV> lines;
        for (int i = 0; i < static_cast<int>(m.linedefCount()); ++i) {
            const map::Linedef& l = m.linedef(i);
            if (l.v1 == map::kNoRef || l.v2 == map::kNoRef)
                continue;
            const util::Vec2 a = m.vertex(l.v1).pos;
            const util::Vec2 b = m.vertex(l.v2).pos;
            const float twoSided[3] = {0.42f, 0.47f, 0.55f};
            const float oneSided[3] = {0.92f, 0.92f, 0.92f};
            const float* base = l.twoSided() ? twoSided : oneSided;
            const float* c = emphasis(edit::ObjType::Linedef, i, base);
            push(lines, a.x, a.y, c[0], c[1], c[2]);
            push(lines, b.x, b.y, c[0], c[1], c[2]);
        }
        drawBatch(lines, render::Topology::Lines);
    }

    // 4) Things — small green markers (overlay recolours + enlarges).
    {
        std::vector<MV> pts;
        for (int i = 0; i < static_cast<int>(m.thingCount()); ++i) {
            const util::Vec2 p = m.thing(i).pos;
            const float base[3] = {0.35f, 0.85f, 0.45f};
            const float* c = emphasis(edit::ObjType::Thing, i, base);
            push(pts, p.x, p.y, c[0], c[1], c[2]);
        }
        drawBatch(pts, render::Topology::Points, 7.f);
        // Re-draw the emphasized thing larger, on top.
        if (ov.selection.type == edit::ObjType::Thing || ov.highlight.type == edit::ObjType::Thing) {
            std::vector<MV> hi;
            auto add = [&](const edit::Selection& s, const float* c) {
                if (s.type == edit::ObjType::Thing && s.index >= 0 &&
                    s.index < static_cast<int>(m.thingCount())) {
                    const util::Vec2 p = m.thing(s.index).pos;
                    push(hi, p.x, p.y, c[0], c[1], c[2]);
                }
            };
            add(ov.highlight, kHl);
            add(ov.selection, kSel);
            drawBatch(hi, render::Topology::Points, 12.f);
        }
    }

    // 5) Vertices — small cyan points (overlay recolours + enlarges).
    {
        std::vector<MV> pts;
        for (int i = 0; i < static_cast<int>(m.vertexCount()); ++i) {
            const util::Vec2 p = m.vertex(i).pos;
            const float base[3] = {0.20f, 0.90f, 0.95f};
            const float* c = emphasis(edit::ObjType::Vertex, i, base);
            push(pts, p.x, p.y, c[0], c[1], c[2]);
        }
        drawBatch(pts, render::Topology::Points, 5.f);
        // Re-draw the emphasized vertex larger, on top.
        if (ov.selection.type == edit::ObjType::Vertex || ov.highlight.type == edit::ObjType::Vertex) {
            std::vector<MV> hi;
            auto add = [&](const edit::Selection& s, const float* c) {
                if (s.type == edit::ObjType::Vertex && s.index >= 0 &&
                    s.index < static_cast<int>(m.vertexCount())) {
                    const util::Vec2 p = m.vertex(s.index).pos;
                    push(hi, p.x, p.y, c[0], c[1], c[2]);
                }
            };
            add(ov.highlight, kHl);
            add(ov.selection, kSel);
            drawBatch(hi, render::Topology::Points, 10.f);
        }
    }

    // 6) In-progress draw-sector loop — a hover-coloured polyline through the traced points,
    // plus a point at each, so the user sees the sector taking shape.
    if (ov.drawLoop.size() >= 1) {
        std::vector<MV> line, pts;
        for (size_t i = 0; i + 1 < ov.drawLoop.size(); ++i) {
            push(line, ov.drawLoop[i].x, ov.drawLoop[i].y, kHl[0], kHl[1], kHl[2]);
            push(line, ov.drawLoop[i + 1].x, ov.drawLoop[i + 1].y, kHl[0], kHl[1], kHl[2]);
        }
        for (const util::Vec2& p : ov.drawLoop)
            push(pts, p.x, p.y, kHl[0], kHl[1], kHl[2]);
        drawBatch(line, render::Topology::Lines);
        drawBatch(pts, render::Topology::Points, 7.f);
    }
}

} // namespace elads::view
