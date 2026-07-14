// SPDX-License-Identifier: GPL-3.0-or-later
// elads — desktop OpenGL 3.3+ core implementation of the render abstraction.
//
// Resource handles ARE the GL object names. The current vertex-buffer layout is the standard
// "map vertex" (vec2 position @0, vec4 color @8, stride 24) used by the 2D map renderer; a
// richer layout system arrives with textured geometry. The GLES backend for the Pi mirrors
// this class against the same interface (ADR-0002).
#pragma once

#include "render/backend/render_backend.h"

namespace elads::render {

class GLDevice : public IRenderDevice {
public:
    const char* backendName() const override { return "opengl"; }
    Caps caps() const override;

    BufferHandle createBuffer(BufferType, BufferUsage, const void* data, size_t bytes) override;
    void updateBuffer(BufferHandle, const void* data, size_t bytes, size_t offset) override;
    void destroyBuffer(BufferHandle) override;

    TextureHandle createTexture(const TextureDesc&) override;
    void destroyTexture(TextureHandle) override;

    ShaderHandle createProgram(const ShaderSources&) override; // throws on compile/link error
    void destroyProgram(ShaderHandle) override;
};

class GLContext : public IRenderContext {
public:
    GLContext();
    ~GLContext() override;

    void beginFrame(const Viewport&) override;
    void endFrame() override;
    void clear(const Color&) override;
    void setViewport(const Viewport&) override;
    void setDepthTest(bool enabled) override;

    void bindProgram(ShaderHandle) override;
    void bindVertexBuffer(BufferHandle, const VertexLayout&) override;
    void bindIndexBuffer(BufferHandle, IndexType) override;
    void bindUniformBuffer(unsigned slot, BufferHandle) override;
    void bindTexture(unsigned unit, TextureHandle) override;
    void setUniformMat4(const char* name, const float m[16]) override;
    void setUniformVec4(const char* name, float x, float y, float z, float w) override;

    void draw(Topology, uint32_t firstVertex, uint32_t vertexCount) override;
    void drawIndexed(Topology, uint32_t indexCount, uint32_t firstIndex) override;

private:
    unsigned vao_ = 0;
    unsigned currentProgram_ = 0;
    unsigned indexType_ = 0; // GL_UNSIGNED_SHORT / GL_UNSIGNED_INT
};

} // namespace elads::render
