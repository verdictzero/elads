// SPDX-License-Identifier: GPL-3.0-or-later
#include "render/gl/gl_backend.h"

#include <epoxy/gl.h>

#include <stdexcept>
#include <string>
#include <vector>

namespace elads::render {
namespace {

GLenum bufferTarget(BufferType t) {
    switch (t) {
        case BufferType::Vertex:  return GL_ARRAY_BUFFER;
        case BufferType::Index:   return GL_ELEMENT_ARRAY_BUFFER;
        case BufferType::Uniform: return GL_UNIFORM_BUFFER;
    }
    return GL_ARRAY_BUFFER;
}
GLenum bufferUsage(BufferUsage u) {
    switch (u) {
        case BufferUsage::Static:  return GL_STATIC_DRAW;
        case BufferUsage::Dynamic: return GL_DYNAMIC_DRAW;
        case BufferUsage::Stream:  return GL_STREAM_DRAW;
    }
    return GL_STATIC_DRAW;
}
GLenum primitive(Topology t) {
    switch (t) {
        case Topology::Points:        return GL_POINTS;
        case Topology::Lines:         return GL_LINES;
        case Topology::LineStrip:     return GL_LINE_STRIP;
        case Topology::Triangles:     return GL_TRIANGLES;
        case Topology::TriangleStrip: return GL_TRIANGLE_STRIP;
    }
    return GL_TRIANGLES;
}

GLuint compile(GLenum stage, std::string_view src) {
    const GLuint sh = glCreateShader(stage);
    const char* p = src.data();
    const GLint len = static_cast<GLint>(src.size());
    glShaderSource(sh, 1, &p, &len);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint n = 0;
        glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &n);
        std::string log(static_cast<size_t>(n > 0 ? n : 1), '\0');
        glGetShaderInfoLog(sh, n, nullptr, log.data());
        glDeleteShader(sh);
        throw std::runtime_error("shader compile failed: " + log);
    }
    return sh;
}

} // namespace

// ---------------- GLDevice ----------------

Caps GLDevice::caps() const {
    Caps c;
    c.isGLES = false;
    GLint maj = 3, min = 3, maxTex = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &maj);
    glGetIntegerv(GL_MINOR_VERSION, &min);
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTex);
    c.glMajor = maj;
    c.glMinor = min;
    c.maxTextureSize = maxTex;
    c.explicitAttribLocation = true;
    c.computeShaders = (maj > 4) || (maj == 4 && min >= 3);
    c.storageBuffers = c.computeShaders;
    return c;
}

BufferHandle GLDevice::createBuffer(BufferType type, BufferUsage usage, const void* data, size_t bytes) {
    GLuint id = 0;
    glGenBuffers(1, &id);
    const GLenum tgt = bufferTarget(type);
    glBindBuffer(tgt, id);
    glBufferData(tgt, static_cast<GLsizeiptr>(bytes), data, bufferUsage(usage));
    return static_cast<BufferHandle>(id);
}

void GLDevice::updateBuffer(BufferHandle h, const void* data, size_t bytes, size_t offset) {
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(h));
    glBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(bytes), data);
}

void GLDevice::destroyBuffer(BufferHandle h) {
    GLuint id = static_cast<GLuint>(h);
    glDeleteBuffers(1, &id);
}

TextureHandle GLDevice::createTexture(const TextureDesc& d) {
    GLenum fmt = GL_RGBA, internal = GL_RGBA8;
    if (d.format == TextureFormat::RGB8) {
        fmt = GL_RGB;
        internal = GL_RGB8;
    } else if (d.format == TextureFormat::R8) {
        fmt = GL_RED;
        internal = GL_R8;
    }
    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(internal), d.width, d.height, 0, fmt,
                 GL_UNSIGNED_BYTE, d.pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, d.mipmaps ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    if (d.mipmaps)
        glGenerateMipmap(GL_TEXTURE_2D);
    return static_cast<TextureHandle>(id);
}

void GLDevice::destroyTexture(TextureHandle h) {
    GLuint id = static_cast<GLuint>(h);
    glDeleteTextures(1, &id);
}

ShaderHandle GLDevice::createProgram(const ShaderSources& s) {
    const GLuint vs = compile(GL_VERTEX_SHADER, s.vertex);
    const GLuint fs = compile(GL_FRAGMENT_SHADER, s.fragment);
    const GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint n = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &n);
        std::string log(static_cast<size_t>(n > 0 ? n : 1), '\0');
        glGetProgramInfoLog(prog, n, nullptr, log.data());
        glDeleteProgram(prog);
        throw std::runtime_error("program link failed: " + log);
    }
    return static_cast<ShaderHandle>(prog);
}

void GLDevice::destroyProgram(ShaderHandle h) { glDeleteProgram(static_cast<GLuint>(h)); }

// ---------------- GLContext ----------------

GLContext::GLContext() {
    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

GLContext::~GLContext() {
    if (vao_)
        glDeleteVertexArrays(1, &vao_);
}

void GLContext::beginFrame(const Viewport& vp) { setViewport(vp); }
void GLContext::endFrame() { glFlush(); }

void GLContext::clear(const Color& c) {
    glClearColor(c.r, c.g, c.b, c.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void GLContext::setViewport(const Viewport& vp) { glViewport(vp.x, vp.y, vp.width, vp.height); }

void GLContext::bindProgram(ShaderHandle h) {
    currentProgram_ = static_cast<unsigned>(h);
    glUseProgram(currentProgram_);
}

void GLContext::bindVertexBuffer(BufferHandle h) {
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(h));
    // Standard "map vertex": vec2 position @0, vec4 color @8, stride 24.
    const GLsizei stride = 6 * sizeof(float);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(2 * sizeof(float)));
}

void GLContext::bindIndexBuffer(BufferHandle h, IndexType t) {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLuint>(h));
    indexType_ = (t == IndexType::U16) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
}

void GLContext::bindUniformBuffer(unsigned slot, BufferHandle h) {
    glBindBufferBase(GL_UNIFORM_BUFFER, slot, static_cast<GLuint>(h));
}

void GLContext::bindTexture(unsigned unit, TextureHandle h) {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(h));
}

void GLContext::setUniformMat4(const char* name, const float m[16]) {
    const GLint loc = glGetUniformLocation(currentProgram_, name);
    if (loc >= 0)
        glUniformMatrix4fv(loc, 1, GL_FALSE, m);
}

void GLContext::setUniformVec4(const char* name, float x, float y, float z, float w) {
    const GLint loc = glGetUniformLocation(currentProgram_, name);
    if (loc >= 0)
        glUniform4f(loc, x, y, z, w);
}

void GLContext::draw(Topology topo, uint32_t first, uint32_t count) {
    glDrawArrays(primitive(topo), static_cast<GLint>(first), static_cast<GLsizei>(count));
}

void GLContext::drawIndexed(Topology topo, uint32_t count, uint32_t first) {
    const size_t elemSize = (indexType_ == GL_UNSIGNED_SHORT) ? 2 : 4;
    glDrawElements(primitive(topo), static_cast<GLsizei>(count), indexType_,
                   reinterpret_cast<void*>(first * elemSize));
}

} // namespace elads::render
