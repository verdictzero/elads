// SPDX-License-Identifier: GPL-3.0-or-later
#include "archive/wad.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace elads::archive {

namespace {
constexpr size_t kHeaderSize = 12;
constexpr size_t kDirEntrySize = 16;
constexpr size_t kNameLen = 8;

std::string upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}
} // namespace

Wad Wad::read(const util::Bytes& bytes) {
    util::ByteReader r(bytes);

    const std::string id = r.fixedString(4);
    Wad wad;
    if (id == "IWAD")
        wad.type_ = WadType::Iwad;
    else if (id == "PWAD")
        wad.type_ = WadType::Pwad;
    else
        throw std::runtime_error("not a WAD: bad identification '" + id + "'");

    const uint32_t numLumps = r.u32();
    const uint32_t dirOffset = r.u32();

    if (static_cast<size_t>(dirOffset) + static_cast<size_t>(numLumps) * kDirEntrySize > bytes.size())
        throw std::runtime_error("WAD directory extends past end of file");

    wad.lumps_.reserve(numLumps);
    r.seek(dirOffset);
    for (uint32_t i = 0; i < numLumps; ++i) {
        const uint32_t filePos = r.u32();
        const uint32_t size = r.u32();
        const std::string name = r.fixedString(kNameLen);

        if (size > 0 && static_cast<size_t>(filePos) + size > bytes.size())
            throw std::runtime_error("lump '" + name + "' data extends past end of file");

        Lump lump;
        lump.name = name;
        if (size > 0)
            lump.data.assign(bytes.begin() + filePos, bytes.begin() + filePos + size);
        wad.lumps_.push_back(std::move(lump));
    }
    return wad;
}

util::Bytes Wad::write() const {
    util::ByteWriter w;

    // Header: id, numlumps, infotableofs (back-patched once the directory offset is known).
    w.fixedString(type_ == WadType::Iwad ? "IWAD" : "PWAD", 4);
    w.u32(static_cast<uint32_t>(lumps_.size()));
    const size_t dirOffsetPatchPos = w.size();
    w.u32(0); // placeholder for infotableofs

    // Lump data, contiguous after the header. Record each lump's filepos.
    std::vector<uint32_t> filePos(lumps_.size());
    for (size_t i = 0; i < lumps_.size(); ++i) {
        filePos[i] = static_cast<uint32_t>(w.size());
        w.bytes(lumps_[i].data);
    }

    // Directory at the end.
    const uint32_t dirOffset = static_cast<uint32_t>(w.size());
    for (size_t i = 0; i < lumps_.size(); ++i) {
        // A zero-length lump conventionally records filepos 0.
        w.u32(lumps_[i].data.empty() ? 0u : filePos[i]);
        w.u32(static_cast<uint32_t>(lumps_[i].data.size()));
        w.fixedString(lumps_[i].name, kNameLen);
    }

    w.patchU32(dirOffsetPatchPos, dirOffset);
    return w.take();
}

Lump& Wad::add(const std::string& name, util::Bytes data) {
    lumps_.push_back(Lump{name, std::move(data)});
    return lumps_.back();
}

int Wad::indexOf(const std::string& name, size_t from) const {
    const std::string target = upper(name);
    for (size_t i = from; i < lumps_.size(); ++i)
        if (upper(lumps_[i].name) == target)
            return static_cast<int>(i);
    return -1;
}

const Lump* Wad::find(const std::string& name) const {
    const int i = indexOf(name);
    return i < 0 ? nullptr : &lumps_[static_cast<size_t>(i)];
}

} // namespace elads::archive
