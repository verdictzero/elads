// SPDX-License-Identifier: GPL-3.0-or-later
// elads — 2D map renderer (backend-agnostic: draws through the render abstraction only).
//
// Builds grid/sector-fill/linedef/vertex geometry from a MapModel and draws it via
// IRenderDevice/IRenderContext, so the same code runs on the desktop-GL and (later) GLES
// backends. See docs/design/04-map-editor.md §2.
#pragma once

#include <string>
#include <unordered_map>

#include "graphics/material_set.h"
#include "mapeditor/edit/selection.h"
#include "mapeditor/model/map_model.h"
#include "render/backend/render_backend.h"

namespace elads::view {

// Optional editor overlay: the hovered + selected objects, emphasized on top of the base map,
// plus an in-progress draw-sector loop (a polyline through the traced points).
struct MapOverlay {
    edit::Selection highlight;         // hovered object (drawn in the hover colour)
    edit::Selection selection;         // selected object (drawn in the selection colour)
    std::vector<util::Vec2> drawLoop;  // draw-sector points traced so far (may be empty)
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
// Owns its shader program (created from the device on construction). When a MaterialSet is
// supplied, sector fills are textured with each sector's floor flat (D5); otherwise they are
// flat-shaded by sector light.
class MapRenderer2D {
public:
    explicit MapRenderer2D(render::IRenderDevice& device, const gfx::MaterialSet* materials = nullptr);
    ~MapRenderer2D();

    void render(render::IRenderContext&, const map::MapModel&, const Camera2D&);
    // Same, plus an editor overlay emphasizing the hovered/selected object.
    void render(render::IRenderContext&, const map::MapModel&, const Camera2D&, const MapOverlay&);

private:
    struct Tex {
        render::TextureHandle handle = render::TextureHandle::Invalid;
        int w = 64, h = 64;
        bool real = false;
    };
    Tex resolve(const std::string& name);

    render::IRenderDevice& dev_;
    const gfx::MaterialSet* materials_ = nullptr;
    render::ShaderHandle program_ = render::ShaderHandle::Invalid;    // colour (grid/lines/verts)
    render::ShaderHandle texProgram_ = render::ShaderHandle::Invalid; // textured fills
    render::TextureHandle white_ = render::TextureHandle::Invalid;
    std::unordered_map<std::string, Tex> cache_;
};

} // namespace elads::view
