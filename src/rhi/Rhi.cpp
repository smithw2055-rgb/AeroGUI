#include "Rhi.hpp"

#include "Graphics.hpp"

namespace Aero::Rhi {
namespace {

Base::Status InvalidArgument(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidArgument, message);
}

Base::Status InvalidState(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

Base::Status OutOfRange(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::OutOfRange, message);
}

TextureFormat ToBaseTextureFormat(
    GraphicsTextureFormat format) noexcept {
    switch (format) {
    case GraphicsTextureFormat::Rgba8Unorm:
        return TextureFormat::Rgba8Unorm;
    case GraphicsTextureFormat::Bgra8Unorm:
        return TextureFormat::Bgra8Unorm;
    case GraphicsTextureFormat::R8Unorm:
        return TextureFormat::R8Unorm;
    case GraphicsTextureFormat::Depth24Stencil8:
        break;
    }
    return TextureFormat::R8Unorm;
}

} // namespace

RhiDevice::RhiDevice(
    IGraphicsBackend& backend,
    Base::IAllocator* allocator) noexcept
    : backend_(&backend),
      allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()),
      slots_(allocator_),
      deferred_(allocator_) {}

RhiDevice::~RhiDevice() noexcept {
    if (backend_ == nullptr) return;
    for (std::uint32_t index = 0U; index < slots_.Size(); ++index) {
        const ResourceSlot& slot = slots_[index];
        if (slot.alive) {
            backend_->DestroyResource(
                {index, slot.generation, slot.descriptor.type});
        }
    }
    for (const DeferredDestroy& item : deferred_) {
        backend_->DestroyResource(item.handle);
    }
}

Base::Result<void> RhiDevice::Initialize() noexcept {
    if (initialized_) return {};
    if (backend_ == nullptr || backend_->IsDeviceLost()) {
        return InvalidState("Cannot initialize an unavailable graphics backend");
    }
    capabilities_ = backend_->Capabilities();
    const GraphicsCapabilities graphics =
        backend_->QueryGraphicsCapabilities();
    if (capabilities_.abiVersion != RhiAbiVersion ||
        capabilities_.maxFramesInFlight == 0U ||
        graphics.abiVersion != GraphicsAbiVersion ||
        graphics.backendKind != backend_->Kind()) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Graphics backend ABI or capabilities are incompatible");
    }
    lastSubmittedFence_ = backend_->LastSubmittedFence();
    initialized_ = true;
    return {};
}

Base::Result<void> RhiDevice::VerifyReady() const noexcept {
    if (!initialized_) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "RHI device is not initialized");
    }
    if (backend_ == nullptr || backend_->IsDeviceLost()) {
        return InvalidState("RHI device is lost");
    }
    return {};
}

Base::Result<void> RhiDevice::ValidateDescriptor(
    const ResourceDescriptor& descriptor) const noexcept {
    switch (descriptor.type) {
    case ResourceType::Buffer:
        return descriptor.buffer.sizeBytes != 0U
            ? Base::Result<void>()
            : Base::Result<void>(
                InvalidArgument("Buffer size must be non-zero"));
    case ResourceType::Texture:
    case ResourceType::RenderTarget:
        if (descriptor.texture.width == 0U ||
            descriptor.texture.height == 0U ||
            descriptor.texture.width > capabilities_.maxTextureDimension ||
            descriptor.texture.height > capabilities_.maxTextureDimension) {
            return InvalidArgument("Texture dimensions are invalid");
        }
        return {};
    case ResourceType::Sampler:
    case ResourceType::Pipeline:
        return {};
    case ResourceType::Invalid:
        break;
    }
    return InvalidArgument("Resource type is invalid");
}

