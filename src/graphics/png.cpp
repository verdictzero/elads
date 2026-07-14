// SPDX-License-Identifier: GPL-3.0-or-later
#include "graphics/png.h"

#include <stdexcept>

extern "C" {
#include "miniz.h"
}

namespace elads::gfx {

util::Bytes encodePng(const Image& img) {
    if (img.width <= 0 || img.height <= 0)
        throw std::runtime_error("encodePng: empty image");

    size_t len = 0;
    void* png = tdefl_write_image_to_png_file_in_memory(img.rgba.data(), img.width, img.height,
                                                        4, &len);
    if (!png)
        throw std::runtime_error("encodePng: miniz PNG writer failed");

    const uint8_t* bytes = static_cast<const uint8_t*>(png);
    util::Bytes out(bytes, bytes + len);
    mz_free(png);
    return out;
}

} // namespace elads::gfx
