#include "DeviceRenderer.hpp"

#include "ImageGpuResources.hpp"
#include "MeshGpuResources.hpp"
#include "TextGpuResources.hpp"

#include <new>

namespace Aero::Render {
namespace {

Base::Status NotInitialized(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::NotInitialized, message);
}

Base::Status OutOfMemory(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::OutOfMemory, message);
}

} // namespace

struct DeviceRenderer::Impl {
    Impl(
        Graphics::Device& device,
        const FrameShaderSet& shaders,
        std::uint64_t generation,
        Base::IAllocator& allocator) noexcept
        : encoder(device, shaders, &allocator),
          textResources(device, encoder, generation, allocator),
          meshResources(device, encoder, generation, allocator),
          imageResources(device, encoder, generation, allocator) {}

    FrameEncoder encoder;
    Detail::TextGpuResources textResources;
    Detail::MeshGpuResources meshResources;
    Detail::ImageGpuResources imageResources;
    bool initialized = false;
};

DeviceRenderer::DeviceRenderer(
    Graphics::Device& device,
    const FrameShaderSet& shaders,
    Base::IAllocator* allocator) noexcept
    : device_(&device),
      shaders_(shaders),
      allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {}

DeviceRenderer::~DeviceRenderer() noexcept {
    Shutdown();
}

Base::Result<void> DeviceRenderer::Initialize() noexcept {
    if (IsInitialized()) return {};
    if (device_ == nullptr || device_->Backend().IsDeviceLost()) {
        return NotInitialized(
            "Device renderer requires a ready graphics device");
    }
    if (impl_ == nullptr) {
        void* memory = allocator_->Allocate({
            sizeof(Impl), alignof(Impl), Base::MemoryTag::Render});
        if (memory == nullptr) {
            return OutOfMemory(
                "Failed to allocate device renderer state");
        }
        ++resourceGeneration_;
        if (resourceGeneration_ == 0U) ++resourceGeneration_;
        impl_ = new (memory) Impl(
            *device_, shaders_, resourceGeneration_, *allocator_);
    }
    Base::Result<void> initialized = impl_->encoder.Initialize();
    if (!initialized) {
        Shutdown();
        return initialized;
    }
    impl_->encoder.SetBatchingEnabled(batchingEnabled_);
    impl_->initialized = true;
    return {};
}

void DeviceRenderer::Shutdown() noexcept {
    if (impl_ == nullptr) return;
    impl_->imageResources.Shutdown();
    impl_->meshResources.Shutdown();
    impl_->textResources.Shutdown();
    impl_->encoder.Shutdown();
    impl_->~Impl();
    allocator_->Deallocate(
        impl_, sizeof(Impl), alignof(Impl), Base::MemoryTag::Render);
    impl_ = nullptr;
}

bool DeviceRenderer::IsInitialized() const noexcept {
    return impl_ != nullptr && impl_->initialized &&
        impl_->encoder.IsInitialized();
}

Base::Result<void> DeviceRenderer::RegisterImage(
    RenderImageId image,
    Graphics::ResourceHandle texture,
    Graphics::ResourceHandle sampler) noexcept {
    return IsInitialized()
        ? impl_->encoder.RegisterImage(image, texture, sampler)
        : Base::Result<void>(NotInitialized(
              "Device renderer is not initialized"));
}

Base::Result<void> DeviceRenderer::UnregisterImage(
    RenderImageId image) noexcept {
    return IsInitialized()
        ? impl_->encoder.UnregisterImage(image)
        : Base::Result<void>(NotInitialized(
              "Device renderer is not initialized"));
}

Base::Result<void> DeviceRenderer::RegisterMesh(
    RenderMeshId mesh,
    Graphics::ResourceHandle vertexBuffer,
    Graphics::ResourceHandle indexBuffer,
    std::uint32_t indexCount,
    Graphics::IndexType indexType) noexcept {
    return IsInitialized()
        ? impl_->encoder.RegisterMesh(
              mesh, vertexBuffer, indexBuffer, indexCount, indexType)
        : Base::Result<void>(NotInitialized(
              "Device renderer is not initialized"));
}

