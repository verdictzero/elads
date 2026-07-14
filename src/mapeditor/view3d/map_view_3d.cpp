// SPDX-License-Identifier: GPL-3.0-or-later
#include "mapeditor/view3d/map_view_3d.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "mapeditor/model/planes.h"
#include "mapeditor/model/sector_tri.h"

namespace elads::view {
namespace {

// Textured 3D vertex: position (vec3) + uv (vec2) + tint (vec3 = light * base).
struct TV {
    float x, y, z, u, v, r, g, b;
};
constexpr render::VertexAttrib kAttribs[] = {{0, render::AttribType::Float3, 0},
                                             {1, render::AttribType::Float2, 12},
                                             {2, render::AttribType::Float3, 20}};
constexpr render::VertexLayout kLayout{kAttribs, 3, sizeof(TV)};

const char* kVert = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec3 aTint;
uniform mat4 uVp;
out vec2 vUV;
out vec3 vTint;
void main() {
    gl_Position = uVp * vec4(aPos, 1.0);
    vUV = aUV;
    vTint = aTint;
}
)";
const char* kFrag = R"(#version 330 core
in vec2 vUV;
in vec3 vTint;
out vec4 fragColor;
uniform sampler2D uTex;
void main() { fragColor = vec4(texture(uTex, vUV).rgb * vTint, 1.0); }
)";

float shade(int light) {
    const float l = std::clamp(light, 0, 255) / 255.f;
    return 0.20f + l * 0.75f;
}

struct RGB {
    float r, g, b;
};
// Tint: neutral (texture shows true color) when real; otherwise a surface-typed shaded color.
RGB tintFor(bool real, float sh, char kind) {
    if (real)
        return {sh, sh, sh};
    switch (kind) {
        case 'F': return {sh * 0.60f, sh * 0.55f, sh * 0.50f}; // floor
        case 'C': return {sh * 0.52f, sh * 0.58f, sh * 0.72f}; // ceiling
        case 'S': return {sh * 0.70f, sh * 0.68f, sh * 0.66f}; // step
        default:  return {sh * 0.82f, sh * 0.82f, sh * 0.85f}; // wall
    }
}

} // namespace

util::Mat4 Camera3D::viewProj() const {
    const float aspect = height > 0 ? static_cast<float>(width) / height : 1.f;
    const util::Mat4 proj = util::Mat4::perspective(fovY, aspect, 4.f, 20000.f);
    util::Mat4 view = util::Mat4::multiply(util::Mat4::rotateX(-pitch), util::Mat4::rotateY(-yaw));
    view = util::Mat4::multiply(view, util::Mat4::translate(-(float)x, -(float)y, -(float)z));
    return util::Mat4::multiply(proj, view);
}

Camera3D autoCamera3D(const map::MapModel& m, int width, int height) {
    Camera3D cam;
    cam.width = width;
    cam.height = height;
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
    if (bb.valid()) {
        const double dirX = bb.center().x - cam.x;
        const double dirZ = bb.center().y - cam.z;
        if (std::fabs(dirX) > 1e-3 || std::fabs(dirZ) > 1e-3)
            cam.yaw = std::atan2(-dirX, -dirZ);
    }
    cam.pitch = -0.10f;
    return cam;
}

Ray3D screenRay(const Camera3D& c, double screenX, double screenY) {
    const double aspect = c.height > 0 ? static_cast<double>(c.width) / c.height : 1.0;
    const double tanHalf = std::tan(c.fovY * 0.5);
    const double ndcX = c.width > 0 ? (screenX / c.width) * 2.0 - 1.0 : 0.0;
    const double ndcY = c.height > 0 ? 1.0 - (screenY / c.height) * 2.0 : 0.0;

    // World-space camera basis matching Camera3D::viewProj (rotateX(-pitch)*rotateY(-yaw)).
    const double cp = std::cos(c.pitch), sp = std::sin(c.pitch);
    const double cy = std::cos(c.yaw), sy = std::sin(c.yaw);
    const double fx = -sy * cp, fy = sp, fz = -cy * cp;       // forward (view -Z)
    const double rx = cy, ry = 0.0, rz = -sy;                 // right (view +X)
    const double ux = sy * sp, uy = cp, uz = cy * sp;         // up (view +Y)

    const double sX = ndcX * aspect * tanHalf, sY = ndcY * tanHalf;
    double dx = fx + sX * rx + sY * ux;
    double dy = fy + sX * ry + sY * uy;
    double dz = fz + sX * rz + sY * uz;
    const double len = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (len > 1e-12) {
        dx /= len;
        dy /= len;
        dz /= len;
    }
    return Ray3D{c.x, c.y, c.z, dx, dy, dz};
}

