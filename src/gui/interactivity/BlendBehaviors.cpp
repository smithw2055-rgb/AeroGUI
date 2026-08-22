#include <Aero/Interactivity/BlendBehaviors.hpp>
#include <Aero/Controls.hpp>
#include <Aero/Controls.hpp>
#include <Aero/Shapes.hpp>
#include "gui/meta/MetadataState.hpp"
#include "gui/core/State.hpp"
#include "gui/core/State.hpp"
#include "gui/input/InputState.hpp"
#include "gui/core/State.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "gui/media/MediaState.hpp"

#include <algorithm>
#include <cmath>

namespace Aero::Interactivity {
namespace {

Base::Transform2D Translation(double x, double y) noexcept {
    Base::Transform2D result;
    result.dx = x;
    result.dy = y;
    return result;
}

Base::Transform2D ToRootTransform(const ::Aero::Media::Visual& visual) noexcept {
    Base::Transform2D result;
    const ::Aero::Media::Visual* current = &visual;
    while (current != nullptr) {
        const UIElement* element = current->AsUIElement();
        const FrameworkElement* framework =
            current->AsFrameworkElement();
        if (element != nullptr) {
            Base::Transform2D local = framework != nullptr
                ? framework->GetLocalVisualTransform()
                : Base::Transform2D{};
            const Rect slot = element->GetLayoutSlot();
            local = Media::ComposeTransforms(
                local, Translation(slot.x, slot.y));
            result = Media::ComposeTransforms(result, local);
        }
        current = current->GetVisualParent();
    }
    return result;
}

Base::Result<Base::Ref<Media::Brush>> ReadBackground(
    FrameworkElement& source) noexcept {
    const Meta::PropertyInfo* property =
        source.PropertyRegistry().Types().FindProperty(
            source.RuntimeType(), "Background", false);
    if (property == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "BackgroundEffectBehavior Source has no Background property");
    }
    Meta::PropertyValue value = source.GetValue(
        Meta::DependencyPropertyHandle{property->Id()});
    if (value.Kind() != Meta::ValueKind::Object ||
        value.IsNullObject() || !value.AsObject() ||
        !source.PropertyRegistry().Types().IsDerivedFrom(
            value.AsObject()->RuntimeType(),
            Media::Brush::StaticTypeId())) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "BackgroundEffectBehavior Source background is empty");
    }
    return Base::Ref<Media::Brush>::FromBorrowed(
        *static_cast<Media::Brush*>(value.AsObject().Get()));
}

Base::Result<void> SetShapeFill(
    FrameworkElement& target,
    Base::Ref<Media::Brush> brush) noexcept {
    const Meta::TypeRegistry& types =
        target.PropertyRegistry().Types();
    if (types.IsDerivedFrom(
            target.RuntimeType(), Shapes::Shape::StaticTypeId())) {
        static_cast<Shapes::Shape&>(target).SetFill(std::move(brush));
        return {};
    }
    if (types.IsDerivedFrom(
            target.RuntimeType(), Shapes::Path::StaticTypeId())) {
        static_cast<Shapes::Path&>(target).SetFill(std::move(brush));
        return {};
    }
    return Base::Status::Failure(
        Base::ErrorCode::Unsupported,
        "BackgroundEffectBehavior requires a Shape or Path target");
}

Base::Ref<Media::Brush> GetShapeFill(
    FrameworkElement& target) noexcept {
    const Meta::TypeRegistry& types =
        target.PropertyRegistry().Types();
    if (types.IsDerivedFrom(
            target.RuntimeType(), Shapes::Shape::StaticTypeId())) {
        return static_cast<Shapes::Shape&>(target).GetFill();
    }
    if (types.IsDerivedFrom(
            target.RuntimeType(), Shapes::Path::StaticTypeId())) {
        return static_cast<Shapes::Path&>(target).GetFill();
    }
    return {};
}

Rect NormalizeImageViewbox(
    const Media::ImageBrush& brush) noexcept {
    Rect viewbox = brush.GetViewbox();
    if (brush.GetViewboxUnits() ==
        Media::BrushMappingMode::Absolute) {
        const double width = static_cast<double>(
            Media::BrushPrivate::PixelWidth(brush));
        const double height = static_cast<double>(
            Media::BrushPrivate::PixelHeight(brush));
        if (width > 0.0 && height > 0.0) {
            viewbox = {
                viewbox.x / width,
                viewbox.y / height,
                viewbox.width / width,
                viewbox.height / height};
        }
    }
    return viewbox;
}

