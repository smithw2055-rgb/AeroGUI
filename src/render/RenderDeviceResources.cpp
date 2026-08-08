#include "render/private/RenderDevice.hpp"

namespace Aero {
using namespace ::Aero::Graphics;
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

void RenderDevice::Impl::ShutdownResources() noexcept {
    if (resourcesInitialized_) {
        for (std::uint32_t index = 0U; index < resourceSlots_.Size(); ++index) {
            const ResourceSlot& slot = resourceSlots_[index];
            if (slot.alive) {
                DestroyNativeResource(
                    {index, slot.generation, slot.descriptor.type});
            }
        }
        for (const DeferredDestroy& item : deferred_) {
            DestroyNativeResource(item.handle);
        }
    }
    resourceSlots_.Clear();
    deferred_.Clear();
    capabilities_ = {};
    lastSubmittedFence_ = 0U;
    resourcesInitialized_ = false;
}

Base::Result<void> RenderDevice::Impl::InitializeResources() noexcept {
    if (resourcesInitialized_) return {};
    if (NativeDeviceLost()) {
        return InvalidState("Cannot initialize an unavailable graphics backend");
    }
    capabilities_ = QueryNativeDeviceCapabilities();
    const ::Aero::Graphics::GraphicsCapabilities graphics =
        QueryNativeGraphicsCapabilities();
    if (capabilities_.abiVersion != RenderResourceAbiVersion ||
        capabilities_.maxFramesInFlight == 0U ||
        graphics.abiVersion != GraphicsAbiVersion ||
        graphics.backendKind != NativeBackendKind()) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Graphics backend ABI or capabilities are incompatible");
    }
    lastSubmittedFence_ = NativeLastSubmittedFence();
    resourcesInitialized_ = true;
    return {};
}

bool RenderDevice::Impl::AreResourcesReady() const noexcept {
    return resourcesInitialized_ && !NativeDeviceLost();
}

Base::Result<void> RenderDevice::Impl::VerifyResourcesReady() const noexcept {
    if (!resourcesInitialized_) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "RenderDevice resources is not initialized");
    }
    return AreResourcesReady()
        ? Base::Result<void>()
        : Base::Result<void>(
              InvalidState("RenderDevice resources is lost"));
}

