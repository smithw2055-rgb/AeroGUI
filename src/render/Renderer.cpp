#include "Renderer.hpp"

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

struct Renderer::Impl {
    Impl(
        Graphics::Device& device,
        const FrameShaderSet& shaders,
        std::uint64_t generation,
        Base::IAllocator& allocator) noexcept
        : encoder(device, shaders, &allocator),
          textResources(device, encoder, generation, allocator),
          meshResources(device, encoder, generation, allocator),
          imageResources(device, encoder, generation, allocator) {}

    Detail::Renderer encoder;
    Detail::TextGpuResources textResources;
    Detail::MeshGpuResources meshResources;
    Detail::ImageGpuResources imageResources;
    bool initialized = false;
};

Renderer::Renderer(
    Graphics::Device& device,
    const FrameShaderSet& shaders,
    std::uint64_t generation,
    Base::IAllocator* allocator) noexcept
    : device_(&device),
      shaders_(shaders),
      allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()),
      generation_(generation) {}

Renderer::~Renderer() noexcept {
    Shutdown();
}

Base::Result<void> Renderer::Initialize() noexcept {
    if (IsInitialized()) return {};
    if (device_ == nullptr || !device_->IsReady() ||
        generation_ == 0U) {
        return NotInitialized(
            "Renderer requires a ready graphics device and generation");
    }
    if (impl_ == nullptr) {
        void* memory = allocator_->Allocate({
            sizeof(Impl), alignof(Impl), Base::MemoryTag::Render});
        if (memory == nullptr) {
            return OutOfMemory("Failed to allocate renderer state");
        }
        impl_ = new (memory) Impl(
            *device_, shaders_, generation_, *allocator_);
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

void Renderer::Shutdown() noexcept {
    if (impl_ == nullptr) return;
    impl_->~Impl();
    allocator_->Deallocate(
        impl_, sizeof(Impl), alignof(Impl), Base::MemoryTag::Render);
    impl_ = nullptr;
}

bool Renderer::IsInitialized() const noexcept {
    return impl_ != nullptr && impl_->initialized &&
        impl_->encoder.IsInitialized();
}

Base::Result<void> Renderer::RegisterImage(
    RenderImageId image,
    Graphics::ResourceHandle texture,
    Graphics::ResourceHandle sampler) noexcept {
    return IsInitialized()
        ? impl_->encoder.RegisterImage(image, texture, sampler)
        : Base::Result<void>(NotInitialized("Renderer is not initialized"));
}

Base::Result<void> Renderer::UnregisterImage(
    RenderImageId image) noexcept {
    return IsInitialized()
        ? impl_->encoder.UnregisterImage(image)
        : Base::Result<void>(NotInitialized("Renderer is not initialized"));
}

Base::Result<void> Renderer::RegisterMesh(
    RenderMeshId mesh,
    Graphics::ResourceHandle vertexBuffer,
    Graphics::ResourceHandle indexBuffer,
    std::uint32_t indexCount,
    Graphics::IndexType indexType) noexcept {
    return IsInitialized()
        ? impl_->encoder.RegisterMesh(
              mesh, vertexBuffer, indexBuffer, indexCount, indexType)
        : Base::Result<void>(NotInitialized("Renderer is not initialized"));
}

Base::Result<void> Renderer::UnregisterMesh(
    RenderMeshId mesh) noexcept {
    return IsInitialized()
        ? impl_->encoder.UnregisterMesh(mesh)
        : Base::Result<void>(NotInitialized("Renderer is not initialized"));
}

Base::Result<void> Renderer::RegisterGlyphRun(
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
        : Base::Result<void>(NotInitialized("Renderer is not initialized"));
}

Base::Result<void> Renderer::UnregisterGlyphRun(
    RenderGlyphRunId glyphRun) noexcept {
    return IsInitialized()
        ? impl_->encoder.UnregisterGlyphRun(glyphRun)
        : Base::Result<void>(NotInitialized("Renderer is not initialized"));
}

Base::Result<Graphics::CommandList> Renderer::Record(
    const ::Aero::Render::Detail::RenderFrame& frame,
    const RenderTarget& target) noexcept {
    return IsInitialized()
        ? impl_->encoder.Record(frame, target)
        : Base::Result<Graphics::CommandList>(
              NotInitialized("Renderer is not initialized"));
}

Base::Result<Graphics::CommandList> Renderer::RecordOffscreen(
    const void* rendererToken,
    const ::Aero::Render::Detail::RenderFrame& frame) noexcept {
    return IsInitialized()
        ? impl_->encoder.RecordOffscreen(rendererToken, frame)
        : Base::Result<Graphics::CommandList>(
              NotInitialized("Renderer is not initialized"));
}

Base::Result<Graphics::CommandList> Renderer::RecordOnscreen(
    const void* rendererToken,
    const ::Aero::Render::Detail::RenderFrame& frame,
    const RenderTarget& target) noexcept {
    return IsInitialized()
        ? impl_->encoder.RecordOnscreen(rendererToken, frame, target)
        : Base::Result<Graphics::CommandList>(
              NotInitialized("Renderer is not initialized"));
}

void Renderer::ReleaseRenderer(
    const void* rendererToken) noexcept {
    if (impl_ != nullptr) {
        impl_->encoder.ReleaseRenderer(rendererToken);
    }
}

FrameEncoderStatistics Renderer::LastStatistics() const noexcept {
    return impl_ != nullptr
        ? impl_->encoder.LastStatistics()
        : FrameEncoderStatistics{};
}

void Renderer::SetBatchingEnabled(bool enabled) noexcept {
    batchingEnabled_ = enabled;
    if (impl_ != nullptr) {
        impl_->encoder.SetBatchingEnabled(enabled);
    }
}

bool Renderer::IsBatchingEnabled() const noexcept {
    return impl_ != nullptr
        ? impl_->encoder.IsBatchingEnabled()
        : batchingEnabled_;
}

Detail::RenderResources Renderer::Resources() noexcept {
    return IsInitialized()
        ? Detail::RenderResources{
              &impl_->textResources.Table(),
              &impl_->meshResources.Table(),
              &impl_->imageResources.Table()}
        : Detail::RenderResources{};
}

} // namespace Aero::Render
