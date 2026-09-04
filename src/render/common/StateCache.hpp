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

    /// Index into the D3D11 blendStates_[colorEnable * Count + blendMode] table.
    static constexpr uint8_t TableIndex(BlendStateKey key) noexcept {
        return static_cast<uint8_t>(
            key.colorEnable * BlendMode::Count + key.blendMode);
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

struct SamplerBindKey {
    uint8_t index = 0U; // SamplerState.v & 0x3F
    bool bound = false;

    constexpr bool operator==(SamplerBindKey other) const noexcept {
        return bound == other.bound && (!bound || index == other.index);
    }
    constexpr bool operator!=(SamplerBindKey other) const noexcept {
        return !(*this == other);
    }
};

/// Shared GPU state-change filter used by D3D11 and OpenGL33 DrawBatch paths.
/// Update* returns true when the backend must emit a native state change.
/// Call Reset() when the framebuffer / scissor / color-mask may have been
/// mutated outside DrawBatch (BeginOnscreen/Offscreen, SetRenderTarget).
class StateCache {
public:
    static constexpr uint8_t kSamplerSlots = 2U;
    static constexpr uint8_t kSamplerTableSize = 64U;

    void Reset() noexcept {
        valid_ = false;
        blend_ = {};
        depthStencil_ = {};
        pipeline_ = {};
        pipelineHandle_ = 0U;
        for (uint8_t i = 0U; i < kSamplerSlots; ++i) {
            samplers_[i] = {};
        }
    }

    [[nodiscard]] bool UpdateBlend(BlendStateKey key) noexcept {
        if (valid_ && blend_ == key) {
            return false;
        }
        blend_ = key;
        valid_ = true;
        return true;
    }

    [[nodiscard]] bool UpdateDepthStencil(DepthStencilStateKey key) noexcept {
        if (valid_ && depthStencil_ == key) {
            return false;
        }
        depthStencil_ = key;
        valid_ = true;
        return true;
    }

    [[nodiscard]] bool UpdatePipeline(ShaderPipelineKey key) noexcept {
        if (valid_ && pipeline_ == key) {
            return false;
        }
        pipeline_ = key;
        valid_ = true;
        return true;
    }

    [[nodiscard]] bool UpdatePipelineHandle(std::uintptr_t handle) noexcept {
        if (valid_ && pipelineHandle_ == handle) {
            return false;
        }
        pipelineHandle_ = handle;
        valid_ = true;
        return true;
    }

    [[nodiscard]] bool UpdateSampler(uint8_t slot, SamplerBindKey key) noexcept {
        if (slot >= kSamplerSlots) {
            return true;
        }
        if (valid_ && samplers_[slot] == key) {
            return false;
        }
        samplers_[slot] = key;
        valid_ = true;
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
    bool valid_ = false;
    BlendStateKey blend_{};
    DepthStencilStateKey depthStencil_{};
    ShaderPipelineKey pipeline_{};
    std::uintptr_t pipelineHandle_ = 0U;
    SamplerBindKey samplers_[kSamplerSlots]{};
};

} // namespace Aero::Render