Base::Result<void> RenderDevice::Impl::ValidateResourceDescriptor(
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

Base::Result<ResourceHandle> RenderDevice::Impl::CreateResource(
    const ResourceDescriptor& descriptor) noexcept {
    Base::Result<void> ready = VerifyResourcesReady();
    if (!ready) return ready.GetStatus();
    Base::Result<void> valid = ValidateResourceDescriptor(descriptor);
    if (!valid) return valid.GetStatus();

    std::uint32_t index = UINT32_MAX;
    for (std::uint32_t current = 0U;
         current < resourceSlots_.Size(); ++current) {
        if (resourceSlots_[current].alive) continue;
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
        Base::Result<void> appended = resourceSlots_.PushBack(slot);
        if (!appended) return appended.GetStatus();
        index = resourceSlots_.Size() - 1U;
    } else {
        ResourceSlot& slot = resourceSlots_[index];
        if (slot.generation == UINT32_MAX) {
            return OutOfRange("Resource generation space exhausted");
        }
        ++slot.generation;
        slot.descriptor = descriptor;
        slot.alive = true;
    }

    ResourceSlot& slot = resourceSlots_[index];
    const ResourceHandle handle{
        index, slot.generation, descriptor.type};
    Base::Result<void> created =
        CreateNativeResource(handle, descriptor);
    if (!created) {
        slot.alive = false;
        return created.GetStatus();
    }
    return handle;
}

void RenderDevice::Impl::RollbackResource(ResourceHandle handle) noexcept {
    if (!handle.IsValid() || handle.index >= resourceSlots_.Size()) return;
    ResourceSlot& slot = resourceSlots_[handle.index];
    if (!slot.alive || slot.generation != handle.generation) return;
    DestroyNativeResource(handle);
    slot.alive = false;
}

Base::Result<ResourceHandle> RenderDevice::Impl::CreateBuffer(
    const BufferDescriptor& descriptor) noexcept {
    ResourceDescriptor resource;
    resource.type = ResourceType::Buffer;
    resource.buffer = descriptor;
    return CreateResource(resource);
}

Base::Result<ResourceHandle> RenderDevice::Impl::CreateTexture(
    const TextureResourceDescriptor& descriptor) noexcept {
    Base::Result<void> valid = ValidateTextureDescriptor(
        descriptor, QueryNativeGraphicsCapabilities());
    if (!valid) return valid.GetStatus();

    ResourceDescriptor resource;
    resource.type = ResourceType::Texture;
    resource.texture.width = descriptor.width;
    resource.texture.height = descriptor.height;
    resource.texture.format = ToBaseTextureFormat(descriptor.format);
    Base::Result<ResourceHandle> created = CreateResource(resource);
    if (!created) return created.GetStatus();
    Base::Result<void> configured =
        ConfigureNativeTexture(created.Value(), descriptor);
    if (!configured) {
        RollbackResource(created.Value());
        return configured.GetStatus();
    }
    return created.Value();
}

Base::Result<ResourceHandle> RenderDevice::Impl::CreateExternalTexture(
    const TextureResourceDescriptor& descriptor) noexcept {
    Base::Result<void> valid = ValidateTextureDescriptor(
        descriptor, QueryNativeGraphicsCapabilities());
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

Base::Result<ResourceHandle> RenderDevice::Impl::CreateRenderTarget(
    const TextureResourceDescriptor& descriptor) noexcept {
    Base::Result<void> valid = ValidateTextureDescriptor(
        descriptor, QueryNativeGraphicsCapabilities());
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
        ConfigureNativeTexture(created.Value(), descriptor);
    if (!configured) {
        RollbackResource(created.Value());
        return configured.GetStatus();
    }
    return created.Value();
}

Base::Result<ResourceHandle> RenderDevice::Impl::CreateExternalRenderTarget(
    const TextureResourceDescriptor& descriptor) noexcept {
    Base::Result<void> valid = ValidateTextureDescriptor(
        descriptor, QueryNativeGraphicsCapabilities());
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

Base::Result<ResourceHandle> RenderDevice::Impl::CreateSampler(
    const SamplerDescriptor& descriptor) noexcept {
    Base::Result<void> valid = ValidateSamplerDescriptor(
        descriptor, QueryNativeGraphicsCapabilities());
    if (!valid) return valid.GetStatus();

    ResourceDescriptor resource;
    resource.type = ResourceType::Sampler;
    Base::Result<ResourceHandle> created = CreateResource(resource);
    if (!created) return created.GetStatus();
    Base::Result<void> configured =
        ConfigureNativeSampler(created.Value(), descriptor);
    if (!configured) {
        RollbackResource(created.Value());
        return configured.GetStatus();
    }
    return created.Value();
}

Base::Result<ResourceHandle> RenderDevice::Impl::CreatePipeline(
    const PipelineDescriptor& descriptor) noexcept {
    Base::Result<void> valid = ValidatePipelineDescriptor(
        descriptor, QueryNativeGraphicsCapabilities());
    if (!valid) return valid.GetStatus();

    ResourceDescriptor resource;
    resource.type = ResourceType::Pipeline;
    Base::Result<ResourceHandle> created = CreateResource(resource);
    if (!created) return created.GetStatus();
    Base::Result<void> configured =
        ConfigureNativePipeline(created.Value(), descriptor);
    if (!configured) {
        RollbackResource(created.Value());
        return configured.GetStatus();
    }
    return created.Value();
}

bool RenderDevice::Impl::IsAlive(ResourceHandle handle) const noexcept {
    if (!handle.IsValid() || handle.index >= resourceSlots_.Size()) return false;
    const ResourceSlot& slot = resourceSlots_[handle.index];
    return slot.alive &&
        slot.generation == handle.generation &&
        slot.descriptor.type == handle.type;
}

Base::Result<void> RenderDevice::Impl::DestroyResource(
    ResourceHandle handle,
    FenceValue retireAfter) noexcept {
    Base::Result<void> ready = VerifyResourcesReady();
    if (!ready) return ready;
    if (!IsAlive(handle)) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Resource handle is stale or unknown");
    }
    ResourceSlot& slot = resourceSlots_[handle.index];
    slot.alive = false;
    Base::Result<void> appended = deferred_.PushBack(
        {handle, retireAfter});
    if (!appended) {
        slot.alive = true;
        return appended;
    }
    return {};
}

Base::Result<FenceValue> RenderDevice::Impl::SubmitCommands(
    const CommandList& commands) noexcept {
    Base::Result<void> ready = VerifyResourcesReady();
    if (!ready) return ready.GetStatus();
    Base::Result<std::uint32_t> collected = CollectGarbage();
    if (!collected) return collected.GetStatus();

    const FenceValue backendFence = NativeLastSubmittedFence();
    if (backendFence == UINT64_MAX) {
        return OutOfRange("RenderDevice fence space is exhausted");
    }
    const FenceValue signalFence = backendFence + 1U;
    Base::Result<void> submitted =
        SubmitNativeCommands(commands, signalFence);
    if (!submitted) return submitted.GetStatus();
    lastSubmittedFence_ = signalFence;
    return signalFence;
}

Base::Result<std::uint32_t> RenderDevice::Impl::CollectGarbage() noexcept {
    Base::Result<void> ready = VerifyResourcesReady();
    if (!ready) return ready.GetStatus();
    const FenceValue completed = NativeCompletedFence();
    std::uint32_t released = 0U;
    std::uint32_t index = 0U;
    while (index < deferred_.Size()) {
        if (deferred_[index].retireAfter <= completed) {
            DestroyNativeResource(deferred_[index].handle);
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

std::uint32_t RenderDevice::Impl::LiveResourceCount() const noexcept {
    std::uint32_t count = 0U;
    for (const ResourceSlot& slot : resourceSlots_) {
        if (slot.alive) ++count;
    }
    return count;
}

} // namespace Aero
