// SPDX-License-Identifier: GPL-3.0-or-later
// elads — 2D map renderer (backend-agnostic: draws through the render abstraction only).
//
// Builds grid/sector-fill/linedef/vertex geometry from a MapModel and draws it via
// IRenderDevice/IRenderContext, so the same code runs on the desktop-GL and (later) GLES
// backends. See docs/design/04-map-editor.md §2.
#pragma once

#include "mapeditor/edit/selection.h"
#include "mapeditor/model/map_model.h"
#include "render/backend/render_backend.h"

namespace elads::view {

// Optional editor overlay: the hovered + selected objects, emphasized on top of the base map.
struct MapOverlay {
    edit::Selection highlight; // hovered object (drawn in the hover colour)
    edit::Selection selection; // selected object (drawn in the selection colour)
};

// A top-down orthographic camera over map space (Doom units; +Y is north/up).
struct Camera2D {
    double centerX = 0.0;
    double centerY = 0.0;
    double pixelsPerUnit = 1.0;
    int width = 0;
    int height = 0;
};

// Fit the camera so the whole map fits in width x height with a margin fraction.
Camera2D fitCamera(const map::MapModel&, int width, int height, double marginFrac = 0.08);

// Column-major orthographic matrix mapping the camera's world rect to NDC.
void cameraOrtho(const Camera2D&, float outMat[16]);

// Convert a screen pixel (origin top-left, +x right, +y down) to world coordinates, and back.
// Inverse of the ortho projection; used for hit-testing / picking (see docs/design/04 §2).
util::Vec2 screenToWorld(const Camera2D&, double screenX, double screenY);
util::Vec2 worldToScreen(const Camera2D&, util::Vec2 world);

// Renders sector fills, linedefs, vertices, and a grid for `map` via the render context.
// Owns its shader program (created from the device on construction).
class MapRenderer2D {
public:
    explicit MapRenderer2D(render::IRenderDevice& device);
    ~MapRenderer2D();

    void render(render::IRenderContext&, const map::MapModel&, const Camera2D&);
    // Same, plus an editor overlay emphasizing the hovered/selected object.
    void render(render::IRenderContext&, const map::MapModel&, const Camera2D&, const MapOverlay&);

private:
    render::IRenderDevice& dev_;
    render::ShaderHandle program_ = render::ShaderHandle::Invalid;
};

} // namespace elads::view
