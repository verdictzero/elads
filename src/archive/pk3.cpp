// SPDX-License-Identifier: GPL-3.0-or-later
#include "archive/pk3.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <stdexcept>

extern "C" {
#include "miniz.h"
}

namespace elads::archive {
namespace {

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

} // namespace

std::string pk3Namespace(const std::string& path) {
    const size_t slash = path.find('/');
    if (slash == std::string::npos)
        return "global"; // root-level lump
    const std::string top = lower(path.substr(0, slash));
    static const char* kNs[] = {"textures", "flats",  "patches", "sprites", "graphics", "sounds",
                                "music",    "colormaps", "acs",   "voxels",  "hires",    "maps"};
    for (const char* n : kNs)
        if (top == n)
            return top;
    return "global";
}

Pk3 Pk3::read(const util::Bytes& zipBytes) {
    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof(zip));
    if (!mz_zip_reader_init_mem(&zip, zipBytes.data(), zipBytes.size(), 0))
        throw std::runtime_error("Pk3: not a valid zip archive");

    Pk3 pk3;
    const mz_uint count = mz_zip_reader_get_num_files(&zip);
    for (mz_uint i = 0; i < count; ++i) {
        if (mz_zip_reader_is_file_a_directory(&zip, i))
            continue;
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&zip, i, &st))
            continue;

        size_t outSize = 0;
        void* p = mz_zip_reader_extract_to_heap(&zip, i, &outSize, 0);
        if (!p) {
            mz_zip_reader_end(&zip);
            throw std::runtime_error(std::string("Pk3: failed to extract '") + st.m_filename + "'");
        }
        Pk3Entry entry;
        entry.path = st.m_filename;
        const uint8_t* bytes = static_cast<const uint8_t*>(p);
        entry.data.assign(bytes, bytes + outSize);
        mz_free(p);
        pk3.entries_.push_back(std::move(entry));
    }
    mz_zip_reader_end(&zip);
    return pk3;
}

util::Bytes Pk3::write(bool compress) const {
    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_heap(&zip, 0, 0))
        throw std::runtime_error("Pk3: failed to init zip writer");

    const mz_uint level = compress ? MZ_BEST_COMPRESSION : MZ_NO_COMPRESSION;
    for (const auto& e : entries_) {
        if (!mz_zip_writer_add_mem(&zip, e.path.c_str(),
                                   e.data.empty() ? "" : static_cast<const void*>(e.data.data()),
                                   e.data.size(), level)) {
            mz_zip_writer_end(&zip);
            throw std::runtime_error("Pk3: failed to add '" + e.path + "'");
        }
    }

    void* buf = nullptr;
    size_t size = 0;
    if (!mz_zip_writer_finalize_heap_archive(&zip, &buf, &size)) {
        mz_zip_writer_end(&zip);
        throw std::runtime_error("Pk3: failed to finalize archive");
    }
    const uint8_t* bytes = static_cast<const uint8_t*>(buf);
    util::Bytes out(bytes, bytes + size);
    mz_free(buf);
    mz_zip_writer_end(&zip);
    return out;
}

Pk3Entry& Pk3::add(std::string path, util::Bytes data) {
    entries_.push_back(Pk3Entry{std::move(path), std::move(data)});
    return entries_.back();
}

const Pk3Entry* Pk3::find(const std::string& path) const {
    for (const auto& e : entries_)
        if (e.path == path)
            return &e;
    return nullptr;
}

} // namespace elads::archive
