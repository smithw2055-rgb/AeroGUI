#pragma once

#include "render/GraphicsTypes.hpp"

namespace Aero::Render::Detail {

enum class UiShader : std::uint8_t {
    Rectangle = 0U,
    Image,
    Mask,
    Effect,
    Mesh,
    Glyph
};

enum class UiBlendMode : std::uint8_t {
    Normal = 0U,
    Multiply,
    Screen,
    Additive,
    Mask,
    Opaque
};

struct UiPipelineKey {
    UiShader shader = UiShader::Rectangle;
    UiBlendMode blend = UiBlendMode::Normal;
};

// UI-specific operations consumed by native RenderDevice implementations.
// Binding state is folded into draw records; there is no public or private
// generic GPU command-list lifecycle between Renderer and RenderDevice.
enum class RenderStepKind : std::uint8_t {
    UploadBuffer = 0U,
    UploadTexture,
    BeginPass,
    EndPass,
    Draw
};

struct RenderDrawState {
    Graphics::ResourceHandle pipeline;
    Graphics::ResourceHandle vertexBuffers[Graphics::MaxVertexBuffers]{};
    std::uint64_t vertexOffsets[Graphics::MaxVertexBuffers]{};
    Graphics::ResourceHandle indexBuffer;
    std::uint64_t indexOffset = 0U;
    Graphics::IndexType indexType = Graphics::IndexType::UInt16;
    Graphics::ResourceHandle uniformBuffers[4]{};
    std::uint64_t uniformOffsets[4]{};
    std::uint32_t uniformSizes[4]{};
    Graphics::ResourceHandle textures[8]{};
    Graphics::ResourceHandle samplers[8]{};
    Base::Rect scissor{};
};

struct RenderStep {
    RenderStepKind kind = RenderStepKind::Draw;
    Graphics::ResourceHandle resource;
    Graphics::RenderPassDescriptor pass;
    Graphics::TextureRegion textureRegion;
    RenderDrawState drawState;
    std::uint64_t resourceOffset = 0U;
    std::uint32_t resourceSize = 0U;
    std::uint32_t uploadOffset = 0U;
    std::uint32_t uploadSize = 0U;
    std::uint32_t first = 0U;
    std::uint32_t count = 0U;
    std::uint32_t instanceCount = 0U;
    std::uint32_t firstInstance = 0U;
    std::int32_t baseVertex = 0;
    bool indexed = false;
};

class RenderBatch final {
public:
    explicit RenderBatch(Base::IAllocator* allocator = nullptr) noexcept
        : steps_(allocator), uploadBytes_(allocator) {}

    RenderBatch(RenderBatch&&) noexcept = default;
    RenderBatch& operator=(RenderBatch&&) noexcept = default;
    RenderBatch(const RenderBatch&) = delete;
    RenderBatch& operator=(const RenderBatch&) = delete;

    Base::Span<const RenderStep> Steps() const noexcept {
        return {steps_.Data(), steps_.Size()};
    }
    Base::Span<const std::uint8_t> UploadBytes() const noexcept {
        return {uploadBytes_.Data(), uploadBytes_.Size()};
    }
    std::uint32_t StepCount() const noexcept { return steps_.Size(); }
    std::uint32_t UploadByteCount() const noexcept {
        return uploadBytes_.Size();
    }
    bool Empty() const noexcept { return steps_.Empty(); }
    std::uint64_t StableHash() const noexcept;

private:
    friend class RenderBatchBuilder;
    Base::Vector<RenderStep> steps_;
    Base::Vector<std::uint8_t> uploadBytes_;
};

class RenderBatchBuilder final {
public:
    explicit RenderBatchBuilder(Base::IAllocator* allocator = nullptr) noexcept
        : batch_(allocator) {}

    Base::Result<void> UploadBuffer(
        Graphics::ResourceHandle buffer,
        std::uint64_t destinationOffset,
        Base::Span<const std::uint8_t> data) noexcept;
    Base::Result<void> UploadTexture(
        Graphics::ResourceHandle texture,
        Graphics::TextureRegion region,
        Base::Span<const std::uint8_t> data) noexcept;
    Base::Result<void> BeginRenderPass(
        const Graphics::RenderPassDescriptor& descriptor) noexcept;
    Base::Result<void> EndRenderPass() noexcept;
    Base::Result<void> BindPipeline(Graphics::ResourceHandle pipeline) noexcept;
    Base::Result<void> BindVertexBuffer(
        std::uint32_t slot,
        Graphics::ResourceHandle buffer,
        std::uint64_t offset = 0U) noexcept;
    Base::Result<void> BindIndexBuffer(
        Graphics::ResourceHandle buffer,
        Graphics::IndexType type,
        std::uint64_t offset = 0U) noexcept;
    Base::Result<void> BindUniformBuffer(
        std::uint32_t slot,
        Graphics::ResourceHandle buffer,
        std::uint64_t offset,
        std::uint32_t size) noexcept;
    Base::Result<void> BindTextureSampler(
        std::uint32_t slot,
        Graphics::ResourceHandle texture,
        Graphics::ResourceHandle sampler) noexcept;
    Base::Result<void> SetScissor(Base::Rect rect) noexcept;
    Base::Result<void> Draw(
        std::uint32_t vertexCount,
        std::uint32_t instanceCount = 1U,
        std::uint32_t firstVertex = 0U,
        std::uint32_t firstInstance = 0U) noexcept;
    Base::Result<void> DrawIndexed(
        std::uint32_t indexCount,
        std::uint32_t instanceCount = 1U,
        std::uint32_t firstIndex = 0U,
        std::int32_t baseVertex = 0,
        std::uint32_t firstInstance = 0U) noexcept;
    Base::Result<RenderBatch> Finish() noexcept;

private:
    RenderBatch batch_;
    RenderDrawState state_{};
    bool inRenderPass_ = false;
    bool finished_ = false;

    Base::Result<void> VerifyRecording() const noexcept;
    Base::Result<void> Append(const RenderStep& step) noexcept;
    Base::Result<void> AppendUpload(
        RenderStep& step,
        Base::Span<const std::uint8_t> data) noexcept;
};

} // namespace Aero::Render::Detail
