// SPDX-License-Identifier: GPL-3.0-or-later
// elads — interactive 2D map editor controller (GUI-free).
//
// Owns the model, the 2D camera, a selection + hover highlight, and the undo stack, and turns
// screen-space input (hover, click, drag, keys) into undoable edits. All logic lives here so the
// UI layer (elads-view now, the wx shell later) is a thin translator of raw input + a renderer of
// `model()` with the `highlight()`/`selection()` overlay — and so the editor is fully unit-testable
// without a window. See docs/design/04-map-editor.md and the "bind B2/B3 into the window" step in
// docs/implementation-plan.md.
#pragma once

#include <vector>

#include "mapeditor/edit/selection.h"
#include "mapeditor/model/map_model.h"
#include "mapeditor/view2d/map_view_2d.h"
#include "util/geometry.h"
#include "util/undo.h"

namespace elads::edit {

class MapEditor {
public:
    // Which object kind clicks and hovers resolve to (Draw = trace a new sector).
    enum class Mode { Vertices, Linedefs, Sectors, Things, Draw };

    explicit MapEditor(map::MapModel model) : model_(std::move(model)) {}

    map::MapModel& model() { return model_; }
    const map::MapModel& model() const { return model_; }
    view::Camera2D& camera() { return cam_; }
    const view::Camera2D& camera() const { return cam_; }
    util::UndoManager& undo() { return undo_; }

    Mode mode() const { return mode_; }
    void setMode(Mode m);
    ObjType modeType() const;

    const Selection& selection() const { return selection_; }
    const Selection& highlight() const { return highlight_; }
    void clearSelection() { selection_ = {}; }

    void setGridSnap(bool on) { snap_ = on; }
    bool gridSnap() const { return snap_; }
    void setGridSize(double g) { if (g > 0.0) grid_ = g; }
    double gridSize() const { return grid_; }

    // Pick radius (world units) for a fixed on-screen pixel radius at the current zoom.
    double pickRadiusWorld() const;
    util::Vec2 screenToWorld(double sx, double sy) const { return view::screenToWorld(cam_, sx, sy); }

    // --- input actions (screen pixels, origin top-left) ---
    void hover(double sx, double sy);              // update the hover highlight
    void clickSelect(double sx, double sy);        // set selection to the object under the cursor

    bool beginDrag(double sx, double sy);          // grab a draggable object (vertex/thing); true if grabbed
    void updateDrag(double sx, double sy);         // live-preview move of the grabbed object
    void endDrag();                                // commit the move as one undo step
    void cancelDrag();                             // discard the in-progress move
    bool dragging() const { return dragging_; }

    void deleteSelection();                         // delete the selected object (things supported)
    void nudgeSelection(double dx, double dy);      // move selected vertex/thing by a world delta (undoable)

    // --- draw-sector tool (Draw mode) ---
    // Add a loop point at the cursor (grid-snapped). If >=3 points and the cursor is near the
    // first point, close the loop into a new sector (undoable) and return true; else return false.
    bool addDrawPoint(double sx, double sy);
    void cancelDraw() { drawPoints_.clear(); }
    const std::vector<util::Vec2>& drawPoints() const { return drawPoints_; }

    bool undoLast() { return undo_.undo(); }
    bool redoLast() { return undo_.redo(); }

    // --- camera ---
    void panPixels(double dxPixels, double dyPixels);
    void zoomAt(double factor, double sx, double sy); // zoom keeping the world point under (sx,sy) fixed

private:
    static constexpr double kPickPixels = 8.0;
    util::Vec2 maybeSnap(util::Vec2 p) const;
    // Pointer to the mutable position of a draggable selection (vertex/thing), or null.
    util::Vec2* draggablePos(const Selection&);

    map::MapModel model_;
    view::Camera2D cam_;
    util::UndoManager undo_;
    Selection selection_, highlight_;
    Mode mode_ = Mode::Vertices;
    bool snap_ = true;
    double grid_ = 8.0;

    bool dragging_ = false;
    Selection dragObj_;
    util::Vec2 dragOrig_;

    std::vector<util::Vec2> drawPoints_; // in-progress draw-sector loop
};

} // namespace elads::edit
