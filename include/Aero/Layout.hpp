#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Geometry.hpp>
#include <Aero/Controls/GridLength.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Diagnostics/Layout.hpp>
#include <Aero/ElementEnums.hpp>

namespace Aero {

using Point = Base::Point;
using Size = Base::Size;
using Rect = Base::Rect;
using Thickness = Base::Thickness;
using CornerRadius = Base::CornerRadius;

} // namespace Aero

AERO_DECLARE_TYPE_VALUE(Base::Thickness, "Thickness")
AERO_DECLARE_TYPE_VALUE(Base::CornerRadius, "CornerRadius")

namespace Aero {

using Meta::NoMetadataBase;

struct Length {
    AERO_DECLARE_TYPE(Length, NoMetadataBase)
    double value = 0.0;
    bool isAuto = true;

    static constexpr Length Auto() noexcept { return {}; }
    static constexpr Length Pixels(double value) noexcept { return {value, false}; }
};

AERO_GUI_API bool IsFinite(Point value) noexcept;
AERO_GUI_API bool IsFinite(Size value) noexcept;
AERO_GUI_API bool IsFinite(Rect value) noexcept;
AERO_GUI_API bool IsFinite(Thickness value) noexcept;
AERO_GUI_API bool IsValidLayoutSize(Size value) noexcept;
AERO_GUI_API bool IsValidLayoutRect(Rect value) noexcept;
AERO_GUI_API Size Deflate(Size value, Thickness padding) noexcept;
AERO_GUI_API Size Inflate(Size value, Thickness padding) noexcept;
AERO_GUI_API Rect Intersect(Rect left, Rect right) noexcept;
AERO_GUI_API double RoundLayoutValue(double value, double dpiScale) noexcept;

} // namespace Aero
