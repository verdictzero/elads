// SPDX-License-Identifier: GPL-3.0-or-later
// elads — map editing operations (GUI/GL-free).
//
// Pure mutations on a MapModel, each recorded through util::UndoManager as an {apply, revert}
// pair so every edit is undoable/redoable. Keeping the operations here (not in MapModel or the
// views) makes them fully headless-testable — the actual "editor" is these functions plus a
// Selection (see selection.h). Structural ops append at the end and undo by truncating, so undo
// is strict LIFO (guaranteed by UndoManager). See docs/design/04-map-editor.md and the B3 plan.
#pragma once

#include <string>
#include <vector>

#include "mapeditor/model/map_model.h"
#include "util/geometry.h"
#include "util/undo.h"

namespace elads::edit {

// Which of a sidedef's three texture slots an op targets.
enum class SideTex { Upper, Middle, Lower };

// --- vertex geometry ---
void moveVertex(map::MapModel&, util::UndoManager&, int vertexIndex, util::Vec2 newPos);
// Translate several vertices by the same delta as one undoable step.
void moveVertices(map::MapModel&, util::UndoManager&, const std::vector<int>& vertexIndices,
                  util::Vec2 delta);

// --- sector / sidedef properties ---
void setSectorHeights(map::MapModel&, util::UndoManager&, int sectorIndex, int floorH, int ceilH);
void setSectorTexture(map::MapModel&, util::UndoManager&, int sectorIndex, bool floor,
                      const std::string& name);
void setSidedefTexture(map::MapModel&, util::UndoManager&, int sidedefIndex, SideTex which,
                       const std::string& name);
void setSidedefOffset(map::MapModel&, util::UndoManager&, int sidedefIndex, int offsetX,
                      int offsetY);

// --- linedef topology ---
// Reverse a linedef's direction (swap v1/v2 and front/back so sectors stay on the same side).
void flipLinedef(map::MapModel&, util::UndoManager&, int lineIndex);
// Insert a vertex at parameter t in (0,1) along the line, splitting it into two collinear lines
// (the new half gets copies of the sidedefs). Returns the new vertex index, or kNoRef if invalid.
int splitLinedef(map::MapModel&, util::UndoManager&, int lineIndex, double t);

// --- things ---
int addThing(map::MapModel&, util::UndoManager&, const map::Thing&); // returns new index
void deleteThing(map::MapModel&, util::UndoManager&, int thingIndex);
void setThingPosition(map::MapModel&, util::UndoManager&, int thingIndex, util::Vec2 newPos);

// --- sector authoring ---
// Create a new one-sided sector from a closed loop of >=3 points (in order). Adds the vertices,
// per-edge sidedefs (using `sideProto`, sector set automatically), one-sided linedefs, and the
// sector (from `sectorProto`). Returns the new sector index, or kNoRef if the loop is too small.
// This is the "draw a room in open space" case; auto-split/merge against existing geometry is
// future work (see the B3 plan item).
int createSector(map::MapModel&, util::UndoManager&, const std::vector<util::Vec2>& loop,
                 const map::Sector& sectorProto, const map::Sidedef& sideProto);

} // namespace elads::edit
