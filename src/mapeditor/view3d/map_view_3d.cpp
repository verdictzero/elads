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
    const std::vector<map::SectorPlanes> planes = map::computeSectorPlanes(m);
    bool placed = false;
    for (int s = 0; s < static_cast<int>(m.sectorCount()) && !placed; ++s) {
        const map::Triangulation t = map::triangulateSector(m, s);
        if (t.indices.size() >= 3) {
            const util::Vec2 a = t.points[t.indices[0]];
            const util::Vec2 b = t.points[t.indices[1]];
            const util::Vec2 c = t.points[t.indices[2]];
            cam.x = (a.x + b.x + c.x) / 3.0;
            cam.z = (a.y + b.y + c.y) / 3.0;
            // Sit ~56 units above the actual (possibly sloped) floor at this point.
            cam.y = planes[static_cast<size_t>(s)].floor.heightAt(cam.x, cam.z) + 56.0;
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

    // Sloped floor/ceiling planes per sector (flat by default).
    const std::vector<map::SectorPlanes> planes = map::computeSectorPlanes(m);

    // Floors + ceilings — height evaluated per vertex from the sector plane.
    for (int s = 0; s < static_cast<int>(m.sectorCount()); ++s) {
        const map::Sector& sec = m.sector(s);
        const map::SectorPlanes& sp = planes[static_cast<size_t>(s)];
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
                fb.push_back(vtx(p.x, sp.floor.heightAt(p.x, p.y), p.y, p.x / ftex.w, p.y / ftex.h, ftint));
            }
            for (int k = 0; k < 3; ++k) {
                const util::Vec2 p = t.points[t.indices[i + k]];
                cb.push_back(vtx(p.x, sp.ceil.heightAt(p.x, p.y), p.y, p.x / ctex.w, p.y / ctex.h, ctint));
            }
        }
    }

    // Walls — per-endpoint bottom/top heights (so walls follow sloped floors/ceilings).
    auto wall = [&](util::Vec2 a, util::Vec2 b, double zBotA, double zTopA, double zBotB, double zTopB,
                    const Tex& tex, RGB tint) {
        if (zTopA <= zBotA && zTopB <= zBotB)
            return;
        const double len = (b - a).length();
        const double uMax = len / tex.w;
        const double vA = (zTopA - zBotA) / tex.h;
        const double vB = (zTopB - zBotB) / tex.h;
        auto& batch = batchFor(tex.handle);
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
            wall(a, b, fp.floor.heightAt(a.x, a.y), fp.ceil.heightAt(a.x, a.y),
                 fp.floor.heightAt(b.x, b.y), fp.ceil.heightAt(b.x, b.y), tex, tintFor(tex.real, sh, 'W'));
        } else {
            const map::SectorPlanes& bp = planes[static_cast<size_t>(bsIdx)];
            const double fFloA = fp.floor.heightAt(a.x, a.y), fFloB = fp.floor.heightAt(b.x, b.y);
            const double bFloA = bp.floor.heightAt(a.x, a.y), bFloB = bp.floor.heightAt(b.x, b.y);
            const double fCeA = fp.ceil.heightAt(a.x, a.y), fCeB = fp.ceil.heightAt(b.x, b.y);
            const double bCeA = bp.ceil.heightAt(a.x, a.y), bCeB = bp.ceil.heightAt(b.x, b.y);
            if (fFloA != bFloA || fFloB != bFloB) {
                const Tex tex = resolve(side.lower);
                wall(a, b, std::min(fFloA, bFloA), std::max(fFloA, bFloA), std::min(fFloB, bFloB),
                     std::max(fFloB, bFloB), tex, tintFor(tex.real, sh, 'S'));
            }
            if (fCeA != bCeA || fCeB != bCeB) {
                const Tex tex = resolve(side.upper);
                wall(a, b, std::min(fCeA, bCeA), std::max(fCeA, bCeA), std::min(fCeB, bCeB),
                     std::max(fCeB, bCeB), tex, tintFor(tex.real, sh, 'S'));
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