bool rayHitHeight(const Ray3D& r, double height, util::Vec2& outMap) {
    if (std::fabs(r.dy) < 1e-9)
        return false; // parallel to the plane
    const double t = (height - r.oy) / r.dy;
    if (t < 0.0)
        return false; // behind the camera
    outMap = {r.ox + r.dx * t, r.oz + r.dz * t};
    return true;
}

MapRenderer3D::MapRenderer3D(render::IRenderDevice& device, const gfx::MaterialSet* materials)
    : dev_(device), materials_(materials) {
    program_ = dev_.createProgram(render::ShaderSources{kVert, kFrag});
    const uint8_t whitePx[4] = {255, 255, 255, 255};
    render::TextureDesc wd;
    wd.width = 1;
    wd.height = 1;
    wd.pixels = whitePx;
    white_ = dev_.createTexture(wd);
}

MapRenderer3D::~MapRenderer3D() {
    for (auto& kv : cache_)
        if (kv.second.real)
            dev_.destroyTexture(kv.second.handle);
    if (white_ != render::TextureHandle::Invalid)
        dev_.destroyTexture(white_);
    if (program_ != render::ShaderHandle::Invalid)
        dev_.destroyProgram(program_);
}

MapRenderer3D::Tex MapRenderer3D::resolve(const std::string& name) {
    if (name.empty() || name == "-")
        return {white_, 64, 64, false};
    const auto it = cache_.find(name);
    if (it != cache_.end())
        return it->second;

    Tex tex{white_, 64, 64, false};
    if (materials_) {
        if (const gfx::Image* img = materials_->find(name)) {
            render::TextureDesc d;
            d.width = img->width;
            d.height = img->height;
            d.pixels = img->rgba.data();
            tex = {dev_.createTexture(d), img->width, img->height, true};
        }
    }
    cache_.emplace(name, tex);
    return tex;
}