Base::Result<ResourceHandle> RhiDevice::CreateResource(
    const ResourceDescriptor& descriptor) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    Base::Result<void> valid = ValidateDescriptor(descriptor);
    if (!valid) return valid.GetStatus();

    std::uint32_t index = UINT32_MAX;
    for (std::uint32_t current = 0U;
         current < slots_.Size(); ++current) {
        if (slots_[current].alive) continue;
        bool pending = false;
        for (const DeferredDestroy& item : deferred_) {
            if (item.handle.index == current) {
                pending = true;
                break;
            }
        }
        if (!pending) {
            index = current;
            break;
        }
    }

    if (index == UINT32_MAX) {
        ResourceSlot slot;
        slot.descriptor = descriptor;
        slot.alive = true;
        Base::Result<void> appended = slots_.TryPushBack(slot);
        if (!appended) return appended.GetStatus();
        index = slots_.Size() - 1U;
    } else {
        ResourceSlot& slot = slots_[index];
        if (slot.generation == UINT32_MAX) {
            return OutOfRange("Resource generation space exhausted");
        }
        ++slot.generation;
        slot.descriptor = descriptor;
        slot.alive = true;
    }

    ResourceSlot& slot = slots_[index];
    const ResourceHandle handle{
        index, slot.generation, descriptor.type};
    Base::Result<void> created =
        backend_->CreateResource(handle, descriptor);
    if (!created) {
        slot.alive = false;
        return created.GetStatus();
    }
    return handle;
}

void RhiDevice::Rollback(ResourceHandle handle) noexcept {
    if (!handle.IsValid() || handle.index >= slots_.Size()) return;
    ResourceSlot& slot = slots_[handle.index];
    if (!slot.alive || slot.generation != handle.generation) return;
    backend_->DestroyResource(handle);
    slot.alive = false;
}

Base::Result<ResourceHandle> RhiDevice::CreateBuffer(
    const BufferDescriptor& descriptor) noexcept {
    ResourceDescriptor resource;
    resource.type = ResourceType::Buffer;
    resource.buffer = descriptor;
    return CreateResource(resource);
}

Base::Result<ResourceHandle> RhiDevice::CreateTexture(
    const TextureResourceDescriptor& descriptor) noexcept {
    Base::Result<void> valid = ValidateTextureDescriptor(
        descriptor, backend_->QueryGraphicsCapabilities());
    if (!valid) return valid.GetStatus();

    ResourceDescriptor resource;
    resource.type = ResourceType::Texture;
    resource.texture.width = descriptor.width;
    resource.texture.height = descriptor.height;
    resource.texture.format = ToBaseTextureFormat(descriptor.format);
    Base::Result<ResourceHandle> created = CreateResource(resource);
    if (!created) return created.GetStatus();
    Base::Result<void> configured =
        backend_->ConfigureTexture(created.Value(), descriptor);
    if (!configured) {
        Rollback(created.Value());
        return configured.GetStatus();
    }
    return created.Value();
}

Base::Result<ResourceHandle> RhiDevice::CreateExternalTexture(
    const TextureResourceDescriptor& descriptor) noexcept {
    Base::Result<void> valid = ValidateTextureDescriptor(
        descriptor, backend_->QueryGraphicsCapabilities());
    if (!valid) return valid.GetStatus();
    if (!HasTextureUsage(descriptor.usage, TextureUsage::Sampled)) {
        return InvalidArgument(
            "External textures require Sampled usage");
    }

    ResourceDescriptor resource;
    resource.type = ResourceType::Texture;
    resource.texture.width = descriptor.width;
    resource.texture.height = descriptor.height;
    resource.texture.format = ToBaseTextureFormat(descriptor.format);
    return CreateResource(resource);
}

Base::Result<ResourceHandle> RhiDevice::CreateRenderTarget(
    const TextureResourceDescriptor& descriptor) noexcept {
    Base::Result<void> valid = ValidateTextureDescriptor(
        descriptor, backend_->QueryGraphicsCapabilities());
    if (!valid) return valid.GetStatus();
    if (!HasTextureUsage(descriptor.usage, TextureUsage::RenderTarget)) {
        return InvalidArgument(
            "Render-target resources require RenderTarget usage");
    }

    ResourceDescriptor resource;
    resource.type = ResourceType::RenderTarget;
    resource.texture.width = descriptor.width;
    resource.texture.height = descriptor.height;
    resource.texture.format = ToBaseTextureFormat(descriptor.format);
    Base::Result<ResourceHandle> created = CreateResource(resource);
    if (!created) return created.GetStatus();
    Base::Result<void> configured =
        backend_->ConfigureTexture(created.Value(), descriptor);
    if (!configured) {
        Rollback(created.Value());
        return configured.GetStatus();
    }
    return created.Value();
}

