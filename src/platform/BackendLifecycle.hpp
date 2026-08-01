#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include "render/RenderDevice.hpp"

#include <cmath>
#include <cstdint>

namespace Aero::Graphics {

enum class BackendOwnership : std::uint8_t {
    Borrowed = 0U,
    Owned,
};

enum class BackendLifecycleState : std::uint8_t {
    Uninitialized = 0U,
    Ready,
    Suspended,
    SurfaceLost,
    DeviceLost,
    Destroyed,
};

struct BackendLifecycleStatistics final {
    std::uint64_t generation = 0U;
    std::uint64_t framesBegun = 0U;
    std::uint64_t framesPresented = 0U;
    std::uint64_t surfaceLosses = 0U;
    std::uint64_t deviceLosses = 0U;
    std::uint64_t restores = 0U;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
};

class BackendLifecycle final {
public:
    Base::Result<void> Initialize(
        BackendOwnership ownership,
        std::uint32_t width,
        std::uint32_t height) noexcept {
        if (state_ != BackendLifecycleState::Uninitialized) {
            return InvalidState("Backend lifecycle is already initialized");
        }
        if (width == 0U || height == 0U) {
            return InvalidArgument("Backend dimensions must be non-zero");
        }
        ownership_ = ownership;
        statistics_.width = width;
        statistics_.height = height;
        statistics_.generation = 1U;
        state_ = BackendLifecycleState::Ready;
        return {};
    }

    Base::Result<void> BeginFrame() noexcept {
        if (state_ != BackendLifecycleState::Ready || frameActive_) {
            return InvalidState("Backend frame cannot begin in the current state");
        }
        frameActive_ = true;
        ++statistics_.framesBegun;
        return {};
    }

    Base::Result<void> EndFrame(bool presented = true) noexcept {
        if (state_ != BackendLifecycleState::Ready || !frameActive_) {
            return InvalidState("Backend frame is not active");
        }
        frameActive_ = false;
        if (presented) ++statistics_.framesPresented;
        return {};
    }

    Base::Result<void> Resize(
        std::uint32_t width,
        std::uint32_t height) noexcept {
        if (state_ != BackendLifecycleState::Ready || frameActive_) {
            return InvalidState("Backend cannot resize in the current state");
        }
        if (width == 0U || height == 0U) {
            return InvalidArgument("Backend dimensions must be non-zero");
        }
        statistics_.width = width;
        statistics_.height = height;
        return AdvanceGeneration();
    }

    Base::Result<void> Suspend() noexcept {
        if (state_ != BackendLifecycleState::Ready || frameActive_) {
            return InvalidState("Backend cannot suspend in the current state");
        }
        state_ = BackendLifecycleState::Suspended;
        return {};
    }

    Base::Result<void> Resume() noexcept {
        if (state_ != BackendLifecycleState::Suspended) {
            return InvalidState("Backend is not suspended");
        }
        state_ = BackendLifecycleState::Ready;
        return AdvanceGeneration();
    }

    Base::Result<void> NotifySurfaceLost() noexcept {
        if (state_ == BackendLifecycleState::SurfaceLost) return {};
        if (state_ != BackendLifecycleState::Ready &&
            state_ != BackendLifecycleState::Suspended) {
            return InvalidState("Backend surface cannot be lost in the current state");
        }
        frameActive_ = false;
        state_ = BackendLifecycleState::SurfaceLost;
        ++statistics_.surfaceLosses;
        return AdvanceGeneration();
    }

    Base::Result<void> RestoreSurface(
        std::uint32_t width,
        std::uint32_t height) noexcept {
        if (state_ != BackendLifecycleState::SurfaceLost) {
            return InvalidState("Backend surface is not lost");
        }
        if (width == 0U || height == 0U) {
            return InvalidArgument("Restored surface dimensions must be non-zero");
        }
        statistics_.width = width;
        statistics_.height = height;
        state_ = BackendLifecycleState::Ready;
        ++statistics_.restores;
        return AdvanceGeneration();
    }

