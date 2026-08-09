#pragma once

#include <AeroRender/RenderTarget.hpp>
#include "render/FrameEncoder.hpp"
#include "render/RenderDeviceState.hpp"

#include <utility>

namespace Aero::Render {

// Source-private implementation half of the installed RenderTarget contract.
// Backend products derive their concrete target directly from this base.
class AERO_GUI_INTERNAL_API RenderTargetBase : public Aero::RenderTarget {
public:
    explicit RenderTargetBase(
        Base::Ref<Aero::RenderDevice> selectedDevice,
        Aero::RenderTargetKind selectedKind =
            Aero::RenderTargetKind::Embedded) noexcept
        : device(std::move(selectedDevice)), kind(selectedKind) {}
    ~RenderTargetBase() noexcept override = default;

    RenderTargetBase(const RenderTargetBase&) = delete;
    RenderTargetBase& operator=(const RenderTargetBase&) = delete;

    // Backend modules only acquire and retire a native frame target. The GUI
    // module owns ViewRenderer and performs the actual UI frame submission.
    virtual Base::Result<FrameTarget> AcquireFrameTarget() noexcept = 0;
    virtual Base::Result<void> RetireFrameTarget(
        const FrameTarget& target) noexcept = 0;
    Base::Ref<Aero::RenderDevice> device;
    Aero::RenderTargetKind kind = Aero::RenderTargetKind::Embedded;

    static RenderTargetBase* From(Aero::RenderTarget& target) noexcept {
        return static_cast<RenderTargetBase*>(&target);
    }
    static const RenderTargetBase* From(
        const Aero::RenderTarget& target) noexcept {
        return static_cast<const RenderTargetBase*>(&target);
    }

private:
    Aero::RenderTargetKind BackendKind() const noexcept override {
        return kind;
    }
};

struct RenderTargetServices {
    static Base::Result<void> Render(
        Aero::RenderTarget& target,
        ::Aero::ViewRenderer& renderer,
        const ::Aero::Render::RenderFrame& frame) noexcept;
};

} // namespace Aero::Render
