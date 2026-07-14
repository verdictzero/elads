// SPDX-License-Identifier: GPL-3.0-or-later
#include "archive/entry_type.h"

#include <algorithm>
#include <cctype>
#include <cstdint>

#include "graphics/doom_gfx.h" // looksLikeDoomGfx
#include "graphics/flat.h"     // inferFlatDimensions

namespace elads::archive {
namespace {

std::string upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

bool magic(const util::Bytes& d, std::initializer_list<int> bytes) {
    if (d.size() < bytes.size())
        return false;
    size_t i = 0;
    for (int b : bytes)
        if (d[i++] != static_cast<uint8_t>(b))
            return false;
    return true;
}

// Names that are definitively text definition lumps.
bool isTextLumpName(const std::string& u) {
    static const char* kNames[] = {
        "MAPINFO",  "ZMAPINFO", "GAMEINFO", "DECORATE", "ZSCRIPT", "SNDINFO",  "SNDSEQ",
        "ANIMDEFS", "GLDEFS",   "LANGUAGE", "DEHACKED", "TEXTURES", "CVARINFO", "MODELDEF",
        "KEYCONF",  "TERRAIN",  "LOCKDEFS", "DECALDEF", "MENUDEF",  "TEXTMAP",  "DIALOGUE",
        "REVERBS",  "X11R6RGB", "DMXGUS",
    };
    for (const char* n : kNames)
        if (u == n)
            return true;
    return false;
}

bool looksLikeText(const util::Bytes& d) {
    for (uint8_t c : d)
        if (!(c == '\t' || c == '\n' || c == '\r' || (c >= 0x20 && c <= 0x7E)))
            return false;
    return !d.empty();
}

} // namespace

const char* entryTypeName(EntryType t) {
    switch (t) {
        case EntryType::Marker:   return "marker";
        case EntryType::Palette:  return "palette";
        case EntryType::Colormap: return "colormap";
        case EntryType::Pnames:   return "pnames";
        case EntryType::TextureX: return "texturex";
        case EntryType::Flat:     return "flat";
        case EntryType::DoomGfx:  return "gfx";
        case EntryType::Png:      return "png";
        case EntryType::Mus:      return "mus";
        case EntryType::Midi:     return "midi";
        case EntryType::DmxSound: return "sound";
        case EntryType::Wad:      return "wad";
        case EntryType::Zip:      return "zip";
        case EntryType::Text:     return "text";
        case EntryType::Unknown:  return "unknown";
    }
    return "unknown";
}

EntryType detectEntryType(const std::string& name, const util::Bytes& data) {
    if (data.empty())
        return EntryType::Marker;

    // 1) Definitive lump names.
    const std::string u = upper(name);
    if (u == "PLAYPAL") return EntryType::Palette;
    if (u == "COLORMAP") return EntryType::Colormap;
    if (u == "PNAMES") return EntryType::Pnames;
    if (u == "TEXTURE1" || u == "TEXTURE2") return EntryType::TextureX;
    if (isTextLumpName(u)) return EntryType::Text;

    // 2) Magic bytes.
    if (magic(data, {'I', 'W', 'A', 'D'}) || magic(data, {'P', 'W', 'A', 'D'}))
        return EntryType::Wad;
    if (magic(data, {0x50, 0x4B, 0x03, 0x04}))
        return EntryType::Zip;
    if (magic(data, {0x89, 'P', 'N', 'G'}))
        return EntryType::Png;
    if (magic(data, {'M', 'T', 'h', 'd'}))
        return EntryType::Midi;
    if (magic(data, {'M', 'U', 'S', 0x1A}))
        return EntryType::Mus;

    // 3) DMX digital sound: format id 3 (uint16 LE), plausible sample rate, and a declared
    //    sample count consistent with the lump size (padding tolerated). The consistency
    //    checks avoid colliding with a Doom picture whose width happens to be 3.
    if (data.size() >= 8 && data[0] == 0x03 && data[1] == 0x00) {
        util::ByteReader r(data);
        r.u16();                          // format id (== 3)
        const uint16_t rate = r.u16();
        const uint32_t samples = r.u32();
        const size_t declared = static_cast<size_t>(8) + samples;
        if (rate >= 4000 && rate <= 48000 && data.size() >= declared &&
            data.size() <= declared + 48)
            return EntryType::DmxSound;
    }

    // 4) Flat by canonical size.
    if (gfx::inferFlatDimensions(data.size()).known)
        return EntryType::Flat;

    // 5) Doom picture structure.
    if (gfx::looksLikeDoomGfx(data))
        return EntryType::DoomGfx;

    // 6) Printable text.
    if (looksLikeText(data))
        return EntryType::Text;

    return EntryType::Unknown;
}

} // namespace elads::archive