void MapRenderer3D::render(render::IRenderContext& ctx, const map::MapModel& m, const Camera3D& cam) {
    // Per-texture triangle batches (key = GL texture handle value).
    std::unordered_map<uint32_t, std::vector<TV>> batches;
    auto batchFor = [&](render::TextureHandle h) -> std::vector<TV>& {
        return batches[static_cast<uint32_t>(h)];
    };

    auto vtx = [](double x, double y, double z, double u, double v, RGB t) {
        return TV{(float)x, (float)y, (float)z, (float)u, (float)v, t.r, t.g, t.b};
    };

    // Sector floor/ceiling planes (flat unless a slope source applies). Evaluated per-vertex so
    // sloped surfaces tilt; flat sectors reduce to a constant height.
    const std::vector<map::SectorPlanes> planes = map::computeSectorPlanes(m);

    // Floors + ceilings.
    for (int s = 0; s < static_cast<int>(m.sectorCount()); ++s) {
        const map::Sector& sec = m.sector(s);
        const map::SectorPlanes& pl = planes[static_cast<size_t>(s)];
        const map::Triangulation t = map::triangulateSector(m, s);
        const float sh = shade(sec.lightLevel);
        const Tex ftex = resolve(sec.floorTex);
        const Tex ctex = resolve(sec.ceilTex);
        const RGB ftint = tintFor(ftex.real, sh, 'F');
        const RGB ctint = tintFor(ctex.real, sh, 'C');
        auto& fb = batchFor(ftex.handle);
        auto& cb = batchFor(ctex.handle);
        for (size_t i = 0; i + 3 <= t.indices.size(); i += 3) {
            for (int k = 0; k < 3; ++k) {
                const util::Vec2 p = t.points[t.indices[i + k]];
                fb.push_back(
                    vtx(p.x, pl.floor.heightAt(p), p.y, p.x / ftex.w, p.y / ftex.h, ftint));
            }
            for (int k = 0; k < 3; ++k) {
                const util::Vec2 p = t.points[t.indices[i + k]];
                cb.push_back(vtx(p.x, pl.ceil.heightAt(p), p.y, p.x / ctex.w, p.y / ctex.h, ctint));
            }
        }
    }

    // Wall quad with independent per-endpoint bottom/top heights (so it follows sloped floors
    // and ceilings). UV: U = distance-along / tex.w, V from each endpoint's own height span.
    auto wall = [&](util::Vec2 a, util::Vec2 b, double zBotA, double zTopA, double zBotB,
                    double zTopB, const Tex& tex, RGB tint) {
        if (zTopA <= zBotA && zTopB <= zBotB)
            return; // degenerate at both ends
        const double len = (b - a).length();
        const double uMax = len / tex.w;
        const double vA = (zTopA - zBotA) / tex.h;
        const double vB = (zTopB - zBotB) / tex.h;
        auto& batch = batchFor(tex.handle);
        // two triangles: (a,bot)-(b,bot)-(b,top) and (a,bot)-(b,top)-(a,top)
        batch.push_back(vtx(a.x, zBotA, a.y, 0, vA, tint));
        batch.push_back(vtx(b.x, zBotB, b.y, uMax, vB, tint));
        batch.push_back(vtx(b.x, zTopB, b.y, uMax, 0, tint));
        batch.push_back(vtx(a.x, zBotA, a.y, 0, vA, tint));
        batch.push_back(vtx(b.x, zTopB, b.y, uMax, 0, tint));
        batch.push_back(vtx(a.x, zTopA, a.y, 0, 0, tint));
    };

    for (int i = 0; i < static_cast<int>(m.linedefCount()); ++i) {
        const map::Linedef& l = m.linedef(i);
        if (l.v1 == map::kNoRef || l.v2 == map::kNoRef || l.front == map::kNoRef)
            continue;
        const int fs = m.frontSector(l);
        if (fs < 0)
            continue;
        const util::Vec2 a = m.vertex(l.v1).pos;
        const util::Vec2 b = m.vertex(l.v2).pos;
        const map::Sector& f = m.sector(fs);
        const map::SectorPlanes& fp = planes[static_cast<size_t>(fs)];
        const map::Sidedef& side = m.sidedef(l.front);
        const float sh = shade(f.lightLevel);
        const int bsIdx = m.backSector(l);
        if (bsIdx == map::kNoRef) {
            const Tex tex = resolve(side.middle);
            wall(a, b, fp.floor.heightAt(a), fp.ceil.heightAt(a), fp.floor.heightAt(b),
                 fp.ceil.heightAt(b), tex, tintFor(tex.real, sh, 'W'));
        } else {
            const map::SectorPlanes& bp = planes[static_cast<size_t>(bsIdx)];
            // Lower step: between the two floor planes (bottom = lower plane, top = higher).
            const double fFA = fp.floor.heightAt(a), bFA = bp.floor.heightAt(a);
            const double fFB = fp.floor.heightAt(b), bFB = bp.floor.heightAt(b);
            if (fFA != bFA || fFB != bFB) {
                const Tex tex = resolve(side.lower);
                wall(a, b, std::min(fFA, bFA), std::max(fFA, bFA), std::min(fFB, bFB),
                     std::max(fFB, bFB), tex, tintFor(tex.real, sh, 'S'));
            }
            // Upper: between the two ceiling planes.
            const double fCA = fp.ceil.heightAt(a), bCA = bp.ceil.heightAt(a);
            const double fCB = fp.ceil.heightAt(b), bCB = bp.ceil.heightAt(b);
            if (fCA != bCA || fCB != bCB) {
                const Tex tex = resolve(side.upper);
                wall(a, b, std::min(fCA, bCA), std::max(fCA, bCA), std::min(fCB, bCB),
                     std::max(fCB, bCB), tex, tintFor(tex.real, sh, 'S'));
            }
        }
    }

    ctx.setDepthTest(true);
    ctx.clear(render::Color{0.05f, 0.06f, 0.09f, 1.f});
    ctx.bindProgram(program_);
    const util::Mat4 vp = cam.viewProj();
    ctx.setUniformMat4("uVp", vp.data());

    for (auto& kv : batches) {
        if (kv.second.empty())
            continue;
        ctx.bindTexture(0, static_cast<render::TextureHandle>(kv.first));
        const render::BufferHandle buf =
            dev_.createBuffer(render::BufferType::Vertex, render::BufferUsage::Stream,
                              kv.second.data(), kv.second.size() * sizeof(TV));
        ctx.bindVertexBuffer(buf, kLayout);
        ctx.draw(render::Topology::Triangles, 0, static_cast<uint32_t>(kv.second.size()));
        dev_.destroyBuffer(buf);
    }
}

} // namespace elads::view
