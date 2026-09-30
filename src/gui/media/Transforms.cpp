#include <Aero/Media/Transform2D.hpp>
#include <Aero/Media/Transform3D.hpp>
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include <Aero/FrameworkElement.hpp>

#include <algorithm>
#include <cmath>
#include "gui/core/Describe.hpp"
#include "gui/core/TypeRegistryCore.hpp"
#include "gui/core/RenderStateCallbacks.hpp"
#include "gui/core/ValueConversion.hpp"
#include "gui/data/BindingEngine.hpp"
#include <Aero/Interactivity/Conditions.hpp>
#include <Aero/Interactivity/Behavior.hpp>
#include <Aero/Interactivity/BlendBehaviors.hpp>
#include <Aero/Interactivity/Interaction.hpp>
#include <Aero/Interactivity/InteractionTriggers.hpp>
#include <Aero/Interactivity/TriggerAction.hpp>
#include <Aero/Style.hpp>
#include <Aero/Triggers.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Events/EventArgs.hpp>
#include <Aero/Events/CommandEventArgs.hpp>
#include <Aero/Media/Animation.hpp>
#include <Aero/Media/Animation/MediaActions.hpp>
#include <Aero/Media/Animation/StoryboardActions.hpp>
#include <Aero/Media/Animation/StoryboardCompletedTrigger.hpp>
#include <Aero/Media/Animation/TimerTrigger.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Effects.hpp>
#include <Aero/Media/Images.hpp>
#include <Aero/Media/MediaElement.hpp>
#include <Aero/Media/Geometries.hpp>
#include <Aero/Media/Pen.hpp>
#include <Aero/Media/Fonts.hpp>
#include <Aero/Layout.hpp>
#include <Aero/Collections.hpp>
#include <Aero/Input.hpp>
#include <Aero/ICommand.hpp>
#include <Aero/RoutedCommand.hpp>
#include <Aero/InputBinding.hpp>
#include <Aero/EventSetter.hpp>
#include <Aero/KeyboardNavigation.hpp>
#include <Aero/CommandBinding.hpp>
#include <Aero/ApplicationCommands.hpp>
#include <Aero/InputGesture.hpp>
#include <Aero/Data/Binding.hpp>
#include <Aero/Data/MultiBinding.hpp>
#include <Aero/Data/BooleanToVisibilityConverter.hpp>
#include <Aero/Data/IMultiValueConverter.hpp>
#include <Aero/Data/IValueConverter.hpp>
#include <Aero/DataObject.hpp>
#include <Aero/DragDrop.hpp>
#include <Aero/Input/Cursor.hpp>
#include <Aero/Input/Mouse.hpp>
#include <Aero/Input/Keyboard.hpp>
#include <Aero/Animatable.hpp>
#include <cctype>
#include <cstdlib>
#include <utility>
#include "gui/core/DependencyObjectAccess.hpp"

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
    if (!DependencyObjectAccess::PropertyRegistry((value)).Types().IsDerivedFrom(
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

std::uint64_t Transform::GetRevision() const noexcept {
    return (*this).Revision();
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

Base::Transform3 CompositeTransform3D::GetTransform3D() const noexcept {
    constexpr double DegToRad = Pi / 180.0;
    const double cx = GetCenterX();
    const double cy = GetCenterY();
    const double cz = GetCenterZ();
    Base::Transform3 transform = Base::MakeTranslate3(-cx, -cy, -cz);
    transform = Base::Compose(
        transform,
        Base::MakeScale3(GetScaleX(), GetScaleY(), GetScaleZ()));
    transform = Base::Compose(
        transform,
        Base::MakeRotationX(GetRotationX() * DegToRad));
    transform = Base::Compose(
        transform,
        Base::MakeRotationY(GetRotationY() * DegToRad));
    transform = Base::Compose(
        transform,
        Base::MakeRotationZ(GetRotationZ() * DegToRad));
    transform = Base::Compose(
        transform,
        Base::MakeTranslate3(
            cx + GetTranslateX(),
            cy + GetTranslateY(),
            cz + GetTranslateZ()));
    return transform;
}

Base::Transform3 PerspectiveTransform3D::GetTransform3D() const noexcept {
    return Base::IdentityTransform3();
}

Base::Transform3 MatrixTransform3D::GetTransform3D() const noexcept {
    return GetMatrix();
}

double TranslateTransform::GetX() const noexcept {
    return GetValue(XProperty);
}
double TranslateTransform::GetY() const noexcept {
    return GetValue(YProperty);
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
    return GetValue(ScaleXProperty);
}
double ScaleTransform::GetScaleY() const noexcept {
    return GetValue(ScaleYProperty);
}
double ScaleTransform::GetCenterX() const noexcept {
    return GetValue(CenterXProperty);
}
double ScaleTransform::GetCenterY() const noexcept {
    return GetValue(CenterYProperty);
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
    return GetValue(AngleProperty);
}
double RotateTransform::GetCenterX() const noexcept {
    return GetValue(CenterXProperty);
}
double RotateTransform::GetCenterY() const noexcept {
    return GetValue(CenterYProperty);
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
    return GetValue(AngleXProperty);
}
double SkewTransform::GetAngleY() const noexcept {
    return GetValue(AngleYProperty);
}
double SkewTransform::GetCenterX() const noexcept {
    return GetValue(CenterXProperty);
}
double SkewTransform::GetCenterY() const noexcept {
    return GetValue(CenterYProperty);
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

Base::Transform2D CompositeTransform::GetMatrix() const noexcept {
    Base::Transform2D scale;
    scale.m11 = GetScaleX();
    scale.m22 = GetScaleY();
    Base::Transform2D skew;
    skew.m21 = std::tan(GetSkewX() * Pi / 180.0);
    skew.m12 = std::tan(GetSkewY() * Pi / 180.0);
    const double radians = GetRotation() * Pi / 180.0;
    const double cosine = std::cos(radians);
    const double sine = std::sin(radians);
    Base::Transform2D rotate;
    rotate.m11 = cosine;
    rotate.m12 = sine;
    rotate.m21 = -sine;
    rotate.m22 = cosine;
    Base::Transform2D translate;
    translate.dx = GetTranslateX();
    translate.dy = GetTranslateY();
    Base::Transform2D composed = ComposeTransforms(scale, skew);
    composed = ComposeTransforms(composed, rotate);
    composed = AroundCenter(composed, GetCenterX(), GetCenterY());
    return ComposeTransforms(composed, translate);
}

Base::Transform2D MatrixTransform::GetMatrixValue() const noexcept {
    return GetValue(MatrixProperty);
}
void MatrixTransform::SetMatrixValue(
    Base::Transform2D value) noexcept {
    DependencyObject::SetValue(MatrixProperty, value);
}

void TransformGroup::AddChild(
    Base::Ref<Transform> value) noexcept {
    Base::Result<void> writable = WritePreamble();
    if (!writable) { AERO_ASSERT(false); return; }
    if (!value) { AERO_ASSERT(false); return; }
    if (ContainsTransform(*value, this)) { AERO_ASSERT(false); return; }
    if (childChangedHandler_.Empty()) {
        childChangedHandler_ = FreezableChangedHandler(
            this, &TransformGroup::OnChildChanged);
    }
    Transform* retained = value.Get();
    if (!retained->IsFrozen()) {
        retained->AddChangedHandler(childChangedHandler_);
    }
    children_.Add(std::move(value));
    WritePostscript();
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

// Metadata registration for the types implemented in this file.
namespace Aero::MetadataSupport {
using namespace ::Aero::Meta;
using namespace ::Aero::Threading;
using namespace ::Aero::Input;
using namespace ::Aero::Media;
using namespace ::Aero::Data;
using namespace ::Aero::Interactivity;
    using namespace Interactivity;
    using Media::Animation::BeginStoryboard;
    using Media::Animation::BooleanAnimationUsingKeyFrames;
    using Media::Animation::BooleanKeyFrame;
    using Media::Animation::ColorAnimationUsingKeyFrames;
    using Media::Animation::ColorKeyFrame;
    using Media::Animation::DoubleAnimationUsingKeyFrames;
    using Media::Animation::DoubleKeyFrame;
    using Media::Animation::EventTrigger;
    using Media::Animation::Int16AnimationUsingKeyFrames;
    using Media::Animation::Int16KeyFrame;
    using Media::Animation::Int32AnimationUsingKeyFrames;
    using Media::Animation::Int32KeyFrame;
    using Media::Animation::Int64AnimationUsingKeyFrames;
    using Media::Animation::Int64KeyFrame;
    using Media::Animation::MatrixAnimationUsingKeyFrames;
    using Media::Animation::MatrixKeyFrame;
    using Media::Animation::ObjectAnimationUsingKeyFrames;
    using Media::Animation::ObjectKeyFrame;
    using Media::Animation::PointAnimationUsingKeyFrames;
    using Media::Animation::PointKeyFrame;
    using Media::Animation::SizeAnimationUsingKeyFrames;
    using Media::Animation::SizeKeyFrame;
    using Media::Animation::Storyboard;
    using Media::Animation::StoryboardCompletedTrigger;
    using Media::Animation::StringAnimationUsingKeyFrames;
    using Media::Animation::StringKeyFrame;
    using Media::Animation::ThicknessAnimationUsingKeyFrames;
    using Media::Animation::ThicknessKeyFrame;
    using Media::Animation::Timeline;
    using Media::Animation::TimelineGroup;
    using Media::Effect;
    using Media::FontFamily;
    using Media::Geometry;
    using Media::GeometryGroup;
    using Media::PathFigure;
    using Media::PathGeometry;
    using Media::PathSegment;
    using Media::StreamGeometry;
namespace {

void AddTransformGroupChild(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<Transform> retained =
        Base::Ref<Transform>::TryFromBorrowed(
            static_cast<Transform&>(*value));
    if (!retained) {
        return;
    }
    static_cast<void>(
        static_cast<TransformGroup&>(owner)
            .AddChild(std::move(retained)));
}

void ClearTransformGroupChildren(
    Base::Object& owner,
    void*) noexcept {
    static_cast<TransformGroup&>(owner).ClearChildren();
    return;
}

} // namespace
} // namespace Aero::MetadataSupport

namespace Aero::Media {

AERO_DESCRIBE(Transform) {
    using namespace Aero::Meta;
    Register<Transform>(context, TypeFlags::Abstract);
}

AERO_DESCRIBE(TranslateTransform) {
    using namespace Aero::Meta;
    Register<TranslateTransform>(context)
            .Property(TranslateTransform::XProperty, 0.0, AffectsRender)
            .Property(TranslateTransform::YProperty, 0.0, AffectsRender)
            .Factory();
}

AERO_DESCRIBE(ScaleTransform) {
    using namespace Aero::Meta;
    Register<ScaleTransform>(context)
            .Property(ScaleTransform::ScaleXProperty, 1.0, AffectsRender)
            .Property(ScaleTransform::ScaleYProperty, 1.0, AffectsRender)
            .Property(ScaleTransform::CenterXProperty, 0.0, AffectsRender)
            .Property(ScaleTransform::CenterYProperty, 0.0, AffectsRender)
            .Factory();
}

AERO_DESCRIBE(RotateTransform) {
    using namespace Aero::Meta;
    Register<RotateTransform>(context)
            .Property(RotateTransform::AngleProperty, 0.0, AffectsRender)
            .Property(RotateTransform::CenterXProperty, 0.0, AffectsRender)
            .Property(RotateTransform::CenterYProperty, 0.0, AffectsRender)
            .Factory();
}

AERO_DESCRIBE(SkewTransform) {
    using namespace Aero::Meta;
    Register<SkewTransform>(context)
            .Property(SkewTransform::AngleXProperty, 0.0, AffectsRender)
            .Property(SkewTransform::AngleYProperty, 0.0, AffectsRender)
            .Property(SkewTransform::CenterXProperty, 0.0, AffectsRender)
            .Property(SkewTransform::CenterYProperty, 0.0, AffectsRender)
            .Factory();
}

AERO_DESCRIBE(MatrixTransform) {
    using namespace Aero::Meta;
    Register<MatrixTransform>(context)
            .Property(MatrixTransform::MatrixProperty, Base::Transform2D{}, AffectsRender)
            .Factory();
}

AERO_DESCRIBE(CompositeTransform) {
    using namespace Aero::Meta;
    Register<CompositeTransform>(context)
            .Property(CompositeTransform::CenterXProperty, 0.0, AffectsRender)
            .Property(CompositeTransform::CenterYProperty, 0.0, AffectsRender)
            .Property(CompositeTransform::ScaleXProperty, 1.0, AffectsRender)
            .Property(CompositeTransform::ScaleYProperty, 1.0, AffectsRender)
            .Property(CompositeTransform::SkewXProperty, 0.0, AffectsRender)
            .Property(CompositeTransform::SkewYProperty, 0.0, AffectsRender)
            .Property(CompositeTransform::RotationProperty, 0.0, AffectsRender)
            .Property(CompositeTransform::TranslateXProperty, 0.0, AffectsRender)
            .Property(CompositeTransform::TranslateYProperty, 0.0, AffectsRender)
            .Factory();
}

AERO_DESCRIBE(TransformGroup) {
    using namespace Aero::Meta;
    Register<TransformGroup>(context)
            .Content<Transform>("Children", ContentKind::Collection, &::Aero::MetadataSupport::AddTransformGroupChild, &::Aero::MetadataSupport::ClearTransformGroupChildren)
            .Factory();
}

AERO_DESCRIBE(Transform3D) {
    using namespace Aero::Meta;
    Register<Transform3D>(context, TypeFlags::Abstract);
}

AERO_DESCRIBE(CompositeTransform3D) {
    using namespace Aero::Meta;
    Register<CompositeTransform3D>(context)
            .Property(CompositeTransform3D::CenterXProperty, 0.0, AffectsRender)
            .Property(CompositeTransform3D::CenterYProperty, 0.0, AffectsRender)
            .Property(CompositeTransform3D::CenterZProperty, 0.0, AffectsRender)
            .Property(CompositeTransform3D::RotationXProperty, 0.0, AffectsRender)
            .Property(CompositeTransform3D::RotationYProperty, 0.0, AffectsRender)
            .Property(CompositeTransform3D::RotationZProperty, 0.0, AffectsRender)
            .Property(CompositeTransform3D::ScaleXProperty, 1.0, AffectsRender)
            .Property(CompositeTransform3D::ScaleYProperty, 1.0, AffectsRender)
            .Property(CompositeTransform3D::ScaleZProperty, 1.0, AffectsRender)
            .Property(CompositeTransform3D::TranslateXProperty, 0.0, AffectsRender)
            .Property(CompositeTransform3D::TranslateYProperty, 0.0, AffectsRender)
            .Property(CompositeTransform3D::TranslateZProperty, 0.0, AffectsRender)
            .Factory();
}

AERO_DESCRIBE(PerspectiveTransform3D) {
    using namespace Aero::Meta;
    Register<PerspectiveTransform3D>(context)
            .Property(PerspectiveTransform3D::DepthProperty, Base::DefaultPerspectiveDepth, AffectsRender)
            .Property(PerspectiveTransform3D::OffsetXProperty, 0.0, AffectsRender)
            .Property(PerspectiveTransform3D::OffsetYProperty, 0.0, AffectsRender)
            .Factory();
}

AERO_DESCRIBE(MatrixTransform3D) {
    using namespace Aero::Meta;
    Register<MatrixTransform3D>(context)
            .Property(MatrixTransform3D::MatrixProperty, Base::IdentityTransform3(), AffectsRender)
            .Factory();
}

} // namespace Aero::Media

