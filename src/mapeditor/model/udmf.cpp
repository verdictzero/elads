// SPDX-License-Identifier: GPL-3.0-or-later
#include "mapeditor/model/udmf.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace elads::map {
namespace {

std::string lower(std::string s) {
    for (char& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a])))
        ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1])))
        --b;
    return s.substr(a, b - a);
}

// Strip surrounding quotes and unescape \" and \\ .
std::string unquote(const std::string& tok) {
    if (tok.size() < 2 || tok.front() != '"' || tok.back() != '"')
        return tok;
    std::string out;
    for (size_t i = 1; i + 1 < tok.size(); ++i) {
        if (tok[i] == '\\' && i + 2 < tok.size())
            ++i;
        out += tok[i];
    }
    return out;
}

std::string quote(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\')
            out += '\\';
        out += c;
    }
    out += '"';
    return out;
}

long asLong(const std::string& tok) { return std::strtol(tok.c_str(), nullptr, 0); }
double asDouble(const std::string& tok) { return std::strtod(tok.c_str(), nullptr); }

// Compact number formatting: integral doubles print without a fractional part.
std::string fmt(double v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.10g", v);
    return buf;
}
std::string fmt(int v) { return std::to_string(v); }

// --- tiny recursive-descent parser ------------------------------------------
class Parser {
public:
    explicit Parser(const std::string& s) : s_(s) {}

    UdmfMap parse() {
        UdmfMap map;
        skip();
        while (!eof()) {
            const std::string id = ident();
            if (id.empty())
                throw std::runtime_error("UDMF: expected identifier at offset " + std::to_string(i_));
            skip();
            if (peek() == '=') {
                ++i_;
                const std::string tok = valueToken();
                if (lower(id) == "namespace")
                    map.namespaceId = unquote(tok);
                // other global assignments are currently ignored
            } else if (peek() == '{') {
                KeyVals kv = block();
                dispatch(lower(id), kv, map.model);
            } else {
                throw std::runtime_error("UDMF: expected '=' or '{' after '" + id + "'");
            }
            skip();
        }
        return map;
    }

private:
    const std::string& s_;
    size_t i_ = 0;

    bool eof() const { return i_ >= s_.size(); }
    char peek() const { return eof() ? '\0' : s_[i_]; }

    void skip() { // whitespace + // and /* */ comments
        for (;;) {
            while (!eof() && std::isspace(static_cast<unsigned char>(s_[i_])))
                ++i_;
            if (i_ + 1 < s_.size() && s_[i_] == '/' && s_[i_ + 1] == '/') {
                i_ += 2;
                while (!eof() && s_[i_] != '\n')
                    ++i_;
            } else if (i_ + 1 < s_.size() && s_[i_] == '/' && s_[i_ + 1] == '*') {
                i_ += 2;
                while (i_ + 1 < s_.size() && !(s_[i_] == '*' && s_[i_ + 1] == '/'))
                    ++i_;
                i_ += 2;
            } else {
                break;
            }
        }
    }

    std::string ident() {
        skip();
        size_t start = i_;
        while (!eof() && (std::isalnum(static_cast<unsigned char>(s_[i_])) || s_[i_] == '_'))
            ++i_;
        return s_.substr(start, i_ - start);
    }

    void expect(char c) {
        skip();
        if (peek() != c)
            throw std::runtime_error(std::string("UDMF: expected '") + c + "' at offset " +
                                     std::to_string(i_));
        ++i_;
    }

    // Exact source text of a value (quotes included for strings), up to the ';'.
    std::string valueToken() {
        skip();
        size_t start = i_;
        if (peek() == '"') { // consume a quoted string, honoring escapes
            ++i_;
            while (!eof() && s_[i_] != '"') {
                if (s_[i_] == '\\' && i_ + 1 < s_.size())
                    ++i_;
                ++i_;
            }
            if (!eof())
                ++i_;
        }
        while (!eof() && s_[i_] != ';')
            ++i_;
        std::string tok = trim(s_.substr(start, i_ - start));
        expect(';');
        return tok;
    }

    KeyVals block() {
        KeyVals kv;
        expect('{');
        for (;;) {
            skip();
            if (peek() == '}') {
                ++i_;
                break;
            }
            if (eof())
                throw std::runtime_error("UDMF: unterminated block");
            const std::string key = lower(ident());
            expect('=');
            const std::string tok = valueToken();
            kv.push_back({key, tok});
        }
        return kv;
    }

