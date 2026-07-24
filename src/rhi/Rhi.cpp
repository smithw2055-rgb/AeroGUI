#include <Aero/Rhi/Rhi.hpp>

#include <Aero/Base/Allocator.hpp>

#include <cmath>
#include <cstring>
#include <new>
#include <utility>

namespace Aero::Rhi {
namespace {

constexpr std::uint64_t HashOffset = 1469598103934665603ULL;
constexpr std::uint64_t HashPrime = 1099511628211ULL;

Base::Status InvalidArgument(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidArgument, message);
}

Base::Status InvalidState(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

Base::Status OutOfMemory(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::OutOfMemory, message);
}

Base::Status OutOfRange(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::OutOfRange, message);
}

void HashBytes(std::uint64_t& hash, const void* data, std::size_t size) noexcept {
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    for (std::size_t index = 0U; index < size; ++index) {
        hash ^= bytes[index];
        hash *= HashPrime;
    }
}

template<class T>
void HashValue(std::uint64_t& hash, const T& value) noexcept {
    HashBytes(hash, &value, sizeof(T));
}

bool IsPowerOfTwo(std::uint32_t value) noexcept {
    return value != 0U && (value & (value - 1U)) == 0U;
}

bool IsValidColor(Base::Color value) noexcept {
    return Base::IsFiniteColor(value) &&
        value.red >= 0.0F && value.red <= 1.0F &&
        value.green >= 0.0F && value.green <= 1.0F &&
        value.blue >= 0.0F && value.blue <= 1.0F &&
        value.alpha >= 0.0F && value.alpha <= 1.0F;
}

} // namespace

std::uint64_t CommandBuffer::StableHash() const noexcept {
    std::uint64_t hash = HashOffset;
    for (const RhiCommand& command : commands_) {
        HashValue(hash, command.kind);
        HashValue(hash, command.rect.x);
        HashValue(hash, command.rect.y);
        HashValue(hash, command.rect.width);
        HashValue(hash, command.rect.height);
        HashValue(hash, command.transform.m11);
        HashValue(hash, command.transform.m12);
        HashValue(hash, command.transform.m21);
        HashValue(hash, command.transform.m22);
        HashValue(hash, command.transform.dx);
        HashValue(hash, command.transform.dy);
        HashValue(hash, command.color.red);
        HashValue(hash, command.color.green);
        HashValue(hash, command.color.blue);
        HashValue(hash, command.color.alpha);
        HashValue(hash, command.scalar);
        HashValue(hash, command.nodeId);
    }
    return hash;
}

UploadArena::UploadArena(
    std::uint32_t capacityBytes,
    Base::IAllocator* allocator) noexcept
    : bytes_(allocator), capacity_(capacityBytes) {}

Base::Result<void> UploadArena::Initialize() noexcept {
    if (initialized_) {
        return {};
    }
    if (capacity_ == 0U) {
        return InvalidArgument("UploadArena capacity must be non-zero");
    }
    Base::Result<void> reserve = bytes_.TryReserve(capacity_);
    if (!reserve) {
        return reserve;
    }
    for (std::uint32_t index = 0U; index < capacity_; ++index) {
        Base::Result<void> appended = bytes_.TryPushBack(0U);
        if (!appended) {
            bytes_.Clear();
            return appended;
        }
    }
    initialized_ = true;
    return {};
}

Base::Result<UploadSlice> UploadArena::Allocate(
    std::uint32_t size,
    std::uint32_t alignment) noexcept {
    if (!initialized_) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "UploadArena must be initialized before allocation");
    }
    if (size == 0U || !IsPowerOfTwo(alignment)) {
        return InvalidArgument("Upload allocation requires non-zero size and power-of-two alignment");
    }
    const std::uint32_t mask = alignment - 1U;
    if (offset_ > UINT32_MAX - mask) {
        return Base::Status::Failure(Base::ErrorCode::OutOfRange,
            "UploadArena alignment overflow");
    }
    const std::uint32_t aligned = (offset_ + mask) & ~mask;
    if (aligned > capacity_ || size > capacity_ - aligned) {
        return OutOfMemory("UploadArena capacity exhausted");
    }
    UploadSlice slice;
    slice.offset = aligned;
    slice.size = size;
    slice.data = bytes_.Data() + aligned;
    offset_ = aligned + size;
    return slice;
}

