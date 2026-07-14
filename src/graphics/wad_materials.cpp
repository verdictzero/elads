// SPDX-License-Identifier: GPL-3.0-or-later
#include "graphics/wad_materials.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <string>

#include "graphics/composite.h"
#include "graphics/doom_gfx.h"
#include "graphics/flat.h"
#include "graphics/palette.h"
#include "graphics/texturex.h"

namespace elads::gfx {
namespace {

std::string upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

Palette loadPalette(const archive::Wad& wad) {
    if (const archive::Lump* pp = wad.find("PLAYPAL")) {
        try {
            return Palette::fromPlaypal(pp->data, 0);
        } catch (...) {
        }
    }
    // Fallback grayscale palette if the WAD has no PLAYPAL.
    Palette p;
    for (int i = 0; i < 256; ++i)
        p.setColor(i, {static_cast<uint8_t>(i), static_cast<uint8_t>(i), static_cast<uint8_t>(i)});
    return p;
}

bool isFlatStart(const std::string& n) { return n == "F_START" || n == "FF_START"; }
bool isFlatEnd(const std::string& n) { return n == "F_END" || n == "FF_END"; }
bool isFlatSublist(const std::string& n) {
    // F1_START/F2_START/F3_START and their _END markers delimit sublists; skip the markers.
    return (n.size() >= 2 && n.front() == 'F' &&
            (n.find("_START") != std::string::npos || n.find("_END") != std::string::npos));
}

} // namespace

MaterialSet buildMaterialSetFromWad(const archive::Wad& wad) {
    MaterialSet mats;
    const Palette pal = loadPalette(wad);

    // --- Patches (named by PNAMES), decoded as Doom pictures ---
    std::map<std::string, Image> patchImages;
    std::vector<std::string> pnames;
    if (const archive::Lump* pn = wad.find("PNAMES")) {
        pnames = parsePnames(pn->data);
        for (const std::string& name : pnames) {
            const int idx = wad.indexOf(name);
            if (idx < 0)
                continue;
            try {
                patchImages[upper(name)] = decodeDoomGfx(wad.lumps()[static_cast<size_t>(idx)].data, pal);
            } catch (...) {
            }
        }
    }
    auto patchLookup = [&](const std::string& n) -> const Image* {
        const auto it = patchImages.find(upper(n));
        return it == patchImages.end() ? nullptr : &it->second;
    };

    // --- Composite textures from TEXTURE1 / TEXTURE2 ---
    for (const char* texLump : {"TEXTURE1", "TEXTURE2"}) {
        if (const archive::Lump* tx = wad.find(texLump)) {
            for (const TextureDef& def : parseTextureX(tx->data)) {
                if (def.width <= 0 || def.height <= 0)
                    continue;
                mats.add(def.name, assembleTexture(def, pnames, patchLookup));
            }
        }
    }

    // --- Flats between F_START/F_END markers ---
    const auto& lumps = wad.lumps();
    bool inFlats = false;
    for (const auto& l : lumps) {
        const std::string u = upper(l.name);
        if (isFlatStart(u)) {
            inFlats = true;
            continue;
        }
        if (isFlatEnd(u)) {
            inFlats = false;
            continue;
        }
        if (!inFlats || isFlatSublist(u) || l.data.empty())
            continue;
        const FlatDims dims = inferFlatDimensions(l.data.size());
        if (!dims.known)
            continue;
        try {
            mats.add(l.name, decodeFlat(l.data, pal, dims.width, dims.height));
        } catch (...) {
        }
    }

    return mats;
}

} // namespace elads::gfx
