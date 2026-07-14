// SPDX-License-Identifier: GPL-3.0-or-later
#include "mapeditor/view3d/map_view_3d.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "mapeditor/model/sector_tri.h"

namespace elads::view {
namespace {

struct V3 {
    float x, y, z, r, g, b;
};

constexpr render::VertexAttrib kAttribs[] = {{0, render::AttribType::Float3, 0},
                                             {1, render::AttribType::Float3, 12}};
constexpr render::VertexLayout kLayout{kAttribs, 2, sizeof(V3)};

const char* kVert = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
uniform mat4 uVp;
out vec3 vColor;
void main() {
    gl_Position = uVp * vec4(aPos, 1.0);
    vColor = aColor;
}
)";
const char* kFrag = R"(#version 330 core
in vec3 vColor;
out vec4 fragColor;
void main() { fragColor = vec4(vColor, 1.0); }
)";

float shade(int light) {
    const float l = std::clamp(light, 0, 255) / 255.f;
    return 0.14f + l * 0.72f;
}

void tri(std::vector<V3>& v, double ax, double ay, double az, double bx, double by, double bz,
         double cx, double cy, double cz, float r, float g, float b) {
    v.push_back({(float)ax, (float)ay, (float)az, r, g, b});
    v.push_back({(float)bx, (float)by, (float)bz, r, g, b});
    v.push_back({(float)cx, (float)cy, (float)cz, r, g, b});
}

// Vertical wall quad between map points a,b spanning heights [zBot, zTop].
void wallQuad(std::vector<V3>& v, util::Vec2 a, util::Vec2 b, double zBot, double zTop, float r,
              float g, float bl) {
    tri(v, a.x, zBot, a.y, b.x, zBot, b.y, b.x, zTop, b.y, r, g, bl);
    tri(v, a.x, zBot, a.y, b.x, zTop, b.y, a.x, zTop, a.y, r, g, bl);
}

} // namespace

util::Mat4 Camera3D::viewProj() const {
    const float aspect = height > 0 ? static_cast<float>(width) / height : 1.f;
    const util::Mat4 proj = util::Mat4::perspective(fovY, aspect, 4.f, 20000.f);
    // view = inverse(camera pose) = rotX(-pitch) * rotY(-yaw) * translate(-pos)
    util::Mat4 view = util::Mat4::multiply(util::Mat4::rotateX(-pitch), util::Mat4::rotateY(-yaw));
    view = util::Mat4::multiply(view, util::Mat4::translate(-(float)x, -(float)y, -(float)z));
    return util::Mat4::multiply(proj, view);
}

Camera3D autoCamera3D(const map::MapModel& m, int width, int height) {
    Camera3D cam;
    cam.width = width;
    cam.height = height;

    // A point guaranteed inside a sector: the centroid of its first triangle.
    bool placed = false;
    for (int s = 0; s < static_cast<int>(m.sectorCount()) && !placed; ++s) {
        const map::Triangulation t = map::triangulateSector(m, s);
        if (t.indices.size() >= 3) {
            const util::Vec2 a = t.points[t.indices[0]];
            const util::Vec2 b = t.points[t.indices[1]];
            const util::Vec2 c = t.points[t.indices[2]];
            cam.x = (a.x + b.x + c.x) / 3.0;
            cam.z = (a.y + b.y + c.y) / 3.0;
            cam.y = m.sector(s).floorHeight + 56.0;
            placed = true;
        }
    }
    const util::BBox bb = m.bounds();
    if (!placed && bb.valid()) {
        cam.x = bb.center().x;
        cam.z = bb.center().y;
        cam.y = 56.0;
    }

    // Look toward the map centre.
    if (bb.valid()) {
        const double dirX = bb.center().x - cam.x;
        const double dirZ = bb.center().y - cam.z;
        if (std::fabs(dirX) > 1e-3 || std::fabs(dirZ) > 1e-3)
            cam.yaw = std::atan2(-dirX, -dirZ);
    }
    cam.pitch = -0.10f;
    return cam;
}

