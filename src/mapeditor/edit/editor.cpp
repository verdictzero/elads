// SPDX-License-Identifier: GPL-3.0-or-later
#include "mapeditor/edit/editor.h"

#include <cmath>

#include "mapeditor/edit/map_edit.h"

namespace elads::edit {

void MapEditor::setMode(Mode m) {
    if (m == mode_)
        return;
    mode_ = m;
    selection_ = {};
    highlight_ = {};
    cancelDrag();
}

ObjType MapEditor::modeType() const {
    switch (mode_) {
        case Mode::Vertices: return ObjType::Vertex;
        case Mode::Linedefs: return ObjType::Linedef;
        case Mode::Sectors: return ObjType::Sector;
        case Mode::Things: return ObjType::Thing;
    }
    return ObjType::Vertex;
}

double MapEditor::pickRadiusWorld() const {
    const double ppu = cam_.pixelsPerUnit > 1e-9 ? cam_.pixelsPerUnit : 1.0;
    return kPickPixels / ppu;
}

util::Vec2 MapEditor::maybeSnap(util::Vec2 p) const {
    if (!snap_ || grid_ <= 0.0)
        return p;
    return {std::round(p.x / grid_) * grid_, std::round(p.y / grid_) * grid_};
}

void MapEditor::hover(double sx, double sy) {
    highlight_ = pickOfType(model_, screenToWorld(sx, sy), pickRadiusWorld(), modeType());
}

void MapEditor::clickSelect(double sx, double sy) {
    selection_ = pickOfType(model_, screenToWorld(sx, sy), pickRadiusWorld(), modeType());
}

util::Vec2* MapEditor::draggablePos(const Selection& s) {
    if (s.type == ObjType::Vertex && s.index >= 0 && s.index < static_cast<int>(model_.vertexCount()))
        return &model_.vertex(s.index).pos;
    if (s.type == ObjType::Thing && s.index >= 0 && s.index < static_cast<int>(model_.thingCount()))
        return &model_.thing(s.index).pos;
    return nullptr;
}

bool MapEditor::beginDrag(double sx, double sy) {
    if (dragging_)
        return true;
    const Selection s = pickOfType(model_, screenToWorld(sx, sy), pickRadiusWorld(), modeType());
    util::Vec2* pos = draggablePos(s);
    if (!pos)
        return false;
    dragging_ = true;
    dragObj_ = s;
    selection_ = s;
    dragOrig_ = *pos;
    return true;
}

void MapEditor::updateDrag(double sx, double sy) {
    if (!dragging_)
        return;
    if (util::Vec2* pos = draggablePos(dragObj_))
        *pos = maybeSnap(screenToWorld(sx, sy)); // live preview; committed in endDrag
}

void MapEditor::endDrag() {
    if (!dragging_)
        return;
    util::Vec2* pos = draggablePos(dragObj_);
    if (pos) {
        const util::Vec2 finalPos = *pos;
        *pos = dragOrig_; // rewind so the edit op records a clean origin->final undo step
        if (dragObj_.type == ObjType::Vertex)
            moveVertex(model_, undo_, dragObj_.index, finalPos);
        else if (dragObj_.type == ObjType::Thing)
            setThingPosition(model_, undo_, dragObj_.index, finalPos);
    }
    dragging_ = false;
    dragObj_ = {};
}

void MapEditor::cancelDrag() {
    if (!dragging_)
        return;
    if (util::Vec2* pos = draggablePos(dragObj_))
        *pos = dragOrig_;
    dragging_ = false;
    dragObj_ = {};
}

void MapEditor::deleteSelection() {
    if (dragging_)
        cancelDrag();
    if (selection_.type == ObjType::Thing) {
        deleteThing(model_, undo_, selection_.index);
        selection_ = {};
        highlight_ = {};
    }
    // Deleting vertices/linedefs/sectors requires topology repair (attached sides/sectors); left
    // for the fuller B3 edit set. No-op for those kinds today.
}

void MapEditor::nudgeSelection(double dx, double dy) {
    if (dragging_)
        return;
    if (selection_.type == ObjType::Vertex) {
        const util::Vec2 p = model_.vertex(selection_.index).pos;
        moveVertex(model_, undo_, selection_.index, {p.x + dx, p.y + dy});
    } else if (selection_.type == ObjType::Thing) {
        const util::Vec2 p = model_.thing(selection_.index).pos;
        setThingPosition(model_, undo_, selection_.index, {p.x + dx, p.y + dy});
    }
}

void MapEditor::panPixels(double dxPixels, double dyPixels) {
    const double ppu = cam_.pixelsPerUnit > 1e-9 ? cam_.pixelsPerUnit : 1.0;
    cam_.centerX -= dxPixels / ppu; // dragging right moves the view left
    cam_.centerY += dyPixels / ppu; // screen y is flipped relative to world y
}

void MapEditor::zoomAt(double factor, double sx, double sy) {
    if (factor <= 0.0)
        return;
    const util::Vec2 before = screenToWorld(sx, sy);
    cam_.pixelsPerUnit *= factor;
    cam_.pixelsPerUnit = std::max(0.02, std::min(64.0, cam_.pixelsPerUnit));
    const util::Vec2 after = screenToWorld(sx, sy);
    cam_.centerX += before.x - after.x; // keep the point under the cursor fixed
    cam_.centerY += before.y - after.y;
}

} // namespace elads::edit