Rect DisplayedImageViewbox(
    const Media::ImageBrush& brush,
    Size destination) noexcept {
    Rect uv = NormalizeImageViewbox(brush);
    const double pixelWidth = static_cast<double>(
        Media::BrushPrivate::PixelWidth(brush));
    const double pixelHeight = static_cast<double>(
        Media::BrushPrivate::PixelHeight(brush));
    const double sourceWidth = pixelWidth * std::fabs(uv.width);
    const double sourceHeight = pixelHeight * std::fabs(uv.height);
    if (sourceWidth <= 0.0 || sourceHeight <= 0.0 ||
        destination.width <= 0.0 || destination.height <= 0.0 ||
        brush.GetStretch() == Media::Stretch::Fill ||
        brush.GetStretch() == Media::Stretch::None) {
        return uv;
    }
    const double scaleX = destination.width / sourceWidth;
    const double scaleY = destination.height / sourceHeight;
    if (brush.GetStretch() == Media::Stretch::UniformToFill) {
        const double scale = std::max(scaleX, scaleY);
        const double drawnWidth = sourceWidth * scale;
        const double drawnHeight = sourceHeight * scale;
        const double cropX = 1.0 - destination.width / drawnWidth;
        const double cropY = 1.0 - destination.height / drawnHeight;
        const double alignX = brush.GetAlignmentX() == HorizontalAlignment::Left
            ? 0.0
            : brush.GetAlignmentX() == HorizontalAlignment::Right ? 1.0 : 0.5;
        const double alignY = brush.GetAlignmentY() == VerticalAlignment::Top
            ? 0.0
            : brush.GetAlignmentY() == VerticalAlignment::Bottom ? 1.0 : 0.5;
        uv.x += uv.width * cropX * alignX;
        uv.y += uv.height * cropY * alignY;
        uv.width *= 1.0 - cropX;
        uv.height *= 1.0 - cropY;
    }
    return uv;
}

Base::Result<Base::Ref<Media::ImageBrush>> ProjectImageBrush(
    const Media::ImageBrush& sourceBrush,
    FrameworkElement& source,
    FrameworkElement& target) noexcept {
    Base::Result<Base::Ref<Media::ImageBrush>> created =
        Base::MakeRef<Media::ImageBrush>();
    if (!created) return created.GetStatus();
    Base::Ref<Media::ImageBrush> brush = std::move(created).Value();
    brush->SetSource(sourceBrush.GetSource());
    brush->SetOpacity(sourceBrush.GetOpacity());
    brush->SetStretch(Media::Stretch::Fill);
    brush->SetViewboxUnits(
        Media::BrushMappingMode::RelativeToBoundingBox);
    brush->SetViewport({0.0, 0.0, 1.0, 1.0});
    brush->SetViewportUnits(
        Media::BrushMappingMode::RelativeToBoundingBox);
    brush->SetTileMode(Media::TileMode::None);

    const Rect sourceBounds = Media::TransformBounds(
        ToRootTransform(source),
        {0.0, 0.0,
         source.GetRenderSize().width,
         source.GetRenderSize().height});
    const Rect targetBounds = Media::TransformBounds(
        ToRootTransform(target),
        {0.0, 0.0,
         target.GetRenderSize().width,
         target.GetRenderSize().height});
    Rect displayed = DisplayedImageViewbox(
        sourceBrush, source.GetRenderSize());
    if (sourceBounds.width > 0.0 && sourceBounds.height > 0.0 &&
        targetBounds.width > 0.0 && targetBounds.height > 0.0) {
        const double relativeX =
            (targetBounds.x - sourceBounds.x) / sourceBounds.width;
        const double relativeY =
            (targetBounds.y - sourceBounds.y) / sourceBounds.height;
        const double relativeWidth =
            targetBounds.width / sourceBounds.width;
        const double relativeHeight =
            targetBounds.height / sourceBounds.height;
        displayed = {
            displayed.x + displayed.width * relativeX,
            displayed.y + displayed.height * relativeY,
            displayed.width * relativeWidth,
            displayed.height * relativeHeight};
    }
    brush->SetViewbox(displayed);
    return brush;
}

} // namespace

MouseDragElementBehavior::MouseDragElementBehavior() noexcept
    : Behavior(StaticTypeId()),
      mouseDownHandler_(this, &MouseDragElementBehavior::OnMouseDown),
      mouseMoveHandler_(this, &MouseDragElementBehavior::OnMouseMove),
      mouseUpHandler_(this, &MouseDragElementBehavior::OnMouseUp) {}