Base::Result<ResourceHandle> RhiDevice::CreateExternalRenderTarget(
    const TextureResourceDescriptor& descriptor) noexcept {
    Base::Result<void> valid = ValidateTextureDescriptor(
        descriptor, backend_->QueryGraphicsCapabilities());
    if (!valid) return valid.GetStatus();
    if (!HasTextureUsage(descriptor.usage, TextureUsage::RenderTarget)) {
        return InvalidArgument(
            "External render targets require RenderTarget usage");
    }

    ResourceDescriptor resource;
    resource.type = ResourceType::RenderTarget;
    resource.texture.width = descriptor.width;
    resource.texture.height = descriptor.height;
    resource.texture.format = ToBaseTextureFormat(descriptor.format);
    return CreateResource(resource);
}

Base::Result<ResourceHandle> RhiDevice::CreateSampler(
    const SamplerDescriptor& descriptor) noexcept {
    Base::Result<void> valid = ValidateSamplerDescriptor(
        descriptor, backend_->QueryGraphicsCapabilities());
    if (!valid) return valid.GetStatus();

    ResourceDescriptor resource;
    resource.type = ResourceType::Sampler;
    Base::Result<ResourceHandle> created = CreateResource(resource);
    if (!created) return created.GetStatus();
    Base::Result<void> configured =
        backend_->ConfigureSampler(created.Value(), descriptor);
    if (!configured) {
        Rollback(created.Value());
        return configured.GetStatus();
    }
    return created.Value();
}

Base::Result<ResourceHandle> RhiDevice::CreatePipeline(
    const PipelineDescriptor& descriptor) noexcept {
    Base::Result<void> valid = ValidatePipelineDescriptor(
        descriptor, backend_->QueryGraphicsCapabilities());
    if (!valid) return valid.GetStatus();

    ResourceDescriptor resource;
    resource.type = ResourceType::Pipeline;
    Base::Result<ResourceHandle> created = CreateResource(resource);
    if (!created) return created.GetStatus();
    Base::Result<void> configured =
        backend_->ConfigurePipeline(created.Value(), descriptor);
    if (!configured) {
        Rollback(created.Value());
        return configured.GetStatus();
    }
    return created.Value();
}

bool RhiDevice::IsAlive(ResourceHandle handle) const noexcept {
    if (!handle.IsValid() || handle.index >= slots_.Size()) return false;
    const ResourceSlot& slot = slots_[handle.index];
    return slot.alive &&
        slot.generation == handle.generation &&
        slot.descriptor.type == handle.type;
}

Base::Result<void> RhiDevice::DestroyResource(
    ResourceHandle handle,
    FenceValue retireAfter) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready;
    if (!IsAlive(handle)) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Resource handle is stale or unknown");
    }
    ResourceSlot& slot = slots_[handle.index];
    slot.alive = false;
    Base::Result<void> appended = deferred_.TryPushBack(
        {handle, retireAfter});
    if (!appended) {
        slot.alive = true;
        return appended;
    }
    return {};
}

Base::Result<FenceValue> RhiDevice::Submit(
    const CommandList& commands) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    Base::Result<std::uint32_t> collected = CollectGarbage();
    if (!collected) return collected.GetStatus();

    const FenceValue backendFence = backend_->LastSubmittedFence();
    if (backendFence == UINT64_MAX) {
        return OutOfRange("RHI fence space is exhausted");
    }
    const FenceValue signalFence = backendFence + 1U;
    Base::Result<void> submitted =
        backend_->Submit(commands, signalFence);
    if (!submitted) return submitted.GetStatus();
    lastSubmittedFence_ = signalFence;
    return signalFence;
}

Base::Result<std::uint32_t> RhiDevice::CollectGarbage() noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    const FenceValue completed = backend_->CompletedFence();
    std::uint32_t released = 0U;
    std::uint32_t index = 0U;
    while (index < deferred_.Size()) {
        if (deferred_[index].retireAfter <= completed) {
            backend_->DestroyResource(deferred_[index].handle);
            for (std::uint32_t current = index + 1U;
                 current < deferred_.Size(); ++current) {
                deferred_[current - 1U] = deferred_[current];
            }
            deferred_.PopBack();
            ++released;
        } else {
            ++index;
        }
    }
    return released;
}

std::uint32_t RhiDevice::LiveResourceCount() const noexcept {
    std::uint32_t count = 0U;
    for (const ResourceSlot& slot : slots_) {
        if (slot.alive) ++count;
    }
    return count;
}

} // namespace Aero::Rhi
