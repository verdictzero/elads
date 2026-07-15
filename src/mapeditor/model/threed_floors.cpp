// SPDX-License-Identifier: GPL-3.0-or-later
#include "mapeditor/model/threed_floors.h"

#include <algorithm>
#include <utility>

namespace elads::map {

std::vector<ThreeDFloor> compute3DFloors(const MapModel& m) {
    std::vector<ThreeDFloor> out;
    for (int i = 0; i < static_cast<int>(m.linedefCount()); ++i) {
        const Linedef& l = m.linedef(i);
        if (l.special != kSpecialSet3DFloor)
            continue;
        const int control = m.frontSector(l);
        if (control == kNoRef)
            continue;
        const int tag = l.args[0];
        if (tag == 0)
            continue;

        const Sector& cs = m.sector(control);
        double top = static_cast<double>(cs.ceilHeight);
        double bot = static_cast<double>(cs.floorHeight);
        if (top < bot)
            std::swap(top, bot); // tolerate an inverted control sector

        std::string side = kNoTexture;
        if (l.front != kNoRef)
            side = m.sidedef(l.front).middle;

        for (int s = 0; s < static_cast<int>(m.sectorCount()); ++s) {
            if (s == control)
                continue; // a control sector is not its own target
            if (m.sector(s).tag != tag)
                continue;
            ThreeDFloor f;
            f.targetSector = s;
            f.topZ = top;
            f.botZ = bot;
            f.texTop = cs.ceilTex;
            f.texBot = cs.floorTex;
            f.texSide = side;
            f.type = l.args[1];
            f.alpha = l.args[3];
            out.push_back(std::move(f));
        }
    }
    return out;
}

} // namespace elads::map
