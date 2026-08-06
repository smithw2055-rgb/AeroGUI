#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>

#include <cstdint>

namespace Aero::Integration { class RenderFrame; }

namespace Aero {

class IRenderer;
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

// Host-thread-affine UI render device shared by one or more View renderers on
// the same render thread. Native graphics resources and surfaces remain hidden
// behind Integration factories and the private Graphics layer.
class AERO_API RenderDevice final : public Base::Object {
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

    RenderDeviceMode Mode() const noexcept;
    RenderDeviceState State() const noexcept;
    std::uint64_t Generation() const noexcept;
    RenderDeviceStatistics Statistics() const noexcept;
    RenderFrameStatistics LastFrameStatistics() const noexcept;

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
    friend class IRenderer;
    template<class T, class... Args>
    friend Base::Result<Base::Ref<T>>
    Base::MakeRefWithAllocator(
        Base::IAllocator&,
        Args&&...) noexcept;

    Base::Result<void> RenderOffscreen(
        const void* rendererToken,
        const Integration::RenderFrame& frame) noexcept;
    Base::Result<void> Render(
        const void* rendererToken,
        const Integration::RenderFrame& frame) noexcept;
    void ReleaseRenderer(const void* rendererToken) noexcept;
    Base::Status GetFrameStatus() noexcept;
    Base::Result<RenderFrameStatistics> Analyze(
        const Integration::RenderFrame& frame) noexcept;
    void MergeBackendStatistics(
        RenderFrameStatistics& result) const noexcept;

    Impl* impl_ = nullptr;
};

} // namespace Aero