Base::Result<void> MouseDragElementBehavior::OnAttached() noexcept {
    FrameworkElement* associated = GetAssociatedObject();
    if (associated == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "MouseDragElementBehavior has no associated object");
    }
    originalTransform_ = associated->GetRenderTransform();
    Base::Result<Base::Ref<Media::TransformGroup>> group =
        Base::MakeRef<Media::TransformGroup>();
    if (!group) return group.GetStatus();
    Base::Result<Base::Ref<Media::TranslateTransform>> translation =
        Base::MakeRef<Media::TranslateTransform>();
    if (!translation) return translation.GetStatus();
    transformGroup_ = std::move(group).Value();
    translation_ = std::move(translation).Value();
    if (originalTransform_) {
        Base::Result<void> appended =
            transformGroup_->AddChild(originalTransform_);
        if (!appended) return appended.GetStatus();
    }
    Base::Result<void> appended =
        transformGroup_->AddChild(
            Base::Ref<Media::Transform>(translation_));
    if (!appended) return appended.GetStatus();
    associated->SetRenderTransform(
        Base::Ref<Media::Transform>(transformGroup_));
    SynchronizeTransform();
    associated->PreviewMouseLeftButtonDown().Add(mouseDownHandler_);
    associated->PreviewMouseMove().Add(mouseMoveHandler_);
    associated->PreviewMouseLeftButtonUp().Add(mouseUpHandler_);
    return {};
}

void MouseDragElementBehavior::OnDetaching() noexcept {
    FrameworkElement* associated = GetAssociatedObject();
    if (associated != nullptr) {
        static_cast<void>(
            associated->PreviewMouseLeftButtonDown().Remove(
                mouseDownHandler_));
        static_cast<void>(
            associated->PreviewMouseMove().Remove(
                mouseMoveHandler_));
        static_cast<void>(
            associated->PreviewMouseLeftButtonUp().Remove(
                mouseUpHandler_));
        if (dragging_ && pointerId_ != UINT32_MAX) {
            Aero::InputRouter* input =
                Aero::Media::Visual::Access::InputRouterFor(
                    *associated);
            if (input != nullptr) {
                static_cast<void>(input->ReleasePointer(pointerId_));
            }
        }
        associated->SetRenderTransform(originalTransform_);
    }
    pointerId_ = UINT32_MAX;
    dragging_ = false;
    translation_.Reset();
    transformGroup_.Reset();
    originalTransform_.Reset();
}

void MouseDragElementBehavior::OnPositionChanged(
    DependencyObject& object,
    const Meta::DependencyPropertyChangedEventArgs&) noexcept {
    static_cast<MouseDragElementBehavior&>(object)
        .SynchronizeTransform();
}

void MouseDragElementBehavior::SynchronizeTransform() noexcept {
    if (!translation_ || updatingPosition_) return;
    translation_->SetX(GetX());
    translation_->SetY(GetY());
}

Base::Point MouseDragElementBehavior::RootPosition(
    Base::Point local) const noexcept {
    FrameworkElement* associated = GetAssociatedObject();
    return associated != nullptr
        ? Media::TransformPoint(ToRootTransform(*associated), local)
        : local;
}

void MouseDragElementBehavior::OnMouseDown(
    Base::Object*,
    MouseButtonEventArgs& args) noexcept {
    if (args.GetChangedButton() != Input::MouseButton::Left ||
        pointerId_ != UINT32_MAX) {
        return;
    }
    FrameworkElement* associated = GetAssociatedObject();
    if (associated == nullptr) return;
    pointerId_ = args.GetPointerId();
    dragStartRoot_ = RootPosition(args.GetPosition());
    dragStartX_ = GetX();
    dragStartY_ = GetY();
    dragging_ = false;
}

void MouseDragElementBehavior::OnMouseMove(
    Base::Object*,
    MouseEventArgs& args) noexcept {
    if (pointerId_ == UINT32_MAX ||
        args.GetPointerId() != pointerId_) {
        return;
    }
    FrameworkElement* associated = GetAssociatedObject();
    if (associated == nullptr) return;
    const Base::Point current = RootPosition(args.GetPosition());
    const double deltaX = current.x - dragStartRoot_.x;
    const double deltaY = current.y - dragStartRoot_.y;
    if (!dragging_) {
        constexpr double DragThreshold = 3.0;
        if (deltaX * deltaX + deltaY * deltaY <
            DragThreshold * DragThreshold) {
            return;
        }
        Aero::InputRouter* input =
            Aero::Media::Visual::Access::InputRouterFor(*associated);
        if (input == nullptr ||
            !input->CapturePointer(pointerId_, *associated)) {
            pointerId_ = UINT32_MAX;
            return;
        }
        dragging_ = true;
    }
    double x = dragStartX_ + deltaX;
    double y = dragStartY_ + deltaY;
    if (GetConstrainToParentBounds()) {
        UIElement* parent = associated->GetVisualParent() != nullptr
            ? associated->GetVisualParent()->AsUIElement()
            : nullptr;
        if (parent != nullptr) {
            const Rect slot = associated->GetLayoutSlot();
            const Size size = associated->GetRenderSize();
            const Size parentSize = parent->GetRenderSize();
            x = std::clamp(
                x,
                -slot.x,
                std::max(-slot.x,
                    parentSize.width - slot.x - size.width));
            y = std::clamp(
                y,
                -slot.y,
                std::max(-slot.y,
                    parentSize.height - slot.y - size.height));
        }
    }
    updatingPosition_ = true;
    SetValue(XProperty, x);
    SetValue(YProperty, y);
    updatingPosition_ = false;
    SynchronizeTransform();
    args.SetHandled(true);
}