    Base::Result<void> NotifyDeviceLost() noexcept {
        if (state_ == BackendLifecycleState::DeviceLost) return {};
        if (state_ == BackendLifecycleState::Destroyed ||
            state_ == BackendLifecycleState::Uninitialized) {
            return InvalidState("Backend device cannot be lost in the current state");
        }
        frameActive_ = false;
        state_ = BackendLifecycleState::DeviceLost;
        ++statistics_.deviceLosses;
        return AdvanceGeneration();
    }

    Base::Result<void> RestoreDevice() noexcept {
        if (state_ != BackendLifecycleState::DeviceLost) {
            return InvalidState("Backend device is not lost");
        }
        state_ = BackendLifecycleState::Ready;
        ++statistics_.restores;
        return AdvanceGeneration();
    }

    void Shutdown() noexcept {
        frameActive_ = false;
        state_ = BackendLifecycleState::Destroyed;
    }

    BackendLifecycleState State() const noexcept { return state_; }
    BackendOwnership Ownership() const noexcept { return ownership_; }
    bool FrameActive() const noexcept { return frameActive_; }
    BackendLifecycleStatistics Statistics() const noexcept {
        return statistics_;
    }

private:
    BackendLifecycleState state_ = BackendLifecycleState::Uninitialized;
    BackendOwnership ownership_ = BackendOwnership::Borrowed;
    BackendLifecycleStatistics statistics_;
    bool frameActive_ = false;

    Base::Result<void> AdvanceGeneration() noexcept {
        if (statistics_.generation == UINT64_MAX) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Backend generation space is exhausted");
        }
        ++statistics_.generation;
        return {};
    }
    static Base::Status InvalidArgument(const char* message) noexcept {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument, message);
    }
    static Base::Status InvalidState(const char* message) noexcept {
        return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
    }
};

struct Gles30Contract final {
    std::uint8_t majorVersion = 3U;
    std::uint8_t minorVersion = 0U;
    std::uint32_t maxSampledTextures = 8U;
    bool supportsVertexArrayObjects = true;
    bool supportsUniformBuffers = true;
    bool assumesComputeShaders = false;
    bool assumesShaderStorageBuffers = false;

    Base::Result<void> Validate() const noexcept {
        if (majorVersion != 3U || minorVersion != 0U ||
            maxSampledTextures < 8U ||
            !supportsVertexArrayObjects ||
            !supportsUniformBuffers ||
            assumesComputeShaders ||
            assumesShaderStorageBuffers) {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "GLES contract requires an ES 3.0-only baseline");
        }
        return {};
    }
};

class AndroidLifecycleHost final {
public:
    Base::Result<void> Create(
        std::uint32_t width,
        std::uint32_t height,
        BackendOwnership ownership = BackendOwnership::Owned) noexcept {
        Base::Result<void> valid = gles_.Validate();
        if (!valid) return valid.GetStatus();
        return lifecycle_.Initialize(ownership, width, height);
    }
    Base::Result<void> Pause() noexcept { return lifecycle_.Suspend(); }
    Base::Result<void> Resume() noexcept { return lifecycle_.Resume(); }
    Base::Result<void> LoseSurface() noexcept {
        return lifecycle_.NotifySurfaceLost();
    }
    Base::Result<void> RestoreSurface(
        std::uint32_t width,
        std::uint32_t height) noexcept {
        return lifecycle_.RestoreSurface(width, height);
    }
    BackendLifecycle& Lifecycle() noexcept { return lifecycle_; }
    const Gles30Contract& GlesContract() const noexcept { return gles_; }
private:
    Gles30Contract gles_;
    BackendLifecycle lifecycle_;
};

enum class PortableResourceState : std::uint8_t {
    Undefined = 0U,
    CopySource,
    CopyDestination,
    ShaderRead,
    RenderTarget,
    Present,
};

