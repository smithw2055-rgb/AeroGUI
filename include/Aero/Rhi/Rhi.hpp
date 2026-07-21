#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Rendering.hpp>

#include <cstddef>
#include <cstdint>

namespace Aero::Rhi {

constexpr std::uint32_t RhiAbiVersion = 1U;
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

    AERO_NODISCARD constexpr bool IsValid() const noexcept {
        return index != UINT32_MAX && generation != 0U &&
            type != ResourceType::Invalid;
    }
};

AERO_NODISCARD constexpr bool operator==(
    ResourceHandle left,
    ResourceHandle right) noexcept {
    return left.index == right.index &&
        left.generation == right.generation &&
        left.type == right.type;
}

AERO_NODISCARD constexpr bool operator!=(
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

enum class RhiCommandKind : std::uint8_t {
    BeginPass = 0U,
    EndPass,
    PushClip,
    PopClip,
    PushOpacity,
    PopOpacity,
    PushTransform,
    PopTransform,
    DrawFilledRect,
    DrawStrokedRect
};

struct RhiCommand final {
    RhiCommandKind kind = RhiCommandKind::DrawFilledRect;
    Core::Rect rect;
    Core::Transform2D transform;
    Core::Color color;
    double scalar = 0.0;
    Core::RenderNodeId nodeId = Core::InvalidRenderNodeId;
};

class AERO_API CommandBuffer final {
public:
    explicit CommandBuffer(Base::IAllocator* allocator = nullptr) noexcept
        : commands_(allocator) {}

    AERO_NODISCARD Base::Span<const RhiCommand> Commands() const noexcept {
        return {commands_.Data(), commands_.Size()};
    }
    AERO_NODISCARD std::uint32_t CommandCount() const noexcept {
        return commands_.Size();
    }
    AERO_NODISCARD std::uint64_t StableHash() const noexcept;

private:
    friend class RenderPlanTranslator;
    Base::Vector<RhiCommand> commands_;
};

class AERO_API RenderPlanTranslator final {
public:
    explicit RenderPlanTranslator(
        Base::IAllocator* allocator = nullptr) noexcept
        : allocator_(allocator) {}

    AERO_NODISCARD Base::Result<CommandBuffer> Translate(
        const Core::RenderPlan& plan) const noexcept;

private:
    Base::IAllocator* allocator_ = nullptr;
};

struct UploadSlice final {
    std::uint32_t offset = 0U;
    std::uint32_t size = 0U;
    void* data = nullptr;
};

class AERO_API UploadArena final {
public:
    explicit UploadArena(
        std::uint32_t capacityBytes,
        Base::IAllocator* allocator = nullptr) noexcept;

    AERO_NODISCARD Base::Result<void> Initialize() noexcept;
    void Reset() noexcept { offset_ = 0U; }
    AERO_NODISCARD Base::Result<UploadSlice> Allocate(
        std::uint32_t size,
        std::uint32_t alignment) noexcept;
    AERO_NODISCARD std::uint32_t Capacity() const noexcept { return capacity_; }
    AERO_NODISCARD std::uint32_t Used() const noexcept { return offset_; }

private:
    Base::Vector<std::uint8_t> bytes_;
    std::uint32_t capacity_ = 0U;
    std::uint32_t offset_ = 0U;
    bool initialized_ = false;
};

struct FrameContext final {
    std::uint32_t frameIndex = 0U;
    FenceValue signalFence = 0U;
    UploadArena* uploadArena = nullptr;
};

class AERO_API IRhiBackend {
public:
    virtual ~IRhiBackend() = default;
    AERO_NODISCARD virtual DeviceCapabilities Capabilities() const noexcept = 0;
    AERO_NODISCARD virtual Base::Result<void> CreateResource(
        ResourceHandle handle,
        const ResourceDescriptor& descriptor) noexcept = 0;
    virtual void DestroyResource(ResourceHandle handle) noexcept = 0;
    AERO_NODISCARD virtual Base::Result<void> Submit(
        const CommandBuffer& commands,
        FenceValue signalFence) noexcept = 0;
    AERO_NODISCARD virtual FenceValue CompletedFence() const noexcept = 0;
    AERO_NODISCARD virtual bool IsDeviceLost() const noexcept = 0;
};

class AERO_API RhiDevice final {
public:
    RhiDevice(
        IRhiBackend& backend,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~RhiDevice() noexcept;

    RhiDevice(const RhiDevice&) = delete;
    RhiDevice& operator=(const RhiDevice&) = delete;

    AERO_NODISCARD Base::Result<void> Initialize() noexcept;
    AERO_NODISCARD const DeviceCapabilities& Capabilities() const noexcept {
        return capabilities_;
    }

    AERO_NODISCARD Base::Result<ResourceHandle> CreateResource(
        const ResourceDescriptor& descriptor) noexcept;
    AERO_NODISCARD Base::Result<void> DestroyResource(
        ResourceHandle handle,
        FenceValue retireAfter) noexcept;
    AERO_NODISCARD bool IsAlive(ResourceHandle handle) const noexcept;

    AERO_NODISCARD Base::Result<FrameContext> BeginFrame() noexcept;
    AERO_NODISCARD Base::Result<FenceValue> Submit(
        FrameContext& frame,
        const CommandBuffer& commands) noexcept;
    AERO_NODISCARD Base::Result<std::uint32_t> CollectGarbage() noexcept;

    AERO_NODISCARD FenceValue LastSubmittedFence() const noexcept {
        return lastSubmittedFence_;
    }
    AERO_NODISCARD std::uint32_t LiveResourceCount() const noexcept;
    AERO_NODISCARD std::uint32_t PendingDestroyCount() const noexcept {
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

    IRhiBackend* backend_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    DeviceCapabilities capabilities_;
    Base::Vector<ResourceSlot> slots_;
    Base::Vector<DeferredDestroy> deferred_;
    Base::Vector<UploadArena*> frameArenas_;
    FenceValue lastSubmittedFence_ = 0U;
    std::uint32_t nextFrameIndex_ = 0U;
    bool initialized_ = false;

    AERO_NODISCARD Base::Result<void> VerifyReady() const noexcept;
    AERO_NODISCARD Base::Result<void> ValidateDescriptor(
        const ResourceDescriptor& descriptor) const noexcept;
    void ReleaseFrameArenas() noexcept;
};

class AERO_API NullRhiBackend final : public IRhiBackend {
public:
    explicit NullRhiBackend(
        Base::IAllocator* allocator = nullptr) noexcept
        : resources_(allocator) {}

    AERO_NODISCARD DeviceCapabilities Capabilities() const noexcept override;
    AERO_NODISCARD Base::Result<void> CreateResource(
        ResourceHandle handle,
        const ResourceDescriptor& descriptor) noexcept override;
    void DestroyResource(ResourceHandle handle) noexcept override;
    AERO_NODISCARD Base::Result<void> Submit(
        const CommandBuffer& commands,
        FenceValue signalFence) noexcept override;
    AERO_NODISCARD FenceValue CompletedFence() const noexcept override {
        return completedFence_;
    }
    AERO_NODISCARD bool IsDeviceLost() const noexcept override {
        return deviceLost_;
    }

    void CompleteThrough(FenceValue fence) noexcept;
    void SimulateDeviceLoss() noexcept { deviceLost_ = true; }

    AERO_NODISCARD std::uint32_t SubmissionCount() const noexcept {
        return submissionCount_;
    }
    AERO_NODISCARD std::uint64_t LastCommandHash() const noexcept {
        return lastCommandHash_;
    }
    AERO_NODISCARD std::uint32_t LiveBackendResourceCount() const noexcept;

private:
    struct BackendResource final {
        ResourceHandle handle;
        ResourceDescriptor descriptor;
    };

    Base::Vector<BackendResource> resources_;
    FenceValue completedFence_ = 0U;
    FenceValue lastSubmittedFence_ = 0U;
    std::uint64_t lastCommandHash_ = 0U;
    std::uint32_t submissionCount_ = 0U;
    bool deviceLost_ = false;
};

} // namespace Aero::Rhi
