// SPDX-License-Identifier: GPL-3.0-or-later
// elads — UDMF (Universal Doom Map Format) text (de)serialization.
//
// Parses a TEXTMAP lump into a MapModel and writes it back. Keys elads models become typed
// fields; every other key is preserved verbatim in each object's `extra` list so a
// load→save round-trip is lossless. Grammar: docs/design/08-formats-reference.md §5.
#pragma once

#include <string>

#include "mapeditor/model/map_model.h"

namespace elads::map {

struct UdmfMap {
    std::string namespaceId = "zdoom";
    MapModel model;
};

// Parse a TEXTMAP string. Throws std::runtime_error on malformed input.
UdmfMap parseUdmf(const std::string& text);

// Serialize to TEXTMAP text.
std::string writeUdmf(const UdmfMap&);
std::string writeUdmf(const MapModel&, const std::string& namespaceId = "zdoom");

} // namespace elads::map
