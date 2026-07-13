// SPDX-License-Identifier: GPL-3.0-or-later
// elads — lightweight WAD reader/writer (GUI/GL-free core).
//
// This is elads' minimal, dependency-free WAD I/O used by CLI tooling, tests, and CI.
// The full hierarchical archive model (PK3/PAK/GRP/RFF, namespaced VFS, entry-type
// detection) is reused from SLADE when the map/graphics editors land (see
// docs/design/03-data-model.md and ADR-0001); this core covers the base WAD container.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "util/byte_io.h"

namespace elads::archive {

enum class WadType { Iwad, Pwad };

// One directory entry + its bytes. Zero-length lumps are valid (markers such as
// MAP01, THINGS, or S_START/S_END).
struct Lump {
    std::string name; // up to 8 chars, as stored (conventionally uppercase)
    util::Bytes data;
};

// A whole WAD held in memory. read()/write() round-trip byte-for-byte for the
// canonical "header, then lump data, then directory" layout elads emits.
class Wad {
public:
    Wad() = default;

    // Parse a WAD from bytes. Throws std::runtime_error/std::out_of_range on malformed input.
    static Wad read(const util::Bytes& bytes);

    // Serialize to bytes (header + contiguous lump data + directory at end).
    util::Bytes write() const;

    WadType type() const { return type_; }
    void setType(WadType t) { type_ = t; }

    size_t lumpCount() const { return lumps_.size(); }
    const std::vector<Lump>& lumps() const { return lumps_; }
    std::vector<Lump>& lumps() { return lumps_; }

    // Append a lump and return a reference to it.
    Lump& add(const std::string& name, util::Bytes data = {});

    // Case-insensitive lookup (Doom lump names are matched case-insensitively).
    // Returns -1 if not found. `from` allows scanning for repeated names.
    int indexOf(const std::string& name, size_t from = 0) const;
    const Lump* find(const std::string& name) const;

private:
    WadType type_ = WadType::Pwad;
    std::vector<Lump> lumps_;
};

} // namespace elads::archive
