#pragma once

#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Value.hpp>

namespace Aero::Media {
using Point = Base::Point;
using Size = Base::Size;
using Rect = Base::Rect;
using Thickness = Base::Thickness;
using CornerRadius = Base::CornerRadius;
class AERO_API Geometry final : public Base::Object {
    AERO_DECLARE_TYPE(Geometry, Base::Object)
public:
    Geometry() noexcept = default;
    Core::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    Base::StringView Value() const noexcept { return value_.View(); }
    Base::Result<void> SetValue(Base::StringView value) noexcept { return value_.TryAssign(value); }
private:
    Base::String value_;
};
} // namespace Aero::Media