RhiDevice::RhiDevice(
    IRhiBackend& backend,
    Base::IAllocator* allocator) noexcept
    : backend_(&backend),
      allocator_(allocator != nullptr ? allocator : &Base::GetDefaultAllocator()),
      slots_(allocator_),
      deferred_(allocator_),
      frameArenas_(allocator_) {}

RhiDevice::~RhiDevice() noexcept {
    if (backend_ != nullptr) {
        for (std::uint32_t index = 0U; index < slots_.Size(); ++index) {
            const ResourceSlot& slot = slots_[index];
            if (slot.alive) {
                backend_->DestroyResource({index, slot.generation, slot.descriptor.type});
            }
        }
        for (const DeferredDestroy& item : deferred_) {
            backend_->DestroyResource(item.handle);
        }
    }
    ReleaseFrameArenas();
}

Base::Result<void> RhiDevice::Initialize() noexcept {
    if (initialized_) {
        return {};
    }
    if (backend_->IsDeviceLost()) {
        return InvalidState("Cannot initialize a lost RHI device");
    }
    capabilities_ = backend_->Capabilities();
    if (capabilities_.abiVersion != RhiAbiVersion ||
        capabilities_.maxFramesInFlight == 0U) {
        return Base::Status::Failure(Base::ErrorCode::Unsupported,
            "RHI backend ABI or frame capability is unsupported");
    }

    Base::Result<void> reserve = frameArenas_.TryReserve(
        capabilities_.maxFramesInFlight);
    if (!reserve) {
        return reserve;
    }
    for (std::uint32_t index = 0U;
         index < capabilities_.maxFramesInFlight;
         ++index) {
        void* memory = allocator_->Allocate({
            sizeof(UploadArena), alignof(UploadArena), Base::MemoryTag::Render});
        if (memory == nullptr) {
            ReleaseFrameArenas();
            return OutOfMemory("Failed to allocate frame upload arena");
        }
        UploadArena* arena = new (memory) UploadArena(256U * 1024U, allocator_);
        Base::Result<void> initialized = arena->Initialize();
        if (!initialized) {
            arena->~UploadArena();
            allocator_->Deallocate(memory, sizeof(UploadArena),
                alignof(UploadArena), Base::MemoryTag::Render);
            ReleaseFrameArenas();
            return initialized;
        }
        Base::Result<void> appended = frameArenas_.TryPushBack(arena);
        if (!appended) {
            arena->~UploadArena();
            allocator_->Deallocate(memory, sizeof(UploadArena),
                alignof(UploadArena), Base::MemoryTag::Render);
            ReleaseFrameArenas();
            return appended;
        }
    }
    lastSubmittedFence_ = backend_->LastSubmittedFence();
    initialized_ = true;
    return {};
}

Base::Result<void> RhiDevice::VerifyReady() const noexcept {
    if (!initialized_) {
        return Base::Status::Failure(Base::ErrorCode::NotInitialized,
            "RHI device is not initialized");
    }
    if (backend_->IsDeviceLost()) {
        return InvalidState("RHI device is lost");
    }
    return {};
}

Base::Result<void> RhiDevice::ValidateDescriptor(
    const ResourceDescriptor& descriptor) const noexcept {
    switch (descriptor.type) {
    case ResourceType::Buffer:
        if (descriptor.buffer.sizeBytes == 0U) {
            return InvalidArgument("Buffer size must be non-zero");
        }
        return {};
    case ResourceType::Texture:
    case ResourceType::RenderTarget:
        if (descriptor.texture.width == 0U || descriptor.texture.height == 0U ||
            descriptor.texture.width > capabilities_.maxTextureDimension ||
            descriptor.texture.height > capabilities_.maxTextureDimension) {
            return InvalidArgument("Texture dimensions are invalid");
        }
        return {};
    case ResourceType::Sampler:
    case ResourceType::Pipeline:
        return {};
    default:
        return InvalidArgument("Resource type is invalid");
    }
}

Base::Result<ResourceHandle> RhiDevice::CreateResource(
    const ResourceDescriptor& descriptor) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) {
        return ready.GetStatus();
    }
    Base::Result<void> valid = ValidateDescriptor(descriptor);
    if (!valid) {
        return valid.GetStatus();
    }

    std::uint32_t index = UINT32_MAX;
    for (std::uint32_t current = 0U; current < slots_.Size(); ++current) {
        if (!slots_[current].alive) {
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
    }

    if (index == UINT32_MAX) {
        ResourceSlot slot;
        slot.descriptor = descriptor;
        slot.alive = true;
        Base::Result<void> appended = slots_.TryPushBack(slot);
        if (!appended) {
            return appended.GetStatus();
        }
        index = slots_.Size() - 1U;
    } else {
        ResourceSlot& slot = slots_[index];
        if (slot.generation == UINT32_MAX) {
            return Base::Status::Failure(Base::ErrorCode::OutOfRange,
                "Resource generation space exhausted");
        }
        ++slot.generation;
        slot.descriptor = descriptor;
        slot.alive = true;
    }

    ResourceSlot& slot = slots_[index];
    ResourceHandle handle{index, slot.generation, descriptor.type};
    Base::Result<void> created = backend_->CreateResource(handle, descriptor);
    if (!created) {
        slot.alive = false;
        return created.GetStatus();
    }
    return handle;
}

