#pragma once

#include <Aero/Base/Geometry.hpp>
#include <Aero/Freezable.hpp>

namespace Aero::Media {

/// 3D affine transform applied to a visual (UWP-style Transform3D).
/// Direct Freezable subclass — no Animatable layer.
class AERO_GUI_API Transform3D : public ::Aero::Freezable {
    AERO_DECLARE_TYPE(Transform3D, ::Aero::Freezable)
public:
    ~Transform3D() override = default;

    /// Local 3D affine (identity for camera-like PerspectiveTransform3D).
    [[nodiscard]] virtual Base::Transform3 GetTransform3D() const noexcept = 0;

protected:
    explicit Transform3D(Meta::TypeId runtimeType) noexcept
        : Freezable(runtimeType) {}
};

/// Tree-walk state shared by render collapse and hit-test unproject.
/// Not a public WPF type. `active` is true when a PerspectiveTransform3D
/// ancestor (or the implicit view-root default Depth) is in effect.
struct Transform3DContext {
    bool active = false;
    Base::Transform3 accumulated = Base::IdentityTransform3();
    double depth = 0.0;
    Base::Point offset{};
    /// Collapse center in the perspective-root's local pixels
    /// (`renderSize/2 + offset`). Shared vanishing point for the subtree.
    Base::Point center{};
};

} // namespace Aero::Media

namespace Aero::Meta {

template<>
struct TypeTraits<Base::Transform3> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("Transform3");
    }
    static constexpr StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr StringView Name() noexcept {
        return "Transform3";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

} // namespace Aero::Meta
