#include <Aero/Media/Transforms.hpp>

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

bool TryInvertTransform(
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
    TransformOwnerRole role) noexcept {
    if (owner == nullptr) return;
    if (owner_ != owner) {
        owner_ = owner;
        ownerRoles_ = 0U;
    }
    ownerRoles_ |= static_cast<std::uint8_t>(role);
}

void Transform::DetachOwner(
    FrameworkElement* owner,
    TransformOwnerRole role) noexcept {
    if (owner_ != owner || owner == nullptr) return;
    ownerRoles_ &= static_cast<std::uint8_t>(
        ~static_cast<std::uint8_t>(role));
    if (ownerRoles_ == 0U) {
        owner_ = nullptr;
    }
}

Base::Result<void> Transform::OnPropertyInvalidated(
    Core::PropertyInvalidationFlags flags) noexcept {
    Base::Result<void> base =
        DependencyObject::OnPropertyInvalidated(flags);
    if (!base) return base.GetStatus();
    FrameworkElement* owner = Owner();
    if (owner == nullptr) return {};
    return HasOwnerRole(TransformOwnerRole::Layout)
        ? owner->InvalidateMeasure()
        : owner->InvalidateRender();
}

double TranslateTransform::X() const noexcept {
    return GetValueOr(XProperty, 0.0);
}
double TranslateTransform::Y() const noexcept {
    return GetValueOr(YProperty, 0.0);
}
Base::Result<void> TranslateTransform::SetX(double value) noexcept {
    return SetValue(XProperty, value);
}
Base::Result<void> TranslateTransform::SetY(double value) noexcept {
    return SetValue(YProperty, value);
}
Base::Transform2D TranslateTransform::Matrix() const noexcept {
    Base::Transform2D value;
    value.dx = X();
    value.dy = Y();
    return value;
}

double ScaleTransform::ScaleX() const noexcept {
    return GetValueOr(ScaleXProperty, 1.0);
}
double ScaleTransform::ScaleY() const noexcept {
    return GetValueOr(ScaleYProperty, 1.0);
}
double ScaleTransform::CenterX() const noexcept {
    return GetValueOr(CenterXProperty, 0.0);
}
double ScaleTransform::CenterY() const noexcept {
    return GetValueOr(CenterYProperty, 0.0);
}
Base::Result<void> ScaleTransform::SetScaleX(double value) noexcept {
    return SetValue(ScaleXProperty, value);
}
Base::Result<void> ScaleTransform::SetScaleY(double value) noexcept {
    return SetValue(ScaleYProperty, value);
}
Base::Result<void> ScaleTransform::SetCenterX(double value) noexcept {
    return SetValue(CenterXProperty, value);
}
Base::Result<void> ScaleTransform::SetCenterY(double value) noexcept {
    return SetValue(CenterYProperty, value);
}
Base::Transform2D ScaleTransform::Matrix() const noexcept {
    Base::Transform2D value;
    value.m11 = ScaleX();
    value.m22 = ScaleY();
    return AroundCenter(value, CenterX(), CenterY());
}

double RotateTransform::Angle() const noexcept {
    return GetValueOr(AngleProperty, 0.0);
}
double RotateTransform::CenterX() const noexcept {
    return GetValueOr(CenterXProperty, 0.0);
}
double RotateTransform::CenterY() const noexcept {
    return GetValueOr(CenterYProperty, 0.0);
}
Base::Result<void> RotateTransform::SetAngle(double value) noexcept {
    return SetValue(AngleProperty, value);
}
Base::Result<void> RotateTransform::SetCenterX(double value) noexcept {
    return SetValue(CenterXProperty, value);
}
Base::Result<void> RotateTransform::SetCenterY(double value) noexcept {
    return SetValue(CenterYProperty, value);
}
Base::Transform2D RotateTransform::Matrix() const noexcept {
    const double radians = Angle() * Pi / 180.0;
    const double cosine = std::cos(radians);
    const double sine = std::sin(radians);
    Base::Transform2D value;
    value.m11 = cosine;
    value.m12 = sine;
    value.m21 = -sine;
    value.m22 = cosine;
    return AroundCenter(value, CenterX(), CenterY());
}

