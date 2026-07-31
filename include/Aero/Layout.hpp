#pragma once


#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Dispatcher.hpp>
#include <Aero/Media/Geometry.hpp>

#include <cstdint>

namespace Aero {

namespace Input { class RoutedCommand; }

using namespace Aero::Core;
using Point = Base::Point;
using Size = Base::Size;
using Rect = Base::Rect;
using Thickness = Base::Thickness;
using CornerRadius = Base::CornerRadius;

enum class HorizontalAlignment : std::uint8_t { Stretch = 0U, Left, Center, Right };
enum class VerticalAlignment : std::uint8_t { Stretch = 0U, Top, Center, Bottom };
enum class Visibility : std::uint8_t { Visible = 0U, Hidden, Collapsed };
enum class BlendMode : std::uint8_t {
    Normal = 0U,
    Multiply,
    Screen,
    Additive
};

} // namespace Aero

namespace Aero::Core {

template<>
struct MetaTypeTraits<Aero::HorizontalAlignment> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("HorizontalAlignment");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "HorizontalAlignment";
    }
    static constexpr TypeId BaseType() noexcept { return InvalidTypeId; }
};

template<>
struct MetaTypeTraits<Aero::VerticalAlignment> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("VerticalAlignment");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "VerticalAlignment";
    }
    static constexpr TypeId BaseType() noexcept { return InvalidTypeId; }
};

template<>
struct MetaTypeTraits<Aero::Visibility> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("Visibility");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "Visibility";
    }
    static constexpr TypeId BaseType() noexcept { return InvalidTypeId; }
};

template<>
struct MetaTypeTraits<Aero::BlendMode> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("BlendMode");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "BlendMode";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

template<>
struct MetaTypeTraits<Base::Thickness> {
    static constexpr TypeId Id() noexcept { return MakeTypeId("Thickness"); }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept { return "Thickness"; }
    static constexpr TypeId BaseType() noexcept { return InvalidTypeId; }
};

template<>
struct MetaTypeTraits<Base::CornerRadius> {
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

} // namespace Aero::Core

namespace Aero {

using namespace Aero::Core;

struct Length final {
    AERO_DECLARE_TYPE(Length, NoMetadataBase)
    double value = 0.0;
    bool isAuto = true;

    static constexpr Length Auto() noexcept { return {}; }
    static constexpr Length Pixels(double value) noexcept {
        return {value, false};
    }
};

AERO_API bool IsFinite(Point value) noexcept;
AERO_API bool IsFinite(Size value) noexcept;
AERO_API bool IsFinite(Rect value) noexcept;
AERO_API bool IsFinite(Thickness value) noexcept;
AERO_API bool IsValidLayoutSize(Size value) noexcept;
AERO_API bool IsValidLayoutRect(Rect value) noexcept;
AERO_API Size Deflate(Size value, Thickness padding) noexcept;
AERO_API Size Inflate(Size value, Thickness padding) noexcept;
AERO_API Rect Intersect(Rect left, Rect right) noexcept;
AERO_API double RoundLayoutValue(double value, double dpiScale) noexcept;

struct LayoutDiagnostics final {
    std::uint64_t passVersion = 0U;
    std::uint32_t measuredCount = 0U;
    std::uint32_t arrangedCount = 0U;
    std::uint32_t pendingMeasureCount = 0U;
    std::uint32_t pendingArrangeCount = 0U;
};

} // namespace Aero