    static void dispatch(const std::string& type, KeyVals& kv, MapModel& m) {
        if (type == "vertex")
            vertex(kv, m);
        else if (type == "linedef")
            linedef(kv, m);
        else if (type == "sidedef")
            sidedef(kv, m);
        else if (type == "sector")
            sector(kv, m);
        else if (type == "thing")
            thing(kv, m);
        // unknown block types are dropped (rare); could be preserved if needed
    }

    // Each mapper pulls known keys into typed fields and keeps the rest in `extra`.
    static void vertex(KeyVals& kv, MapModel& m) {
        Vertex v;
        for (auto& [k, t] : kv) {
            if (k == "x") v.pos.x = asDouble(t);
            else if (k == "y") v.pos.y = asDouble(t);
            else v.extra.push_back({k, t});
        }
        m.addVertex(v.pos);
        m.vertices().back().extra = std::move(v.extra);
    }
    static void linedef(KeyVals& kv, MapModel& m) {
        Linedef l;
        for (auto& [k, t] : kv) {
            if (k == "v1") l.v1 = static_cast<int>(asLong(t));
            else if (k == "v2") l.v2 = static_cast<int>(asLong(t));
            else if (k == "sidefront") l.front = static_cast<int>(asLong(t));
            else if (k == "sideback") l.back = static_cast<int>(asLong(t));
            else if (k == "special") l.special = static_cast<int>(asLong(t));
            else if (k == "id") l.tag = static_cast<int>(asLong(t));
            else if (k.rfind("arg", 0) == 0 && k.size() == 4 && std::isdigit((unsigned char)k[3]))
                l.args[k[3] - '0'] = static_cast<int>(asLong(t));
            else l.extra.push_back({k, t});
        }
        m.addLinedef(std::move(l));
    }
    static void sidedef(KeyVals& kv, MapModel& m) {
        Sidedef s;
        for (auto& [k, t] : kv) {
            if (k == "sector") s.sector = static_cast<int>(asLong(t));
            else if (k == "offsetx") s.offsetX = static_cast<int>(asLong(t));
            else if (k == "offsety") s.offsetY = static_cast<int>(asLong(t));
            else if (k == "texturetop") s.upper = unquote(t);
            else if (k == "texturemiddle") s.middle = unquote(t);
            else if (k == "texturebottom") s.lower = unquote(t);
            else s.extra.push_back({k, t});
        }
        m.addSidedef(std::move(s));
    }
    static void sector(KeyVals& kv, MapModel& m) {
        Sector s;
        for (auto& [k, t] : kv) {
            if (k == "heightfloor") s.floorHeight = static_cast<int>(asLong(t));
            else if (k == "heightceiling") s.ceilHeight = static_cast<int>(asLong(t));
            else if (k == "texturefloor") s.floorTex = unquote(t);
            else if (k == "textureceiling") s.ceilTex = unquote(t);
            else if (k == "lightlevel") s.lightLevel = static_cast<int>(asLong(t));
            else if (k == "special") s.special = static_cast<int>(asLong(t));
            else if (k == "id") s.tag = static_cast<int>(asLong(t));
            else s.extra.push_back({k, t});
        }
        m.addSector(std::move(s));
    }
    static void thing(KeyVals& kv, MapModel& m) {
        Thing th;
        for (auto& [k, t] : kv) {
            if (k == "x") th.pos.x = asDouble(t);
            else if (k == "y") th.pos.y = asDouble(t);
            else if (k == "height") th.z = asDouble(t);
            else if (k == "angle") th.angle = static_cast<int>(asLong(t));
            else if (k == "type") th.type = static_cast<int>(asLong(t));
            else if (k == "id") th.tid = static_cast<int>(asLong(t));
            else if (k == "special") th.special = static_cast<int>(asLong(t));
            else if (k.rfind("arg", 0) == 0 && k.size() == 4 && std::isdigit((unsigned char)k[3]))
                th.args[k[3] - '0'] = static_cast<int>(asLong(t));
            else th.extra.push_back({k, t});
        }
        m.addThing(std::move(th));
    }
};

void emitExtras(std::string& out, const KeyVals& extra) {
    for (const auto& [k, v] : extra)
        out += "\t" + k + " = " + v + ";\n";
}

} // namespace

UdmfMap parseUdmf(const std::string& text) { return Parser(text).parse(); }

