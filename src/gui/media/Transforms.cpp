#include <Aero/Media/Transforms.hpp>
#include "gui/core/State.hpp" 
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "gui/media/MediaState.hpp"

#include <Aero/FrameworkElement.hpp>

#include <algorithm>
#include <cmath>

namespace Aero::Media {
namespace {

constexpr double Pi = 3.1415926535897932384626433832795;

Base::Transform2D AroundCenter(
    Base::Transform2D value,
    double centerX,
    double centerY) noexcept {
    Base::Transform2D before;
    before.dx = -centerX;
    before.dy = -centerY;
    Base::Transform2D after;
    after.dx = centerX;
    after.dy = centerY;
    return ComposeTransforms(
        ComposeTransforms(before, value), after);
}

bool ContainsTransform(
    const Transform& value,
    const Transform* sought) noexcept {
    if (&value == sought) return true;
    if (!value.PropertyRegistry().Types().IsDerivedFrom(
            value.RuntimeType(), TransformGroup::StaticTypeId())) {
        return false;
    }
    const auto& group = static_cast<const TransformGroup&>(value);
    for (const Base::Ref<Transform>& child : group.GetChildren()) {
        if (child && ContainsTransform(*child, sought)) return true;
    }
    return false;
}

} // namespace

} // namespace Aero::Media

namespace Aero::Media {

std::uint64_t TransformRuntime::Revision(
    const Transform& transform) noexcept {
    return AeroGuiInternal::FreezableRevision(transform);
}

} // namespace Aero::Media

