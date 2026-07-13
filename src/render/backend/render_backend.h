// SPDX-License-Identifier: GPL-3.0-or-later
// elads — Render Abstraction Layer: the backend interface (GUI/GL-free header).
//
// Every visual module draws through IRenderDevice / IRenderContext so that no code
// hard-depends on a specific GL version. The primary implementation is a GLES 3.1
// backend (the only conformant HW-accelerated path on the Raspberry Pi 5); desktop-GL
// and Zink backends implement the same interface. See docs/design/02-render-abstraction.md
// and ADR-0002. This header declares the contract only — no GL symbols appear here.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace elads::render {

// --- Opaque, strongly-typed resource handles ------------------------------------
// 0 is the invalid/null handle. Backends map these to their own GL/Vulkan objects.
enum class BufferHandle : uint32_t { Invalid = 0 };
enum class TextureHandle : uint32_t { Invalid = 0 };
enum class ShaderHandle : uint32_t { Invalid = 0 };

// --- Enums ----------------------------------------------------------------------
enum class BufferType { Vertex, Index, Uniform };
enum class BufferUsage { Static, Dynamic, Stream };

// Only primitives expressible on GLES 3.1. Quads/polygons are lowered to triangles by
// callers (see the lowering-rules table in docs/design/02-render-abstraction.md §4).
enum class Topology { Points, Lines, LineStrip, Triangles, TriangleStrip };

enum class TextureFormat { RGBA8, RGB8, R8 };
enum class IndexType { U16, U32 };

// --- Descriptors ----------------------------------------------------------------
struct TextureDesc {
    int width = 0;
    int height = 0;
    TextureFormat format = TextureFormat::RGBA8;
    bool mipmaps = false;
    const void* pixels = nullptr; // optional initial upload (tightly packed)
};

// Shader sources are authored as GLSL ES 3.10 ("#version 310 es"); the desktop-GL
// backend transpiles/patches the version directive as needed.
struct ShaderSources {
    std::string_view vertex;
    std::string_view fragment;
};

struct Viewport {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

struct Color {
    float r = 0.f, g = 0.f, b = 0.f, a = 1.f;
};

// What the active backend/GPU actually supports — modules query this instead of
// assuming GL 3.3+ features.
struct Caps {
    bool isGLES = true;       // GLES vs desktop-GL path
    int glMajor = 3;
    int glMinor = 1;
    bool computeShaders = false;   // GLES 3.1 yes; desktop GL 3.1 no
    bool storageBuffers = false;   // SSBO
    bool explicitAttribLocation = true;
    int maxTextureSize = 0;
};

// --- Device: creates/destroys GPU resources & reports capabilities --------------
class IRenderDevice {
public:
    virtual ~IRenderDevice() = default;

    virtual const char* backendName() const = 0;
    virtual Caps caps() const = 0;

    virtual BufferHandle createBuffer(BufferType type, BufferUsage usage,
                                      const void* data, size_t bytes) = 0;
    virtual void updateBuffer(BufferHandle, const void* data, size_t bytes, size_t offset = 0) = 0;
    virtual void destroyBuffer(BufferHandle) = 0;

    virtual TextureHandle createTexture(const TextureDesc&) = 0;
    virtual void destroyTexture(TextureHandle) = 0;

    virtual ShaderHandle createProgram(const ShaderSources&) = 0;
    virtual void destroyProgram(ShaderHandle) = 0;
};

// --- Context: records draw commands against a target ----------------------------
class IRenderContext {
public:
    virtual ~IRenderContext() = default;

    virtual void beginFrame(const Viewport&) = 0;
    virtual void endFrame() = 0;

    virtual void clear(const Color&) = 0;
    virtual void setViewport(const Viewport&) = 0;

    virtual void bindProgram(ShaderHandle) = 0;
    virtual void bindVertexBuffer(BufferHandle) = 0;
    virtual void bindIndexBuffer(BufferHandle, IndexType) = 0;
    virtual void bindUniformBuffer(unsigned slot, BufferHandle) = 0;
    virtual void bindTexture(unsigned unit, TextureHandle) = 0;

    virtual void draw(Topology, uint32_t firstVertex, uint32_t vertexCount) = 0;
    virtual void drawIndexed(Topology, uint32_t indexCount, uint32_t firstIndex = 0) = 0;
};

} // namespace elads::render
