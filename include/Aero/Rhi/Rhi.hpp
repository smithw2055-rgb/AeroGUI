#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>

#include <cstdint>

namespace Aero::Rhi {

constexpr std::uint32_t RhiAbiVersion = 2U;
using FenceValue = std::uint64_t;

struct DeviceCapabilities final {
    std::uint32_t abiVersion = RhiAbiVersion;
    std::uint32_t maxFramesInFlight = 2U;
    std::uint32_t maxTextureDimension = 16384U;
    bool supportsTimestampQueries = false;
};

enum class ResourceType : std::uint8_t {
    Invalid = 0U,
    Buffer,
    Texture,
    Sampler,
    Pipeline,
    RenderTarget
};

struct ResourceHandle final {
    std::uint32_t index = UINT32_MAX;
    std::uint32_t generation = 0U;
    ResourceType type = ResourceType::Invalid;

    constexpr bool IsValid() const noexcept {
        return index != UINT32_MAX && generation != 0U &&
            type != ResourceType::Invalid;
    }
};

constexpr bool operator==(
    ResourceHandle left,
    ResourceHandle right) noexcept {
    return left.index == right.index &&
        left.generation == right.generation &&
        left.type == right.type;
}

constexpr bool operator!=(
    ResourceHandle left,
    ResourceHandle right) noexcept {
    return !(left == right);
}

enum class BufferUsage : std::uint8_t {
    Vertex = 0U,
    Index,
    Uniform,
    Upload
};

enum class TextureFormat : std::uint8_t {
    Rgba8Unorm = 0U,
    Bgra8Unorm,
    R8Unorm
};

struct BufferDescriptor final {
    std::uint64_t sizeBytes = 0U;
    BufferUsage usage = BufferUsage::Vertex;
};

struct TextureDescriptor final {
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    TextureFormat format = TextureFormat::Rgba8Unorm;
};

struct ResourceDescriptor final {
    ResourceType type = ResourceType::Invalid;
    BufferDescriptor buffer;
    TextureDescriptor texture;
};

class GraphicsCommandBuffer;
class IGraphicsBackend;
struct TextureResourceDescriptor;
struct SamplerDescriptor;
struct PipelineDescriptor;
struct ExternalRenderTargetDescriptor;

// The only polymorphic RHI boundary. It owns native resources, the global
// submission timeline, and device-loss state. UI/render concepts never enter it.
class AERO_API IRhiBackend {
public:
    virtual ~IRhiBackend() = default;
    virtual DeviceCapabilities Capabilities() const noexcept = 0;
    virtual Base::Result<void> CreateResource(
        ResourceHandle handle,
        const ResourceDescriptor& descriptor) noexcept = 0;
    virtual void DestroyResource(ResourceHandle handle) noexcept = 0;
    virtual FenceValue LastSubmittedFence() const noexcept = 0;
    virtual FenceValue CompletedFence() const noexcept = 0;
    virtual bool IsDeviceLost() const noexcept = 0;
};

struct DeviceSubmissionCapture final {
    FenceValue signalFence = 0U;
    std::uint32_t commandCount = 0U;
    std::uint32_t uploadByteCount = 0U;
    std::uint64_t commandHash = 0U;
};

// Public resource and submission facade. Callers no longer coordinate a
// factory and queue around the same backend; one Device owns handles, deferred
// destruction, validation, and the fence timeline.
class AERO_API RhiDevice final {
public:
    RhiDevice(
        IGraphicsBackend& backend,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~RhiDevice() noexcept;

    RhiDevice(const RhiDevice&) = delete;
    RhiDevice& operator=(const RhiDevice&) = delete;

    Base::Result<void> Initialize() noexcept;
    const DeviceCapabilities& Capabilities() const noexcept {
        return capabilities_;
    }

    Base::Result<ResourceHandle> CreateResource(
        const ResourceDescriptor& descriptor) noexcept;
    Base::Result<ResourceHandle> CreateBuffer(
        const BufferDescriptor& descriptor) noexcept;
    Base::Result<ResourceHandle> CreateTexture(
        const TextureResourceDescriptor& descriptor) noexcept;
    Base::Result<ResourceHandle> CreateRenderTarget(
        const TextureResourceDescriptor& descriptor) noexcept;
    Base::Result<ResourceHandle> CreateSampler(
        const SamplerDescriptor& descriptor) noexcept;
    Base::Result<ResourceHandle> CreatePipeline(
        const PipelineDescriptor& descriptor) noexcept;
    Base::Result<ResourceHandle> ImportRenderTarget(
        const ExternalRenderTargetDescriptor& descriptor) noexcept;

    Base::Result<void> DestroyResource(
        ResourceHandle handle,
        FenceValue retireAfter) noexcept;
    bool IsAlive(ResourceHandle handle) const noexcept;
    Base::Result<FenceValue> Submit(
        const GraphicsCommandBuffer& commands) noexcept;
    Base::Result<std::uint32_t> CollectGarbage() noexcept;

    FenceValue LastSubmittedFence() const noexcept {
        return lastSubmittedFence_;
    }
    const DeviceSubmissionCapture& LastCapture() const noexcept {
        return lastCapture_;
    }
    FenceValue CompletedFence() const noexcept;
    bool IsDeviceLost() const noexcept;
    std::uint32_t LiveResourceCount() const noexcept;
    std::uint32_t PendingDestroyCount() const noexcept {
        return deferred_.Size();
    }

private:
    struct ResourceSlot final {
        ResourceDescriptor descriptor;
        std::uint32_t generation = 1U;
        bool alive = false;
    };

    struct DeferredDestroy final {
        ResourceHandle handle;
        FenceValue retireAfter = 0U;
    };

    IGraphicsBackend* backend_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    DeviceCapabilities capabilities_;
    Base::Vector<ResourceSlot> slots_;
    Base::Vector<DeferredDestroy> deferred_;
    FenceValue lastSubmittedFence_ = 0U;
    DeviceSubmissionCapture lastCapture_;
    bool initialized_ = false;

    Base::Result<void> VerifyReady() const noexcept;
    Base::Result<void> ValidateDescriptor(
        const ResourceDescriptor& descriptor) const noexcept;
    Base::Result<ResourceHandle> CreateTextureInternal(
        const TextureResourceDescriptor& descriptor,
        ResourceType resourceType) noexcept;
    void Rollback(ResourceHandle handle) noexcept;
};

} // namespace Aero::Rhi