double SkewTransform::AngleX() const noexcept {
    return GetValueOr(AngleXProperty, 0.0);
}
double SkewTransform::AngleY() const noexcept {
    return GetValueOr(AngleYProperty, 0.0);
}
double SkewTransform::CenterX() const noexcept {
    return GetValueOr(CenterXProperty, 0.0);
}
double SkewTransform::CenterY() const noexcept {
    return GetValueOr(CenterYProperty, 0.0);
}
Base::Result<void> SkewTransform::SetAngleX(double value) noexcept {
    return SetValue(AngleXProperty, value);
}
Base::Result<void> SkewTransform::SetAngleY(double value) noexcept {
    return SetValue(AngleYProperty, value);
}
Base::Result<void> SkewTransform::SetCenterX(double value) noexcept {
    return SetValue(CenterXProperty, value);
}
Base::Result<void> SkewTransform::SetCenterY(double value) noexcept {
    return SetValue(CenterYProperty, value);
}
Base::Transform2D SkewTransform::Matrix() const noexcept {
    Base::Transform2D value;
    value.m21 = std::tan(AngleX() * Pi / 180.0);
    value.m12 = std::tan(AngleY() * Pi / 180.0);
    return AroundCenter(value, CenterX(), CenterY());
}

Base::Transform2D MatrixTransform::Value() const noexcept {
    return GetValueOr(MatrixProperty, Base::Transform2D{});
}
Base::Result<void> MatrixTransform::SetValue(
    Base::Transform2D value) noexcept {
    return DependencyObject::SetValue(MatrixProperty, value);
}

Base::Result<void> TransformGroup::TryAddChild(
    Base::Ref<Transform> value) noexcept {
    if (!value) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "TransformGroup child cannot be null");
    }
    if (HasOwnerRole(TransformOwnerRole::Render)) {
        value->AttachOwner(
            Owner(),
            TransformOwnerRole::Render);
    }
    if (HasOwnerRole(TransformOwnerRole::Layout)) {
        value->AttachOwner(
            Owner(),
            TransformOwnerRole::Layout);
    }
    Base::Result<void> added =
        children_.TryPushBack(std::move(value));
    if (!added) return added.GetStatus();
    FrameworkElement* owner = Owner();
    if (owner == nullptr) return {};
    return HasOwnerRole(TransformOwnerRole::Layout)
        ? owner->InvalidateMeasure()
        : owner->InvalidateRender();
}

Base::Result<void> TransformGroup::ClearChildren() noexcept {
    for (Base::Ref<Transform>& child : children_) {
        if (!child) continue;
        if (HasOwnerRole(TransformOwnerRole::Render)) {
            child->DetachOwner(
                Owner(),
                TransformOwnerRole::Render);
        }
        if (HasOwnerRole(TransformOwnerRole::Layout)) {
            child->DetachOwner(
                Owner(),
                TransformOwnerRole::Layout);
        }
    }
    children_.Clear();
    FrameworkElement* owner = Owner();
    if (owner == nullptr) return {};
    return HasOwnerRole(TransformOwnerRole::Layout)
        ? owner->InvalidateMeasure()
        : owner->InvalidateRender();
}

void TransformGroup::SetOwner(FrameworkElement* owner) noexcept {
    Transform::SetOwner(owner);
    for (Base::Ref<Transform>& child : children_) {
        if (child) child->SetOwner(owner);
    }
}

void TransformGroup::AttachOwner(
    FrameworkElement* owner,
    TransformOwnerRole role) noexcept {
    Transform::AttachOwner(owner, role);
    for (Base::Ref<Transform>& child : children_) {
        if (child) {
            child->AttachOwner(owner, role);
        }
    }
}

void TransformGroup::DetachOwner(
    FrameworkElement* owner,
    TransformOwnerRole role) noexcept {
    for (Base::Ref<Transform>& child : children_) {
        if (child) {
            child->DetachOwner(owner, role);
        }
    }
    Transform::DetachOwner(owner, role);
}

Base::Transform2D TransformGroup::Matrix() const noexcept {
    Base::Transform2D result;
    for (const Base::Ref<Transform>& child : children_) {
        if (child) {
            result = ComposeTransforms(result, child->Matrix());
        }
    }
    return result;
}

} // namespace Aero::Media
