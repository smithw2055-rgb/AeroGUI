#pragma once


#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Threading.hpp>
#include <Aero/Gui/Geometry.hpp>

#include <cstdint>

namespace Aero {

namespace Input { class RoutedCommand; }

using Point = Base::Point;
using Size = Base::Size;
using Rect = Base::Rect;
using Thickness = Base::Thickness;
using CornerRadius = Base::CornerRadius;

enum class HorizontalAlignment : std::uint8_t { Stretch = 0U, Left, Center, Right };
enum class VerticalAlignment : std::uint8_t { Stretch = 0U, Top, Center, Bottom };
// Inherited by FrameworkElement so text and templates keep the same logical
// reading direction without every control carrying a duplicate property.
enum class FlowDirection : std::uint8_t { LeftToRight = 0U, RightToLeft };
enum class Visibility : std::uint8_t { Visible = 0U, Hidden, Collapsed };
enum class BlendMode : std::uint8_t {
    Normal = 0U,
    Multiply,
    Screen,
    Additive
};

} // namespace Aero

AERO_DECLARE_TYPE_ENUM(Aero::HorizontalAlignment)
AERO_DECLARE_TYPE_ENUM(Aero::VerticalAlignment)
AERO_DECLARE_TYPE_ENUM(Aero::FlowDirection)
AERO_DECLARE_TYPE_ENUM(Aero::Visibility)
AERO_DECLARE_TYPE_ENUM(Aero::BlendMode)

namespace Aero::Meta {

template<>
struct TypeTraits<Base::Thickness> {
    static constexpr TypeId Id() noexcept { return MakeTypeId("Thickness"); }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept { return "Thickness"; }
    static constexpr TypeId BaseType() noexcept { return InvalidTypeId; }
};

template<>
struct TypeTraits<Base::CornerRadius> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("CornerRadius");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "CornerRadius";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

} // namespace Aero::Meta

namespace Aero {

using Meta::NoMetadataBase;


struct Length {
    AERO_DECLARE_TYPE(Length, NoMetadataBase)
    double value = 0.0;
    bool isAuto = true;

    static constexpr Length Auto() noexcept { return {}; }
    static constexpr Length Pixels(double value) noexcept {
        return {value, false};
    }
};

enum class GridUnitType : std::uint8_t {
    Auto = 0U,
    Pixel,
    Star
};

struct GridLength {
    double value = 1.0;
    GridUnitType unit = GridUnitType::Star;

    constexpr double GetValue() const noexcept { return value; }
    constexpr GridUnitType GetGridUnitType() const noexcept { return unit; }
    constexpr bool GetIsAbsolute() const noexcept {
        return unit == GridUnitType::Pixel;
    }
    constexpr bool GetIsAuto() const noexcept {
        return unit == GridUnitType::Auto;
    }
    constexpr bool GetIsStar() const noexcept {
        return unit == GridUnitType::Star;
    }

    static constexpr GridLength Auto() noexcept {
        return {0.0, GridUnitType::Auto};
    }
    static constexpr GridLength Pixel(double value) noexcept {
        return {value, GridUnitType::Pixel};
    }
    static constexpr GridLength Star(double weight = 1.0) noexcept {
        return {weight, GridUnitType::Star};
    }
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

struct LayoutDiagnostics {
    std::uint64_t passVersion = 0U;
    std::uint32_t measuredCount = 0U;
    std::uint32_t arrangedCount = 0U;
    std::uint32_t pendingMeasureCount = 0U;
    std::uint32_t pendingArrangeCount = 0U;
};

} // namespace Aero

namespace Aero::Meta {

template<>
struct TypeTraits<::Aero::GridLength> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("GridLength");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "GridLength";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

} // namespace Aero::Meta
