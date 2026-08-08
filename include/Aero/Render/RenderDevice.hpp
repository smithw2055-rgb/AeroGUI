#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>

#include <cstdint>

namespace Aero::Render { class RenderFrame; }

namespace Aero {

class IRenderer;
class RenderDevice;

namespace Diagnostics {
struct RenderDeviceStatistics;
struct RenderFrameStatistics;
AERO_GUI_API RenderDeviceStatistics GetRenderDeviceStatistics(
    const Aero::RenderDevice& device) noexcept;
AERO_GUI_API RenderFrameStatistics GetLastRenderFrameStatistics(
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
class AERO_GUI_API RenderDevice final : public Base::Object {
    struct ConstructionToken {};

public:
    struct Access;

    RenderDevice(
        ConstructionToken,
        Access* implementation) noexcept;
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
    friend struct Access;
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

    Base::Status GetFrameStatus() noexcept;
    Base::Result<Diagnostics::RenderFrameStatistics> Analyze(
        const ::Aero::Render::RenderFrame& frame) noexcept;
    Access* impl_ = nullptr;
};

} // namespace Aero
