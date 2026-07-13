// SPDX-License-Identifier: GPL-3.0-or-later
// elads — the in-memory map model (GUI/GL-free).
//
// MapModel is the canonical name for the editable map across the docs (it reuses the
// role of SLADE's SLADEMap). It owns the object vectors and topology helpers; rendering,
// UDMF (de)serialization, and undo live in adjacent components. See
// docs/design/03-data-model.md and docs/design/04-map-editor.md.
#pragma once

#include <vector>

#include "mapeditor/model/map_objects.h"
#include "util/geometry.h"

namespace elads::map {

class MapModel {
public:
    // --- construction: append and return the new object's index ---
    int addVertex(util::Vec2 pos);
    int addSidedef(Sidedef s);
    int addLinedef(Linedef l);
    int addSector(Sector s);
    int addThing(Thing t);

    // --- counts ---
    size_t vertexCount() const { return vertices_.size(); }
    size_t sidedefCount() const { return sidedefs_.size(); }
    size_t linedefCount() const { return linedefs_.size(); }
    size_t sectorCount() const { return sectors_.size(); }
    size_t thingCount() const { return things_.size(); }

    // --- accessors (bounds-checked with at()) ---
    Vertex& vertex(int i) { return vertices_.at(static_cast<size_t>(i)); }
    Sidedef& sidedef(int i) { return sidedefs_.at(static_cast<size_t>(i)); }
    Linedef& linedef(int i) { return linedefs_.at(static_cast<size_t>(i)); }
    Sector& sector(int i) { return sectors_.at(static_cast<size_t>(i)); }
    Thing& thing(int i) { return things_.at(static_cast<size_t>(i)); }
    const Vertex& vertex(int i) const { return vertices_.at(static_cast<size_t>(i)); }
    const Sidedef& sidedef(int i) const { return sidedefs_.at(static_cast<size_t>(i)); }
    const Linedef& linedef(int i) const { return linedefs_.at(static_cast<size_t>(i)); }
    const Sector& sector(int i) const { return sectors_.at(static_cast<size_t>(i)); }
    const Thing& thing(int i) const { return things_.at(static_cast<size_t>(i)); }

    std::vector<Vertex>& vertices() { return vertices_; }
    std::vector<Linedef>& linedefs() { return linedefs_; }
    const std::vector<Vertex>& vertices() const { return vertices_; }
    const std::vector<Linedef>& linedefs() const { return linedefs_; }

    void clear();
    bool empty() const;

    // --- topology helpers ---
    // Sector index a linedef's front/back sidedef points at, or kNoRef.
    int frontSector(const Linedef& l) const;
    int backSector(const Linedef& l) const;
    // Bounding box over all vertices (invalid BBox if there are none).
    util::BBox bounds() const;

    // Nearest vertex to p within pickRadius (map units); kNoRef if none.
    int nearestVertex(util::Vec2 p, double pickRadius) const;
    // Nearest linedef to p within pickRadius (perpendicular distance); kNoRef if none.
    int nearestLinedef(util::Vec2 p, double pickRadius) const;

private:
    std::vector<Vertex> vertices_;
    std::vector<Sidedef> sidedefs_;
    std::vector<Linedef> linedefs_;
    std::vector<Sector> sectors_;
    std::vector<Thing> things_;
};

} // namespace elads::map
