// SPDX-License-Identifier: GPL-3.0-or-later
// elads — 3D "visual mode" preview (backend-agnostic).
//
// Builds solid 3D geometry from a map — floors/ceilings (earcut) and walls (from linedef +
// sector heights) — and draws it with a perspective camera through the render abstraction.
// v1 is untextured, shaded by sector light. Textures/slopes/3D-floors are later stages
// (docs/design/04-map-editor.md §3). World axes: X = map X, Y = height (up), Z = map Y.
#pragma once

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

class MapRenderer3D {
public:
    explicit MapRenderer3D(render::IRenderDevice&);
    ~MapRenderer3D();

    void render(render::IRenderContext&, const map::MapModel&, const Camera3D&);

private:
    render::IRenderDevice& dev_;
    render::ShaderHandle program_ = render::ShaderHandle::Invalid;
};

} // namespace elads::view