namespace Aero::Media {

Base::Transform2D ComposeTransforms(
    const Base::Transform2D& first,
    const Base::Transform2D& second) noexcept {
    Base::Transform2D output;
    output.m11 =
        first.m11 * second.m11 + first.m12 * second.m21;
    output.m12 =
        first.m11 * second.m12 + first.m12 * second.m22;
    output.m21 =
        first.m21 * second.m11 + first.m22 * second.m21;
    output.m22 =
        first.m21 * second.m12 + first.m22 * second.m22;
    output.dx =
        first.dx * second.m11 + first.dy * second.m21 + second.dx;
    output.dy =
        first.dx * second.m12 + first.dy * second.m22 + second.dy;
    return output;
}

Base::Point TransformPoint(
    const Base::Transform2D& transform,
    Base::Point point) noexcept {
    return {
        point.x * transform.m11 +
            point.y * transform.m21 +
            transform.dx,
        point.x * transform.m12 +
            point.y * transform.m22 +
            transform.dy};
}

Base::Rect TransformBounds(
    const Base::Transform2D& transform,
    Base::Rect rect) noexcept {
    const Base::Point p0 =
        TransformPoint(
            transform,
            {rect.x, rect.y});
    const Base::Point p1 =
        TransformPoint(
            transform,
            {rect.x + rect.width, rect.y});
    const Base::Point p2 =
        TransformPoint(
            transform,
            {rect.x, rect.y + rect.height});
    const Base::Point p3 =
        TransformPoint(
            transform,
            {rect.x + rect.width,
             rect.y + rect.height});
    const double left = std::min(
        std::min(p0.x, p1.x),
        std::min(p2.x, p3.x));
    const double top = std::min(
        std::min(p0.y, p1.y),
        std::min(p2.y, p3.y));
    const double right = std::max(
        std::max(p0.x, p1.x),
        std::max(p2.x, p3.x));
    const double bottom = std::max(
        std::max(p0.y, p1.y),
        std::max(p2.y, p3.y));
    return {
        left,
        top,
        std::max(0.0, right - left),
        std::max(0.0, bottom - top)};
}

bool InvertTransform(
    const Base::Transform2D& transform,
    Base::Transform2D& inverse) noexcept {
    if (!Base::IsFiniteTransform(transform)) {
        return false;
    }
    const double determinant =
        transform.m11 * transform.m22 -
        transform.m12 * transform.m21;
    if (!std::isfinite(determinant) ||
        std::abs(determinant) <= 1.0e-12) {
        return false;
    }
    inverse.m11 = transform.m22 / determinant;
    inverse.m12 = -transform.m12 / determinant;
    inverse.m21 = -transform.m21 / determinant;
    inverse.m22 = transform.m11 / determinant;
    inverse.dx = -(
        transform.dx * inverse.m11 +
        transform.dy * inverse.m21);
    inverse.dy = -(
        transform.dx * inverse.m12 +
        transform.dy * inverse.m22);
    return Base::IsFiniteTransform(inverse);
}

Base::Transform2D CompositeTransform3D::GetProjectedMatrix() const noexcept {
    constexpr double Perspective = 1000.0;
    const double radiansX = GetRotationX() * Pi / 180.0;
    const double radiansY = GetRotationY() * Pi / 180.0;
    const double radiansZ = GetRotationZ() * Pi / 180.0;
    const double depth = std::max(-Perspective * 0.95,
        std::min(Perspective * 0.95, GetTranslateZ() + GetCenterZ()));
    const double perspective = Perspective / (Perspective - depth);
    const double scaleX = GetScaleX() * std::cos(radiansY) * perspective;
    const double scaleY = GetScaleY() * std::cos(radiansX) * perspective;
    const double cosine = std::cos(radiansZ);
    const double sine = std::sin(radiansZ);
    Base::Transform2D matrix;
    matrix.m11 = scaleX * cosine;
    matrix.m12 = scaleX * sine;
    matrix.m21 = -scaleY * sine;
    matrix.m22 = scaleY * cosine;
    matrix.dx = GetTranslateX();
    matrix.dy = GetTranslateY();
    return AroundCenter(matrix, GetCenterX(), GetCenterY());
}

double TranslateTransform::GetX() const noexcept {
    return GetValueOr(XProperty, 0.0);
}
double TranslateTransform::GetY() const noexcept {
    return GetValueOr(YProperty, 0.0);
}
void TranslateTransform::SetX(double value) noexcept {
    SetValue(XProperty, value);
}
void TranslateTransform::SetY(double value) noexcept {
    SetValue(YProperty, value);
}
Base::Transform2D TranslateTransform::GetMatrix() const noexcept {
    Base::Transform2D value;
    value.dx = GetX();
    value.dy = GetY();
    return value;
}

double ScaleTransform::GetScaleX() const noexcept {
    return GetValueOr(ScaleXProperty, 1.0);
}
double ScaleTransform::GetScaleY() const noexcept {
    return GetValueOr(ScaleYProperty, 1.0);
}
double ScaleTransform::GetCenterX() const noexcept {
    return GetValueOr(CenterXProperty, 0.0);
}
double ScaleTransform::GetCenterY() const noexcept {
    return GetValueOr(CenterYProperty, 0.0);
}
void ScaleTransform::SetScaleX(double value) noexcept {
    SetValue(ScaleXProperty, value);
}
void ScaleTransform::SetScaleY(double value) noexcept {
    SetValue(ScaleYProperty, value);
}
void ScaleTransform::SetCenterX(double value) noexcept {
    SetValue(CenterXProperty, value);
}
void ScaleTransform::SetCenterY(double value) noexcept {
    SetValue(CenterYProperty, value);
}
Base::Transform2D ScaleTransform::GetMatrix() const noexcept {
    Base::Transform2D value;
    value.m11 = GetScaleX();
    value.m22 = GetScaleY();
    return AroundCenter(value, GetCenterX(), GetCenterY());
}

double RotateTransform::GetAngle() const noexcept {
    return GetValueOr(AngleProperty, 0.0);
}
double RotateTransform::GetCenterX() const noexcept {
    return GetValueOr(CenterXProperty, 0.0);
}
double RotateTransform::GetCenterY() const noexcept {
    return GetValueOr(CenterYProperty, 0.0);
}
void RotateTransform::SetAngle(double value) noexcept {
    SetValue(AngleProperty, value);
}
void RotateTransform::SetCenterX(double value) noexcept {
    SetValue(CenterXProperty, value);
}
void RotateTransform::SetCenterY(double value) noexcept {
    SetValue(CenterYProperty, value);
}
Base::Transform2D RotateTransform::GetMatrix() const noexcept {
    const double radians = GetAngle() * Pi / 180.0;
    const double cosine = std::cos(radians);
    const double sine = std::sin(radians);
    Base::Transform2D value;
    value.m11 = cosine;
    value.m12 = sine;
    value.m21 = -sine;
    value.m22 = cosine;
    return AroundCenter(value, GetCenterX(), GetCenterY());
}

double SkewTransform::GetAngleX() const noexcept {
    return GetValueOr(AngleXProperty, 0.0);
}
double SkewTransform::GetAngleY() const noexcept {
    return GetValueOr(AngleYProperty, 0.0);
}
double SkewTransform::GetCenterX() const noexcept {
    return GetValueOr(CenterXProperty, 0.0);
}
double SkewTransform::GetCenterY() const noexcept {
    return GetValueOr(CenterYProperty, 0.0);
}
void SkewTransform::SetAngleX(double value) noexcept {
    SetValue(AngleXProperty, value);
}
void SkewTransform::SetAngleY(double value) noexcept {
    SetValue(AngleYProperty, value);
}
void SkewTransform::SetCenterX(double value) noexcept {
    SetValue(CenterXProperty, value);
}
void SkewTransform::SetCenterY(double value) noexcept {
    SetValue(CenterYProperty, value);
}
Base::Transform2D SkewTransform::GetMatrix() const noexcept {
    Base::Transform2D value;
    value.m21 = std::tan(GetAngleX() * Pi / 180.0);
    value.m12 = std::tan(GetAngleY() * Pi / 180.0);
    return AroundCenter(value, GetCenterX(), GetCenterY());
}

Base::Transform2D MatrixTransform::GetMatrixValue() const noexcept {
    return GetValueOr(MatrixProperty, Base::Transform2D{});
}
void MatrixTransform::SetMatrixValue(
    Base::Transform2D value) noexcept {
    DependencyObject::SetValue(MatrixProperty, value);
}

Base::Result<void> TransformGroup::AddChild(
    Base::Ref<Transform> value) noexcept {
    Base::Result<void> writable = WritePreamble();
    if (!writable) return writable.GetStatus();
    if (!value) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "TransformGroup child cannot be null");
    }
    if (ContainsTransform(*value, this)) {
        return Base::Status::Failure(
            Base::ErrorCode::CycleDetected,
            "TransformGroup cannot contain itself directly or indirectly");
    }
    if (childChangedHandler_.Empty()) {
        childChangedHandler_ = FreezableChangedHandler(
            this, &TransformGroup::OnChildChanged);
    }
    Transform* retained = value.Get();
    if (!retained->IsFrozen()) {
        Base::Result<void> subscribed =
            retained->AddChangedHandlerChecked(childChangedHandler_);
        if (!subscribed) return subscribed.GetStatus();
    }
    Base::Result<void> added =
        children_.PushBack(std::move(value));
    if (!added) {
        if (!retained->IsFrozen()) {
            static_cast<void>(
                retained->RemoveChangedHandler(childChangedHandler_));
        }
        return added.GetStatus();
    }
    WritePostscript();
    return {};
}

