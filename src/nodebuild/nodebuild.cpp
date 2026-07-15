// SPDX-License-Identifier: GPL-3.0-or-later
#include "nodebuild/nodebuild.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <utility>

#include "util/byte_io.h"

namespace elads::nodebuild {
namespace {

constexpr uint16_t kSubsectorBit = 0x8000; // NF_SUBSECTOR: a child ref is a subsector, not a node
constexpr int kMaxDepth = 512;             // recursion backstop for pathological input
constexpr double kEps = 0.1;               // on-line tolerance (map units)

int16_t toI16(double v) {
    if (v > 32767.0) v = 32767.0;
    if (v < -32768.0) v = -32768.0;
    return static_cast<int16_t>(std::lround(v));
}

struct Seg {
    int v1, v2;            // working-vertex indices
    double x1, y1, x2, y2; // cached endpoints
    int linedef, side;
    double offset;         // distance along the linedef to this seg's start
};

struct BBox {
    double minX = 1e30, minY = 1e30, maxX = -1e30, maxY = -1e30;
    void add(double x, double y) {
        minX = std::min(minX, x);
        minY = std::min(minY, y);
        maxX = std::max(maxX, x);
        maxY = std::max(maxY, y);
    }
};

struct Node {
    double x, y, dx, dy;
    BBox rBox, lBox;
    uint16_t rChild, lChild;
};

struct Subsector {
    int firstSeg, count;
};

class Builder {
public:
    explicit Builder(const map::MapModel& m) : model_(m) {
        // Working vertices start as the model's (kept as doubles; split points are appended).
        for (size_t i = 0; i < m.vertexCount(); ++i)
            verts_.push_back({m.vertex(static_cast<int>(i)).pos.x, m.vertex(static_cast<int>(i)).pos.y});
    }

    // Initial segs: a front seg for every sided linedef, a back seg for two-sided ones.
    std::vector<Seg> initialSegs() {
        std::vector<Seg> segs;
        for (int i = 0; i < static_cast<int>(model_.linedefCount()); ++i) {
            const map::Linedef& l = model_.linedef(i);
            if (l.v1 == map::kNoRef || l.v2 == map::kNoRef)
                continue;
            if (l.front != map::kNoRef)
                segs.push_back(makeSeg(l.v1, l.v2, i, 0));
            if (l.back != map::kNoRef)
                segs.push_back(makeSeg(l.v2, l.v1, i, 1));
        }
        return segs;
    }

    // Recursively partition `segs` into the BSP tree; returns a child ref (node index or
    // subsector|kSubsectorBit).
    uint16_t build(std::vector<Seg> segs, int depth) {
        int part = depth < kMaxDepth ? pickPartition(segs) : -1;
        if (part < 0)
            return static_cast<uint16_t>(makeSubsector(segs) | kSubsectorBit);

        const Seg p = segs[static_cast<size_t>(part)];
        const double px = p.x1, py = p.y1, pdx = p.x2 - p.x1, pdy = p.y2 - p.y1;

        std::vector<Seg> front, back;
        for (const Seg& s : segs)
            partitionSeg(s, px, py, pdx, pdy, front, back);

        Node n;
        n.x = px;
        n.y = py;
        n.dx = pdx;
        n.dy = pdy;
        n.rBox = bboxOf(front);
        n.lBox = bboxOf(back);
        n.rChild = build(std::move(front), depth + 1);
        n.lChild = build(std::move(back), depth + 1);
        nodes_.push_back(n);
        return static_cast<uint16_t>(nodes_.size() - 1);
    }

    BuildResult emit();

private:
    Seg makeSeg(int a, int b, int linedef, int side) {
        Seg s;
        s.v1 = a;
        s.v2 = b;
        s.x1 = verts_[static_cast<size_t>(a)].first;
        s.y1 = verts_[static_cast<size_t>(a)].second;
        s.x2 = verts_[static_cast<size_t>(b)].first;
        s.y2 = verts_[static_cast<size_t>(b)].second;
        s.linedef = linedef;
        s.side = side;
        s.offset = 0.0;
        return s;
    }