MapRenderer3D::MapRenderer3D(render::IRenderDevice& device) : dev_(device) {
    program_ = dev_.createProgram(render::ShaderSources{kVert, kFrag});
}
MapRenderer3D::~MapRenderer3D() {
    if (program_ != render::ShaderHandle::Invalid)
        dev_.destroyProgram(program_);
}

void MapRenderer3D::render(render::IRenderContext& ctx, const map::MapModel& m, const Camera3D& cam) {
    std::vector<V3> geo;

    // Floors + ceilings.
    for (int s = 0; s < static_cast<int>(m.sectorCount()); ++s) {
        const map::Sector& sec = m.sector(s);
        const map::Triangulation t = map::triangulateSector(m, s);
        const float sh = shade(sec.lightLevel);
        for (size_t i = 0; i + 3 <= t.indices.size(); i += 3) {
            const util::Vec2 a = t.points[t.indices[i]];
            const util::Vec2 b = t.points[t.indices[i + 1]];
            const util::Vec2 c = t.points[t.indices[i + 2]];
            tri(geo, a.x, sec.floorHeight, a.y, b.x, sec.floorHeight, b.y, c.x, sec.floorHeight, c.y,
                sh * 0.60f, sh * 0.55f, sh * 0.50f); // floor (warm)
            tri(geo, a.x, sec.ceilHeight, a.y, b.x, sec.ceilHeight, b.y, c.x, sec.ceilHeight, c.y,
                sh * 0.52f, sh * 0.58f, sh * 0.72f); // ceiling (cool)
        }
    }

    // Walls.
    for (int i = 0; i < static_cast<int>(m.linedefCount()); ++i) {
        const map::Linedef& l = m.linedef(i);
        if (l.v1 == map::kNoRef || l.v2 == map::kNoRef)
            continue;
        const int fs = m.frontSector(l);
        if (fs < 0)
            continue;
        const util::Vec2 a = m.vertex(l.v1).pos;
        const util::Vec2 b = m.vertex(l.v2).pos;
        const map::Sector& f = m.sector(fs);
        const float sh = shade(f.lightLevel);
        const int bsIdx = m.backSector(l);
        if (bsIdx == map::kNoRef) {
            wallQuad(geo, a, b, f.floorHeight, f.ceilHeight, sh * 0.82f, sh * 0.82f, sh * 0.85f);
        } else {
            const map::Sector& bk = m.sector(bsIdx);
            const double floLo = std::min(f.floorHeight, bk.floorHeight);
            const double floHi = std::max(f.floorHeight, bk.floorHeight);
            const double ceLo = std::min(f.ceilHeight, bk.ceilHeight);
            const double ceHi = std::max(f.ceilHeight, bk.ceilHeight);
            if (floHi > floLo)
                wallQuad(geo, a, b, floLo, floHi, sh * 0.70f, sh * 0.68f, sh * 0.66f); // step
            if (ceHi > ceLo)
                wallQuad(geo, a, b, ceLo, ceHi, sh * 0.60f, sh * 0.62f, sh * 0.70f);
        }
    }

    ctx.setDepthTest(true);
    ctx.clear(render::Color{0.05f, 0.06f, 0.09f, 1.f});
    ctx.bindProgram(program_);
    const util::Mat4 vp = cam.viewProj();
    ctx.setUniformMat4("uVp", vp.data());

    if (!geo.empty()) {
        const render::BufferHandle buf = dev_.createBuffer(render::BufferType::Vertex,
                                                           render::BufferUsage::Stream, geo.data(),
                                                           geo.size() * sizeof(V3));
        ctx.bindVertexBuffer(buf, kLayout);
        ctx.draw(render::Topology::Triangles, 0, static_cast<uint32_t>(geo.size()));
        dev_.destroyBuffer(buf);
    }
}

} // namespace elads::view
