// SPDX-License-Identifier: GPL-3.0-or-later
#include "mapeditor/model/sector_tri.h"

#include <array>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "earcut.hpp"

namespace elads::map {

double Triangulation::area() const {
    double total = 0.0;
    for (size_t t = 0; t + 3 <= indices.size(); t += 3) {
        const util::Vec2& a = points[indices[t]];
        const util::Vec2& b = points[indices[t + 1]];
        const util::Vec2& c = points[indices[t + 2]];
        total += std::fabs((b - a).cross(c - a)) * 0.5;
    }
    return total;
}

Triangulation triangulateSector(const MapModel& m, int sectorIndex) {
    Triangulation out;

    // Collect directed boundary edges, oriented so the sector interior is consistent:
    // a linedef whose FRONT side is in this sector contributes v1->v2; a BACK side, v2->v1.
    std::vector<std::pair<int, int>> edges;
    for (size_t i = 0; i < m.linedefCount(); ++i) {
        const Linedef& l = m.linedef(static_cast<int>(i));
        if (l.v1 == kNoRef || l.v2 == kNoRef)
            continue;
        if (m.frontSector(l) == sectorIndex)
            edges.push_back({l.v1, l.v2});
        if (m.backSector(l) == sectorIndex)
            edges.push_back({l.v2, l.v1});
    }
    if (edges.size() < 3)
        return out;

    // Chain edges into closed loops by following start->end vertex links.
    std::unordered_map<int, int> nextOf;
    for (const auto& e : edges)
        nextOf.emplace(e.first, e.second); // first mapping wins (simple sectors)

    std::unordered_set<int> visited;
    std::vector<std::vector<int>> loops;
    for (const auto& e : edges) {
        if (visited.count(e.first))
            continue;
        std::vector<int> loop;
        int v = e.first;
        while (nextOf.count(v) && !visited.count(v)) {
            visited.insert(v);
            loop.push_back(v);
            v = nextOf[v];
        }
        if (loop.size() >= 3)
            loops.push_back(std::move(loop));
    }
    if (loops.empty())
        return out;

    // Feed loops to earcut (first loop = outer ring, remaining = holes).
    using Point = std::array<double, 2>;
    std::vector<std::vector<Point>> rings;
    rings.reserve(loops.size());
    for (const auto& loop : loops) {
        std::vector<Point> ring;
        ring.reserve(loop.size());
        for (int vi : loop) {
            const util::Vec2 p = m.vertex(vi).pos;
            ring.push_back({p.x, p.y});
            out.points.push_back(p);
        }
        rings.push_back(std::move(ring));
    }

    out.indices = mapbox::earcut<uint32_t>(rings);
    return out;
}

} // namespace elads::map
