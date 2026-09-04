#pragma once

#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Freezable.hpp>

namespace Aero::Media {

using Transform2D = Base::Transform2D;

class AERO_GUI_API Transform : public ::Aero::Freezable {
    AERO_DECLARE_TYPE(Transform, ::Aero::Freezable)
public:

    virtual Base::Transform2D GetMatrix() const noexcept = 0;

protected:
    explicit Transform(Meta::TypeId runtimeType) noexcept
        : Freezable(runtimeType) {}
};

AERO_GUI_API Base::Transform2D ComposeTransforms(
    const Base::Transform2D& first,
    const Base::Transform2D& second) noexcept;
AERO_GUI_API Base::Point TransformPoint(
    const Base::Transform2D& transform,
    Base::Point point) noexcept;
AERO_GUI_API Base::Rect TransformBounds(
    const Base::Transform2D& transform,
    Base::Rect rect) noexcept;
AERO_GUI_API bool InvertTransform(
    const Base::Transform2D& transform,
    Base::Transform2D& inverse) noexcept;
} // namespace Aero::Media

namespace Aero::Meta {

template<>
struct TypeTraits<Base::Point> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("Point");
    }
    static constexpr StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr StringView Name() noexcept {
        return "Point";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

template<>
struct TypeTraits<Base::Transform2D> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("Matrix");
    }
    static constexpr StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr StringView Name() noexcept {
        return "Matrix";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

} // namespace Aero::Meta
