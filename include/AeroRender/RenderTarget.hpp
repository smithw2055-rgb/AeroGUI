#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <AeroRender/RenderDevice.hpp>
#include <AeroRender/Texture.hpp>

#include <cstdint>

namespace Aero {

class RenderDevice;

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

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Base class for 2D textures that can be used as render target (reference: NoesisGUI NsRender/RenderTarget.h)
////////////////////////////////////////////////////////////////////////////////////////////////////
class AERO_GUI_API RenderTarget : public Base::Object {
public:
    ~RenderTarget() noexcept override;

    RenderTarget(const RenderTarget&) = delete;
    RenderTarget& operator=(const RenderTarget&) = delete;

    /// Returns the resolve texture
    virtual Texture* GetTexture() noexcept = 0;

    RenderTargetKind Kind() const noexcept { return kind_; }
    RenderTargetState State() const noexcept { return state_; }
    Ref<RenderDevice> GetDevice() const noexcept { return device_; }

    virtual Result<void> Resize(
        std::uint32_t width,
        std::uint32_t height) noexcept;
    void NotifyLost() noexcept;
    Result<void> Restore() noexcept;

protected:
    RenderTarget() noexcept = default;

    virtual RenderTargetKind BackendKind() const noexcept { return kind_; }
    virtual RenderTargetState BackendState() const noexcept { return state_; }
    virtual Result<void> ResizeBackend(
        std::uint32_t width,
        std::uint32_t height) noexcept {
        static_cast<void>(width);
        static_cast<void>(height);
        return {};
    }
    virtual void NotifyBackendLost() noexcept {}
    virtual Result<void> RestoreBackend() noexcept { return {}; }

    RenderTargetKind kind_ = RenderTargetKind::Embedded;
    RenderTargetState state_ = RenderTargetState::Ready;
    Ref<RenderDevice> device_;
};

} // namespace Aero
