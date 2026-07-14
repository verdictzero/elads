// SPDX-License-Identifier: GPL-3.0-or-later
// elads — 3D "visual mode" preview (backend-agnostic).
//
// Builds solid 3D geometry from a map — floors/ceilings (earcut) and walls (from linedef +
// sector heights) — and draws it with a perspective camera through the render abstraction.
// Surfaces are textured from a MaterialSet (wall textures + flats), UV-mapped and shaded by
// sector light; surfaces with no material fall back to flat shaded color. Textures/slopes/
// 3D-floors progression: docs/design/04-map-editor.md §3. World axes: X = map X, Y = up, Z = map Y.
#pragma once

#include <string>
#include <unordered_map>

#include "graphics/material_set.h"
#include "mapeditor/model/map_model.h"
#include "render/backend/render_backend.h"
#include "util/mat4.h"

namespace elads::view {

struct Camera3D {
    double x = 0.0;    // world position (map X)
    double y = 48.0;   // world height (up)
    double z = 0.0;    // world position (map Y)
    float yaw = 0.f;   // radians, around Y
    float pitch = 0.f; // radians, around X
    float fovY = 1.2f; // ~69 degrees
    int width = 0;
    int height = 0;

    util::Mat4 viewProj() const;
};

// Place a camera inside the map looking toward its centre (a sensible establishing shot).
Camera3D autoCamera3D(const map::MapModel&, int width, int height);

// A world-space ray (origin + unit direction). World axes: X = map X, Y = up, Z = map Y.
struct Ray3D {
    double ox = 0, oy = 0, oz = 0; // origin (camera position)
    double dx = 0, dy = 0, dz = 0; // unit direction
};

// Ray through screen pixel (origin top-left, +y down) for the given camera. Consistent with
// Camera3D::viewProj so a rendered pixel and its pick ray agree (used for 3D picking, B2).
Ray3D screenRay(const Camera3D&, double screenX, double screenY);

// Intersect a ray with the horizontal plane Y = height. On a hit in front of the camera,
// writes the map-space (x, y) = world (x, z) of the intersection and returns true.
bool rayHitHeight(const Ray3D&, double height, util::Vec2& outMap);

// Four world-space corners of an upright (rotates only around Y), camera-facing billboard whose
// bottom sits at `baseY`, centered over map point (cx, cz) = world (x, z). Writes rows
// {bottom-left, bottom-right, top-left, top-right} as (x, y, z). `yaw` is the camera yaw; used to
// keep the quad broadside to the camera. This is the placeholder for thing sprites (A5).
void billboardCorners(double cx, double baseY, double cz, float yaw, double halfWidth,
                      double height, double out[4][3]);

class MapRenderer3D {
public:
    // `materials` is optional; when null (or a name is missing) surfaces render flat-shaded.
    explicit MapRenderer3D(render::IRenderDevice&, const gfx::MaterialSet* materials = nullptr);
    ~MapRenderer3D();

    void render(render::IRenderContext&, const map::MapModel&, const Camera3D&);

private:
    struct Tex {
        render::TextureHandle handle = render::TextureHandle::Invalid;
        int w = 64;
        int h = 64;
        bool real = false; // true if backed by a material (vs the fallback white texture)
    };
    Tex resolve(const std::string& name);

    render::IRenderDevice& dev_;
    const gfx::MaterialSet* materials_ = nullptr;
    render::ShaderHandle program_ = render::ShaderHandle::Invalid;
    render::TextureHandle white_ = render::TextureHandle::Invalid;
    std::unordered_map<std::string, Tex> cache_;
};

} // namespace elads::view
