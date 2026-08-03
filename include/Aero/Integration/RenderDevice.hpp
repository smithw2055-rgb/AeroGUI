#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>

#include <cstdint>

namespace Aero::Integration { class RenderFrame; }

namespace Aero::Integration {

class RenderDevice;

enum class RenderDeviceMode : std::uint8_t {
    Headless = 0U,
    Embedded,
    Window
};

enum class RenderPresentMode : std::uint8_t {
    Immediate = 0U,
    Fifo,
    Mailbox
};

enum class RenderDeviceState : std::uint8_t {
    Ready = 0U,
    SurfaceLost,
    DeviceLost,
    Failed,
    Shutdown
};

struct RenderDeviceStatistics {
    std::uint64_t acceptedFrameCount = 0U;
    std::uint64_t completedFrameCount = 0U;
    std::uint64_t failedFrameCount = 0U;
    std::uint64_t lastAcceptedVersion = 0U;
    std::uint64_t lastCompletedVersion = 0U;
    std::uint64_t generation = 1U;
};

struct RenderFrameStatistics {
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

// Host-thread-affine native UI device shared by one or more View Renderers on
// the same render thread. Frame submission remains a private Renderer detail.
class AERO_API RenderDevice : public Base::Object {
    struct ConstructionToken {};

public:
    struct Impl;

    RenderDevice(
        ConstructionToken,
        RenderDeviceMode mode,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~RenderDevice() noexcept override;

    RenderDevice(const RenderDevice&) = delete;
    RenderDevice& operator=(const RenderDevice&) = delete;

    RenderDeviceMode Mode() const noexcept { return mode_; }
    RenderDeviceState State() const noexcept { return state_; }
    std::uint64_t Generation() const noexcept {
        return statistics_.generation;
    }
    RenderDeviceStatistics Statistics() const noexcept {
        return statistics_;
    }
    RenderFrameStatistics LastFrameStatistics() const noexcept {
        return lastFrameStatistics_;
    }

    Base::Result<void> Resize(
        std::uint32_t width,
        std::uint32_t height) noexcept;
    void NotifySurfaceLost() noexcept;
    void NotifyDeviceLost() noexcept;
    Base::Result<void> Restore() noexcept;
    Base::Result<void> WaitIdle(
        std::uint32_t timeoutMilliseconds = 5000U) noexcept;

private:
    friend struct Impl;
    template<class T, class... Args>
    friend Base::Result<Base::Ref<T>>
    Base::MakeRefWithAllocator(
        Base::IAllocator&,
        Args&&...) noexcept;

    Base::Result<void> RenderOffscreen(
        const void* rendererToken,
        const RenderFrame& frame) noexcept;
    Base::Result<void> Render(
        const void* rendererToken,
        const RenderFrame& frame) noexcept;
    void ReleaseRenderer(const void* rendererToken) noexcept;
    Base::Status GetFrameStatus() noexcept;
    Base::Result<RenderFrameStatistics> Analyze(
        const RenderFrame& frame) noexcept;
    void MergeBackendStatistics(
        RenderFrameStatistics& result) const noexcept;

    Base::IAllocator* allocator_ = nullptr;
    void* stateData_ = nullptr;
    const void* functions_ = nullptr;
    RenderDeviceMode mode_ = RenderDeviceMode::Headless;
    RenderDeviceState state_ = RenderDeviceState::Ready;
    RenderDeviceStatistics statistics_;
    RenderFrameStatistics lastFrameStatistics_;
};

} // namespace Aero::Integration
