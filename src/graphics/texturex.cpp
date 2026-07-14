// SPDX-License-Identifier: GPL-3.0-or-later
#include "graphics/texturex.h"

#include <cstdint>

namespace elads::gfx {

std::vector<std::string> parsePnames(const util::Bytes& data) {
    std::vector<std::string> names;
    if (data.size() < 4)
        return names;
    util::ByteReader r(data);
    const uint32_t count = r.u32();
    names.reserve(count);
    for (uint32_t i = 0; i < count; ++i)
        names.push_back(r.fixedString(8));
    return names;
}

util::Bytes writePnames(const std::vector<std::string>& names) {
    util::ByteWriter w;
    w.u32(static_cast<uint32_t>(names.size()));
    for (const auto& n : names)
        w.fixedString(n, 8);
    return w.take();
}

std::vector<TextureDef> parseTextureX(const util::Bytes& data) {
    std::vector<TextureDef> textures;
    if (data.size() < 4)
        return textures;
    util::ByteReader r(data);
    const uint32_t count = r.u32();

    std::vector<uint32_t> offsets(count);
    for (uint32_t i = 0; i < count; ++i)
        offsets[i] = r.u32();

    textures.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        util::ByteReader t(data);
        t.seek(offsets[i]);
        TextureDef def;
        def.name = t.fixedString(8);
        t.u32();                       // masked/flags (legacy, ignored)
        def.width = t.u16();
        def.height = t.u16();
        t.u32();                       // columndirectory (obsolete)
        const uint16_t patchCount = t.u16();
        def.patches.reserve(patchCount);
        for (uint16_t p = 0; p < patchCount; ++p) {
            PatchRef ref;
            ref.originX = t.i16();
            ref.originY = t.i16();
            ref.patchIndex = t.u16();
            t.u16();                   // stepdir (obsolete)
            t.u16();                   // colormap (obsolete)
            def.patches.push_back(ref);
        }
        textures.push_back(std::move(def));
    }
    return textures;
}

util::Bytes writeTextureX(const std::vector<TextureDef>& textures) {
    // Serialize each texture record first so we can compute absolute offsets.
    std::vector<util::Bytes> records;
    records.reserve(textures.size());
    for (const auto& def : textures) {
        util::ByteWriter t;
        t.fixedString(def.name, 8);
        t.u32(0);                                          // masked/flags
        t.u16(static_cast<uint16_t>(def.width));
        t.u16(static_cast<uint16_t>(def.height));
        t.u32(0);                                          // columndirectory
        t.u16(static_cast<uint16_t>(def.patches.size()));
        for (const auto& p : def.patches) {
            t.i16(static_cast<int16_t>(p.originX));
            t.i16(static_cast<int16_t>(p.originY));
            t.u16(static_cast<uint16_t>(p.patchIndex));
            t.u16(0);                                      // stepdir
            t.u16(0);                                      // colormap
        }
        records.push_back(t.take());
    }

    const uint32_t count = static_cast<uint32_t>(textures.size());
    const uint32_t headerSize = 4 + count * 4; // count + offset table
    uint32_t cursor = headerSize;

    util::ByteWriter w;
    w.u32(count);
    for (uint32_t i = 0; i < count; ++i) {
        w.u32(cursor);
        cursor += static_cast<uint32_t>(records[i].size());
    }
    for (const auto& rec : records)
        w.bytes(rec);
    return w.take();
}

} // namespace elads::gfx
