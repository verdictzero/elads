// SPDX-License-Identifier: GPL-3.0-or-later
// elads — archive entry-type detection.
//
// Classifies a lump by an ordered pipeline (known name -> magic bytes -> flat size ->
// Doom-picture structure -> text heuristic), mirroring the approach in
// docs/design/03-data-model.md. Map-marker detection is contextual (needs the next lump)
// and lives in mapeditor/model/doom_map_io.h instead.
#pragma once

#include <string>

#include "util/byte_io.h"

namespace elads::archive {

enum class EntryType {
    Unknown,
    Marker,     // zero-length lump (map/namespace marker)
    Palette,    // PLAYPAL
    Colormap,   // COLORMAP
    Pnames,     // PNAMES
    TextureX,   // TEXTURE1 / TEXTURE2
    Flat,       // raw floor/ceiling texture (by size)
    DoomGfx,    // Doom picture (patch/sprite/graphic)
    Png,        // PNG image
    Mus,        // DMX MUS music
    Midi,       // MIDI music
    DmxSound,   // DMX digital sound (DS*)
    Wad,        // nested WAD
    Zip,        // nested zip (PK3)
    Text,       // ZScript/DECORATE/MAPINFO/etc. or printable text
};

const char* entryTypeName(EntryType);

// Detect the type of a lump from its name and bytes.
EntryType detectEntryType(const std::string& name, const util::Bytes& data);

} // namespace elads::archive