bool RhiDevice::IsAlive(ResourceHandle handle) const noexcept {
    if (!handle.IsValid() || handle.index >= slots_.Size()) {
        return false;
    }
    const ResourceSlot& slot = slots_[handle.index];
    return slot.alive && slot.generation == handle.generation &&
        slot.descriptor.type == handle.type;
}

Base::Result<void> RhiDevice::DestroyResource(
    ResourceHandle handle,
    FenceValue retireAfter) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) {
        return ready;
    }
    if (!IsAlive(handle)) {
        return Base::Status::Failure(Base::ErrorCode::NotFound,
            "Resource handle is stale or unknown");
    }
    ResourceSlot& slot = slots_[handle.index];
    slot.alive = false;
    DeferredDestroy pending;
    pending.handle = handle;
    pending.retireAfter = retireAfter;
    Base::Result<void> appended = deferred_.TryPushBack(pending);
    if (!appended) {
        slot.alive = true;
        return appended;
    }
    return {};
}

Base::Result<FrameContext> RhiDevice::BeginFrame() noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) {
        return ready.GetStatus();
    }
    Base::Result<std::uint32_t> collected = CollectGarbage();
    if (!collected) {
        return collected.GetStatus();
    }
    const FenceValue backendFence = backend_->LastSubmittedFence();
    if (backendFence == UINT64_MAX) {
        return OutOfRange("RHI fence space is exhausted");
    }
    lastSubmittedFence_ = backendFence;
    FrameContext frame;
    frame.frameIndex = nextFrameIndex_;
    frame.signalFence = lastSubmittedFence_ + 1U;
    frame.uploadArena = frameArenas_[nextFrameIndex_];
    frame.uploadArena->Reset();
    nextFrameIndex_ = (nextFrameIndex_ + 1U) % capabilities_.maxFramesInFlight;
    return frame;
}

Base::Result<FenceValue> RhiDevice::Submit(
    FrameContext& frame,
    const CommandBuffer& commands) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) {
        return ready.GetStatus();
    }
    if (frame.uploadArena == nullptr) {
        return InvalidState("FrameContext is stale or already submitted");
    }
    const FenceValue backendFence = backend_->LastSubmittedFence();
    if (backendFence == UINT64_MAX) {
        return OutOfRange("RHI fence space is exhausted");
    }
    frame.signalFence = backendFence + 1U;
    Base::Result<void> submitted = backend_->Submit(commands, frame.signalFence);
    if (!submitted) {
        return submitted.GetStatus();
    }
    lastSubmittedFence_ = frame.signalFence;
    frame.uploadArena = nullptr;
    return lastSubmittedFence_;
}

Base::Result<std::uint32_t> RhiDevice::CollectGarbage() noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) {
        return ready.GetStatus();
    }
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
        if (slot.alive) {
            ++count;
        }
    }
    return count;
}

void RhiDevice::ReleaseFrameArenas() noexcept {
    for (UploadArena* arena : frameArenas_) {
        if (arena != nullptr) {
            arena->~UploadArena();
            allocator_->Deallocate(arena, sizeof(UploadArena),
                alignof(UploadArena), Base::MemoryTag::Render);
        }
    }
    frameArenas_.Clear();
}

DeviceCapabilities NullRhiBackend::Capabilities() const noexcept {
    DeviceCapabilities capabilities;
    capabilities.abiVersion = RhiAbiVersion;
    capabilities.maxFramesInFlight = 3U;
    capabilities.maxTextureDimension = 8192U;
    capabilities.supportsTimestampQueries = false;
    return capabilities;
}

