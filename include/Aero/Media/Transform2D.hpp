#pragma once

// 2D transform family, including the Transform base.
// XAML type names stay Transform, TranslateTransform, RotateTransform, ...
#include <Aero/Animatable.hpp>
#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Media/FreezableCollection.hpp>

#include <cstdint>

namespace Aero::Media {

using Transform2D = Base::Transform2D;

class AERO_GUI_API Transform : public ::Aero::Animatable {
    AERO_DECLARE_TYPE(Transform, ::Aero::Animatable)
public:

    virtual Base::Transform2D GetMatrix() const noexcept = 0;

    // Freezable content revision (render cache invalidation).
    std::uint64_t GetRevision() const noexcept;

protected:
    explicit Transform(Meta::TypeId runtimeType) noexcept
        : Animatable(runtimeType) {}
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

class AERO_GUI_API TranslateTransform : public Transform {
    AERO_DECLARE_TYPE(TranslateTransform, Transform)
public:
    TranslateTransform() noexcept : Transform(StaticTypeId()) {}
    double GetX() const noexcept;
    double GetY() const noexcept;
    void SetX(double value) noexcept;
    void SetY(double value) noexcept;

    AERO_DEPENDENCY_PROPERTY(double, X);
    AERO_DEPENDENCY_PROPERTY(double, Y);

    Base::Transform2D GetMatrix() const noexcept override;
};

class AERO_GUI_API ScaleTransform : public Transform {
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

    AERO_DEPENDENCY_PROPERTY(double, ScaleX);
    AERO_DEPENDENCY_PROPERTY(double, ScaleY);
    AERO_DEPENDENCY_PROPERTY(double, CenterX);
    AERO_DEPENDENCY_PROPERTY(double, CenterY);

    Base::Transform2D GetMatrix() const noexcept override;
};

class AERO_GUI_API RotateTransform : public Transform {
    AERO_DECLARE_TYPE(RotateTransform, Transform)
public:
    RotateTransform() noexcept : Transform(StaticTypeId()) {}
    double GetAngle() const noexcept;
    double GetCenterX() const noexcept;
    double GetCenterY() const noexcept;
    void SetAngle(double value) noexcept;
    void SetCenterX(double value) noexcept;
    void SetCenterY(double value) noexcept;

    AERO_DEPENDENCY_PROPERTY(double, Angle);
    AERO_DEPENDENCY_PROPERTY(double, CenterX);
    AERO_DEPENDENCY_PROPERTY(double, CenterY);

    Base::Transform2D GetMatrix() const noexcept override;
};

class AERO_GUI_API SkewTransform : public Transform {
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

    AERO_DEPENDENCY_PROPERTY(double, AngleX);
    AERO_DEPENDENCY_PROPERTY(double, AngleY);
    AERO_DEPENDENCY_PROPERTY(double, CenterX);
    AERO_DEPENDENCY_PROPERTY(double, CenterY);

    Base::Transform2D GetMatrix() const noexcept override;
};

class AERO_GUI_API MatrixTransform : public Transform {
    AERO_DECLARE_TYPE(MatrixTransform, Transform)
public:
    MatrixTransform() noexcept : Transform(StaticTypeId()) {}
    Base::Transform2D GetMatrixValue() const noexcept;
    void SetMatrixValue(Base::Transform2D value) noexcept;
    AERO_DEPENDENCY_PROPERTY(Base::Transform2D, Matrix);
    Base::Transform2D GetMatrix() const noexcept override {
        return GetMatrixValue();
    }
};

/// 2D composite: Center + Scale / Skew / Rotate / Translate.
/// GetMatrix() composes the existing 2D transform primitives around Center.
class AERO_GUI_API CompositeTransform : public Transform {
    AERO_DECLARE_TYPE(CompositeTransform, Transform)
public:
    CompositeTransform() noexcept : Transform(StaticTypeId()) {}

    double GetCenterX() const noexcept { return GetValue(CenterXProperty); }
    double GetCenterY() const noexcept { return GetValue(CenterYProperty); }
    double GetScaleX() const noexcept { return GetValue(ScaleXProperty); }
    double GetScaleY() const noexcept { return GetValue(ScaleYProperty); }
    double GetSkewX() const noexcept { return GetValue(SkewXProperty); }
    double GetSkewY() const noexcept { return GetValue(SkewYProperty); }
    double GetRotation() const noexcept { return GetValue(RotationProperty); }
    double GetTranslateX() const noexcept { return GetValue(TranslateXProperty); }
    double GetTranslateY() const noexcept { return GetValue(TranslateYProperty); }

    void SetCenterX(double value) noexcept { SetValue(CenterXProperty, value); }
    void SetCenterY(double value) noexcept { SetValue(CenterYProperty, value); }
    void SetScaleX(double value) noexcept { SetValue(ScaleXProperty, value); }
    void SetScaleY(double value) noexcept { SetValue(ScaleYProperty, value); }
    void SetSkewX(double value) noexcept { SetValue(SkewXProperty, value); }
    void SetSkewY(double value) noexcept { SetValue(SkewYProperty, value); }
    void SetRotation(double value) noexcept { SetValue(RotationProperty, value); }
    void SetTranslateX(double value) noexcept { SetValue(TranslateXProperty, value); }
    void SetTranslateY(double value) noexcept { SetValue(TranslateYProperty, value); }

    Base::Transform2D GetMatrix() const noexcept override;

    AERO_DEPENDENCY_PROPERTY(double, CenterX);
    AERO_DEPENDENCY_PROPERTY(double, CenterY);
    AERO_DEPENDENCY_PROPERTY(double, ScaleX);
    AERO_DEPENDENCY_PROPERTY(double, ScaleY);
    AERO_DEPENDENCY_PROPERTY(double, SkewX);
    AERO_DEPENDENCY_PROPERTY(double, SkewY);
    AERO_DEPENDENCY_PROPERTY(double, Rotation);
    AERO_DEPENDENCY_PROPERTY(double, TranslateX);
    AERO_DEPENDENCY_PROPERTY(double, TranslateY);
};

class AERO_GUI_API TransformGroup : public Transform {
    AERO_DECLARE_TYPE(TransformGroup, Transform)
public:
    TransformGroup() noexcept : Transform(StaticTypeId()) {}
    ~TransformGroup() override;
    void AddChild(
        Ref<Transform> value) noexcept;
    void ClearChildren() noexcept;
    Span<const Ref<Transform>>
    GetChildren() const noexcept {
        return children_.AsSpan();
    }
    Base::Transform2D GetMatrix() const noexcept override;

private:
    bool FreezeCore(bool isChecking) noexcept override;
    void OnChildChanged(Freezable&) noexcept;
    FreezableCollection<Transform> children_;
    FreezableChangedHandler childChangedHandler_;
};

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
