#pragma once

#include "FrameEncoder.hpp"
#include "render/RenderResources.hpp"

namespace Aero::Render {

// Backend-neutral device renderer shared by every native surface adapter.
// It is the single owner of frame encoding, image/mesh/glyph registrations
// and their device-generation-scoped resource tables.
class DeviceRenderer {
public:
    DeviceRenderer(
        Graphics::Device& device,
        const FrameShaderSet& shaders,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~DeviceRenderer() noexcept;

    DeviceRenderer(const DeviceRenderer&) = delete;
    DeviceRenderer& operator=(const DeviceRenderer&) = delete;

    Base::Result<void> Initialize() noexcept;
    void Shutdown() noexcept;
    bool IsInitialized() const noexcept;

    Base::Result<void> RegisterImage(
        RenderImageId image,
        Graphics::ResourceHandle texture,
        Graphics::ResourceHandle sampler) noexcept;
    Base::Result<void> UnregisterImage(RenderImageId image) noexcept;
    Base::Result<void> RegisterMesh(
        RenderMeshId mesh,
        Graphics::ResourceHandle vertexBuffer,
        Graphics::ResourceHandle indexBuffer,
        std::uint32_t indexCount,
        Graphics::IndexType indexType = Graphics::IndexType::UInt16) noexcept;
    Base::Result<void> UnregisterMesh(RenderMeshId mesh) noexcept;
    Base::Result<void> RegisterGlyphRun(
        RenderGlyphRunId glyphRun,
        Graphics::ResourceHandle vertexBuffer,
        Graphics::ResourceHandle indexBuffer,
        std::uint32_t indexCount,
        Graphics::ResourceHandle atlasTexture,
        Graphics::ResourceHandle sampler,
        Graphics::IndexType indexType) noexcept;
    Base::Result<void> UnregisterGlyphRun(RenderGlyphRunId glyphRun) noexcept;

    Base::Result<Graphics::CommandList> Record(
        const ::Aero::Render::Detail::RenderFrame& frame,
        const RenderTarget& target) noexcept;
    Base::Result<Graphics::CommandList> RecordOffscreen(
        const void* rendererToken,
        const ::Aero::Render::Detail::RenderFrame& frame) noexcept;
    Base::Result<Graphics::CommandList> RecordOnscreen(
        const void* rendererToken,
        const ::Aero::Render::Detail::RenderFrame& frame,
        const RenderTarget& target) noexcept;
    void ReleaseRenderer(const void* rendererToken) noexcept;

    FrameEncoderStatistics LastStatistics() const noexcept;
    void SetBatchingEnabled(bool enabled) noexcept;
    bool IsBatchingEnabled() const noexcept;

    Detail::TextResources* GetTextResources() noexcept;
    Detail::MeshResources* GetMeshResources() noexcept;
    Detail::ImageResources* GetImageResources() noexcept;

private:
    struct Impl;

    Graphics::Device* device_ = nullptr;
    FrameShaderSet shaders_;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
    std::uint64_t resourceGeneration_ = 0U;
    bool batchingEnabled_ = true;
};

} // namespace Aero::Render
