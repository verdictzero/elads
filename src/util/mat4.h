// SPDX-License-Identifier: GPL-3.0-or-later
// elads — minimal column-major 4x4 matrix math (header-only) for the 3D camera.
#pragma once

#include <cmath>

namespace elads::util {

struct Mat4 {
    float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}; // column-major identity
    const float* data() const { return m; }

    static Mat4 identity() { return Mat4{}; }

    // Returns a * b (apply b first, then a).
    static Mat4 multiply(const Mat4& a, const Mat4& b) {
        Mat4 r;
        for (int col = 0; col < 4; ++col)
            for (int row = 0; row < 4; ++row) {
                float s = 0.f;
                for (int k = 0; k < 4; ++k)
                    s += a.m[k * 4 + row] * b.m[col * 4 + k];
                r.m[col * 4 + row] = s;
            }
        return r;
    }

    static Mat4 translate(float x, float y, float z) {
        Mat4 r;
        r.m[12] = x;
        r.m[13] = y;
        r.m[14] = z;
        return r;
    }

    static Mat4 rotateX(float rad) {
        const float c = std::cos(rad), s = std::sin(rad);
        Mat4 r;
        r.m[5] = c;
        r.m[6] = s;
        r.m[9] = -s;
        r.m[10] = c;
        return r;
    }

    static Mat4 rotateY(float rad) {
        const float c = std::cos(rad), s = std::sin(rad);
        Mat4 r;
        r.m[0] = c;
        r.m[2] = -s;
        r.m[8] = s;
        r.m[10] = c;
        return r;
    }

    // Right-handed perspective, clip z in [-1, 1]. fovY in radians.
    static Mat4 perspective(float fovY, float aspect, float zNear, float zFar) {
        const float f = 1.f / std::tan(fovY * 0.5f);
        Mat4 r;
        for (int i = 0; i < 16; ++i)
            r.m[i] = 0.f;
        r.m[0] = f / aspect;
        r.m[5] = f;
        r.m[10] = (zFar + zNear) / (zNear - zFar);
        r.m[11] = -1.f;
        r.m[14] = (2.f * zFar * zNear) / (zNear - zFar);
        return r;
    }
};

} // namespace elads::util
