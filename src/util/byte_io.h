// SPDX-License-Identifier: GPL-3.0-or-later
// elads — little-endian byte reader/writer (header-only, GUI/GL-free).
//
// All Doom on-disk structures are little-endian (see docs/design/08-formats-reference.md
// §13); these helpers keep the archive/format code endianness-safe on aarch64.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace elads::util {

using Bytes = std::vector<uint8_t>;

// Reads little-endian scalars from a byte span with bounds checking.
class ByteReader {
public:
    ByteReader(const uint8_t* data, size_t size) : data_(data), size_(size) {}
    explicit ByteReader(const Bytes& b) : data_(b.data()), size_(b.size()) {}

    size_t pos() const { return pos_; }
    size_t remaining() const { return size_ - pos_; }
    void seek(size_t p) {
        if (p > size_)
            throw std::out_of_range("ByteReader::seek past end");
        pos_ = p;
    }

    uint8_t u8() { return read<uint8_t>(); }
    uint16_t u16() { return read<uint16_t>(); }
    uint32_t u32() { return read<uint32_t>(); }
    int16_t i16() { return static_cast<int16_t>(read<uint16_t>()); }
    int32_t i32() { return static_cast<int32_t>(read<uint32_t>()); }

    // Fixed-length, possibly NUL-padded name (e.g. 8-char lump names).
    std::string fixedString(size_t n) {
        require(n);
        const char* p = reinterpret_cast<const char*>(data_ + pos_);
        size_t len = 0;
        while (len < n && p[len] != '\0')
            ++len;
        std::string s(p, len);
        pos_ += n;
        return s;
    }

    Bytes bytes(size_t n) {
        require(n);
        Bytes out(data_ + pos_, data_ + pos_ + n);
        pos_ += n;
        return out;
    }

private:
    void require(size_t n) const {
        if (pos_ + n > size_)
            throw std::out_of_range("ByteReader: read past end of buffer");
    }
    template <typename T>
    T read() {
        require(sizeof(T));
        T v = 0;
        for (size_t i = 0; i < sizeof(T); ++i)
            v |= static_cast<T>(data_[pos_ + i]) << (8 * i);
        pos_ += sizeof(T);
        return v;
    }

    const uint8_t* data_;
    size_t size_;
    size_t pos_ = 0;
};

// Appends little-endian scalars to a growable byte buffer.
class ByteWriter {
public:
    void u8(uint8_t v) { buf_.push_back(v); }
    void u16(uint16_t v) { write(v); }
    void u32(uint32_t v) { write(v); }
    void i16(int16_t v) { write(static_cast<uint16_t>(v)); }
    void i32(int32_t v) { write(static_cast<uint32_t>(v)); }

    // Writes exactly n bytes: the string, NUL-padded/truncated to n.
    void fixedString(const std::string& s, size_t n) {
        for (size_t i = 0; i < n; ++i)
            buf_.push_back(i < s.size() ? static_cast<uint8_t>(s[i]) : 0);
    }

    void bytes(const Bytes& b) { buf_.insert(buf_.end(), b.begin(), b.end()); }

    size_t size() const { return buf_.size(); }
    const Bytes& data() const { return buf_; }
    Bytes take() { return std::move(buf_); }

    // Overwrite a previously-written u32 (for back-patching offsets in headers).
    void patchU32(size_t at, uint32_t v) {
        for (size_t i = 0; i < 4; ++i)
            buf_[at + i] = static_cast<uint8_t>((v >> (8 * i)) & 0xFF);
    }

private:
    template <typename T>
    void write(T v) {
        for (size_t i = 0; i < sizeof(T); ++i)
            buf_.push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xFF));
    }
    Bytes buf_;
};

} // namespace elads::util
