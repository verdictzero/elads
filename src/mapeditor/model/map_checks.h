// SPDX-License-Identifier: GPL-3.0-or-later
// elads — map validation checks.
//
// Structural sanity checks an editor surfaces before build/playtest: reference integrity,
// zero-length lines, unclosed sectors, unused sectors, missing player start. See
// docs/design/04-map-editor.md (error checks).
#pragma once

#include <string>
#include <vector>

#include "mapeditor/model/map_model.h"

namespace elads::map {

enum class Severity { Warning, Error };

struct MapIssue {
    Severity severity = Severity::Warning;
    std::string category;   // "linedef" | "sidedef" | "sector" | "things"
    std::string message;
    int objectIndex = -1;   // index of the offending object, or -1 (map-wide)
};

// Run all checks over a map and return the issues found (empty == clean).
std::vector<MapIssue> checkMap(const MapModel&);

// Convenience: count issues at Error severity.
int countErrors(const std::vector<MapIssue>&);

} // namespace elads::map
