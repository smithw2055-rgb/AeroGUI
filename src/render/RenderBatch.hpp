#pragma once

#include <Aero/Render/RenderDevice.hpp>
#include "render/GraphicsTypes.hpp"

namespace Aero::Render {

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

struct RenderDrawState {
    UiPipelineKey pipeline;
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
    bool pipelineBound = false;
};

// One immutable UI draw submitted to RenderDevice. Texture/buffer updates and
// pass sequencing are separate device operations, so this value can no longer
// become a disguised frame command list.
class RenderBatch final {
public:
    Graphics::RenderPassDescriptor pass;
    RenderDrawState drawState;
    std::uint32_t first = 0U;
    std::uint32_t count = 0U;
    std::uint32_t instanceCount = 1U;
    std::uint32_t firstInstance = 0U;
    std::int32_t baseVertex = 0;
    bool indexed = false;

    bool Empty() const noexcept { return count == 0U; }
    std::uint64_t StableHash() const noexcept;
};

// Renderer-local immediate draw context. It retains only current binding state;
// every Draw call submits exactly one RenderBatch to the device.
class UiDrawContext final {
public:
    explicit UiDrawContext(
        Aero::RenderDevice::Access& device,
        Base::IAllocator* allocator = nullptr) noexcept
        : device_(&device) {
        static_cast<void>(allocator);
    }

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
    Base::Result<void> BindPipeline(UiPipelineKey pipeline) noexcept;
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
    Base::Result<Graphics::FenceValue> Finish() noexcept;

private:
    Aero::RenderDevice::Access* device_ = nullptr;
    Graphics::RenderPassDescriptor pass_;
    RenderDrawState state_{};
    Graphics::FenceValue lastFence_ = 0U;
    bool inRenderPass_ = false;
    bool finished_ = false;
    bool firstDrawInPass_ = true;

    Base::Result<void> VerifyRecording() const noexcept;
    Base::Result<void> SubmitDraw(
        std::uint32_t count,
        std::uint32_t instanceCount,
        std::uint32_t first,
        std::uint32_t firstInstance,
        std::int32_t baseVertex,
        bool indexed) noexcept;
};

} // namespace Aero::Render
