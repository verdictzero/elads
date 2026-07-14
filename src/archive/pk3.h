// SPDX-License-Identifier: GPL-3.0-or-later
// elads — PK3/PKE (zip) archive read/write.
//
// PK3s are ordinary zip archives; the top-level folder carries the resource namespace
// (see docs/design/08-formats-reference.md §2). Backed by vendored miniz. Entries are held
// decompressed in memory, matching the WAD model so the rest of the app is container-agnostic.
#pragma once

#include <string>
#include <vector>

#include "util/byte_io.h"

namespace elads::archive {

struct Pk3Entry {
    std::string path;   // full archive path, e.g. "textures/BRICK.png"
    util::Bytes data;   // decompressed bytes
};

// Map a PK3 path to its WAD-namespace equivalent by its top folder
// (textures/flats/patches/sprites/graphics/sounds/music/colormaps/acs/voxels/hires/maps),
// or "global" for root-level lumps.
std::string pk3Namespace(const std::string& path);

class Pk3 {
public:
    Pk3() = default;

    // Parse a zip archive from bytes. Throws std::runtime_error on an invalid zip.
    static Pk3 read(const util::Bytes& zipBytes);

    // Serialize to a zip archive (deflate by default, or stored).
    util::Bytes write(bool compress = true) const;

    Pk3Entry& add(std::string path, util::Bytes data);

    size_t entryCount() const { return entries_.size(); }
    const std::vector<Pk3Entry>& entries() const { return entries_; }
    const Pk3Entry* find(const std::string& path) const;

private:
    std::vector<Pk3Entry> entries_;
};

} // namespace elads::archive
