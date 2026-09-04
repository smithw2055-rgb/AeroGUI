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
    virtual void SetViewport(
        Size viewport) noexcept = 0;
    virtual void SetHorizontalOffset(
        double value) noexcept = 0;
    virtual void SetVerticalOffset(
        double value) noexcept = 0;
    virtual Result<bool> LineHorizontal(
        double direction) noexcept = 0;
    virtual Result<bool> LineVertical(
        double direction) noexcept = 0;
    virtual Result<bool> PageHorizontal(
        double direction) noexcept = 0;
    virtual Result<bool> PageVertical(
        double direction) noexcept = 0;
};

} // namespace Aero::Controls
