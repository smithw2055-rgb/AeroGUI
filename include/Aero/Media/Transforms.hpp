#pragma once

#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/DependencyProperty.hpp>

#include <cstdint>

namespace Aero { class FrameworkElement; }
namespace Aero::Internal { class TransformPrivate; }

namespace Aero::Media {

using Transform2D = Base::Transform2D;

class AERO_API Transform : public ::Aero::DependencyObject {
    AERO_DECLARE_TYPE(Transform, ::Aero::DependencyObject)
public:
    virtual Base::Transform2D GetMatrix() const noexcept = 0;

protected:
    explicit Transform(Meta::TypeId runtimeType) noexcept
        : DependencyObject(runtimeType) {}
    void OnPropertyInvalidated(
        Meta::PropertyInvalidationFlags flags) noexcept override;

private:
    friend class ::Aero::Internal::TransformPrivate;
    friend class TransformGroup;
    Aero::FrameworkElement* GetOwner() const noexcept { return owner_; }
    virtual void SetOwner(Aero::FrameworkElement* owner) noexcept {
        owner_ = owner;
        ownerRoles_ = owner != nullptr ? 1U : 0U;
    }
    virtual void AttachOwner(
        Aero::FrameworkElement* owner,
        std::uint8_t role) noexcept;
    virtual void DetachOwner(
        Aero::FrameworkElement* owner,
        std::uint8_t role) noexcept;
    bool HasOwnerRole(std::uint8_t role) const noexcept {
        return (ownerRoles_ & role) != 0U;
    }
    Aero::FrameworkElement* owner_ = nullptr;
    std::uint8_t ownerRoles_ = 0U;
};

class AERO_API TranslateTransform : public Transform {
    AERO_DECLARE_TYPE(TranslateTransform, Transform)
public:
    TranslateTransform() noexcept : Transform(StaticTypeId()) {}
    double GetX() const noexcept;
    double GetY() const noexcept;
    void SetX(double value) noexcept;
    void SetY(double value) noexcept;

    inline static constexpr Members::Property<double> XProperty{"X"};
    inline static constexpr Members::Property<double> YProperty{"Y"};

    Base::Transform2D GetMatrix() const noexcept override;
};

class AERO_API ScaleTransform : public Transform {
    AERO_DECLARE_TYPE(ScaleTransform, Transform)
public:
    ScaleTransform() noexcept : Transform(StaticTypeId()) {}
    double GetScaleX() const noexcept;
    double GetScaleY() const noexcept;
    double GetCenterX() const noexcept;
    double GetCenterY() const noexcept;
    void SetScaleX(double value) noexcept;
    void SetScaleY(double value) noexcept;
    void SetCenterX(double value) noexcept;
    void SetCenterY(double value) noexcept;

    inline static constexpr Members::Property<double> ScaleXProperty{"ScaleX"};
    inline static constexpr Members::Property<double> ScaleYProperty{"ScaleY"};
    inline static constexpr Members::Property<double> CenterXProperty{"CenterX"};
    inline static constexpr Members::Property<double> CenterYProperty{"CenterY"};

    Base::Transform2D GetMatrix() const noexcept override;
};

class AERO_API RotateTransform : public Transform {
    AERO_DECLARE_TYPE(RotateTransform, Transform)
public:
    RotateTransform() noexcept : Transform(StaticTypeId()) {}
    double GetAngle() const noexcept;
    double GetCenterX() const noexcept;
    double GetCenterY() const noexcept;
    void SetAngle(double value) noexcept;
    void SetCenterX(double value) noexcept;
    void SetCenterY(double value) noexcept;

    inline static constexpr Members::Property<double> AngleProperty{"Angle"};
    inline static constexpr Members::Property<double> CenterXProperty{"CenterX"};
    inline static constexpr Members::Property<double> CenterYProperty{"CenterY"};

    Base::Transform2D GetMatrix() const noexcept override;
};

class AERO_API SkewTransform : public Transform {
    AERO_DECLARE_TYPE(SkewTransform, Transform)
public:
    SkewTransform() noexcept : Transform(StaticTypeId()) {}
    double GetAngleX() const noexcept;
    double GetAngleY() const noexcept;
    double GetCenterX() const noexcept;
    double GetCenterY() const noexcept;
    void SetAngleX(double value) noexcept;
    void SetAngleY(double value) noexcept;
    void SetCenterX(double value) noexcept;
    void SetCenterY(double value) noexcept;

    inline static constexpr Members::Property<double> AngleXProperty{"AngleX"};
    inline static constexpr Members::Property<double> AngleYProperty{"AngleY"};
    inline static constexpr Members::Property<double> CenterXProperty{"CenterX"};
    inline static constexpr Members::Property<double> CenterYProperty{"CenterY"};

    Base::Transform2D GetMatrix() const noexcept override;
};

class AERO_API MatrixTransform : public Transform {
    AERO_DECLARE_TYPE(MatrixTransform, Transform)
public:
    MatrixTransform() noexcept : Transform(StaticTypeId()) {}
    Base::Transform2D GetMatrixValue() const noexcept;
    void SetMatrixValue(Base::Transform2D value) noexcept;
    inline static constexpr Members::Property<Base::Transform2D> MatrixProperty{"Matrix"};
    Base::Transform2D GetMatrix() const noexcept override {
        return GetMatrixValue();
    }
};

class AERO_API TransformGroup : public Transform {
    AERO_DECLARE_TYPE(TransformGroup, Transform)
public:
    TransformGroup() noexcept : Transform(StaticTypeId()) {}
    Base::Result<void> AddChild(
        Base::Ref<Transform> value) noexcept;
    void ClearChildren() noexcept;
    Base::Span<const Base::Ref<Transform>>
    GetChildren() const noexcept {
        return {children_.Data(), children_.Size()};
    }
    Base::Transform2D GetMatrix() const noexcept override;

private:
    void SetOwner(Aero::FrameworkElement* owner) noexcept override;
    void AttachOwner(
        Aero::FrameworkElement* owner,
        std::uint8_t role) noexcept override;
    void DetachOwner(
        Aero::FrameworkElement* owner,
        std::uint8_t role) noexcept override;
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
AERO_API bool InvertTransform(
    const Base::Transform2D& transform,
    Base::Transform2D& inverse) noexcept;

} // namespace Aero::Media

namespace Aero::Meta {

template<>
struct TypeTraits<Base::Point> {
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
struct TypeTraits<Base::Transform2D> {
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

} // namespace Aero::Meta
