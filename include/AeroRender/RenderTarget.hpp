#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <AeroRender/RenderDevice.hpp>

#include <cstdint>

namespace Aero {

class ViewRenderer;
namespace Render {
class RenderFrame;
class RenderTargetBase;
struct RenderTargetServices;
}

enum class RenderTargetKind : std::uint8_t {
    Embedded = 0U,
    Window
};

enum class RenderTargetState : std::uint8_t {
    Ready = 0U,
    Lost,
    DeviceLost,
    Failed,
    Shutdown
};

// Host-owned onscreen target. Native backend state is the source-private backend
// itself, avoiding a second NativeRenderTarget wrapper. Native acquire/present
// remains an implementation concern under src/render.
class AERO_GUI_API RenderTarget : public Base::Object {
public:
    ~RenderTarget() noexcept override;

    RenderTarget(const RenderTarget&) = delete;
    RenderTarget& operator=(const RenderTarget&) = delete;

    RenderTargetKind Kind() const noexcept;
    RenderTargetState State() const noexcept;
    Ref<Aero::RenderDevice> GetDevice() const noexcept;

    Result<void> Resize(
        std::uint32_t width,
        std::uint32_t height) noexcept;
    void NotifyLost() noexcept;
    Result<void> Restore() noexcept;

protected:
    RenderTarget() noexcept = default;

private:
    virtual RenderTargetKind BackendKind() const noexcept = 0;
    virtual RenderTargetState BackendState() const noexcept = 0;
    virtual Result<void> ResizeBackend(
        std::uint32_t width,
        std::uint32_t height) noexcept = 0;
    virtual void NotifyBackendLost() noexcept = 0;
    virtual Result<void> RestoreBackend() noexcept = 0;

    friend class Render::RenderTargetBase;
    friend struct Render::RenderTargetServices;
};

} // namespace Aero