struct PortableResourceStateRecord final {
    std::uint64_t resource = 0U;
    PortableResourceState state = PortableResourceState::Undefined;
};

class PortableResourceStateTracker final {
public:
    explicit PortableResourceStateTracker(
        Base::IAllocator* allocator = nullptr) noexcept
        : states_(allocator) {}

    Base::Result<void> Register(std::uint64_t resource) noexcept {
        if (resource == 0U || Find(resource) != nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Resource state registration is invalid");
        }
        return states_.TryPushBack({resource, PortableResourceState::Undefined});
    }
    Base::Result<void> Transition(
        std::uint64_t resource,
        PortableResourceState before,
        PortableResourceState after) noexcept {
        PortableResourceStateRecord* record = Find(resource);
        if (record == nullptr || record->state != before || before == after) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Resource transition does not match tracked state");
        }
        record->state = after;
        return {};
    }
    PortableResourceState State(std::uint64_t resource) const noexcept {
        const PortableResourceStateRecord* record = Find(resource);
        return record != nullptr ? record->state : PortableResourceState::Undefined;
    }
    bool Remove(std::uint64_t resource) noexcept {
        for (std::uint32_t index = 0U; index < states_.Size(); ++index) {
            if (states_[index].resource != resource) continue;
            for (std::uint32_t next = index + 1U; next < states_.Size(); ++next) {
                states_[next - 1U] = states_[next];
            }
            states_.PopBack();
            return true;
        }
        return false;
    }
private:
    Base::Vector<PortableResourceStateRecord> states_;
    PortableResourceStateRecord* Find(std::uint64_t resource) noexcept {
        for (PortableResourceStateRecord& state : states_) {
            if (state.resource == resource) return &state;
        }
        return nullptr;
    }
    const PortableResourceStateRecord* Find(std::uint64_t resource) const noexcept {
        for (const PortableResourceStateRecord& state : states_) {
            if (state.resource == resource) return &state;
        }
        return nullptr;
    }
};

struct FenceRetirementRecord final {
    std::uint64_t resource = 0U;
    FenceValue retireAfter = 0U;
};

class FenceRetirementQueue final {
public:
    explicit FenceRetirementQueue(
        Base::IAllocator* allocator = nullptr) noexcept
        : records_(allocator) {}

    Base::Result<void> Retire(
        std::uint64_t resource,
        FenceValue fence) noexcept {
        if (resource == 0U || fence == 0U) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Fence retirement requires a resource and non-zero fence");
        }
        return records_.TryPushBack({resource, fence});
    }
    std::uint32_t Collect(
        FenceValue completed,
        Base::Vector<std::uint64_t>* released = nullptr) noexcept {
        std::uint32_t count = 0U;
        for (std::uint32_t index = 0U; index < records_.Size();) {
            if (records_[index].retireAfter > completed) {
                ++index;
                continue;
            }
            if (released != nullptr) {
                Base::Result<void> appended = released->TryPushBack(records_[index].resource);
                if (!appended) break;
            }
            for (std::uint32_t next = index + 1U; next < records_.Size(); ++next) {
                records_[next - 1U] = records_[next];
            }
            records_.PopBack();
            ++count;
        }
        return count;
    }
    std::uint32_t Pending() const noexcept { return records_.Size(); }
private:
    Base::Vector<FenceRetirementRecord> records_;
};

