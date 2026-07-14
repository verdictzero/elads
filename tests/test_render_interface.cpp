// SPDX-License-Identifier: GPL-3.0-or-later
// Prove the Render Abstraction Layer interface is complete and usable with NO GL:
// a headless mock backend implements IRenderDevice/IRenderContext and records calls.
// This is also the shape of the offscreen/CI backend described in
// docs/design/02-render-abstraction.md §6.
#include "check.h"
#include "render/backend/render_backend.h"

using namespace elads;
using namespace elads::render;

namespace {

class MockDevice : public IRenderDevice {
public:
    const char* backendName() const override { return "mock"; }
    Caps caps() const override {
        Caps c;
        c.isGLES = true;
        c.glMajor = 3;
        c.glMinor = 1;
        c.computeShaders = true;
        c.storageBuffers = true;
        c.maxTextureSize = 4096;
        return c;
    }
    BufferHandle createBuffer(BufferType, BufferUsage, const void*, size_t bytes) override {
        totalBufferBytes += bytes;
        return static_cast<BufferHandle>(++nextHandle);
    }
    void updateBuffer(BufferHandle, const void*, size_t, size_t) override { ++updates; }
    void destroyBuffer(BufferHandle) override { ++destroyed; }
    TextureHandle createTexture(const TextureDesc& d) override {
        lastTexW = d.width;
        return static_cast<TextureHandle>(++nextHandle);
    }
    void destroyTexture(TextureHandle) override { ++destroyed; }
    ShaderHandle createProgram(const ShaderSources&) override {
        return static_cast<ShaderHandle>(++nextHandle);
    }
    void destroyProgram(ShaderHandle) override { ++destroyed; }

    uint32_t nextHandle = 0;
    size_t totalBufferBytes = 0;
    int updates = 0, destroyed = 0, lastTexW = 0;
};

class MockContext : public IRenderContext {
public:
    void beginFrame(const Viewport& vp) override { lastVp = vp; ++frames; }
    void endFrame() override {}
    void clear(const Color&) override { ++clears; }
    void setViewport(const Viewport& vp) override { lastVp = vp; }
    void bindProgram(ShaderHandle) override {}
    void bindVertexBuffer(BufferHandle) override {}
    void bindIndexBuffer(BufferHandle, IndexType) override {}
    void bindUniformBuffer(unsigned, BufferHandle) override {}
    void bindTexture(unsigned, TextureHandle) override {}
    void setUniformMat4(const char*, const float[16]) override { ++uniforms; }
    void setUniformVec4(const char*, float, float, float, float) override { ++uniforms; }
    void draw(Topology, uint32_t, uint32_t count) override { drawCalls++; verts += count; }
    void drawIndexed(Topology, uint32_t count, uint32_t) override { drawCalls++; indices += count; }

    Viewport lastVp{};
    int frames = 0, clears = 0, drawCalls = 0, uniforms = 0;
    uint32_t verts = 0, indices = 0;
};

} // namespace

static void run() {
    MockDevice dev;

    // Caps query drives feature decisions instead of assuming GL 3.3+.
    const Caps caps = dev.caps();
    CHECK(caps.isGLES);
    CHECK(caps.glMajor == 3 && caps.glMinor == 1);
    CHECK(caps.computeShaders);

    // Create resources through the abstract device.
    const float verts[] = {0, 0, 1, 0, 0, 1};
    const BufferHandle vbo = dev.createBuffer(BufferType::Vertex, BufferUsage::Static,
                                              verts, sizeof(verts));
    CHECK(vbo != BufferHandle::Invalid);
    CHECK_EQ(dev.totalBufferBytes, sizeof(verts));

    TextureDesc td;
    td.width = 64;
    td.height = 64;
    const TextureHandle tex = dev.createTexture(td);
    CHECK(tex != TextureHandle::Invalid);
    CHECK_EQ(dev.lastTexW, 64);

    const ShaderSources src{"#version 310 es\nvoid main(){}", "#version 310 es\nvoid main(){}"};
    const ShaderHandle prog = dev.createProgram(src);
    CHECK(prog != ShaderHandle::Invalid);

    // Every returned handle is distinct.
    CHECK(vbo != BufferHandle::Invalid && tex != TextureHandle::Invalid);
    CHECK(static_cast<uint32_t>(vbo) != static_cast<uint32_t>(prog));

    // Record a frame through the abstract context.
    MockContext ctx;
    Viewport vp{0, 0, 320, 200};
    ctx.beginFrame(vp);
    ctx.clear(Color{0.1f, 0.1f, 0.1f, 1.f});
    ctx.bindProgram(prog);
    ctx.bindVertexBuffer(vbo);
    ctx.bindTexture(0, tex);
    ctx.draw(Topology::Triangles, 0, 3);
    ctx.draw(Topology::Lines, 0, 8);
    ctx.endFrame();

    CHECK_EQ(ctx.frames, 1);
    CHECK_EQ(ctx.clears, 1);
    CHECK_EQ(ctx.drawCalls, 2);
    CHECK_EQ(ctx.verts, static_cast<uint32_t>(11));
    CHECK(ctx.lastVp.width == 320 && ctx.lastVp.height == 200);

    dev.destroyBuffer(vbo);
    dev.destroyTexture(tex);
    dev.destroyProgram(prog);
    CHECK_EQ(dev.destroyed, 3);
}

TEST_MAIN(run())
