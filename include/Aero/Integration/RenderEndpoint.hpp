#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>

#include <cstdint>

namespace Aero::Integration {

enum class RenderEndpointMode : std::uint8_t {
    Headless = 0U,
    Embedded,
    Window
};

enum class RenderSubmissionMode : std::uint8_t {
    Immediate = 0U,
    DedicatedThread
};

enum class RenderPresentMode : std::uint8_t {
    Immediate = 0U,
    Fifo,
    Mailbox
};

enum class RenderEndpointState : std::uint8_t {
    Ready = 0U,
    SurfaceLost,
    DeviceLost,
    Failed,
    Shutdown
};

struct RenderEndpointStatistics final {
    std::uint64_t acceptedFrameCount = 0U;
    std::uint64_t completedFrameCount = 0U;
    std::uint64_t coalescedFrameCount = 0U;
    std::uint64_t failedFrameCount = 0U;
    std::uint64_t lastAcceptedVersion = 0U;
    std::uint64_t lastCompletedVersion = 0U;
    std::uint32_t pendingFrameCount = 0U;
    std::uint32_t highWatermark = 0U;
    std::uint64_t generation = 1U;
};

struct RenderFrameStatistics final {
    std::uint32_t sourceCommandCount = 0U;
    std::uint32_t drawPacketCount = 0U;
    std::uint32_t batchCount = 0U;
    std::uint32_t drawCallCount = 0U;
    std::uint32_t mergedPacketCount = 0U;
    std::uint32_t barrierCount = 0U;
    std::uint32_t instanceCount = 0U;
    std::uint32_t stateBindingCount = 0U;
    bool batchingEnabled = true;
};

namespace Detail {
class RenderEndpointAccess;
}

// A RenderEndpoint is the only public rendering attachment owned by a View.
// Backend devices, surfaces, command streams and resource caches are private
// implementation details behind this reference-counted object.
class AERO_API RenderEndpoint final : public Base::Object {
    struct ConstructionToken final {};

public:
    // Factory-only construction. The token is private; the declaration stays
    // public for Base::MakeRefWithAllocator's nothrow construction trait.
    RenderEndpoint(
        ConstructionToken,
        RenderEndpointMode mode,
        RenderSubmissionMode submissionMode,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~RenderEndpoint() noexcept override;

    RenderEndpoint(const RenderEndpoint&) = delete;
    RenderEndpoint& operator=(const RenderEndpoint&) = delete;

    RenderEndpointMode Mode() const noexcept;
    RenderSubmissionMode SubmissionMode() const noexcept;
    RenderEndpointState State() const noexcept;
    std::uint64_t Generation() const noexcept;
    RenderEndpointStatistics Statistics() const noexcept;
    RenderFrameStatistics
        LastFrameStatistics() const noexcept;

    Base::Result<void> Resize(
        std::uint32_t width,
        std::uint32_t height) noexcept;
    Base::Result<void> NotifySurfaceLost() noexcept;
    Base::Result<void> NotifyDeviceLost() noexcept;
    Base::Result<void> Restore() noexcept;
    Base::Result<void> WaitIdle(
        std::uint32_t timeoutMilliseconds = 5000U) noexcept;
    // Test-only A/B gate. Production endpoints start with batching enabled.
    Base::Result<void> SetBatchingEnabledForTesting(
        bool enabled) noexcept;

private:
    friend class Detail::RenderEndpointAccess;
    template<class T, class... Args>
    friend Base::Result<Base::Ref<T>>
    Base::MakeRefWithAllocator(
        Base::IAllocator&,
        Args&&...) noexcept;

    struct Impl;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

} // namespace Aero::Integration