void MouseDragElementBehavior::OnMouseUp(
    Base::Object*,
    MouseButtonEventArgs& args) noexcept {
    if (pointerId_ == UINT32_MAX ||
        args.GetPointerId() != pointerId_ ||
        args.GetChangedButton() != Input::MouseButton::Left) {
        return;
    }
    FrameworkElement* associated = GetAssociatedObject();
    if (dragging_ && associated != nullptr) {
        Aero::InputRouter* input =
            Aero::Media::Visual::Access::InputRouterFor(*associated);
        if (input != nullptr) {
            static_cast<void>(input->ReleasePointer(pointerId_));
        }
        args.SetHandled(true);
    }
    pointerId_ = UINT32_MAX;
    dragging_ = false;
}


Base::Ref<FrameworkElement> BackgroundEffectBehavior::GetSource() const noexcept {
    Base::Ref<Base::Object> source = GetValueOr(
        SourceProperty, Base::Ref<Base::Object>{});
    if (!source || !PropertyRegistry().Types().IsDerivedFrom(
            source->RuntimeType(), FrameworkElement::StaticTypeId())) {
        return {};
    }
    return Base::Ref<FrameworkElement>::FromBorrowed(
        *static_cast<FrameworkElement*>(source.Get()));
}

Base::Result<void> BackgroundEffectBehavior::OnAttached() noexcept {
    FrameworkElement* associated = GetAssociatedObject();
    if (associated == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "BackgroundEffectBehavior has no associated object");
    }
    originalFill_ = GetShapeFill(*associated);
    originalEffect_ = associated->GetEffect();
    return Refresh();
}

void BackgroundEffectBehavior::OnDetaching() noexcept {
    FrameworkElement* associated = GetAssociatedObject();
    if (associated != nullptr) {
        static_cast<void>(SetShapeFill(*associated, originalFill_));
        associated->SetEffect(originalEffect_);
    }
    projectedImage_.Reset();
    originalFill_.Reset();
    originalEffect_.Reset();
}

void BackgroundEffectBehavior::OnLayoutUpdated() noexcept {
    static_cast<void>(Refresh());
}

void BackgroundEffectBehavior::OnBehaviorPropertyChanged(
    DependencyObject& object,
    const Meta::DependencyPropertyChangedEventArgs&) noexcept {
    auto& behavior = static_cast<BackgroundEffectBehavior&>(object);
    if (behavior.GetIsAttached()) {
        static_cast<void>(behavior.Refresh());
    }
}

Base::Result<void> BackgroundEffectBehavior::Refresh() noexcept {
    if (updating_) return {};
    FrameworkElement* target = GetAssociatedObject();
    Base::Ref<FrameworkElement> source = GetSource();
    if (target == nullptr || !source) return {};
    Base::Result<Base::Ref<Media::Brush>> background =
        ReadBackground(*source);
    if (!background) return background.GetStatus();
    Base::Ref<Media::Brush> projected = background.Value();
    if (background.Value()->RuntimeType() ==
        Media::ImageBrush::StaticTypeId()) {
        Base::Result<Base::Ref<Media::ImageBrush>> image =
            ProjectImageBrush(
                *static_cast<Media::ImageBrush*>(
                    background.Value().Get()),
                *source,
                *target);
        if (!image) return image.GetStatus();
        projectedImage_ = std::move(image).Value();
        projected = Base::Ref<Media::Brush>(projectedImage_);
    } else {
        projectedImage_.Reset();
    }
    updating_ = true;
    Base::Result<void> fill = SetShapeFill(*target, std::move(projected));
    if (fill) target->SetEffect(GetEffect());
    updating_ = false;
    return fill;
}

} // namespace Aero::Interactivity
