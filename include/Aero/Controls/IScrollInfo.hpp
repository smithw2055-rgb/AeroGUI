#pragma once

#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Events/ControlEventArgs.hpp>

namespace Aero::Controls {
using Size = ::Aero::Base::Size;

class AERO_GUI_API IScrollInfo {
public:
    virtual ~IScrollInfo() = default;
    virtual ScrollData GetData() const noexcept = 0;
    virtual void SetViewport(Size viewport) noexcept = 0;
    virtual void SetHorizontalOffset(double value) noexcept = 0;
    virtual void SetVerticalOffset(double value) noexcept = 0;
    virtual Result<bool> LineHorizontal(
        double direction) noexcept = 0;
    virtual Result<bool> LineVertical(
        double direction) noexcept = 0;
    virtual Result<bool> PageHorizontal(
        double direction) noexcept = 0;
    virtual Result<bool> PageVertical(
        double direction) noexcept = 0;
    // WPF-parity directional verbs. Default forwards to the legacy
    // Line/Page(double) primitives so existing implementers keep working;
    // new code overrides these directly.
    virtual Result<bool> LineUp() noexcept { return LineVertical(-1.0); }
    virtual Result<bool> LineDown() noexcept { return LineVertical(1.0); }
    virtual Result<bool> LineLeft() noexcept { return LineHorizontal(-1.0); }
    virtual Result<bool> LineRight() noexcept { return LineHorizontal(1.0); }
    virtual Result<bool> PageUp() noexcept { return PageVertical(-1.0); }
    virtual Result<bool> PageDown() noexcept { return PageVertical(1.0); }
    virtual Result<bool> MouseWheelUp() noexcept { return PageVertical(-1.0); }
    virtual Result<bool> MouseWheelDown() noexcept { return PageVertical(1.0); }
    virtual Result<bool> MouseWheelLeft() noexcept { return PageHorizontal(-1.0); }
    virtual Result<bool> MouseWheelRight() noexcept { return PageHorizontal(1.0); }
};

} // namespace Aero::Controls