    int addVertex(double x, double y) {
        verts_.push_back({x, y});
        ++splitVerts_;
        return static_cast<int>(verts_.size()) - 1;
    }

    // Signed side of (px,py) vs the partition; < 0 front (right child), > 0 back (left child).
    static double side(double x, double y, double sx, double sy, double dx, double dy) {
        return dx * (y - sy) - dy * (x - sx);
    }
    static int cls(double s) { return s < -kEps ? -1 : s > kEps ? 1 : 0; }

    // Choose the partition seg with the best split/balance score, or -1 if the region is convex.
    int pickPartition(const std::vector<Seg>& segs) {
        int best = -1;
        double bestScore = 1e30;
        for (size_t c = 0; c < segs.size(); ++c) {
            const Seg& p = segs[c];
            const double px = p.x1, py = p.y1, pdx = p.x2 - p.x1, pdy = p.y2 - p.y1;
            if (pdx == 0.0 && pdy == 0.0)
                continue;
            int fr = 0, bk = 0, sp = 0;
            for (const Seg& s : segs) {
                const int ca = cls(side(s.x1, s.y1, px, py, pdx, pdy));
                const int cb = cls(side(s.x2, s.y2, px, py, pdx, pdy));
                if ((ca < 0 && cb > 0) || (ca > 0 && cb < 0))
                    ++sp;
                else if (ca > 0 || cb > 0)
                    ++bk;
                else
                    ++fr;
            }
            if (bk + sp == 0)
                continue; // this candidate keeps everything on one side => not a divider
            const double score = sp * 3.0 + std::abs(fr - bk);
            if (score < bestScore) {
                bestScore = score;
                best = static_cast<int>(c);
            }
        }
        return best;
    }

    // Classify (and split) `s` against the partition, appending to front/back.
    void partitionSeg(const Seg& s, double px, double py, double pdx, double pdy,
                      std::vector<Seg>& front, std::vector<Seg>& back) {
        const double sa = side(s.x1, s.y1, px, py, pdx, pdy);
        const double sb = side(s.x2, s.y2, px, py, pdx, pdy);
        const int ca = cls(sa), cb = cls(sb);
        if ((ca < 0 && cb > 0) || (ca > 0 && cb < 0)) {
            // Straddles: split at the crossing point.
            const double t = sa / (sa - sb);
            const double ix = s.x1 + t * (s.x2 - s.x1);
            const double iy = s.y1 + t * (s.y2 - s.y1);
            const int iv = addVertex(ix, iy);
            const double d = std::hypot(ix - s.x1, iy - s.y1);
            Seg s1 = s;
            s1.v2 = iv;
            s1.x2 = ix;
            s1.y2 = iy; // [v1 .. I]
            Seg s2 = s;
            s2.v1 = iv;
            s2.x1 = ix;
            s2.y1 = iy;
            s2.offset = s.offset + d; // [I .. v2]
            if (ca < 0) {             // v1 on front
                front.push_back(s1);
                back.push_back(s2);
            } else {
                back.push_back(s1);
                front.push_back(s2);
            }
        } else if (ca > 0 || cb > 0) {
            back.push_back(s);
        } else {
            front.push_back(s);
        }
    }

    int makeSubsector(const std::vector<Seg>& segs) {
        Subsector ss;
        ss.firstSeg = static_cast<int>(outSegs_.size());
        ss.count = static_cast<int>(segs.size());
        for (const Seg& s : segs)
            outSegs_.push_back(s);
        subsectors_.push_back(ss);
        return static_cast<int>(subsectors_.size()) - 1;
    }

    static BBox bboxOf(const std::vector<Seg>& segs) {
        BBox b;
        for (const Seg& s : segs) {
            b.add(s.x1, s.y1);
            b.add(s.x2, s.y2);
        }
        return b;
    }

    util::Bytes buildBlockmap() const;

