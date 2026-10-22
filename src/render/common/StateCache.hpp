#pragma once

#include <AeroRender/RenderDevice.hpp>

#include <cstdint>

namespace Aero::Render {

/// Logical blend key shared by D3D11 (prebuilt blend-state table) and OpenGL33
/// (glEnable/glBlendFuncSeparate). colorEnable maps to D3D write-mask / GL
/// ColorMask.
struct BlendStateKey {
    uint8_t blendMode = static_cast<uint8_t>(BlendMode::SrcOver);
    uint8_t colorEnable = 1U;

    constexpr bool operator==(BlendStateKey other) const noexcept {
        return blendMode == other.blendMode && colorEnable == other.colorEnable;
    }
    constexpr bool operator!=(BlendStateKey other) const noexcept {
        return !(*this == other);
    }
};

/// Stencil/depth-stencil key. stencilRef is compared so Equal_* modes rebind
/// when the clip depth changes.
struct DepthStencilStateKey {
    uint8_t stencilMode = static_cast<uint8_t>(StencilMode::Disabled);
    uint8_t stencilRef = 0U;

    constexpr bool operator==(DepthStencilStateKey other) const noexcept {
        return stencilMode == other.stencilMode &&
            stencilRef == other.stencilRef;
    }
    constexpr bool operator!=(DepthStencilStateKey other) const noexcept {
        return !(*this == other);
    }
};

/// Logical shader pipeline identity (Shader::Enum). Backends may also track a
/// native program/PS handle via UpdatePipelineHandle when the same enum can
/// bind different native objects (e.g. Custom_Effect reload).
struct ShaderPipelineKey {
    uint8_t shader = 0U;

    constexpr bool operator==(ShaderPipelineKey other) const noexcept {
        return shader == other.shader;
    }
    constexpr bool operator!=(ShaderPipelineKey other) const noexcept {
        return !(*this == other);
    }
};

/// Shared GPU state-change filter used by D3D11 and OpenGL33 DrawBatch paths.
/// Update* returns true when the backend must emit a native state change.
/// Call Reset() when the framebuffer / scissor / color-mask may have been
/// mutated outside DrawBatch (BeginOnscreen/Offscreen, SetRenderTarget).
class StateCache {
public:
    static constexpr uint8_t kTextureSlots = 5U;
    static constexpr uint8_t kSamplerTableSize = 64U;

    void Reset() noexcept {
        blendValid_ = false;
        depthStencilValid_ = false;
        pipelineValid_ = false;
        pipelineHandleValid_ = false;
        formatValid_ = false;
        blend_ = {};
        depthStencil_ = {};
        pipeline_ = {};
        pipelineHandle_ = 0U;
        format_ = 0U;
        stride_ = 0U;
        vbOffset_ = 0U;
        for (uint8_t i = 0U; i < kTextureSlots; ++i) {
            slotValid_[i] = false;
            texHandles_[i] = 0U;
            samplerIndices_[i] = 0U;
        }
    }

    [[nodiscard]] bool UpdateBlend(BlendStateKey key) noexcept {
        if (blendValid_ && blend_ == key) {
            return false;
        }
        blend_ = key;
        blendValid_ = true;
        return true;
    }

    [[nodiscard]] bool UpdateDepthStencil(DepthStencilStateKey key) noexcept {
        if (depthStencilValid_ && depthStencil_ == key) {
            return false;
        }
        depthStencil_ = key;
        depthStencilValid_ = true;
        return true;
    }

    [[nodiscard]] bool UpdatePipeline(ShaderPipelineKey key) noexcept {
        if (pipelineValid_ && pipeline_ == key) {
            return false;
        }
        pipeline_ = key;
        pipelineValid_ = true;
        return true;
    }

    [[nodiscard]] bool UpdatePipelineHandle(std::uintptr_t handle) noexcept {
        if (pipelineHandleValid_ && pipelineHandle_ == handle) {
            return false;
        }
        pipelineHandle_ = handle;
        pipelineHandleValid_ = true;
        return true;
    }

