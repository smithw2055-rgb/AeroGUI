#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>

#include <cstdint>

namespace Aero::Render::Detail { class RenderFrame; }

namespace Aero {

class IRenderer;
class RenderDevice;

namespace Diagnostics {
struct RenderDeviceStatistics;
struct RenderFrameStatistics;
AERO_API RenderDeviceStatistics GetRenderDeviceStatistics(
    const Aero::RenderDevice& device) noexcept;
AERO_API RenderFrameStatistics GetLastRenderFrameStatistics(
    const Aero::RenderDevice& device) noexcept;
}

enum class RenderDeviceState : std::uint8_t {
    Ready = 0U,
    DeviceLost,
    Failed,
    Shutdown
};

// Host-thread-affine UI render device shared by one or more View renderers on
// the same render thread. Diagnostics are opt-in under <Aero/Diagnostics/>.
class AERO_API RenderDevice final : public Base::Object {
    struct ConstructionToken {};

public:
    struct Impl;

    RenderDevice(
        ConstructionToken,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~RenderDevice() noexcept override;

    RenderDevice(const RenderDevice&) = delete;
    RenderDevice& operator=(const RenderDevice&) = delete;

    RenderDeviceState State() const noexcept;
    std::uint64_t Generation() const noexcept;

    void NotifyDeviceLost() noexcept;
    Base::Result<void> Restore() noexcept;
    Base::Result<void> WaitIdle(
        std::uint32_t timeoutMilliseconds = 5000U) noexcept;

private:
    friend struct Impl;
    friend Diagnostics::RenderDeviceStatistics
        Diagnostics::GetRenderDeviceStatistics(
            const Aero::RenderDevice& device) noexcept;
    friend Diagnostics::RenderFrameStatistics
        Diagnostics::GetLastRenderFrameStatistics(
            const Aero::RenderDevice& device) noexcept;
    template<class T, class... Args>
    friend Base::Result<Base::Ref<T>>
    Base::MakeRefWithAllocator(
        Base::IAllocator&,
        Args&&...) noexcept;

    Base::Result<void> RenderOffscreen(
        const void* rendererToken,
        const ::Aero::Render::Detail::RenderFrame& frame) noexcept;
    void ReleaseRenderer(const void* rendererToken) noexcept;
    Base::Status GetFrameStatus() noexcept;
    Base::Result<Diagnostics::RenderFrameStatistics> Analyze(
        const ::Aero::Render::Detail::RenderFrame& frame) noexcept;
    void MergeBackendStatistics(
        Diagnostics::RenderFrameStatistics& result) const noexcept;

    Impl* impl_ = nullptr;
};

} // namespace Aero
