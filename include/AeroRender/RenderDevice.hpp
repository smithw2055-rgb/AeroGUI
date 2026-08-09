#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>

#include <cstdint>

namespace Aero::Render { class RenderFrame; }
namespace Aero::Render { class RenderDeviceBase; }

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

enum class RenderBackendKind : std::uint8_t {
    Headless = 0U,
    D3D11,
    OpenGL33
};

enum class RenderBackendHealth : std::uint8_t {
    Ready = 0U,
    DeviceLost,
    Failed
};

// Host-thread-affine UI render device shared by one or more View renderers on
// the same render thread. Diagnostics are opt-in under <Aero/Diagnostics/>.
class AERO_GUI_API RenderDevice : public Base::Object {
public:
    ~RenderDevice() noexcept override;

    RenderDevice(const RenderDevice&) = delete;
    RenderDevice& operator=(const RenderDevice&) = delete;

    RenderDeviceState State() const noexcept;
    RenderBackendKind Backend() const noexcept;
    std::uint64_t Generation() const noexcept;

    void NotifyDeviceLost() noexcept;
    Result<void> Restore() noexcept;
    Result<void> WaitIdle(
        std::uint32_t timeoutMilliseconds = 5000U) noexcept;

protected:
    RenderDevice() noexcept = default;

private:
    virtual RenderBackendKind BackendKind() const noexcept = 0;
    virtual void NotifyBackendDeviceLost() noexcept = 0;
    virtual Result<void> RestoreBackendDevice() noexcept = 0;
    virtual Result<void> WaitBackendIdle(
        std::uint32_t timeoutMilliseconds) noexcept = 0;
    virtual RenderBackendHealth BackendHealth() const noexcept = 0;

    friend class Render::RenderDeviceBase;
    friend Diagnostics::RenderDeviceStatistics
        Diagnostics::GetRenderDeviceStatistics(
            const Aero::RenderDevice& device) noexcept;
    friend Diagnostics::RenderFrameStatistics
        Diagnostics::GetLastRenderFrameStatistics(
            const Aero::RenderDevice& device) noexcept;
    Base::Status GetFrameStatus() noexcept;
    Result<Diagnostics::RenderFrameStatistics> Analyze(
        const ::Aero::Render::RenderFrame& frame) noexcept;
};

} // namespace Aero
