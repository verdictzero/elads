// SPDX-License-Identifier: GPL-3.0-or-later
// elads — wall texture alignment (UV) math (GUI/GL-free, header-only).
//
// Computes the texture coordinates for a wall face from the sidedef X/Y offsets and the classic
// Doom "unpegged" flags, so real maps' wall textures line up the way GZDoom draws them. U runs
// along the wall (distance / texture width, + X offset); V runs down the wall (texture rows), with
// the vertical origin (the world Z where texture row 0 sits) chosen by the peg rules below.
// Sloped walls just evaluate V at each endpoint's own top/bottom Z. See
// docs/design/11-udmf-advanced.md and the A2 item in docs/implementation-plan.md. UDMF per-surface
// scale/rotation and flat panning are a follow-up (they need typed fields promoted from `extra`).
#pragma once

namespace elads::map {

// Classic Doom linedef flags affecting vertical texture pegging.
constexpr int kFlagDontPegTop = 0x0008;    // "upper unpegged"
constexpr int kFlagDontPegBottom = 0x0010; // "lower unpegged"

enum class WallPart {
    OneSidedMiddle, // a solid wall's middle texture (front sector floor..ceiling)
    Upper,          // two-sided upper (between the two ceilings)
    Lower,          // two-sided lower (between the two floors)
};

// World Z at which the texture's top row (V = 0) sits, per Doom peg rules. `sectionBot`/
// `sectionTop` bound the drawn face; `frontCeil` is the front sector's ceiling (used by pegged
// lowers so the texture tiles continuously from the wall above); `texH` is the texture height.
//
// Anchoring (with offsets zero):
//   one-sided middle: pegged -> top at ceiling;      unpegged(bottom) -> bottom at floor
//   upper:            pegged -> bottom at lower ceil; unpegged(top)    -> top at higher ceil
//   lower:            pegged -> tiles from front ceil; unpegged(bottom)-> bottom at lower floor
inline double pegTopZ(WallPart part, double sectionBot, double sectionTop, double frontCeil,
                      double texH, bool dontPegTop, bool dontPegBottom) {
    switch (part) {
        case WallPart::OneSidedMiddle:
            return dontPegBottom ? sectionBot + texH : sectionTop;
        case WallPart::Upper:
            return dontPegTop ? sectionTop : sectionBot + texH;
        case WallPart::Lower:
            return dontPegBottom ? sectionBot + texH : frontCeil;
    }
    return sectionTop;
}

// Texture V (rows, increasing downward) at world height z, given the texture-top world Z.
inline double texV(double z, double topZ, double texH, double offsetY) {
    return (offsetY + (topZ - z)) / texH;
}

// Texture U (columns) at a distance along the wall from its first vertex.
inline double texU(double distAlong, double texW, double offsetX) {
    return (offsetX + distAlong) / texW;
}

} // namespace elads::map