Base::Result<void> DeviceRenderer::UnregisterMesh(
    RenderMeshId mesh) noexcept {
    return IsInitialized()
        ? impl_->encoder.UnregisterMesh(mesh)
        : Base::Result<void>(NotInitialized(
              "Device renderer is not initialized"));
}

Base::Result<void> DeviceRenderer::RegisterGlyphRun(
    RenderGlyphRunId glyphRun,
    Graphics::ResourceHandle vertexBuffer,
    Graphics::ResourceHandle indexBuffer,
    std::uint32_t indexCount,
    Graphics::ResourceHandle atlasTexture,
    Graphics::ResourceHandle sampler,
    Graphics::IndexType indexType) noexcept {
    return IsInitialized()
        ? impl_->encoder.RegisterGlyphRun(
              glyphRun,
              vertexBuffer,
              indexBuffer,
              indexCount,
              atlasTexture,
              sampler,
              indexType)
        : Base::Result<void>(NotInitialized(
              "Device renderer is not initialized"));
}

Base::Result<void> DeviceRenderer::UnregisterGlyphRun(
    RenderGlyphRunId glyphRun) noexcept {
    return IsInitialized()
        ? impl_->encoder.UnregisterGlyphRun(glyphRun)
        : Base::Result<void>(NotInitialized(
              "Device renderer is not initialized"));
}

Base::Result<Graphics::CommandList> DeviceRenderer::Record(
    const Integration::RenderFrame& frame,
    const RenderTarget& target) noexcept {
    return IsInitialized()
        ? impl_->encoder.Record(frame, target)
        : Base::Result<Graphics::CommandList>(NotInitialized(
              "Device renderer is not initialized"));
}

Base::Result<Graphics::CommandList> DeviceRenderer::RecordOffscreen(
    const void* rendererToken,
    const Integration::RenderFrame& frame) noexcept {
    return IsInitialized()
        ? impl_->encoder.RecordOffscreen(rendererToken, frame)
        : Base::Result<Graphics::CommandList>(NotInitialized(
              "Device renderer is not initialized"));
}

Base::Result<Graphics::CommandList> DeviceRenderer::RecordOnscreen(
    const void* rendererToken,
    const Integration::RenderFrame& frame,
    const RenderTarget& target) noexcept {
    return IsInitialized()
        ? impl_->encoder.RecordOnscreen(rendererToken, frame, target)
        : Base::Result<Graphics::CommandList>(NotInitialized(
              "Device renderer is not initialized"));
}

void DeviceRenderer::ReleaseRenderer(
    const void* rendererToken) noexcept {
    if (impl_ != nullptr) {
        impl_->encoder.ReleaseRenderer(rendererToken);
    }
}

FrameEncoderStatistics DeviceRenderer::LastStatistics() const noexcept {
    return impl_ != nullptr
        ? impl_->encoder.LastStatistics()
        : FrameEncoderStatistics{};
}

void DeviceRenderer::SetBatchingEnabled(bool enabled) noexcept {
    batchingEnabled_ = enabled;
    if (impl_ != nullptr) {
        impl_->encoder.SetBatchingEnabled(enabled);
    }
}

bool DeviceRenderer::IsBatchingEnabled() const noexcept {
    return impl_ != nullptr
        ? impl_->encoder.IsBatchingEnabled()
        : batchingEnabled_;
}

Detail::TextResources* DeviceRenderer::GetTextResources() noexcept {
    return IsInitialized() ? &impl_->textResources.Table() : nullptr;
}

Detail::MeshResources* DeviceRenderer::GetMeshResources() noexcept {
    return IsInitialized() ? &impl_->meshResources.Table() : nullptr;
}

Detail::ImageResources* DeviceRenderer::GetImageResources() noexcept {
    return IsInitialized() ? &impl_->imageResources.Table() : nullptr;
}

} // namespace Aero::Render
