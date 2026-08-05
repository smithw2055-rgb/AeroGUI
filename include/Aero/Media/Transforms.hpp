#pragma once

#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Freezable.hpp>

#include <cstdint>

namespace Aero { class FrameworkElement; }

namespace Aero::Media {

using Transform2D = Base::Transform2D;

class AERO_API Transform : public ::Aero::Freezable {
    AERO_DECLARE_TYPE(Transform, ::Aero::Freezable)
public:
    struct Impl;

    virtual Base::Transform2D GetMatrix() const noexcept = 0;

protected:
    explicit Transform(Meta::TypeId runtimeType) noexcept
        : Freezable(runtimeType) {}

private:
    friend struct Impl;
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

// AeroGUIExtensions compatibility transform. The renderer remains 2D, so
// this object projects the authored 3D transform into a deterministic affine
// transform using a fixed perspective distance. RotationZ is exact; X/Y
// rotations and TranslateZ produce the expected foreshortening used by the
// Inventory sample without introducing a second 3D scene graph.
class AERO_API CompositeTransform3D : public ::Aero::DependencyObject {
    AERO_DECLARE_TYPE(CompositeTransform3D, ::Aero::DependencyObject)
public:
    CompositeTransform3D() noexcept : DependencyObject(StaticTypeId()) {}

    double GetCenterX() const noexcept { return GetValueOr(CenterXProperty, 0.0); }
    double GetCenterY() const noexcept { return GetValueOr(CenterYProperty, 0.0); }
    double GetCenterZ() const noexcept { return GetValueOr(CenterZProperty, 0.0); }
    double GetRotationX() const noexcept { return GetValueOr(RotationXProperty, 0.0); }
    double GetRotationY() const noexcept { return GetValueOr(RotationYProperty, 0.0); }
    double GetRotationZ() const noexcept { return GetValueOr(RotationZProperty, 0.0); }
    double GetScaleX() const noexcept { return GetValueOr(ScaleXProperty, 1.0); }
    double GetScaleY() const noexcept { return GetValueOr(ScaleYProperty, 1.0); }
    double GetScaleZ() const noexcept { return GetValueOr(ScaleZProperty, 1.0); }
    double GetTranslateX() const noexcept { return GetValueOr(TranslateXProperty, 0.0); }
    double GetTranslateY() const noexcept { return GetValueOr(TranslateYProperty, 0.0); }
    double GetTranslateZ() const noexcept { return GetValueOr(TranslateZProperty, 0.0); }

    void SetCenterX(double value) noexcept { SetValue(CenterXProperty, value); }
    void SetCenterY(double value) noexcept { SetValue(CenterYProperty, value); }
    void SetCenterZ(double value) noexcept { SetValue(CenterZProperty, value); }
    void SetRotationX(double value) noexcept { SetValue(RotationXProperty, value); }
    void SetRotationY(double value) noexcept { SetValue(RotationYProperty, value); }
    void SetRotationZ(double value) noexcept { SetValue(RotationZProperty, value); }
    void SetScaleX(double value) noexcept { SetValue(ScaleXProperty, value); }
    void SetScaleY(double value) noexcept { SetValue(ScaleYProperty, value); }
    void SetScaleZ(double value) noexcept { SetValue(ScaleZProperty, value); }
    void SetTranslateX(double value) noexcept { SetValue(TranslateXProperty, value); }
    void SetTranslateY(double value) noexcept { SetValue(TranslateYProperty, value); }
    void SetTranslateZ(double value) noexcept { SetValue(TranslateZProperty, value); }

    Base::Transform2D GetProjectedMatrix() const noexcept;

    inline static constexpr Members::Property<double> CenterXProperty{"CenterX"};
    inline static constexpr Members::Property<double> CenterYProperty{"CenterY"};
    inline static constexpr Members::Property<double> CenterZProperty{"CenterZ"};
    inline static constexpr Members::Property<double> RotationXProperty{"RotationX"};
    inline static constexpr Members::Property<double> RotationYProperty{"RotationY"};
    inline static constexpr Members::Property<double> RotationZProperty{"RotationZ"};
    inline static constexpr Members::Property<double> ScaleXProperty{"ScaleX"};
    inline static constexpr Members::Property<double> ScaleYProperty{"ScaleY"};
    inline static constexpr Members::Property<double> ScaleZProperty{"ScaleZ"};
    inline static constexpr Members::Property<double> TranslateXProperty{"TranslateX"};
    inline static constexpr Members::Property<double> TranslateYProperty{"TranslateY"};
    inline static constexpr Members::Property<double> TranslateZProperty{"TranslateZ"};
};

class AERO_API TransformGroup : public Transform {
    AERO_DECLARE_TYPE(TransformGroup, Transform)
public:
    TransformGroup() noexcept : Transform(StaticTypeId()) {}
    ~TransformGroup() override;
    Base::Result<void> AddChild(
        Base::Ref<Transform> value) noexcept;
    void ClearChildren() noexcept;
    Base::Span<const Base::Ref<Transform>>
    GetChildren() const noexcept {
        return {children_.Data(), children_.Size()};
    }
    Base::Transform2D GetMatrix() const noexcept override;

private:
    bool FreezeCore(bool isChecking) noexcept override;
    void OnChildChanged(Freezable&) noexcept;
    Base::Vector<Base::Ref<Transform>> children_;
    FreezableChangedHandler childChangedHandler_;
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
