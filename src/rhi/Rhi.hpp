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

// Forward-declared graphics contracts keep the device/resource lifetime layer
// independent from command encoding details while retaining one backend API.
enum class GraphicsBackendKind : std::uint8_t;
struct GraphicsCapabilities;
struct TextureResourceDescriptor;
struct SamplerDescriptor;
struct PipelineDescriptor;
class CommandList;

class AERO_API IGraphicsBackend {
public:
    virtual ~IGraphicsBackend() = default;

    virtual DeviceCapabilities Capabilities() const noexcept = 0;
    virtual GraphicsBackendKind Kind() const noexcept = 0;
    virtual GraphicsCapabilities
    QueryGraphicsCapabilities() const noexcept = 0;

    virtual Base::Result<void> CreateResource(
        ResourceHandle handle,
        const ResourceDescriptor& descriptor) noexcept = 0;
    virtual void DestroyResource(ResourceHandle handle) noexcept = 0;
    virtual Base::Result<void> ConfigureTexture(
        ResourceHandle handle,
        const TextureResourceDescriptor& descriptor) noexcept = 0;
    virtual Base::Result<void> ConfigureSampler(
        ResourceHandle handle,
        const SamplerDescriptor& descriptor) noexcept = 0;
    virtual Base::Result<void> ConfigurePipeline(
        ResourceHandle handle,
        const PipelineDescriptor& descriptor) noexcept = 0;

    // This is the only command submission path. Backends never consume
    // Render::RenderPlan or UI objects directly.
    virtual Base::Result<void> Submit(
        const CommandList& commands,
        FenceValue signalFence) noexcept = 0;
    virtual FenceValue LastSubmittedFence() const noexcept = 0;
    virtual FenceValue CompletedFence() const noexcept = 0;
    virtual bool IsDeviceLost() const noexcept = 0;
};

class AERO_API RhiDevice final {
public:
    explicit RhiDevice(
        IGraphicsBackend& backend,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~RhiDevice() noexcept;

    RhiDevice(const RhiDevice&) = delete;
    RhiDevice& operator=(const RhiDevice&) = delete;

    Base::Result<void> Initialize() noexcept;
    const DeviceCapabilities& Capabilities() const noexcept {
        return capabilities_;
    }
    IGraphicsBackend& Backend() const noexcept { return *backend_; }

    Base::Result<ResourceHandle> CreateBuffer(
        const BufferDescriptor& descriptor) noexcept;
    Base::Result<ResourceHandle> CreateTexture(
        const TextureResourceDescriptor& descriptor) noexcept;
    // Reserves a sampled-texture handle without creating native storage.
    // Backend-specific import APIs attach a host-owned texture afterwards.
    Base::Result<ResourceHandle> CreateExternalTexture(
        const TextureResourceDescriptor& descriptor) noexcept;
    Base::Result<ResourceHandle> CreateRenderTarget(
        const TextureResourceDescriptor& descriptor) noexcept;
    // Reserves a render-target handle and backend record without creating
    // native storage. Platform backends use this before importing an
    // externally owned swap-chain image or framebuffer.
    Base::Result<ResourceHandle> CreateExternalRenderTarget(
        const TextureResourceDescriptor& descriptor) noexcept;
    Base::Result<ResourceHandle> CreateSampler(
        const SamplerDescriptor& descriptor) noexcept;
    Base::Result<ResourceHandle> CreatePipeline(
        const PipelineDescriptor& descriptor) noexcept;

    Base::Result<void> DestroyResource(
        ResourceHandle handle,
        FenceValue retireAfter = 0U) noexcept;
    bool IsAlive(ResourceHandle handle) const noexcept;

    Base::Result<FenceValue> Submit(
        const CommandList& commands) noexcept;
    Base::Result<std::uint32_t> CollectGarbage() noexcept;

    FenceValue LastSubmittedFence() const noexcept {
        return lastSubmittedFence_;
    }
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
    bool initialized_ = false;

    Base::Result<void> VerifyReady() const noexcept;
    Base::Result<void> ValidateDescriptor(
        const ResourceDescriptor& descriptor) const noexcept;
    Base::Result<ResourceHandle> CreateResource(
        const ResourceDescriptor& descriptor) noexcept;
    void Rollback(ResourceHandle handle) noexcept;
};

} // namespace Aero::Rhi