    [[nodiscard]] bool UpdateVertexFormat(uint8_t format, std::uint32_t stride, std::uintptr_t offset) noexcept {
        if (formatValid_ && format_ == format && stride_ == stride && vbOffset_ == offset) return false;
        format_ = format; stride_ = stride; vbOffset_ = offset;
        formatValid_ = true;
        return true;
    }

    [[nodiscard]] bool UpdateTextureAndSampler(
        uint8_t slot, std::uintptr_t texHandle, uint8_t samplerIdx) noexcept {
        if (slot >= kTextureSlots) return true;
        const uint8_t clamped = static_cast<uint8_t>(samplerIdx & 0x3FU);
        if (slotValid_[slot] && texHandles_[slot] == texHandle && samplerIndices_[slot] == clamped) {
            return false;
        }
        texHandles_[slot] = texHandle;
        samplerIndices_[slot] = clamped;
        slotValid_[slot] = true;
        return true;
    }

    // SamplerState.v packing: wrapMode:3 | minmagFilter:1 << 3 | mipFilter:2 << 4
    static constexpr uint8_t PackSamplerIndex(
        uint8_t wrapMode,
        uint8_t minmag,
        uint8_t mip) noexcept {
        return static_cast<uint8_t>(
            (wrapMode & 0x7U) |
            ((minmag & 0x1U) << 3) |
            ((mip & 0x3U) << 4));
    }

    static constexpr uint8_t ClampSamplerIndex(uint8_t v) noexcept {
        return static_cast<uint8_t>(v & 0x3FU);
    }

    static constexpr uint8_t UnpackWrap(uint8_t v) noexcept {
        return static_cast<uint8_t>(v & 0x7U);
    }

    static constexpr uint8_t UnpackMinMag(uint8_t v) noexcept {
        return static_cast<uint8_t>((v >> 3) & 0x1U);
    }

    static constexpr uint8_t UnpackMip(uint8_t v) noexcept {
        return static_cast<uint8_t>((v >> 4) & 0x3U);
    }

private:
    bool blendValid_ = false;
    bool depthStencilValid_ = false;
    bool pipelineValid_ = false;
    bool pipelineHandleValid_ = false;
    bool formatValid_ = false;
    bool slotValid_[kTextureSlots]{};
    BlendStateKey blend_{};
    DepthStencilStateKey depthStencil_{};
    ShaderPipelineKey pipeline_{};
    std::uintptr_t pipelineHandle_ = 0U;
    uint8_t format_ = 0U;
    std::uint32_t stride_ = 0U;
    std::uintptr_t vbOffset_ = 0U;
    std::uintptr_t texHandles_[kTextureSlots]{};
    uint8_t samplerIndices_[kTextureSlots]{};
};

struct RingBufferTracker {
    uint32_t capacity = 0U;
    uint32_t head = 0U;
    uint32_t currentDrawOffset = 0U;

    void Initialize(uint32_t totalBytes) noexcept {
        capacity = totalBytes;
        head = 0U;
        currentDrawOffset = 0U;
    }

    // Returns true when a wrap-around occurs and the backend must orphan/discard the buffer
    [[nodiscard]] bool Allocate(uint32_t bytes, uint32_t alignment, uint32_t& outOffset) noexcept {
        if (capacity == 0U || bytes == 0U) {
            outOffset = 0U;
            return false;
        }
        const uint32_t alignedBytes = (bytes + alignment - 1U) & ~(alignment - 1U);
        const uint32_t alignedHead = (head + alignment - 1U) & ~(alignment - 1U);
        if (alignedHead + alignedBytes > capacity) {
            outOffset = 0U;
            currentDrawOffset = 0U;
            head = alignedBytes <= capacity ? alignedBytes : 0U;
            return true; // Discard / Orphan needed
        }
        const bool isFirstAlloc = (alignedHead == 0U);
        outOffset = alignedHead;
        currentDrawOffset = alignedHead;
        head = alignedHead + alignedBytes;
        return isFirstAlloc;
    }
};

} // namespace Aero::Render