void TransformGroup::ClearChildren() noexcept {
    if (!WritePreamble() || children_.Empty()) return;
    for (Base::Ref<Transform>& child : children_) {
        if (child && !childChangedHandler_.Empty()) {
            static_cast<void>(
                child->RemoveChangedHandler(childChangedHandler_));
        }
    }
    children_.Clear();
    WritePostscript();
}

TransformGroup::~TransformGroup() {
    for (Base::Ref<Transform>& child : children_) {
        if (child && !childChangedHandler_.Empty()) {
            static_cast<void>(
                child->RemoveChangedHandler(childChangedHandler_));
        }
    }
}

void TransformGroup::OnChildChanged(Freezable&) noexcept {
    WritePostscript();
}

bool TransformGroup::FreezeCore(bool isChecking) noexcept {
    for (Base::Ref<Transform>& child : children_) {
        if (!child) continue;
        if (isChecking) {
            if (!child->CanFreeze()) return false;
        } else {
            static_cast<void>(child->Freeze());
        }
    }
    return Transform::FreezeCore(isChecking);
}

Base::Transform2D TransformGroup::GetMatrix() const noexcept {
    Base::Transform2D result;
    for (const Base::Ref<Transform>& child : children_) {
        if (child) {
            result = ComposeTransforms(result, child->GetMatrix());
        }
    }
    return result;
}

} // namespace Aero::Media