    const map::MapModel& model_;
    std::vector<std::pair<double, double>> verts_;
    std::vector<Seg> outSegs_;
    std::vector<Subsector> subsectors_;
    std::vector<Node> nodes_;
    int splitVerts_ = 0;
};

// Liang–Barsky: does segment [x1,y1]-[x2,y2] intersect the axis-aligned rect?
bool segHitsRect(double x1, double y1, double x2, double y2, double xmin, double ymin, double xmax,
                 double ymax) {
    const double dx = x2 - x1, dy = y2 - y1;
    const double p[4] = {-dx, dx, -dy, dy};
    const double q[4] = {x1 - xmin, xmax - x1, y1 - ymin, ymax - y1};
    double u1 = 0.0, u2 = 1.0;
    for (int i = 0; i < 4; ++i) {
        if (p[i] == 0.0) {
            if (q[i] < 0.0)
                return false; // parallel to this edge and outside it
        } else {
            const double t = q[i] / p[i];
            if (p[i] < 0.0)
                u1 = std::max(u1, t);
            else
                u2 = std::min(u2, t);
        }
    }
    return u1 <= u2;
}

util::Bytes Builder::buildBlockmap() const {
    BBox b;
    for (const auto& v : verts_)
        b.add(v.first, v.second);
    if (b.maxX < b.minX) { // no vertices
        b = BBox{};
        b.add(0, 0);
    }
    const int originX = static_cast<int>(std::floor(b.minX)) - 8;
    const int originY = static_cast<int>(std::floor(b.minY)) - 8;
    const int cols = static_cast<int>((b.maxX - originX)) / 128 + 1;
    const int rows = static_cast<int>((b.maxY - originY)) / 128 + 1;
    const int nBlocks = cols * rows;

    std::vector<std::vector<uint16_t>> lists(static_cast<size_t>(nBlocks));
    for (int i = 0; i < static_cast<int>(model_.linedefCount()); ++i) {
        const map::Linedef& l = model_.linedef(i);
        if (l.v1 == map::kNoRef || l.v2 == map::kNoRef)
            continue;
        const auto a = verts_[static_cast<size_t>(l.v1)];
        const auto c = verts_[static_cast<size_t>(l.v2)];
        const int c0 = std::max(0, static_cast<int>((std::min(a.first, c.first) - originX)) / 128);
        const int c1 = std::min(cols - 1, static_cast<int>((std::max(a.first, c.first) - originX)) / 128);
        const int r0 = std::max(0, static_cast<int>((std::min(a.second, c.second) - originY)) / 128);
        const int r1 = std::min(rows - 1, static_cast<int>((std::max(a.second, c.second) - originY)) / 128);
        for (int by = r0; by <= r1; ++by)
            for (int bx = c0; bx <= c1; ++bx) {
                const double xmin = originX + bx * 128.0, ymin = originY + by * 128.0;
                if (segHitsRect(a.first, a.second, c.first, c.second, xmin, ymin, xmin + 128,
                                ymin + 128))
                    lists[static_cast<size_t>(by * cols + bx)].push_back(static_cast<uint16_t>(i));
            }
    }

    util::ByteWriter w;
    w.i16(static_cast<int16_t>(originX));
    w.i16(static_cast<int16_t>(originY));
    w.i16(static_cast<int16_t>(cols));
    w.i16(static_cast<int16_t>(rows));
    // Offsets (in words from the blockmap start): header(4) + offset table(nBlocks) then lists.
    int wordOffset = 4 + nBlocks;
    for (int i = 0; i < nBlocks; ++i) {
        w.u16(static_cast<uint16_t>(wordOffset));
        wordOffset += 2 + static_cast<int>(lists[static_cast<size_t>(i)].size()); // 0x0000 + ids + 0xFFFF
    }
    for (int i = 0; i < nBlocks; ++i) {
        w.u16(0x0000);
        for (uint16_t id : lists[static_cast<size_t>(i)])
            w.u16(id);
        w.u16(0xFFFF);
    }
    return w.take();
}

BuildResult Builder::emit() {
    BuildResult out;

    util::ByteWriter vtx, segs, ssec, nodes, reject;

    for (const auto& v : verts_) {
        vtx.i16(toI16(v.first));
        vtx.i16(toI16(v.second));
    }
    for (const Seg& s : outSegs_) {
        const int bam =
            static_cast<int>(std::lround(std::atan2(s.y2 - s.y1, s.x2 - s.x1) * 32768.0 / M_PI)) &
            0xFFFF;
        segs.u16(static_cast<uint16_t>(s.v1));
        segs.u16(static_cast<uint16_t>(s.v2));
        segs.u16(static_cast<uint16_t>(bam));
        segs.u16(static_cast<uint16_t>(s.linedef));
        segs.u16(static_cast<uint16_t>(s.side));
        segs.i16(toI16(s.offset));
    }
    for (const Subsector& s : subsectors_) {
        ssec.u16(static_cast<uint16_t>(s.count));
        ssec.u16(static_cast<uint16_t>(s.firstSeg));
    }
    auto writeBox = [](util::ByteWriter& w, const BBox& b) {
        w.i16(toI16(b.maxY)); // top
        w.i16(toI16(b.minY)); // bottom
        w.i16(toI16(b.minX)); // left
        w.i16(toI16(b.maxX)); // right
    };
    for (const Node& n : nodes_) {
        nodes.i16(toI16(n.x));
        nodes.i16(toI16(n.y));
        nodes.i16(toI16(n.dx));
        nodes.i16(toI16(n.dy));
        writeBox(nodes, n.rBox);
        writeBox(nodes, n.lBox);
        nodes.u16(n.rChild);
        nodes.u16(n.lChild);
    }
    const size_t nSectors = model_.sectorCount();
    const size_t rejectBytes = (nSectors * nSectors + 7) / 8;
    for (size_t i = 0; i < rejectBytes; ++i)
        reject.u8(0);

    out.lumps = {
        {"VERTEXES", vtx.take()}, {"SEGS", segs.take()},     {"SSECTORS", ssec.take()},
        {"NODES", nodes.take()},  {"REJECT", reject.take()}, {"BLOCKMAP", buildBlockmap()},
    };
    out.stats.segs = static_cast<int>(outSegs_.size());
    out.stats.subsectors = static_cast<int>(subsectors_.size());
    out.stats.nodes = static_cast<int>(nodes_.size());
    out.stats.splitVertices = splitVerts_;
    return out;
}

} // namespace

BuildResult buildNodes(const map::MapModel& model) {
    if (model.linedefCount() == 0)
        throw std::runtime_error("nodebuild: map has no linedefs");
    Builder b(model);
    std::vector<Seg> segs = b.initialSegs();
    if (segs.empty())
        throw std::runtime_error("nodebuild: map has no sided linedefs");
    b.build(std::move(segs), 0);
    return b.emit();
}

std::vector<archive::Lump> buildMapLumps(const map::MapModel& model, map::MapFormat format) {
    const std::vector<archive::Lump> editable = map::writeMap(model, format);
    const BuildResult built = buildNodes(model);

    auto find = [](const std::vector<archive::Lump>& ls, const char* name) -> archive::Lump {
        for (const archive::Lump& l : ls)
            if (l.name == name)
                return l;
        return archive::Lump{name, {}};
    };

    // Canonical vanilla map order; VERTEXES comes from the builder (augmented with split points).
    std::vector<archive::Lump> out = {
        find(editable, "THINGS"),      find(editable, "LINEDEFS"), find(editable, "SIDEDEFS"),
        find(built.lumps, "VERTEXES"), find(built.lumps, "SEGS"),  find(built.lumps, "SSECTORS"),
        find(built.lumps, "NODES"),    find(editable, "SECTORS"),  find(built.lumps, "REJECT"),
        find(built.lumps, "BLOCKMAP"),
    };
    if (format == map::MapFormat::Hexen)
        out.push_back(find(editable, "BEHAVIOR")); // Hexen ACS follows BLOCKMAP
    return out;
}

} // namespace elads::nodebuild
