#include <Aero/Media/Transforms.hpp>
#include "TransformInternals.hpp"

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

} // namespace

} // namespace Aero::Media

namespace Aero::Media {

Aero::FrameworkElement* Transform::Impl::Owner(
    const Aero::Media::Transform& transform) noexcept {
    return transform.GetOwner();
}

bool Aero::Media::Transform::Impl::HasOwnerRole(
    const Aero::Media::Transform& transform,
    std::uint8_t role) noexcept {
    return transform.HasOwnerRole(role);
}

void Aero::Media::Transform::Impl::AttachOwner(
    Aero::Media::Transform& transform,
    Aero::FrameworkElement* owner,
    std::uint8_t role) noexcept {
    transform.AttachOwner(owner, role);
}

void Aero::Media::Transform::Impl::DetachOwner(
    Aero::Media::Transform& transform,
    Aero::FrameworkElement* owner,
    std::uint8_t role) noexcept {
    transform.DetachOwner(owner, role);
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

void Transform::AttachOwner(
    FrameworkElement* owner,
    std::uint8_t role) noexcept {
    if (owner == nullptr) return;
    if (owner_ != owner) {
        owner_ = owner;
        ownerRoles_ = 0U;
    }
    ownerRoles_ |= role;
}

void Transform::DetachOwner(
    FrameworkElement* owner,
    std::uint8_t role) noexcept {
    if (owner_ != owner || owner == nullptr) return;
    ownerRoles_ &= static_cast<std::uint8_t>(~role);
    if (ownerRoles_ == 0U) {
        owner_ = nullptr;
    }
}

void Transform::OnPropertyInvalidated(
    Meta::PropertyInvalidationFlags flags) noexcept {
    DependencyObject::OnPropertyInvalidated(flags);
    FrameworkElement* owner = GetOwner();
    if (owner == nullptr) return;
    if (HasOwnerRole(Internal::OwnerRoleValue(
            Internal::TransformOwnerRole::Layout))) {
        (void)owner->InvalidateMeasure();
    } else {
        (void)owner->InvalidateVisual();
    }
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
    if (!value) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "TransformGroup child cannot be null");
    }
    if (HasOwnerRole(Internal::OwnerRoleValue(Internal::TransformOwnerRole::Render))) {
        value->AttachOwner(
            GetOwner(),
            Internal::OwnerRoleValue(Internal::TransformOwnerRole::Render));
    }
    if (HasOwnerRole(Internal::OwnerRoleValue(Internal::TransformOwnerRole::Layout))) {
        value->AttachOwner(
            GetOwner(),
            Internal::OwnerRoleValue(Internal::TransformOwnerRole::Layout));
    }
    Base::Result<void> added =
        children_.PushBack(std::move(value));
    if (!added) return added.GetStatus();
    FrameworkElement* owner = GetOwner();
    if (owner == nullptr) return {};
    return HasOwnerRole(Internal::OwnerRoleValue(Internal::TransformOwnerRole::Layout))
        ? owner->InvalidateMeasure()
        : owner->InvalidateVisual();
}

void TransformGroup::ClearChildren() noexcept {
    for (Base::Ref<Transform>& child : children_) {
        if (!child) continue;
        if (HasOwnerRole(Internal::OwnerRoleValue(Internal::TransformOwnerRole::Render))) {
            child->DetachOwner(
                GetOwner(),
                Internal::OwnerRoleValue(Internal::TransformOwnerRole::Render));
        }
        if (HasOwnerRole(Internal::OwnerRoleValue(Internal::TransformOwnerRole::Layout))) {
            child->DetachOwner(
                GetOwner(),
                Internal::OwnerRoleValue(Internal::TransformOwnerRole::Layout));
        }
    }
    children_.Clear();
    FrameworkElement* owner = GetOwner();
    if (owner == nullptr) return;
    if (HasOwnerRole(Internal::OwnerRoleValue(Internal::TransformOwnerRole::Layout))) {
        (void)owner->InvalidateMeasure();
    } else {
        (void)owner->InvalidateVisual();
    }
}

void TransformGroup::SetOwner(FrameworkElement* owner) noexcept {
    Transform::SetOwner(owner);
    for (Base::Ref<Transform>& child : children_) {
        if (child) child->SetOwner(owner);
    }
}

void TransformGroup::AttachOwner(
    FrameworkElement* owner,
    std::uint8_t role) noexcept {
    Transform::AttachOwner(owner, role);
    for (Base::Ref<Transform>& child : children_) {
        if (child) {
            child->AttachOwner(owner, role);
        }
    }
}

void TransformGroup::DetachOwner(
    FrameworkElement* owner,
    std::uint8_t role) noexcept {
    for (Base::Ref<Transform>& child : children_) {
        if (child) {
            child->DetachOwner(owner, role);
        }
    }
    Transform::DetachOwner(owner, role);
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
