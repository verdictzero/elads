// SPDX-License-Identifier: GPL-3.0-or-later
// Validate a good map (clean) and a broken map (specific errors).
#include <string>

#include "check.h"
#include "mapeditor/model/map_checks.h"

using namespace elads;

static bool hasIssue(const std::vector<map::MapIssue>& issues, const std::string& cat,
                     map::Severity sev) {
    for (const auto& i : issues)
        if (i.category == cat && i.severity == sev)
            return true;
    return false;
}

// Closed square sector with a player start -> should be completely clean.
static map::MapModel goodMap() {
    map::MapModel m;
    m.addVertex({0, 0});
    m.addVertex({64, 0});
    m.addVertex({64, 64});
    m.addVertex({0, 64});
    m.addSector(map::Sector{});
    const int e[4][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}};
    for (int i = 0; i < 4; ++i) {
        map::Sidedef sd;
        sd.sector = 0;
        const int side = m.addSidedef(sd);
        map::Linedef l;
        l.v1 = e[i][0];
        l.v2 = e[i][1];
        l.front = side;
        m.addLinedef(l);
    }
    map::Thing p1;
    p1.pos = {32, 32};
    p1.type = 1;
    m.addThing(p1);
    return m;
}

static void run() {
    // Good map: no issues at all.
    {
        const auto issues = map::checkMap(goodMap());
        CHECK_EQ(map::countErrors(issues), 0);
        CHECK(issues.empty());
    }

    // Broken map: multiple reference errors + a warning.
    {
        map::MapModel m;
        m.addVertex({0, 0});
        m.addVertex({64, 0});
        m.addSector(map::Sector{}); // sector 0 (will be unreferenced -> warning)

        // Linedef 0: no front sidedef -> Error.
        map::Linedef noFront;
        noFront.v1 = 0;
        noFront.v2 = 1;
        noFront.front = map::kNoRef;
        m.addLinedef(noFront);

        // Linedef 1: v2 out of range -> Error.
        map::Linedef badVert;
        badVert.v1 = 0;
        badVert.v2 = 99;
        badVert.front = 0; // also no such sidedef yet -> front out of range Error
        m.addLinedef(badVert);

        // Sidedef 0: sector out of range -> Error.
        map::Sidedef badSide;
        badSide.sector = 42;
        m.addSidedef(badSide);

        const auto issues = map::checkMap(m);
        CHECK(map::countErrors(issues) >= 3);
        CHECK(hasIssue(issues, "linedef", map::Severity::Error));
        CHECK(hasIssue(issues, "sidedef", map::Severity::Error));
        CHECK(hasIssue(issues, "sector", map::Severity::Warning));  // unused sector
        CHECK(hasIssue(issues, "things", map::Severity::Warning));  // no player start
    }
}

TEST_MAIN(run())