std::string writeUdmf(const MapModel& m, const std::string& namespaceId) {
    return writeUdmf(UdmfMap{namespaceId, m});
}

std::string writeUdmf(const UdmfMap& map) {
    const MapModel& m = map.model;
    std::string out;
    out += "namespace = " + quote(map.namespaceId) + ";\n\n";

    for (size_t i = 0; i < m.vertexCount(); ++i) {
        const Vertex& v = m.vertex(static_cast<int>(i));
        out += "vertex // " + std::to_string(i) + "\n{\n";
        out += "\tx = " + fmt(v.pos.x) + ";\n";
        out += "\ty = " + fmt(v.pos.y) + ";\n";
        emitExtras(out, v.extra);
        out += "}\n\n";
    }
    for (size_t i = 0; i < m.linedefCount(); ++i) {
        const Linedef& l = m.linedef(static_cast<int>(i));
        out += "linedef // " + std::to_string(i) + "\n{\n";
        out += "\tv1 = " + fmt(l.v1) + ";\n";
        out += "\tv2 = " + fmt(l.v2) + ";\n";
        out += "\tsidefront = " + fmt(l.front) + ";\n";
        if (l.back != kNoRef)
            out += "\tsideback = " + fmt(l.back) + ";\n";
        if (l.special) out += "\tspecial = " + fmt(l.special) + ";\n";
        if (l.tag) out += "\tid = " + fmt(l.tag) + ";\n";
        for (int a = 0; a < 5; ++a)
            if (l.args[a]) out += "\targ" + std::to_string(a) + " = " + fmt(l.args[a]) + ";\n";
        emitExtras(out, l.extra);
        out += "}\n\n";
    }
    for (size_t i = 0; i < m.sidedefCount(); ++i) {
        const Sidedef& s = m.sidedef(static_cast<int>(i));
        out += "sidedef // " + std::to_string(i) + "\n{\n";
        out += "\tsector = " + fmt(s.sector) + ";\n";
        if (s.offsetX) out += "\toffsetx = " + fmt(s.offsetX) + ";\n";
        if (s.offsetY) out += "\toffsety = " + fmt(s.offsetY) + ";\n";
        if (s.upper != kNoTexture) out += "\ttexturetop = " + quote(s.upper) + ";\n";
        if (s.middle != kNoTexture) out += "\ttexturemiddle = " + quote(s.middle) + ";\n";
        if (s.lower != kNoTexture) out += "\ttexturebottom = " + quote(s.lower) + ";\n";
        emitExtras(out, s.extra);
        out += "}\n\n";
    }
    for (size_t i = 0; i < m.sectorCount(); ++i) {
        const Sector& s = m.sector(static_cast<int>(i));
        out += "sector // " + std::to_string(i) + "\n{\n";
        out += "\theightfloor = " + fmt(s.floorHeight) + ";\n";
        out += "\theightceiling = " + fmt(s.ceilHeight) + ";\n";
        out += "\ttexturefloor = " + quote(s.floorTex) + ";\n";
        out += "\ttextureceiling = " + quote(s.ceilTex) + ";\n";
        out += "\tlightlevel = " + fmt(s.lightLevel) + ";\n";
        if (s.special) out += "\tspecial = " + fmt(s.special) + ";\n";
        if (s.tag) out += "\tid = " + fmt(s.tag) + ";\n";
        emitExtras(out, s.extra);
        out += "}\n\n";
    }
    for (size_t i = 0; i < m.thingCount(); ++i) {
        const Thing& t = m.thing(static_cast<int>(i));
        out += "thing // " + std::to_string(i) + "\n{\n";
        out += "\tx = " + fmt(t.pos.x) + ";\n";
        out += "\ty = " + fmt(t.pos.y) + ";\n";
        if (t.z != 0.0) out += "\theight = " + fmt(t.z) + ";\n";
        out += "\tangle = " + fmt(t.angle) + ";\n";
        out += "\ttype = " + fmt(t.type) + ";\n";
        if (t.tid) out += "\tid = " + fmt(t.tid) + ";\n";
        if (t.special) out += "\tspecial = " + fmt(t.special) + ";\n";
        for (int a = 0; a < 5; ++a)
            if (t.args[a]) out += "\targ" + std::to_string(a) + " = " + fmt(t.args[a]) + ";\n";
        emitExtras(out, t.extra);
        out += "}\n\n";
    }
    return out;
}

} // namespace elads::map
