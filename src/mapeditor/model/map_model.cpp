// SPDX-License-Identifier: GPL-3.0-or-later
#include "mapeditor/model/map_model.h"

namespace elads::map {

int MapModel::addVertex(util::Vec2 pos) {
    vertices_.push_back(Vertex{pos});
    return static_cast<int>(vertices_.size()) - 1;
}
int MapModel::addSidedef(Sidedef s) {
    sidedefs_.push_back(std::move(s));
    return static_cast<int>(sidedefs_.size()) - 1;
}
int MapModel::addLinedef(Linedef l) {
    linedefs_.push_back(std::move(l));
    return static_cast<int>(linedefs_.size()) - 1;
}
int MapModel::addSector(Sector s) {
    sectors_.push_back(std::move(s));
    return static_cast<int>(sectors_.size()) - 1;
}
int MapModel::addThing(Thing t) {
    things_.push_back(std::move(t));
    return static_cast<int>(things_.size()) - 1;
}

void MapModel::clear() {
    vertices_.clear();
    sidedefs_.clear();
    linedefs_.clear();
    sectors_.clear();
    things_.clear();
}

bool MapModel::empty() const {
    return vertices_.empty() && linedefs_.empty() && sectors_.empty() && things_.empty();
}

int MapModel::frontSector(const Linedef& l) const {
    if (l.front == kNoRef)
        return kNoRef;
    return sidedefs_.at(static_cast<size_t>(l.front)).sector;
}

int MapModel::backSector(const Linedef& l) const {
    if (l.back == kNoRef)
        return kNoRef;
    return sidedefs_.at(static_cast<size_t>(l.back)).sector;
}

util::BBox MapModel::bounds() const {
    util::BBox box;
    for (const auto& v : vertices_)
        box.extend(v.pos);
    return box;
}

int MapModel::nearestVertex(util::Vec2 p, double pickRadius) const {
    int best = kNoRef;
    double bestDist = pickRadius;
    for (size_t i = 0; i < vertices_.size(); ++i) {
        const double d = (vertices_[i].pos - p).length();
        if (d <= bestDist) {
            bestDist = d;
            best = static_cast<int>(i);
        }
    }
    return best;
}

int MapModel::nearestLinedef(util::Vec2 p, double pickRadius) const {
    int best = kNoRef;
    double bestDist = pickRadius;
    for (size_t i = 0; i < linedefs_.size(); ++i) {
        const Linedef& l = linedefs_[i];
        if (l.v1 == kNoRef || l.v2 == kNoRef)
            continue;
        const util::Vec2 a = vertices_.at(static_cast<size_t>(l.v1)).pos;
        const util::Vec2 b = vertices_.at(static_cast<size_t>(l.v2)).pos;
        const double d = util::distancePointToSegment(p, a, b);
        if (d <= bestDist) {
            bestDist = d;
            best = static_cast<int>(i);
        }
    }
    return best;
}

} // namespace elads::map
