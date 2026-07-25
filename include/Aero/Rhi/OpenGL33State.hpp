#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Rhi/OpenGL33.hpp>

#include <cstdint>

namespace Aero::Rhi {

constexpr std::uint32_t MaxCachedGlTextureUnits = 32U;

struct GlRectangleState final {
    GlInt x = 0;
    GlInt y = 0;
    GlSize width = 0;
    GlSize height = 0;
};

struct GlBlendState final {
    bool enabled = false;
    GlEnum colorEquation = 0U;
    GlEnum alphaEquation = 0U;
    GlEnum sourceColor = 0U;
    GlEnum destinationColor = 0U;
    GlEnum sourceAlpha = 0U;
    GlEnum destinationAlpha = 0U;
    bool writeRed = true;
    bool writeGreen = true;
    bool writeBlue = true;
    bool writeAlpha = true;
};

struct GlDepthState final {
    bool enabled = false;
    GlEnum function = 0U;
    bool writeEnabled = true;
};

struct GlStencilFaceState final {
    GlEnum function = 0U;
    GlInt reference = 0;
    GlUInt readMask = 0U;
    GlEnum stencilFail = 0U;
    GlEnum depthFail = 0U;
    GlEnum pass = 0U;
    GlUInt writeMask = 0U;
};

struct GlStencilState final {
    bool enabled = false;
    GlStencilFaceState front;
    GlStencilFaceState back;
};

struct GlPixelUnpackState final {
    GlInt alignment = 4;
    GlInt rowLength = 0;
    GlInt skipRows = 0;
    GlInt skipPixels = 0;
};

struct GlStateCacheStatistics final {
    std::uint64_t emittedCalls = 0U;
    std::uint64_t redundantCalls = 0U;
    std::uint64_t captures = 0U;
    std::uint64_t restores = 0U;
    std::uint64_t invalidations = 0U;
};

class AERO_API GlStateCache final {
public:
    GlStateCache() noexcept = default;

    Base::Result<void> Initialize(
        const GlFunctionTable& functions,
        const GlCapabilities& capabilities) noexcept;

    Base::Result<void> Begin(
        GlContextGeneration generation,
        GlEmbeddingMode mode) noexcept;
    Base::Result<void> End() noexcept;

    void Invalidate(GlContextGeneration generation) noexcept;

    Base::Result<void> UseProgram(GlUInt program) noexcept;
    Base::Result<void> BindVertexArray(GlUInt vertexArray) noexcept;
    Base::Result<void> BindArrayBuffer(GlUInt buffer) noexcept;
    Base::Result<void> BindElementArrayBuffer(GlUInt buffer) noexcept;
    Base::Result<void> BindUniformBuffer(GlUInt buffer) noexcept;
    Base::Result<void> BindDrawFramebuffer(GlUInt framebuffer) noexcept;
    Base::Result<void> SetViewport(
        const GlRectangleState& viewport) noexcept;
    Base::Result<void> SetScissor(
        bool enabled,
        const GlRectangleState& scissor) noexcept;
    Base::Result<void> SetBlendState(
        const GlBlendState& state) noexcept;
    Base::Result<void> SetDepthState(
        const GlDepthState& state) noexcept;
    Base::Result<void> SetStencilState(
        const GlStencilState& state) noexcept;
    Base::Result<void> BindTextureSampler(
        std::uint32_t unit,
        GlUInt texture,
        GlUInt sampler) noexcept;
    Base::Result<void> SetPixelUnpack(
        const GlPixelUnpackState& state) noexcept;

    bool IsInitialized() const noexcept { return initialized_; }
    bool IsActive() const noexcept { return active_; }
    GlContextGeneration Generation() const noexcept { return generation_; }
    const GlStateCacheStatistics& Statistics() const noexcept {
        return statistics_;
    }
    void ResetStatistics() noexcept { statistics_ = {}; }

private:
    struct TextureUnitState final {
        GlUInt texture2D = 0U;
        GlUInt sampler = 0U;
    };

    struct State final {
        GlUInt program = 0U;
        GlUInt vertexArray = 0U;
        GlUInt arrayBuffer = 0U;
        GlUInt elementArrayBuffer = 0U;
        GlUInt uniformBuffer = 0U;
        GlUInt drawFramebuffer = 0U;
        GlRectangleState viewport;
        bool scissorEnabled = false;
        GlRectangleState scissor;
        GlBlendState blend;
        GlDepthState depth;
        GlStencilState stencil;
        std::uint32_t activeTextureUnit = 0U;
        TextureUnitState textureUnits[MaxCachedGlTextureUnits]{};
        GlPixelUnpackState pixelUnpack;
    };

    struct KnownState final {
        bool program = false;
        bool vertexArray = false;
        bool arrayBuffer = false;
        bool elementArrayBuffer = false;
        bool uniformBuffer = false;
        bool drawFramebuffer = false;
        bool viewport = false;
        bool scissor = false;
        bool blend = false;
        bool depth = false;
        bool stencil = false;
        bool activeTextureUnit = false;
        bool textureUnits[MaxCachedGlTextureUnits]{};
        bool pixelUnpack = false;
    };

    GlFunctionTable functions_;
    GlCapabilities capabilities_;
    State current_;
    State snapshot_;
    KnownState known_;
    GlStateCacheStatistics statistics_;
    GlContextGeneration generation_ = 0U;
    GlEmbeddingMode mode_ = GlEmbeddingMode::HostReset;
    bool initialized_ = false;
    bool active_ = false;

    Base::Result<void> VerifyActive() const noexcept;
    Base::Result<void> Capture() noexcept;
    void Restore() noexcept;
    void MarkCall() noexcept { ++statistics_.emittedCalls; }
    void MarkRedundant() noexcept { ++statistics_.redundantCalls; }
};

} // namespace Aero::Rhi
