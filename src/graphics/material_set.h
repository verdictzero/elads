// SPDX-License-Identifier: GPL-3.0-or-later
// elads — a resolved set of named textures/flats (RGBA), for renderers to sample.
//
// Names are Doom texture/flat names, matched case-insensitively (uppercased). Built either
// procedurally (demos) or from a WAD/PK3 (see wad_materials.h). GL-free.
#pragma once

#include <algorithm>
#include <cctype>
#include <map>
#include <string>

#include "graphics/image.h"

namespace elads::gfx {

class MaterialSet {
public:
    void add(const std::string& name, Image img) { images_[key(name)] = std::move(img); }

    // Returns the image for `name`, or nullptr for missing / "no texture" ("-", "").
    const Image* find(const std::string& name) const {
        if (name.empty() || name == "-")
            return nullptr;
        const auto it = images_.find(key(name));
        return it == images_.end() ? nullptr : &it->second;
    }

    size_t size() const { return images_.size(); }

private:
    static std::string key(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        return s;
    }
    std::map<std::string, Image> images_;
};

} // namespace elads::gfx