class VulkanBackendContract final {
public:
    explicit VulkanBackendContract(
        Base::IAllocator* allocator = nullptr) noexcept
        : states_(allocator), retirement_(allocator) {}
    Base::Result<void> Initialize(
        BackendOwnership ownership,
        std::uint32_t width,
        std::uint32_t height) noexcept {
        return lifecycle_.Initialize(ownership, width, height);
    }
    Base::Result<FenceValue> Submit() noexcept {
        if (lifecycle_.State() != BackendLifecycleState::Ready) {
            return Base::Status::Failure(Base::ErrorCode::InvalidState,
                "Vulkan contract is not ready");
        }
        if (submitted_ == UINT64_MAX) {
            return Base::Status::Failure(Base::ErrorCode::OutOfRange,
                "Vulkan fence space is exhausted");
        }
        return ++submitted_;
    }
    void Complete(FenceValue fence) noexcept {
        if (fence > completed_ && fence <= submitted_) completed_ = fence;
    }
    std::uint32_t Collect() noexcept { return retirement_.Collect(completed_); }
    BackendLifecycle& Lifecycle() noexcept { return lifecycle_; }
    PortableResourceStateTracker& States() noexcept { return states_; }
    FenceRetirementQueue& Retirement() noexcept { return retirement_; }
    FenceValue SubmittedFence() const noexcept { return submitted_; }
    FenceValue CompletedFence() const noexcept { return completed_; }
private:
    BackendLifecycle lifecycle_;
    PortableResourceStateTracker states_;
    FenceRetirementQueue retirement_;
    FenceValue submitted_ = 0U;
    FenceValue completed_ = 0U;
};

class D3D12BackendContract final {
public:
    Base::Result<void> Initialize(
        BackendOwnership ownership,
        std::uint32_t descriptorCapacity,
        std::uint32_t width,
        std::uint32_t height) noexcept {
        if (descriptorCapacity == 0U) {
            return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
                "D3D12 descriptor capacity must be non-zero");
        }
        descriptorCapacity_ = descriptorCapacity;
        return lifecycle_.Initialize(ownership, width, height);
    }
    Base::Result<std::uint32_t> AllocateDescriptors(
        std::uint32_t count) noexcept {
        if (count == 0U || count > descriptorCapacity_ - descriptorUsed_) {
            return Base::Status::Failure(Base::ErrorCode::OutOfRange,
                "D3D12 descriptor heap is exhausted");
        }
        const std::uint32_t first = descriptorUsed_;
        descriptorUsed_ += count;
        return first;
    }
    void ResetDescriptorHeap() noexcept { descriptorUsed_ = 0U; }
    BackendLifecycle& Lifecycle() noexcept { return lifecycle_; }
    std::uint32_t DescriptorUsed() const noexcept { return descriptorUsed_; }
private:
    BackendLifecycle lifecycle_;
    std::uint32_t descriptorCapacity_ = 0U;
    std::uint32_t descriptorUsed_ = 0U;
};

class MetalBackendContract final {
public:
    Base::Result<void> Initialize(
        BackendOwnership ownership,
        std::uint32_t width,
        std::uint32_t height) noexcept {
        drawableAvailable_ = true;
        return lifecycle_.Initialize(ownership, width, height);
    }
    Base::Result<void> SetBackgrounded(bool value) noexcept {
        if (value == backgrounded_) return {};
        backgrounded_ = value;
        drawableAvailable_ = !value;
        return value ? lifecycle_.Suspend() : lifecycle_.Resume();
    }
    void SetDrawableAvailable(bool value) noexcept { drawableAvailable_ = value; }
    Base::Result<void> BeginCommandBuffer() noexcept {
        if (backgrounded_ || !drawableAvailable_) {
            return Base::Status::Failure(Base::ErrorCode::InvalidState,
                "Metal drawable is unavailable");
        }
        return lifecycle_.BeginFrame();
    }
    Base::Result<void> CommitCommandBuffer() noexcept {
        return lifecycle_.EndFrame(true);
    }
    BackendLifecycle& Lifecycle() noexcept { return lifecycle_; }
private:
    BackendLifecycle lifecycle_;
    bool backgrounded_ = false;
    bool drawableAvailable_ = false;
};