Base::Result<void> NullRhiBackend::CreateResource(
    ResourceHandle handle,
    const ResourceDescriptor& descriptor) noexcept {
    if (deviceLost_) {
        return InvalidState("Null RHI backend is lost");
    }
    if (!handle.IsValid() || handle.type != descriptor.type) {
        return InvalidArgument("Backend resource handle does not match descriptor");
    }
    for (const BackendResource& resource : resources_) {
        if (resource.handle == handle) {
            return Base::Status::Failure(Base::ErrorCode::AlreadyExists,
                "Backend resource handle already exists");
        }
    }
    BackendResource resource;
    resource.handle = handle;
    resource.descriptor = descriptor;
    return resources_.TryPushBack(resource);
}

void NullRhiBackend::DestroyResource(ResourceHandle handle) noexcept {
    for (std::uint32_t index = 0U; index < resources_.Size(); ++index) {
        if (resources_[index].handle == handle) {
            for (std::uint32_t current = index + 1U;
                 current < resources_.Size(); ++current) {
                resources_[current - 1U] = resources_[current];
            }
            resources_.PopBack();
            return;
        }
    }
}

Base::Result<void> NullRhiBackend::Submit(
    const CommandBuffer& commands,
    FenceValue signalFence) noexcept {
    if (deviceLost_) {
        return InvalidState("Null RHI backend is lost");
    }
    if (signalFence == 0U || signalFence <= lastSubmittedFence_) {
        return InvalidArgument("RHI fence values must increase monotonically");
    }

    std::uint32_t passDepth = 0U;
    std::uint32_t clipDepth = 0U;
    std::uint32_t opacityDepth = 0U;
    std::uint32_t transformDepth = 0U;
    for (const RhiCommand& command : commands.Commands()) {
        switch (command.kind) {
        case RhiCommandKind::BeginPass:
            if (passDepth != 0U || command.nodeId == Base::InvalidRenderNodeId ||
                !Base::IsValidRect(command.rect)) {
                return InvalidState("Invalid BeginPass command");
            }
            ++passDepth;
            break;
        case RhiCommandKind::EndPass:
            if (passDepth != 1U || clipDepth != 0U || opacityDepth != 0U ||
                transformDepth != 0U) {
                return InvalidState("EndPass encountered unbalanced render state");
            }
            --passDepth;
            break;
        case RhiCommandKind::PushClip:
            if (passDepth == 0U || !Base::IsValidRect(command.rect)) {
                return InvalidArgument("Invalid clip command");
            }
            ++clipDepth;
            break;
        case RhiCommandKind::PopClip:
            if (clipDepth == 0U) return InvalidState("Clip stack underflow");
            --clipDepth;
            break;
        case RhiCommandKind::PushOpacity:
            if (!Base::IsNormalizedOpacity(command.scalar)) {
                return InvalidArgument("Invalid opacity command");
            }
            ++opacityDepth;
            break;
        case RhiCommandKind::PopOpacity:
            if (opacityDepth == 0U) return InvalidState("Opacity stack underflow");
            --opacityDepth;
            break;
        case RhiCommandKind::PushTransform:
            if (!Base::IsFiniteTransform(command.transform)) {
                return InvalidArgument("Invalid transform command");
            }
            ++transformDepth;
            break;
        case RhiCommandKind::PopTransform:
            if (transformDepth == 0U) return InvalidState("Transform stack underflow");
            --transformDepth;
            break;
        case RhiCommandKind::DrawFilledRect:
            if (passDepth == 0U || !Base::IsValidRect(command.rect) ||
                !IsValidColor(command.color)) {
                return InvalidArgument("Invalid filled rectangle command");
            }
            break;
        case RhiCommandKind::DrawStrokedRect:
            if (passDepth == 0U || !Base::IsValidRect(command.rect) ||
                !IsValidColor(command.color) || !std::isfinite(command.scalar) ||
                command.scalar < 0.0) {
                return InvalidArgument("Invalid stroked rectangle command");
            }
            break;
        }
    }
    if (passDepth != 0U || clipDepth != 0U || opacityDepth != 0U ||
        transformDepth != 0U) {
        return InvalidState("Command buffer ended with unbalanced state");
    }

    lastSubmittedFence_ = signalFence;
    lastCommandHash_ = commands.StableHash();
    ++submissionCount_;
    return {};
}

void NullRhiBackend::CompleteThrough(FenceValue fence) noexcept {
    if (fence > completedFence_) {
        completedFence_ = fence <= lastSubmittedFence_ ? fence : lastSubmittedFence_;
    }
}

std::uint32_t NullRhiBackend::LiveBackendResourceCount() const noexcept {
    return resources_.Size();
}

} // namespace Aero::Rhi
