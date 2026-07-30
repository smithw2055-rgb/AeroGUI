#pragma once

#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Property/DependencyProperty.hpp>

#include <cstdint>

namespace Aero::Presentation {

class FrameworkElement;

enum class TransformOwnerRole : std::uint8_t {
    Render = 1U,
    Layout = 2U
};

class AERO_API Transform : public Core::DependencyObject {
    AERO_DECLARE_TYPE(Transform, Core::DependencyObject)
public:
    virtual Base::Transform2D Matrix() const noexcept = 0;
    FrameworkElement* Owner() const noexcept { return owner_; }
    virtual void SetOwner(FrameworkElement* owner) noexcept {
        owner_ = owner;
        ownerRoles_ = owner != nullptr
            ? static_cast<std::uint8_t>(
                  TransformOwnerRole::Render)
            : 0U;
    }
    virtual void AttachOwner(
        FrameworkElement* owner,
        TransformOwnerRole role) noexcept;
    virtual void DetachOwner(
        FrameworkElement* owner,
        TransformOwnerRole role) noexcept;
    bool HasOwnerRole(
        TransformOwnerRole role) const noexcept {
        return (ownerRoles_ &
            static_cast<std::uint8_t>(role)) != 0U;
    }

protected:
    explicit Transform(Core::TypeId runtimeType) noexcept
        : DependencyObject(runtimeType) {}
    Base::Result<void> OnPropertyInvalidated(
        Core::PropertyInvalidationFlags flags) noexcept override;

private:
    FrameworkElement* owner_ = nullptr;
    std::uint8_t ownerRoles_ = 0U;
};

class AERO_API TranslateTransform final : public Transform {
    AERO_DECLARE_TYPE(TranslateTransform, Transform)
public:
    TranslateTransform() noexcept : Transform(StaticTypeId()) {}
    double X() const noexcept;
    double Y() const noexcept;
    Base::Result<void> SetX(double value) noexcept;
    Base::Result<void> SetY(double value) noexcept;

    inline static constexpr Members::Property<double>
        XProperty{"X"};
    inline static constexpr Members::Property<double>
        YProperty{"Y"};

    Base::Transform2D Matrix() const noexcept override;
};

class AERO_API ScaleTransform final : public Transform {
    AERO_DECLARE_TYPE(ScaleTransform, Transform)
public:
    ScaleTransform() noexcept : Transform(StaticTypeId()) {}
    double ScaleX() const noexcept;
    double ScaleY() const noexcept;
    double CenterX() const noexcept;
    double CenterY() const noexcept;
    Base::Result<void> SetScaleX(double value) noexcept;
    Base::Result<void> SetScaleY(double value) noexcept;
    Base::Result<void> SetCenterX(double value) noexcept;
    Base::Result<void> SetCenterY(double value) noexcept;

    inline static constexpr Members::Property<double>
        ScaleXProperty{"ScaleX"};
    inline static constexpr Members::Property<double>
        ScaleYProperty{"ScaleY"};
    inline static constexpr Members::Property<double>
        CenterXProperty{"CenterX"};
    inline static constexpr Members::Property<double>
        CenterYProperty{"CenterY"};

    Base::Transform2D Matrix() const noexcept override;
};

class AERO_API RotateTransform final : public Transform {
    AERO_DECLARE_TYPE(RotateTransform, Transform)
public:
    RotateTransform() noexcept : Transform(StaticTypeId()) {}
    double Angle() const noexcept;
    double CenterX() const noexcept;
    double CenterY() const noexcept;
    Base::Result<void> SetAngle(double value) noexcept;
    Base::Result<void> SetCenterX(double value) noexcept;
    Base::Result<void> SetCenterY(double value) noexcept;

    inline static constexpr Members::Property<double>
        AngleProperty{"Angle"};
    inline static constexpr Members::Property<double>
        CenterXProperty{"CenterX"};
    inline static constexpr Members::Property<double>
        CenterYProperty{"CenterY"};

    Base::Transform2D Matrix() const noexcept override;
};

class AERO_API SkewTransform final : public Transform {
    AERO_DECLARE_TYPE(SkewTransform, Transform)
public:
    SkewTransform() noexcept : Transform(StaticTypeId()) {}
    double AngleX() const noexcept;
    double AngleY() const noexcept;
    double CenterX() const noexcept;
    double CenterY() const noexcept;
    Base::Result<void> SetAngleX(double value) noexcept;
    Base::Result<void> SetAngleY(double value) noexcept;
    Base::Result<void> SetCenterX(double value) noexcept;
    Base::Result<void> SetCenterY(double value) noexcept;

    inline static constexpr Members::Property<double>
        AngleXProperty{"AngleX"};
    inline static constexpr Members::Property<double>
        AngleYProperty{"AngleY"};
    inline static constexpr Members::Property<double>
        CenterXProperty{"CenterX"};
    inline static constexpr Members::Property<double>
        CenterYProperty{"CenterY"};

    Base::Transform2D Matrix() const noexcept override;
};

class AERO_API MatrixTransform final : public Transform {
    AERO_DECLARE_TYPE(MatrixTransform, Transform)
public:
    MatrixTransform() noexcept : Transform(StaticTypeId()) {}
    Base::Transform2D Value() const noexcept;
    Base::Result<void> SetValue(Base::Transform2D value) noexcept;
    inline static constexpr Members::Property<Base::Transform2D>
        MatrixProperty{"Matrix"};
    Base::Transform2D Matrix() const noexcept override {
        return Value();
    }
};

class AERO_API TransformGroup final : public Transform {
    AERO_DECLARE_TYPE(TransformGroup, Transform)
public:
    TransformGroup() noexcept : Transform(StaticTypeId()) {}
    Base::Result<void> TryAddChild(
        Base::Ref<Transform> value) noexcept;
    Base::Result<void> ClearChildren() noexcept;
    Base::Span<const Base::Ref<Transform>>
    Children() const noexcept {
        return {children_.Data(), children_.Size()};
    }
    void SetOwner(FrameworkElement* owner) noexcept override;
    void AttachOwner(
        FrameworkElement* owner,
        TransformOwnerRole role) noexcept override;
    void DetachOwner(
        FrameworkElement* owner,
        TransformOwnerRole role) noexcept override;
    Base::Transform2D Matrix() const noexcept override;

private:
    Base::Vector<Base::Ref<Transform>> children_;
};

AERO_API Base::Transform2D ComposeTransforms(
    const Base::Transform2D& first,
    const Base::Transform2D& second) noexcept;
AERO_API Base::Point TransformPoint(
    const Base::Transform2D& transform,
    Base::Point point) noexcept;
AERO_API Base::Rect TransformBounds(
    const Base::Transform2D& transform,
    Base::Rect rect) noexcept;
AERO_API bool TryInvertTransform(
    const Base::Transform2D& transform,
    Base::Transform2D& inverse) noexcept;

} // namespace Aero::Presentation

namespace Aero::Core {

template<>
struct MetaTypeTraits<Base::Point> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("Point");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "Point";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

template<>
struct MetaTypeTraits<Base::Transform2D> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("Matrix");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "Matrix";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

} // namespace Aero::Core
