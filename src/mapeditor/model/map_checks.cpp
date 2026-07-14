// SPDX-License-Identifier: GPL-3.0-or-later
#include "mapeditor/model/map_checks.h"

#include <unordered_map>

namespace elads::map {

std::vector<MapIssue> checkMap(const MapModel& m) {
    std::vector<MapIssue> issues;
    auto add = [&](Severity sev, const char* cat, std::string msg, int idx) {
        issues.push_back({sev, cat, std::move(msg), idx});
    };

    const int nv = static_cast<int>(m.vertexCount());
    const int nsd = static_cast<int>(m.sidedefCount());
    const int nsec = static_cast<int>(m.sectorCount());

    // --- Linedef reference integrity ---
    for (int i = 0; i < static_cast<int>(m.linedefCount()); ++i) {
        const Linedef& l = m.linedef(i);
        if (l.v1 < 0 || l.v1 >= nv || l.v2 < 0 || l.v2 >= nv)
            add(Severity::Error, "linedef", "references a nonexistent vertex", i);
        else if (l.v1 == l.v2)
            add(Severity::Error, "linedef", "zero-length (v1 == v2)", i);

        if (l.front == kNoRef)
            add(Severity::Error, "linedef", "has no front sidedef", i);
        else if (l.front < 0 || l.front >= nsd)
            add(Severity::Error, "linedef", "front sidedef out of range", i);

        if (l.back != kNoRef && (l.back < 0 || l.back >= nsd))
            add(Severity::Error, "linedef", "back sidedef out of range", i);
    }

    // --- Sidedef -> sector integrity + usage counts ---
    std::vector<int> sectorRefs(static_cast<size_t>(nsec), 0);
    for (int i = 0; i < nsd; ++i) {
        const Sidedef& sd = m.sidedef(i);
        if (sd.sector < 0 || sd.sector >= nsec)
            add(Severity::Error, "sidedef", "references a nonexistent sector", i);
        else
            ++sectorRefs[static_cast<size_t>(sd.sector)];
    }

    // --- Per-sector: unused + boundary closure (degree balance) ---
    for (int s = 0; s < nsec; ++s) {
        if (sectorRefs[static_cast<size_t>(s)] == 0) {
            add(Severity::Warning, "sector", "unused (no sidedef references it)", s);
            continue;
        }
        // A closed boundary visits each vertex equally as edge start and end.
        std::unordered_map<int, int> netDegree; // out(+1) - in(-1)
        for (int i = 0; i < static_cast<int>(m.linedefCount()); ++i) {
            const Linedef& l = m.linedef(i);
            if (l.v1 == kNoRef || l.v2 == kNoRef)
                continue;
            if (m.frontSector(l) == s) {
                netDegree[l.v1]++;
                netDegree[l.v2]--;
            }
            if (m.backSector(l) == s) {
                netDegree[l.v2]++;
                netDegree[l.v1]--;
            }
        }
        for (const auto& kv : netDegree)
            if (kv.second != 0) {
                add(Severity::Warning, "sector", "boundary is not closed", s);
                break;
            }
    }

    // --- Playability: a Player 1 start (thing type 1) ---
    bool hasStart = false;
    for (int i = 0; i < static_cast<int>(m.thingCount()); ++i)
        if (m.thing(i).type == 1) {
            hasStart = true;
            break;
        }
    if (!hasStart)
        add(Severity::Warning, "things", "no Player 1 start (thing type 1)", -1);

    return issues;
}

int countErrors(const std::vector<MapIssue>& issues) {
    int n = 0;
    for (const auto& i : issues)
        if (i.severity == Severity::Error)
            ++n;
    return n;
}

} // namespace elads::map