class WebGl2RuntimeContract final {
public:
    Base::Result<void> Initialize(
        std::uint8_t webGlVersion,
        double devicePixelRatio,
        std::uint32_t cssWidth,
        std::uint32_t cssHeight) noexcept {
        if (webGlVersion != 2U) {
            return Base::Status::Failure(Base::ErrorCode::Unsupported,
                "WebGL 2 is required; WebGL 1 fallback is forbidden");
        }
        Base::Result<void> sized = Resize(devicePixelRatio, cssWidth, cssHeight, false);
        if (!sized) return sized.GetStatus();
        return lifecycle_.Initialize(
            BackendOwnership::Owned, pixelWidth_, pixelHeight_);
    }
    Base::Result<void> Resize(
        double devicePixelRatio,
        std::uint32_t cssWidth,
        std::uint32_t cssHeight,
        bool updateLifecycle = true) noexcept {
        if (!std::isfinite(devicePixelRatio) || devicePixelRatio <= 0.0 ||
            cssWidth == 0U || cssHeight == 0U) {
            return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
                "Web canvas geometry or devicePixelRatio is invalid");
        }
        const double pixelWidth = static_cast<double>(cssWidth) * devicePixelRatio;
        const double pixelHeight = static_cast<double>(cssHeight) * devicePixelRatio;
        if (pixelWidth > static_cast<double>(UINT32_MAX) ||
            pixelHeight > static_cast<double>(UINT32_MAX)) {
            return Base::Status::Failure(Base::ErrorCode::OutOfRange,
                "Web canvas dimensions exceed the runtime range");
        }
        dpr_ = devicePixelRatio;
        cssWidth_ = cssWidth;
        cssHeight_ = cssHeight;
        pixelWidth_ = static_cast<std::uint32_t>(pixelWidth + 0.5);
        pixelHeight_ = static_cast<std::uint32_t>(pixelHeight + 0.5);
        return updateLifecycle
            ? lifecycle_.Resize(pixelWidth_, pixelHeight_)
            : Base::Result<void>();
    }
    Base::Result<std::uint64_t> RequestAnimationFrame() noexcept {
        if (lifecycle_.State() != BackendLifecycleState::Ready || rafPending_) {
            return Base::Status::Failure(Base::ErrorCode::InvalidState,
                "requestAnimationFrame is already pending or context is unavailable");
        }
        if (nextRaf_ == UINT64_MAX) {
            return Base::Status::Failure(Base::ErrorCode::OutOfRange,
                "requestAnimationFrame serial space is exhausted");
        }
        rafPending_ = true;
        return nextRaf_++;
    }
    Base::Result<void> CompleteAnimationFrame(std::uint64_t serial) noexcept {
        if (!rafPending_ || serial + 1U != nextRaf_) {
            return Base::Status::Failure(Base::ErrorCode::InvalidState,
                "requestAnimationFrame serial is stale");
        }
        rafPending_ = false;
        return {};
    }
    Base::Result<void> LoseContext() noexcept {
        rafPending_ = false;
        Base::Result<void> lost = lifecycle_.NotifySurfaceLost();
        if (lost) ++resourceGeneration_;
        return lost;
    }
    Base::Result<void> RestoreContext() noexcept {
        Base::Result<void> restored = lifecycle_.RestoreSurface(pixelWidth_, pixelHeight_);
        if (restored) ++resourceGeneration_;
        return restored;
    }
    BackendLifecycle& Lifecycle() noexcept { return lifecycle_; }
    std::uint64_t ResourceGeneration() const noexcept { return resourceGeneration_; }
    std::uint32_t PixelWidth() const noexcept { return pixelWidth_; }
    std::uint32_t PixelHeight() const noexcept { return pixelHeight_; }
    double DevicePixelRatio() const noexcept { return dpr_; }
private:
    BackendLifecycle lifecycle_;
    double dpr_ = 1.0;
    std::uint32_t cssWidth_ = 0U;
    std::uint32_t cssHeight_ = 0U;
    std::uint32_t pixelWidth_ = 0U;
    std::uint32_t pixelHeight_ = 0U;
    std::uint64_t nextRaf_ = 1U;
    std::uint64_t resourceGeneration_ = 1U;
    bool rafPending_ = false;
};

} // namespace Aero::Graphics
