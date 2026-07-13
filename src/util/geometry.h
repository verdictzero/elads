// SPDX-License-Identifier: GPL-3.0-or-later
// elads — foundational geometry types (GUI/GL-free, header-only).
#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

namespace elads::util {

// Map coordinates are doubles so the same model serves classic integer maps and
// UDMF float maps (see docs/design/03-data-model.md).
struct Vec2 {
    double x = 0.0;
    double y = 0.0;

    constexpr Vec2() = default;
    constexpr Vec2(double x_, double y_) : x(x_), y(y_) {}

    constexpr Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    constexpr Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    constexpr Vec2 operator*(double s) const { return {x * s, y * s}; }

    double length() const { return std::sqrt(x * x + y * y); }
    constexpr double dot(const Vec2& o) const { return x * o.x + y * o.y; }
    // 2D cross product (z of the 3D cross); >0 => o is left of this.
    constexpr double cross(const Vec2& o) const { return x * o.y - y * o.x; }
};

constexpr bool operator==(const Vec2& a, const Vec2& b) { return a.x == b.x && a.y == b.y; }
constexpr bool operator!=(const Vec2& a, const Vec2& b) { return !(a == b); }

// Axis-aligned bounding box. Default is the "empty" box (min > max) so the first
// extend() seeds it correctly.
struct BBox {
    double minX = std::numeric_limits<double>::max();
    double minY = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    double maxY = std::numeric_limits<double>::lowest();

    bool valid() const { return minX <= maxX && minY <= maxY; }

    void extend(const Vec2& p) {
        minX = std::min(minX, p.x);
        minY = std::min(minY, p.y);
        maxX = std::max(maxX, p.x);
        maxY = std::max(maxY, p.y);
    }

    double width() const { return valid() ? maxX - minX : 0.0; }
    double height() const { return valid() ? maxY - minY : 0.0; }
    Vec2 center() const { return {(minX + maxX) * 0.5, (minY + maxY) * 0.5}; }
};

// Perpendicular distance from point p to the segment [a,b] (used by 2D hit-testing;
// see docs/design/04-map-editor.md §2).
inline double distancePointToSegment(const Vec2& p, const Vec2& a, const Vec2& b) {
    const Vec2 ab = b - a;
    const double len2 = ab.dot(ab);
    if (len2 == 0.0)
        return (p - a).length();
    double t = (p - a).dot(ab) / len2;
    t = std::clamp(t, 0.0, 1.0);
    const Vec2 proj{a.x + ab.x * t, a.y + ab.y * t};
    return (p - proj).length();
}

} // namespace elads::util
