// SPDX-License-Identifier: GPL-3.0-or-later
#include "mapeditor/edit/map_edit.h"

#include <utility>

namespace elads::edit {

void moveVertex(map::MapModel& m, util::UndoManager& undo, int vi, util::Vec2 newPos) {
    if (vi < 0 || vi >= static_cast<int>(m.vertexCount()))
        return;
    const util::Vec2 oldPos = m.vertex(vi).pos;
    if (oldPos == newPos)
        return;
    undo.perform(
        "move vertex", [&m, vi, newPos] { m.vertex(vi).pos = newPos; },
        [&m, vi, oldPos] { m.vertex(vi).pos = oldPos; });
}

void moveVertices(map::MapModel& m, util::UndoManager& undo, const std::vector<int>& vis,
                  util::Vec2 delta) {
    if (vis.empty() || (delta.x == 0.0 && delta.y == 0.0))
        return;
    // Snapshot exact before/after positions so undo restores the originals bit-for-bit (adding
    // then subtracting a delta is not an exact round-trip for arbitrary doubles).
    std::vector<int> ids;
    std::vector<util::Vec2> oldPos, newPos;
    for (int vi : vis) {
        if (vi < 0 || vi >= static_cast<int>(m.vertexCount()))
            continue;
        const util::Vec2 p = m.vertex(vi).pos;
        ids.push_back(vi);
        oldPos.push_back(p);
        newPos.push_back(p + delta);
    }
    if (ids.empty())
        return;
    undo.perform(
        "move vertices",
        [&m, ids, newPos] {
            for (size_t i = 0; i < ids.size(); ++i)
                m.vertex(ids[i]).pos = newPos[i];
        },
        [&m, ids, oldPos] {
            for (size_t i = 0; i < ids.size(); ++i)
                m.vertex(ids[i]).pos = oldPos[i];
        });
}

void setSectorHeights(map::MapModel& m, util::UndoManager& undo, int si, int floorH, int ceilH) {
    if (si < 0 || si >= static_cast<int>(m.sectorCount()))
        return;
    const int oldF = m.sector(si).floorHeight, oldC = m.sector(si).ceilHeight;
    if (oldF == floorH && oldC == ceilH)
        return;
    undo.perform(
        "set sector heights",
        [&m, si, floorH, ceilH] {
            m.sector(si).floorHeight = floorH;
            m.sector(si).ceilHeight = ceilH;
        },
        [&m, si, oldF, oldC] {
            m.sector(si).floorHeight = oldF;
            m.sector(si).ceilHeight = oldC;
        });
}

void setSectorTexture(map::MapModel& m, util::UndoManager& undo, int si, bool floor,
                      const std::string& name) {
    if (si < 0 || si >= static_cast<int>(m.sectorCount()))
        return;
    std::string& slot = floor ? m.sector(si).floorTex : m.sector(si).ceilTex;
    if (slot == name)
        return;
    const std::string oldName = slot;
    undo.perform(
        "set sector texture",
        [&m, si, floor, name] { (floor ? m.sector(si).floorTex : m.sector(si).ceilTex) = name; },
        [&m, si, floor, oldName] {
            (floor ? m.sector(si).floorTex : m.sector(si).ceilTex) = oldName;
        });
}

namespace {
std::string& sideSlot(map::Sidedef& sd, SideTex which) {
    switch (which) {
        case SideTex::Upper: return sd.upper;
        case SideTex::Lower: return sd.lower;
        case SideTex::Middle:
        default: return sd.middle;
    }
}
} // namespace

void setSidedefTexture(map::MapModel& m, util::UndoManager& undo, int sdi, SideTex which,
                       const std::string& name) {
    if (sdi < 0 || sdi >= static_cast<int>(m.sidedefCount()))
        return;
    const std::string oldName = sideSlot(m.sidedef(sdi), which);
    if (oldName == name)
        return;
    undo.perform(
        "set sidedef texture",
        [&m, sdi, which, name] { sideSlot(m.sidedef(sdi), which) = name; },
        [&m, sdi, which, oldName] { sideSlot(m.sidedef(sdi), which) = oldName; });
}

void setSidedefOffset(map::MapModel& m, util::UndoManager& undo, int sdi, int offX, int offY) {
    if (sdi < 0 || sdi >= static_cast<int>(m.sidedefCount()))
        return;
    const int oldX = m.sidedef(sdi).offsetX, oldY = m.sidedef(sdi).offsetY;
    if (oldX == offX && oldY == offY)
        return;
    undo.perform(
        "set sidedef offset",
        [&m, sdi, offX, offY] {
            m.sidedef(sdi).offsetX = offX;
            m.sidedef(sdi).offsetY = offY;
        },
        [&m, sdi, oldX, oldY] {
            m.sidedef(sdi).offsetX = oldX;
            m.sidedef(sdi).offsetY = oldY;
        });
}

void flipLinedef(map::MapModel& m, util::UndoManager& undo, int li) {
    if (li < 0 || li >= static_cast<int>(m.linedefCount()))
        return;
    auto swap = [&m, li] {
        map::Linedef& l = m.linedef(li);
        std::swap(l.v1, l.v2);
        std::swap(l.front, l.back);
    };
    undo.perform("flip linedef", swap, swap); // self-inverse
}

int splitLinedef(map::MapModel& m, util::UndoManager& undo, int li, double t) {
    if (li < 0 || li >= static_cast<int>(m.linedefCount()) || t <= 0.0 || t >= 1.0)
        return map::kNoRef;
    const map::Linedef& l = m.linedef(li);
    if (l.v1 == map::kNoRef || l.v2 == map::kNoRef)
        return map::kNoRef;

    // Truncation targets for revert (append-only op).
    const size_t vBase = m.vertexCount(), sdBase = m.sidedefCount(), lBase = m.linedefCount();
    const int newVi = static_cast<int>(vBase);

    undo.perform(
        "split linedef",
        [&m, li, t] {
            // Read everything needed before any append (appends may reallocate the vectors).
            const map::Linedef proto = m.linedef(li);
            const util::Vec2 a = m.vertex(proto.v1).pos;
            const util::Vec2 b = m.vertex(proto.v2).pos;
            map::Sidedef frontCopy, backCopy;
            const bool hasF = proto.front != map::kNoRef, hasB = proto.back != map::kNoRef;
            if (hasF)
                frontCopy = m.sidedef(proto.front);
            if (hasB)
                backCopy = m.sidedef(proto.back);

            const int vi =
                m.addVertex({a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t});
            const int nf = hasF ? m.addSidedef(frontCopy) : map::kNoRef;
            const int nb = hasB ? m.addSidedef(backCopy) : map::kNoRef;
            map::Linedef nl = proto;
            nl.v1 = vi;
            nl.v2 = proto.v2;
            nl.front = nf;
            nl.back = nb;
            m.addLinedef(nl);
            m.linedef(li).v2 = vi; // original now ends at the split point
        },
        [&m, li, vBase, sdBase, lBase, oldV2 = m.linedef(li).v2] {
            m.linedef(li).v2 = oldV2;
            m.vertices().resize(vBase);
            m.sidedefs().resize(sdBase);
            m.linedefs().resize(lBase);
        });
    return newVi;
}

int addThing(map::MapModel& m, util::UndoManager& undo, const map::Thing& thing) {
    const int idx = static_cast<int>(m.thingCount());
    undo.perform(
        "add thing", [&m, thing] { m.addThing(thing); }, [&m] { m.things().pop_back(); });
    return idx;
}

void deleteThing(map::MapModel& m, util::UndoManager& undo, int ti) {
    if (ti < 0 || ti >= static_cast<int>(m.thingCount()))
        return;
    const map::Thing saved = m.thing(ti);
    undo.perform(
        "delete thing", [&m, ti] { m.things().erase(m.things().begin() + ti); },
        [&m, ti, saved] { m.things().insert(m.things().begin() + ti, saved); });
}

int createSector(map::MapModel& m, util::UndoManager& undo, const std::vector<util::Vec2>& loop,
                 const map::Sector& sectorProto, const map::Sidedef& sideProto) {
    if (loop.size() < 3)
        return map::kNoRef;
    const int secIdx = static_cast<int>(m.sectorCount());
    const size_t vBase = m.vertexCount(), sdBase = m.sidedefCount(), lBase = m.linedefCount(),
                 secBase = m.sectorCount();
    const std::vector<util::Vec2> pts = loop;

    undo.perform(
        "create sector",
        [&m, pts, sectorProto, sideProto] {
            const int s = m.addSector(sectorProto);
            const int n = static_cast<int>(pts.size());
            const int v0 = static_cast<int>(m.vertexCount());
            for (const util::Vec2& p : pts)
                m.addVertex(p);
            for (int i = 0; i < n; ++i) {
                map::Sidedef sd = sideProto;
                sd.sector = s;
                const int side = m.addSidedef(sd);
                map::Linedef l;
                l.v1 = v0 + i;
                l.v2 = v0 + ((i + 1) % n);
                l.front = side;
                l.flags = 1; // impassable outer wall
                m.addLinedef(l);
            }
        },
        [&m, vBase, sdBase, lBase, secBase] {
            m.linedefs().resize(lBase);
            m.sidedefs().resize(sdBase);
            m.vertices().resize(vBase);
            m.sectors().resize(secBase);
        });
    return secIdx;
}

} // namespace elads::edit
