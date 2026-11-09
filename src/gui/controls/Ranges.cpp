#include <Aero/Controls.hpp>
#include <Aero/Value.hpp>
#include <algorithm>
#include <cmath>
#include "gui/core/ValueConversion.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/input/InputManager.hpp"
#include "render/DisplayList.hpp"
#include <Aero/Input/Mouse.hpp>
#include <Aero/TryCast.hpp>
#include <limits>
#include "gui/templates/TemplateInstance.hpp"
#include "gui/core/DependencyObjectAccess.hpp"
#include "gui/core/Describe.hpp"
#include <Aero/ClassHandler.hpp>
#include <Aero/VisualStateManager.hpp>
#include "gui/media/BrushRendering.hpp"
#include <Aero/Events.hpp>

#include "gui/controls/ScrollSupport.hpp"

namespace Aero::Controls {

using ScrollSupport::LayoutInfinity;
using ScrollSupport::Same;
using ScrollSupport::SameData;
using ScrollSupport::ValidNonnegative;
using ScrollSupport::ValidData;
using ScrollSupport::ComputeScrollBarVisibility;
using ScrollSupport::ClampOffset;
using ScrollSupport::ReadDouble;
using ScrollSupport::ReadBool;
using ScrollSupport::ReadOrientation;
using ScrollSupport::StoreDouble;
using ScrollSupport::StoreOrientation;

// ---------------------------------------------------------------------------
// ScrollBar, Track, Slider, ProgressBar, GridSplitter Implementation
// ---------------------------------------------------------------------------

using namespace Primitives;
using namespace ::Aero::Render;


Orientation Track::GetOrientation() const noexcept {
    return ReadOrientation(*this, OrientationProperty);
}

double Track::GetMinimum() const noexcept {
    return ReadDouble(*this, MinimumProperty);
}

double Track::GetMaximum() const noexcept {
    return ReadDouble(*this, MaximumProperty);
}

double Track::GetValue() const noexcept {
    return ReadDouble(*this, ValueProperty);
}

double Track::GetViewportSize() const noexcept {
    return ReadDouble(*this, ViewportSizeProperty);
}

double GridSplitter::GetDragIncrement() const noexcept {
    return GetValue(DragIncrementProperty);
}

double GridSplitter::GetKeyboardIncrement() const noexcept {
    return GetValue(KeyboardIncrementProperty);
}

GridResizeDirection GridSplitter::GetResizeDirection() const noexcept {
    return GetValue(ResizeDirectionProperty);
}

GridResizeBehavior GridSplitter::GetResizeBehavior() const noexcept {
    return GetValue(ResizeBehaviorProperty);
}

bool GridSplitter::GetShowsPreview() const noexcept {
    return GetValue(ShowsPreviewProperty);
}

Base::Ref<Aero::Style> GridSplitter::GetPreviewStyle() const noexcept {
    return GetValue(PreviewStyleProperty);
}

void GridSplitter::SetDragIncrement(double value) noexcept {
    SetValue(DragIncrementProperty, value);
}

void GridSplitter::SetKeyboardIncrement(double value) noexcept {
    SetValue(KeyboardIncrementProperty, value);
}

void GridSplitter::SetResizeDirection(
    GridResizeDirection value) noexcept {
    SetValue(ResizeDirectionProperty, value);
}

void GridSplitter::SetResizeBehavior(
    GridResizeBehavior value) noexcept {
    SetValue(ResizeBehaviorProperty, value);
}

void GridSplitter::SetShowsPreview(bool value) noexcept {
    SetValue(ShowsPreviewProperty, value);
}

void GridSplitter::SetPreviewStyle(
    Base::Ref<Aero::Style> value) noexcept {
    SetValue(PreviewStyleProperty, std::move(value));
}

bool Track::GetIsDirectionReversed() const noexcept {
    return GetValue(IsDirectionReversedProperty);
}

void Track::SetOrientation(
    Orientation value) noexcept {
    StoreOrientation(*this, OrientationProperty, value);
}

void Track::SetRange(
    double minimum,
    double maximum) noexcept {
    if (!std::isfinite(minimum) ||
        !std::isfinite(maximum) ||
        maximum < minimum) {
        return;
    }
    StoreDouble(*this, MinimumProperty, minimum);
    StoreDouble(*this, MaximumProperty, maximum);
    SetValue(GetValue());
}

void Track::SetValue(
    double value) noexcept {
    if (!std::isfinite(value)) {
        return;
    }
    if (GetMaximum() < GetMinimum()) {
        return;
    }
    StoreDouble(*this, ValueProperty,
        std::clamp(value, GetMinimum(), GetMaximum()));
}

void Track::SetViewportSize(
    double value) noexcept {
    if (!ValidNonnegative(value)) {
        return;
    }
    StoreDouble(*this, ViewportSizeProperty, value);
}

void Track::SetIsDirectionReversed(
    bool value) noexcept {
    DependencyObject::SetValue(IsDirectionReversedProperty, value);
}

void Track::SetDecreaseRepeatButton(
    Base::Ref<RepeatButton> value) noexcept {
    if (decreaseRepeatButton_ == value) return;
    if (decreaseRepeatButton_ != nullptr) {
        if (ElementTree* tree = VisualTree(this)) {
            Aero::ElementAttachment state{};
            state.logicalParent = this;
            state.visualParent = this;
            state.child = decreaseRepeatButton_.Get();
            state.logicalAttached = true;
            state.visualAttached = true;
            state.layoutAttached = decreaseRepeatButton_->GetIsLayoutAttached();
            state.renderAttached = decreaseRepeatButton_->IsRenderAttached();
            static_cast<void>(tree->DetachElement(state));
        } else if (decreaseRepeatButton_->GetVisualParent() == this) {
            RemoveVisualChild(decreaseRepeatButton_.Get());
        }
    }
    decreaseRepeatButton_ = std::move(value);
    if (decreaseRepeatButton_ != nullptr) {
        if (ElementTree* tree = VisualTree(this)) {
            static_cast<void>(tree->AttachElement(*this, *decreaseRepeatButton_));
        } else if (decreaseRepeatButton_->GetVisualParent() != this) {
            AddVisualChild(decreaseRepeatButton_.Get());
        }
    }
    InvalidateMeasure();
}

void Track::SetThumb(
    Base::Ref<Thumb> value) noexcept {
    if (thumb_ == value) return;
    if (thumb_ != nullptr) {
        if (ElementTree* tree = VisualTree(this)) {
            Aero::ElementAttachment state{};
            state.logicalParent = this;
            state.visualParent = this;
            state.child = thumb_.Get();
            state.logicalAttached = true;
            state.visualAttached = true;
            state.layoutAttached = thumb_->GetIsLayoutAttached();
            state.renderAttached = thumb_->IsRenderAttached();
            static_cast<void>(tree->DetachElement(state));
        } else if (thumb_->GetVisualParent() == this) {
            RemoveVisualChild(thumb_.Get());
        }
    }
    thumb_ = std::move(value);
    if (thumb_ != nullptr) {
        if (ElementTree* tree = VisualTree(this)) {
            static_cast<void>(tree->AttachElement(*this, *thumb_));
        } else if (thumb_->GetVisualParent() != this) {
            AddVisualChild(thumb_.Get());
        }
    }
    InvalidateMeasure();
}

void Track::SetIncreaseRepeatButton(
    Base::Ref<RepeatButton> value) noexcept {
    if (increaseRepeatButton_ == value) return;
    if (increaseRepeatButton_ != nullptr) {
        if (ElementTree* tree = VisualTree(this)) {
            Aero::ElementAttachment state{};
            state.logicalParent = this;
            state.visualParent = this;
            state.child = increaseRepeatButton_.Get();
            state.logicalAttached = true;
            state.visualAttached = true;
            state.layoutAttached = increaseRepeatButton_->GetIsLayoutAttached();
            state.renderAttached = increaseRepeatButton_->IsRenderAttached();
            static_cast<void>(tree->DetachElement(state));
        } else if (increaseRepeatButton_->GetVisualParent() == this) {
            RemoveVisualChild(increaseRepeatButton_.Get());
        }
    }
    increaseRepeatButton_ = std::move(value);
    if (increaseRepeatButton_ != nullptr) {
        if (ElementTree* tree = VisualTree(this)) {
            static_cast<void>(tree->AttachElement(*this, *increaseRepeatButton_));
        } else if (increaseRepeatButton_->GetVisualParent() != this) {
            AddVisualChild(increaseRepeatButton_.Get());
        }
    }
    InvalidateMeasure();
}

double Track::GetThumbLength(
    double trackLength,
    double minimumThumbLength) const noexcept {
    if (!ValidNonnegative(trackLength) ||
        !ValidNonnegative(minimumThumbLength) ||
        trackLength == 0.0) {
        return 0.0;
    }
    const double range =
        std::max(0.0, GetMaximum() - GetMinimum());
    const double viewport = GetViewportSize();
    if (range == 0.0) return trackLength;
    const double proportional =
        viewport > 0.0
        ? trackLength * viewport / (viewport + range)
        : minimumThumbLength;
    return std::clamp(
        proportional,
        std::min(minimumThumbLength, trackLength),
        trackLength);
}

double Track::GetThumbOffset(
    double trackLength,
    double minimumThumbLength) const noexcept {
    const double travel =
        trackLength - GetThumbLength(
            trackLength, minimumThumbLength);
    const double range =
        GetMaximum() - GetMinimum();
    if (travel <= 0.0 || range <= 0.0) return 0.0;
    const double offset = travel *
        (std::clamp(GetValue(), GetMinimum(), GetMaximum()) -
            GetMinimum()) /
        range;
    const bool isRtl = GetFlowDirection() == FlowDirection::RightToLeft;
    const bool invert =
        GetOrientation() == Orientation::Vertical
        ? !GetIsDirectionReversed()
        : (isRtl ? !GetIsDirectionReversed() : GetIsDirectionReversed());
    return invert
        ? travel - offset
        : offset;
}

double Track::ValueFromThumbOffset(
    double offset,
    double trackLength,
    double minimumThumbLength) const noexcept {
    if (!std::isfinite(offset) ||
        !ValidNonnegative(trackLength) ||
        !ValidNonnegative(minimumThumbLength)) {
        return GetMinimum();
    }
    const double travel =
        trackLength - GetThumbLength(
            trackLength, minimumThumbLength);
    const double range =
        GetMaximum() - GetMinimum();
    if (travel <= 0.0 || range <= 0.0) {
        return GetMinimum();
    }
    double normalized =
        std::clamp(offset, 0.0, travel) / travel;
    const bool isRtl = GetFlowDirection() == FlowDirection::RightToLeft;
    const bool invert =
        GetOrientation() == Orientation::Vertical
        ? !GetIsDirectionReversed()
        : (isRtl ? !GetIsDirectionReversed() : GetIsDirectionReversed());
    if (invert) {
        normalized = 1.0 - normalized;
    }
    return GetMinimum() + normalized * range;
}

Size Track::MeasureOverride(
    Size availableSize) noexcept {
    if (ElementTree* tree = VisualTree(this)) {
        const auto ensureAttached = [&](UIElement* child) {
            if (child == nullptr) return;
            if (VisualTree(child) == nullptr && child->GetLogicalParent() == nullptr) {
                static_cast<void>(tree->AttachElement(*this, *child));
            } else if (child->GetVisualParent() != this || !child->GetIsLayoutAttached()) {
                static_cast<void>(tree->AttachVisualChild(*this, *child));
            }
        };
        ensureAttached(decreaseRepeatButton_.Get());
        ensureAttached(thumb_.Get());
        ensureAttached(increaseRepeatButton_.Get());
    }
    Size desired{};
    if (thumb_ != nullptr) {
        if (!thumb_->IsTemplateApplied()) {
            thumb_->ApplyTemplate();
        }
        MeasureChild(*thumb_, availableSize);
        desired = thumb_->GetDesiredSize();
    }
    if (decreaseRepeatButton_ != nullptr) {
        MeasureChild(*decreaseRepeatButton_, availableSize);
        if (GetOrientation() == Orientation::Vertical) {
            desired.width = std::max(desired.width, decreaseRepeatButton_->GetDesiredSize().width);
        } else {
            desired.height = std::max(desired.height, decreaseRepeatButton_->GetDesiredSize().height);
        }
    }
    if (increaseRepeatButton_ != nullptr) {
        MeasureChild(*increaseRepeatButton_, availableSize);
        if (GetOrientation() == Orientation::Vertical) {
            desired.width = std::max(desired.width, increaseRepeatButton_->GetDesiredSize().width);
        } else {
            desired.height = std::max(desired.height, increaseRepeatButton_->GetDesiredSize().height);
        }
    }
    return desired;
}

Size Track::ArrangeOverride(
    Size finalSize) noexcept {
    const bool vertical =
        GetOrientation() == Orientation::Vertical;
    const double length =
        vertical ? finalSize.height : finalSize.width;
    const double desiredThumbLength = thumb_
        ? std::max(0.0, vertical
            ? static_cast<double>(thumb_->GetDesiredSize().height)
            : static_cast<double>(thumb_->GetDesiredSize().width))
        : 8.0;
    const double thumbLength =
        GetThumbLength(length, desiredThumbLength);
    const double thumbOffset =
        GetThumbOffset(length, desiredThumbLength);
    const double before = thumbOffset;
    const double after = std::max(
        0.0, length - thumbOffset - thumbLength);
    const bool isRtl = GetFlowDirection() == FlowDirection::RightToLeft;
    const bool invert = vertical
        ? !GetIsDirectionReversed()
        : (isRtl ? !GetIsDirectionReversed() : GetIsDirectionReversed());
    RepeatButton* first = invert
        ? increaseRepeatButton_.Get()
        : decreaseRepeatButton_.Get();
    RepeatButton* last = invert
        ? decreaseRepeatButton_.Get()
        : increaseRepeatButton_.Get();
    const auto arrange = [&](UIElement* child,
                             double offset,
                             double extent) noexcept
        -> Base::Result<void> {
        if (child == nullptr) return {};
        ArrangeChild(
            *child,
            vertical
                ? Rect{0.0, offset,
                       finalSize.width, extent}
                : Rect{offset, 0.0,
                       extent, finalSize.height});
        return {};
    };
    // Arrange independently: a RepeatButton that is not yet a layout
    // child must not skip the Thumb (QuestLog's 3px gold indicator).
    static_cast<void>(arrange(first, 0.0, before));
    static_cast<void>(arrange(
        thumb_.Get(), thumbOffset, thumbLength));
    static_cast<void>(arrange(
        last,
        thumbOffset + thumbLength,
        after));
    return finalSize;
}

Orientation ScrollBar::GetOrientation() const noexcept {
    return ReadOrientation(*this, OrientationProperty);
}

ScrollBar::ScrollBar() noexcept
    : RangeBase(StaticTypeId()) {
}

ScrollBar::~ScrollBar() {
    if (dragging_) {
        static_cast<void>(ReleasePointer(pointerId_));
        dragging_ = false;
    }
}

void ScrollBar::OnApplyTemplate() noexcept {
    Control::OnApplyTemplate();
    DependencyObject* part =
        GetTemplateChild("PART_Track");
    track_ =
        part != nullptr &&
        DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
            part->RuntimeType(),
            Track::StaticTypeId())
        ? static_cast<Track*>(part)
        : nullptr;
    SynchronizeTrack();
}

void ScrollBar::OnTemplateDetached() noexcept {
    track_ = nullptr;
    Control::OnTemplateDetached();
}

void ScrollBar::OnVisualParentChanged(Visual* oldParent) noexcept {
    Control::OnVisualParentChanged(oldParent);
    if (GetVisualParent() == nullptr && dragging_) {
        static_cast<void>(ReleasePointer(pointerId_));
        dragging_ = false;
    }
}


void ScrollBar::OnMouseLeftButtonDown(MouseButtonEventArgs& args) {
    if (args.GetHandled() || !GetIsEnabled() || args.GetChangedButton() != MouseButton::Left) {
        return;
    }
    const Point local = args.GetPosition();
    const bool horizontal = GetOrientation() == Orientation::Horizontal;
    const double position = horizontal ? local.x : local.y;
    const double length = horizontal ? GetRenderSize().width : GetRenderSize().height;
    const double range = GetMaximum() - GetMinimum();
    const double normalized = range > 0.0 ? (GetValue() - GetMinimum()) / range : 0.0;
    const double thumbPosition = std::clamp(normalized, 0.0, 1.0) * std::max(0.0, length - 16.0);
    const bool isThumb = std::fabs(position - thumbPosition) <= 16.0;

    if (isThumb) {
        dragging_ = true;
        pointerId_ = args.GetPointerId();
        dragOrigin_ = local;
        dragStartValue_ = GetValue();
        static_cast<void>(CapturePointer(args.GetPointerId()));
        args.SetHandled(true);
    } else if (position < thumbPosition) {
        static_cast<void>(PageDecrement());
        args.SetHandled(true);
    } else {
        static_cast<void>(PageIncrement());
        args.SetHandled(true);
    }
}

void ScrollBar::OnMouseMove(MouseEventArgs& args) {
    if (!dragging_ || pointerId_ != args.GetPointerId()) {
        return;
    }
    const Point current = args.GetPosition();
    const Point origin = dragOrigin_;
    const double range = GetMaximum() - GetMinimum();
    if (range <= 0.0) return;

    if (GetOrientation() == Orientation::Vertical) {
        const double trackHeight = GetRenderSize().height;
        const double available = std::max(1.0, trackHeight - 16.0);
        const double deltaY = current.y - origin.y;
        const double deltaVal = deltaY * range / available;
        SetValue(std::clamp(dragStartValue_ + deltaVal, GetMinimum(), GetMaximum()));
        args.SetHandled(true);
    } else {
        const double trackWidth = GetRenderSize().width;
        const double available = std::max(1.0, trackWidth - 16.0);
        const double deltaX = current.x - origin.x;
        const double deltaVal = deltaX * range / available;
        SetValue(std::clamp(dragStartValue_ + deltaVal, GetMinimum(), GetMaximum()));
        args.SetHandled(true);
    }
}

void ScrollBar::OnMouseLeftButtonUp(MouseButtonEventArgs& args) {
    if (args.GetChangedButton() != MouseButton::Left || !dragging_) {
        return;
    }
    dragging_ = false;
    static_cast<void>(ReleasePointer(pointerId_));
    args.SetHandled(true);
}

void ScrollBar::OnKeyDown(KeyEventArgs& args) {
    if (!GetIsEnabled()) return;
    if (args.GetKey() == KeyboardKeyHome) {
        SetValue(GetMinimum());
        args.SetHandled(true);
    } else if (args.GetKey() == KeyboardKeyEnd) {
        SetValue(GetMaximum());
        args.SetHandled(true);
    } else if (args.GetKey() == KeyboardKeyUp || args.GetKey() == KeyboardKeyLeft) {
        static_cast<void>(LineDecrement());
        args.SetHandled(true);
    } else if (args.GetKey() == KeyboardKeyDown || args.GetKey() == KeyboardKeyRight) {
        static_cast<void>(LineIncrement());
        args.SetHandled(true);
    } else if (args.GetKey() == KeyboardKeyPageUp) {
        static_cast<void>(PageDecrement());
        args.SetHandled(true);
    } else if (args.GetKey() == KeyboardKeyPageDown) {
        static_cast<void>(PageIncrement());
        args.SetHandled(true);
    }
}

void ScrollBar::OnLineUpCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept {
    auto* bar = static_cast<ScrollBar*>(sender);
    if (bar != nullptr) {
        static_cast<void>(bar->LineDecrement());
        args.SetHandled(true);
    }
}
void ScrollBar::OnLineDownCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept {
    auto* bar = static_cast<ScrollBar*>(sender);
    if (bar != nullptr) {
        static_cast<void>(bar->LineIncrement());
        args.SetHandled(true);
    }
}
void ScrollBar::OnLineLeftCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept {
    auto* bar = static_cast<ScrollBar*>(sender);
    if (bar != nullptr) {
        static_cast<void>(bar->LineDecrement());
        args.SetHandled(true);
    }
}
void ScrollBar::OnLineRightCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept {
    auto* bar = static_cast<ScrollBar*>(sender);
    if (bar != nullptr) {
        static_cast<void>(bar->LineIncrement());
        args.SetHandled(true);
    }
}
void ScrollBar::OnPageUpCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept {
    auto* bar = static_cast<ScrollBar*>(sender);
    if (bar != nullptr) {
        static_cast<void>(bar->PageDecrement());
        args.SetHandled(true);
    }
}
void ScrollBar::OnPageDownCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept {
    auto* bar = static_cast<ScrollBar*>(sender);
    if (bar != nullptr) {
        static_cast<void>(bar->PageIncrement());
        args.SetHandled(true);
    }
}
void ScrollBar::OnPageLeftCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept {
    auto* bar = static_cast<ScrollBar*>(sender);
    if (bar != nullptr) {
        static_cast<void>(bar->PageDecrement());
        args.SetHandled(true);
    }
}
void ScrollBar::OnPageRightCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept {
    auto* bar = static_cast<ScrollBar*>(sender);
    if (bar != nullptr) {
        static_cast<void>(bar->PageIncrement());
        args.SetHandled(true);
    }
}
void ScrollBar::OnScrollToTopCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept {
    auto* bar = static_cast<ScrollBar*>(sender);
    if (bar != nullptr) {
        bar->SetValue(bar->GetMinimum());
        args.SetHandled(true);
    }
}
void ScrollBar::OnScrollToBottomCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept {
    auto* bar = static_cast<ScrollBar*>(sender);
    if (bar != nullptr) {
        bar->SetValue(bar->GetMaximum());
        args.SetHandled(true);
    }
}
void ScrollBar::OnScrollToLeftEndCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept {
    auto* bar = static_cast<ScrollBar*>(sender);
    if (bar != nullptr) {
        bar->SetValue(bar->GetMinimum());
        args.SetHandled(true);
    }
}
void ScrollBar::OnScrollToRightEndCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept {
    auto* bar = static_cast<ScrollBar*>(sender);
    if (bar != nullptr) {
        bar->SetValue(bar->GetMaximum());
        args.SetHandled(true);
    }
}
void ScrollBar::OnScrollToHorizontalOffsetCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept {
    auto* bar = static_cast<ScrollBar*>(sender);
    if (bar != nullptr) {
        const double value = args.GetParameter().Kind() == Meta::ValueKind::Double
            ? args.GetParameter().AsDouble() : 0.0;
        bar->SetValue(std::clamp(value, bar->GetMinimum(), bar->GetMaximum()));
        args.SetHandled(true);
    }
}
void ScrollBar::OnScrollToVerticalOffsetCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept {
    auto* bar = static_cast<ScrollBar*>(sender);
    if (bar != nullptr) {
        const double value = args.GetParameter().Kind() == Meta::ValueKind::Double
            ? args.GetParameter().AsDouble() : 0.0;
        bar->SetValue(std::clamp(value, bar->GetMinimum(), bar->GetMaximum()));
        args.SetHandled(true);
    }
}

void ScrollBar::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    RangeBase::OnPropertyChanged(args);
    if (args.GetProperty() == OrientationProperty ||
        args.GetProperty() == MinimumProperty ||
        args.GetProperty() == MaximumProperty ||
        args.GetProperty() == ValueProperty ||
        args.GetProperty() == ViewportSizeProperty) {
        SynchronizeTrack();
    }
}

void ScrollBar::SynchronizeTrack() noexcept {
    if (track_ == nullptr) return;
    static_cast<void>(
        track_->SetOrientation(GetOrientation()));
    static_cast<void>(
        track_->SetRange(GetMinimum(), GetMaximum()));
    static_cast<void>(
        track_->SetViewportSize(GetViewportSize()));
    static_cast<void>(
        track_->SetValue(GetValue()));
}

RangeBase::RangeBase(TypeId runtimeType) noexcept
    : Control(runtimeType) {
}

RangeBase::~RangeBase() = default;

double RangeBase::GetMinimum() const noexcept {
    return ReadDouble(*this, MinimumProperty);
}

double RangeBase::GetMaximum() const noexcept {
    return ReadDouble(*this, MaximumProperty);
}

double RangeBase::GetValue() const noexcept {
    return ReadDouble(*this, ValueProperty);
}

void RangeBase::SetMinimum(
    double value) noexcept {
    SetRange(value, GetMaximum());
}

void RangeBase::SetMaximum(
    double value) noexcept {
    SetRange(GetMinimum(), value);
}

void RangeBase::SetRange(
    double minimum,
    double maximum) noexcept {
    if (!std::isfinite(minimum) ||
        !std::isfinite(maximum) ||
        maximum < minimum) {
        return;
    }
    StoreDouble(*this, MinimumProperty, minimum);
    StoreDouble(*this, MaximumProperty, maximum);
    SetValue(GetValue());
}

void RangeBase::SetValue(
    double value) noexcept {
    if (!std::isfinite(value)) {
        return;
    }
    if (GetMaximum() < GetMinimum()) {
        return;
    }
    const double clamped =
        std::clamp(value, GetMinimum(), GetMaximum());
    const double oldValue = GetValue();
    if (Same(clamped, oldValue)) return;
    StoreDouble(*this, ValueProperty, clamped);
}

void RangeBase::OnMinimumChanged(
    double,
    double) noexcept {
    SetValue(GetValue());
}

void RangeBase::OnMaximumChanged(
    double,
    double) noexcept {
    SetValue(GetValue());
}

void RangeBase::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs&
        args) noexcept {
    Control::OnPropertyChanged(args);
    if (args.GetProperty() == ValueProperty) {
        OnValueChanged(
            args.GetOldValue().AsDouble(),
            args.GetNewValue().AsDouble());
    } else if (args.GetProperty() == MinimumProperty) {
        OnMinimumChanged(
            args.GetOldValue().AsDouble(),
            args.GetNewValue().AsDouble());
    } else if (args.GetProperty() == MaximumProperty) {
        OnMaximumChanged(
            args.GetOldValue().AsDouble(),
            args.GetNewValue().AsDouble());
    }
}

void RangeBase::OnValueChanged(
    double oldValue,
    double newValue) noexcept {
    RangeValueChangedEventArgs args(oldValue, newValue);
    RaiseEvent(ValueChangedEvent, &args);
}

PropertyValue RangeBase::CoerceValueCore(
    DependencyPropertyHandle property,
    const PropertyValue& baseValue) noexcept {
    if (baseValue.Kind() != Meta::ValueKind::Double) {
        return baseValue;
    }
    const double raw = baseValue.AsDouble();
    double adjusted = raw;
    if (property == MinimumProperty.Handle()) {
        adjusted = std::min(raw, GetMaximum());
    } else if (property == MaximumProperty.Handle()) {
        adjusted = std::max(raw, GetMinimum());
    } else if (property == ValueProperty.Handle()) {
        adjusted = std::clamp(raw, GetMinimum(), GetMaximum());
    } else {
        return baseValue;
    }
    if (adjusted == raw) {
        return baseValue;
    }
    return Meta::ValueCodec<double>::Encode(adjusted);
}

double ScrollBar::GetViewportSize() const noexcept {
    return ReadDouble(*this, ViewportSizeProperty);
}

double ScrollBar::GetSmallChange() const noexcept {
    return ReadDouble(*this, SmallChangeProperty, 16.0);
}

double ScrollBar::GetLargeChange() const noexcept {
    const double configured =
        ReadDouble(*this, LargeChangeProperty);
    return configured > 0.0
        ? configured
        : std::max(GetViewportSize(), GetSmallChange());
}

void ScrollBar::SetOrientation(
    Orientation value) noexcept {
    StoreOrientation(*this, OrientationProperty, value);
}

void ScrollBar::SetViewportSize(
    double value) noexcept {
    if (!ValidNonnegative(value)) {
        return;
    }
    StoreDouble(*this, ViewportSizeProperty, value);
}

void ScrollBar::SetSmallChange(
    double value) noexcept {
    if (!std::isfinite(value) || value <= 0.0) {
        return;
    }
    StoreDouble(*this, SmallChangeProperty, value);
}

void ScrollBar::SetLargeChange(
    double value) noexcept {
    if (!ValidNonnegative(value)) {
        return;
    }
    StoreDouble(*this, LargeChangeProperty, value);
}

bool ScrollBar::LineDecrement() noexcept {
    const double oldValue = GetValue();
    SetValue(oldValue - GetSmallChange());
    return !Same(oldValue, GetValue());
}

bool ScrollBar::LineIncrement() noexcept {
    const double oldValue = GetValue();
    SetValue(oldValue + GetSmallChange());
    return !Same(oldValue, GetValue());
}

bool ScrollBar::PageDecrement() noexcept {
    const double oldValue = GetValue();
    SetValue(oldValue - GetLargeChange());
    return !Same(oldValue, GetValue());
}

bool ScrollBar::PageIncrement() noexcept {
    const double oldValue = GetValue();
    SetValue(oldValue + GetLargeChange());
    return !Same(oldValue, GetValue());
}

bool ScrollBar::DragThumb(
    double thumbOffset,
    double trackLength,
    double minimumThumbLength) noexcept {
    Track track;
    track.SetOrientation(GetOrientation());
    track.SetRange(GetMinimum(), GetMaximum());
    track.SetViewportSize(GetViewportSize());
    const double value =
        track.ValueFromThumbOffset(
            thumbOffset,
            trackLength,
            minimumThumbLength);
    const double oldValue = GetValue();
    SetValue(value);
    return !Same(oldValue, GetValue());
}

Slider::Slider() noexcept
    : Primitives::RangeBase(StaticTypeId()),
      parsedTicks_(&Base::GetDefaultAllocator()) {
}

Slider::~Slider() {
    if (dragging_) {
        static_cast<void>(ReleasePointer(pointerId_));
        dragging_ = false;
    }
}

void Slider::OnApplyTemplate() noexcept {
    Control::OnApplyTemplate();
    DependencyObject* part = GetTemplateChild("PART_Track");
    track_ = part != nullptr &&
        DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
            part->RuntimeType(), Track::StaticTypeId())
        ? static_cast<Track*>(part)
        : nullptr;
    SynchronizeTrack();
}

void Slider::OnTemplateDetached() noexcept {
    track_ = nullptr;
    Control::OnTemplateDetached();
}

void Slider::OnVisualParentChanged(Visual* oldParent) noexcept {
    Control::OnVisualParentChanged(oldParent);
    if (GetVisualParent() == nullptr && dragging_) {
        static_cast<void>(ReleasePointer(pointerId_));
        dragging_ = false;
    }
}


void Slider::SetFromPoint() noexcept {
    if (track_ != nullptr) {
        SetValueFromTrackPoint(Input::Mouse::GetPosition(track_));
        return;
    }
    const Point local = Input::Mouse::GetPosition(this);
    const bool horizontal = GetOrientation() == Orientation::Horizontal;
    SetValueFromPosition(
        horizontal ? local.x : local.y,
        horizontal ? GetRenderSize().width : GetRenderSize().height);
}

void Slider::OnMouseLeftButtonDown(MouseButtonEventArgs& args) {
    if (!GetIsEnabled() || args.GetChangedButton() != MouseButton::Left) {
        return;
    }
    pointerId_ = args.GetPointerId();
    static_cast<void>(Focus());
    Track* track = track_;
    Thumb* thumb = track != nullptr ? track->GetThumbElement().Get() : nullptr;
    auto* source = ::Aero::TryCast<UIElement>(args.GetOriginalSource());
    const bool onThumb = thumb != nullptr && source != nullptr && (source == thumb || thumb->IsAncestorOf(*source));
    const bool onTrack = track != nullptr && source != nullptr && (source == track || track->IsAncestorOf(*source));
    const bool onSlider = source == nullptr || source == this || IsAncestorOf(*source);
    const bool hasRepeatButtons = track != nullptr && (track->GetDecreaseRepeatButton() || track->GetIncreaseRepeatButton());

    const bool moveToPoint = GetIsMoveToPointEnabled() || ((onTrack || onSlider) && !hasRepeatButtons);
    dragging_ = onThumb || moveToPoint;
    if (!dragging_ && track != nullptr && onTrack) {
        const Point local = Input::Mouse::GetPosition(track);
        const bool horizontal = GetOrientation() == Orientation::Horizontal;
        const Size size = track->GetRenderSize();
        const double length = horizontal ? size.width : size.height;
        const double position = horizontal ? local.x : local.y;
        const double thumbOffset = track->GetThumbOffset(length);
        const double thumbLength = track->GetThumbLength(length);
        dragging_ = std::fabs(position - (thumbOffset + thumbLength * 0.5)) <= std::max(10.0, thumbLength);
    }
    if (dragging_) {
        static_cast<void>(CapturePointer(args.GetPointerId()));
    }
    if (moveToPoint || (dragging_ && onThumb)) {
        SetFromPoint();
    } else if (!dragging_) {
        const Point local = track != nullptr
            ? Input::Mouse::GetPosition(track)
            : Input::Mouse::GetPosition(this);
        const bool horizontal = GetOrientation() == Orientation::Horizontal;
        const double position = horizontal ? local.x : local.y;
        const double length = horizontal
            ? (track != nullptr ? track->GetRenderSize().width : GetRenderSize().width)
            : (track != nullptr ? track->GetRenderSize().height : GetRenderSize().height);
        const bool after = position >= length * 0.5;
        const bool increase = GetIsDirectionReversed() ? !after : after;
        static_cast<void>(increase ? IncreaseLarge() : DecreaseLarge());
    }
    args.SetHandled(true);
}

void Slider::OnMouseMove(MouseEventArgs& args) {
    if (!dragging_ || pointerId_ != args.GetPointerId()) {
        return;
    }
    SetFromPoint();
    args.SetHandled(true);
}

void Slider::OnMouseLeftButtonUp(MouseButtonEventArgs& args) {
    if (args.GetChangedButton() != MouseButton::Left || !dragging_ || pointerId_ != args.GetPointerId()) {
        return;
    }
    SetFromPoint();
    dragging_ = false;
    static_cast<void>(ReleasePointer(args.GetPointerId()));
    args.SetHandled(true);
}

void Slider::OnKeyDown(KeyEventArgs& args) {
    if (!GetIsEnabled()) return;
    bool changed = false;
    bool handled = true;
    const bool reversed = GetIsDirectionReversed();
    if (args.GetKey() == KeyboardKeyHome) {
        const double oldValue = GetValue();
        SetValue(reversed ? GetMaximum() : GetMinimum());
        changed = !Same(oldValue, GetValue());
    } else if (args.GetKey() == KeyboardKeyEnd) {
        const double oldValue = GetValue();
        SetValue(reversed ? GetMinimum() : GetMaximum());
        changed = !Same(oldValue, GetValue());
    } else if (args.GetKey() == KeyboardKeyLeft || args.GetKey() == KeyboardKeyDown) {
        changed = reversed ? IncreaseSmall() : DecreaseSmall();
    } else if (args.GetKey() == KeyboardKeyRight || args.GetKey() == KeyboardKeyUp) {
        changed = reversed ? DecreaseSmall() : IncreaseSmall();
    } else {
        handled = false;
    }
    if (handled && changed) {
        args.SetHandled(true);
    }
}

void Slider::OnDecreaseSmallCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept {
    if (auto* slider = static_cast<Slider*>(sender)) {
        static_cast<void>(slider->DecreaseSmall());
        args.SetHandled(true);
    }
}
void Slider::OnIncreaseSmallCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept {
    if (auto* slider = static_cast<Slider*>(sender)) {
        static_cast<void>(slider->IncreaseSmall());
        args.SetHandled(true);
    }
}
void Slider::OnDecreaseLargeCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept {
    if (auto* slider = static_cast<Slider*>(sender)) {
        static_cast<void>(slider->DecreaseLarge());
        args.SetHandled(true);
    }
}
void Slider::OnIncreaseLargeCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept {
    if (auto* slider = static_cast<Slider*>(sender)) {
        static_cast<void>(slider->IncreaseLarge());
        args.SetHandled(true);
    }
}

void Slider::UpdateParsedTicks(Base::StringView text) noexcept {
    parsedTicks_.Clear();
    std::uint32_t start = 0U;
    while (start < text.SizeBytes() && parsedTicks_.Size() < 1024U) {
        while (start < text.SizeBytes() &&
            (text[start] == ' ' ||
             text[start] == '\t' ||
             text[start] == ',' ||
             text[start] == ';')) {
            ++start;
        }
        if (start >= text.SizeBytes()) break;
        std::uint32_t end = start;
        while (end < text.SizeBytes() &&
            text[end] != ' ' &&
            text[end] != '\t' &&
            text[end] != ',' &&
            text[end] != ';') {
            ++end;
        }
        Base::Result<double> parsed =
            ::Aero::Base::ValueConversion::ParseDouble(
                text.Substr(start, end - start));
        if (parsed && std::isfinite(parsed.Value())) {
            parsedTicks_.PushBack(parsed.Value());
        }
        start = end;
    }
}

bool Slider::ValidateValueCore(
    DependencyPropertyHandle property,
    const PropertyValue& value) const noexcept {
    if (property == TicksProperty.Handle()) {
        if (value.Kind() != Meta::ValueKind::String) {
            return false;
        }
        const Base::StringView text = value.AsString();
        std::uint32_t start = 0U;
        while (start < text.SizeBytes()) {
            while (start < text.SizeBytes() &&
                (text[start] == ' ' ||
                 text[start] == '\t' ||
                 text[start] == ',' ||
                 text[start] == ';')) {
                ++start;
            }
            if (start >= text.SizeBytes()) break;
            std::uint32_t end = start;
            while (end < text.SizeBytes() &&
                text[end] != ' ' &&
                text[end] != '\t' &&
                text[end] != ',' &&
                text[end] != ';') {
                ++end;
            }
            Base::Result<double> parsed =
                ::Aero::Base::ValueConversion::ParseDouble(
                    text.Substr(start, end - start));
            if (!parsed ||
                !std::isfinite(parsed.Value())) {
                return false;
            }
            start = end;
        }
        return true;
    }
    return RangeBase::ValidateValueCore(property, value);
}

void Slider::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    RangeBase::OnPropertyChanged(args);
    if (args.GetProperty() == TicksProperty) {
        UpdateParsedTicks(args.GetNewValue().Kind() == Meta::ValueKind::String
            ? args.GetNewValue().AsString()
            : Base::StringView{});
    } else if (args.GetProperty() == OrientationProperty ||
        args.GetProperty() == MinimumProperty ||
        args.GetProperty() == MaximumProperty ||
        args.GetProperty() == ValueProperty ||
        args.GetProperty() == IsDirectionReversedProperty) {
        SynchronizeTrack();
    }
}

void Slider::SynchronizeTrack() noexcept {
    if (track_ == nullptr) return;
    track_->SetOrientation(GetOrientation());
    track_->SetRange(GetMinimum(), GetMaximum());
    track_->SetValue(GetValue());
    track_->SetIsDirectionReversed(GetIsDirectionReversed());
}

Orientation Slider::GetOrientation() const noexcept {
    return ReadOrientation(*this, OrientationProperty);
}

double Slider::GetSmallChange() const noexcept {
    return ReadDouble(*this, SmallChangeProperty, 1.0);
}

double Slider::GetLargeChange() const noexcept {
    return ReadDouble(*this, LargeChangeProperty, 10.0);
}

TickPlacement Slider::GetTickPlacement() const noexcept {
    return GetValue(TickPlacementProperty);
}

double Slider::GetTickFrequency() const noexcept {
    return ReadDouble(*this, TickFrequencyProperty, 1.0);
}

Base::StringView Slider::GetTicks() const noexcept {
    return GetValue(TicksProperty);
}

bool Slider::GetIsSnapToTickEnabled() const noexcept {
    return ReadBool(
        *this, IsSnapToTickEnabledProperty, false);
}

bool Slider::GetIsDirectionReversed() const noexcept {
    return ReadBool(
        *this, IsDirectionReversedProperty, false);
}

bool Slider::GetIsMoveToPointEnabled() const noexcept {
    return ReadBool(
        *this, IsMoveToPointEnabledProperty, false);
}

void Slider::SetOrientation(
    Orientation value) noexcept {
    StoreOrientation(*this, OrientationProperty, value);
}

void Slider::SetSmallChange(
    double value) noexcept {
    if (!std::isfinite(value) || value <= 0.0) {
        return;
    }
    StoreDouble(*this, SmallChangeProperty, value);
}

void Slider::SetLargeChange(
    double value) noexcept {
    if (!std::isfinite(value) || value <= 0.0) {
        return;
    }
    StoreDouble(*this, LargeChangeProperty, value);
}

void Slider::SetTickPlacement(
    TickPlacement value) noexcept {
    DependencyObject::SetValue(TickPlacementProperty, value);
}

void Slider::SetTickFrequency(
    double value) noexcept {
    if (!std::isfinite(value) || value <= 0.0) {
        return;
    }
    StoreDouble(*this, TickFrequencyProperty, value);
}

void Slider::SetTicks(
    Base::StringView value) noexcept {
    std::uint32_t start = 0U;
    while (start < value.SizeBytes()) {
        while (start < value.SizeBytes() &&
            (value[start] == ' ' ||
             value[start] == '\t' ||
             value[start] == ',' ||
             value[start] == ';')) {
            ++start;
        }
        if (start >= value.SizeBytes()) break;
        std::uint32_t end = start;
        while (end < value.SizeBytes() &&
            value[end] != ' ' &&
            value[end] != '\t' &&
            value[end] != ',' &&
            value[end] != ';') {
            ++end;
        }
        Base::Result<double> parsed =
            ::Aero::Base::ValueConversion::ParseDouble(
                value.Substr(start, end - start));
        if (!parsed || !std::isfinite(parsed.Value())) {
            return;
        }
        start = end;
    }
    DependencyObject::SetValue(TicksProperty, value);
}

void Slider::SetIsSnapToTickEnabled(
    bool value) noexcept {
    DependencyObject::SetValue(IsSnapToTickEnabledProperty, value);
}

void Slider::SetIsDirectionReversed(
    bool value) noexcept {
    DependencyObject::SetValue(IsDirectionReversedProperty, value);
}

void Slider::SetIsMoveToPointEnabled(
    bool value) noexcept {
    DependencyObject::SetValue(IsMoveToPointEnabledProperty, value);
}

double Slider::GetSnapValue(double value) const noexcept {
    value = std::clamp(
        value, GetMinimum(), GetMaximum());
    if (!GetIsSnapToTickEnabled()) return value;

    const Base::Span<const double> ticks = GetTickValues();
    bool found = false;
    double nearest = value;
    double distance =
        std::numeric_limits<double>::infinity();
    for (double tickValue : ticks) {
        if (tickValue >= GetMinimum() &&
            tickValue <= GetMaximum()) {
            const double candidateDistance =
                std::fabs(tickValue - value);
            if (!found ||
                candidateDistance < distance) {
                found = true;
                nearest = tickValue;
                distance = candidateDistance;
            }
        }
    }
    if (found) return nearest;

    const double frequency = GetTickFrequency();
    const double step = std::round(
        (value - GetMinimum()) / frequency);
    return std::clamp(
        GetMinimum() + step * frequency,
        GetMinimum(), GetMaximum());
}

bool Slider::DecreaseSmall() noexcept {
    const double oldValue = GetValue();
    SetValue(GetSnapValue(oldValue - GetSmallChange()));
    return !Same(oldValue, GetValue());
}

bool Slider::IncreaseSmall() noexcept {
    const double oldValue = GetValue();
    SetValue(GetSnapValue(oldValue + GetSmallChange()));
    return !Same(oldValue, GetValue());
}

bool Slider::DecreaseLarge() noexcept {
    const double oldValue = GetValue();
    SetValue(GetSnapValue(oldValue - GetLargeChange()));
    return !Same(oldValue, GetValue());
}

bool Slider::IncreaseLarge() noexcept {
    const double oldValue = GetValue();
    SetValue(GetSnapValue(oldValue + GetLargeChange()));
    return !Same(oldValue, GetValue());
}

void Slider::SetValueFromPosition(
    double position,
    double trackLength) noexcept {
    if (!std::isfinite(position) ||
        !std::isfinite(trackLength) ||
        trackLength <= 0.0) {
        return;
    }
    double normalized =
        std::clamp(position / trackLength, 0.0, 1.0);
    if (GetIsDirectionReversed()) {
        normalized = 1.0 - normalized;
    }
    SetValue(GetSnapValue(
        GetMinimum() +
        normalized * (GetMaximum() - GetMinimum())));
}

void Slider::SetValueFromTrackPoint(
    Point local) noexcept {
    if (track_ == nullptr) {
        const bool horizontal =
            GetOrientation() == Orientation::Horizontal;
        SetValueFromPosition(
            horizontal ? local.x : local.y,
            horizontal
                ? GetRenderSize().width
                : GetRenderSize().height);
        return;
    }
    const bool horizontal =
        GetOrientation() == Orientation::Horizontal;
    const Size size = track_->GetRenderSize();
    const double length =
        horizontal ? size.width : size.height;
    if (!(length > 0.0)) return;
    const double desiredThumbLength = track_->GetThumbElement()
        ? std::max(0.0, horizontal
            ? static_cast<double>(track_->GetThumbElement()->GetDesiredSize().width)
            : static_cast<double>(track_->GetThumbElement()->GetDesiredSize().height))
        : 8.0;
    const double thumbLength = track_->GetThumbLength(length, desiredThumbLength);
    const double position =
        (horizontal ? local.x : local.y) - thumbLength * 0.5;
    const double value =
        track_->ValueFromThumbOffset(position, length, thumbLength);
    SetValue(GetSnapValue(value));
}

Size Slider::ArrangeOverride(
    Size finalSize) noexcept {
    return Control::ArrangeOverride(finalSize);
}

Base::Ref<Media::Brush> TickBar::GetFill() const noexcept {
    return GetValue(FillProperty);
}

TickBarPlacement TickBar::GetPlacement() const noexcept {
    return GetValue(PlacementProperty);
}

void TickBar::SetFill(
    Base::Ref<Media::Brush> value) noexcept {
    SetValue(FillProperty, std::move(value));
}

void TickBar::SetPlacement(
    TickBarPlacement value) noexcept {
    SetValue(PlacementProperty, value);
}

void TickBar::OnRender(
    ::Aero::Media::DrawingContext& context) noexcept {
    auto& builder = Aero::Render::DrawingBridge::Builder(context);
    DependencyObject* parent = GetTemplatedParent();
    if (parent == nullptr ||
        !DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
            parent->RuntimeType(), Slider::StaticTypeId())) {
        return;
    }

    const Slider& slider = static_cast<const Slider&>(*parent);
    const Size size = GetRenderSize();
    const bool horizontal =
        GetPlacement() == TickBarPlacement::Top ||
        GetPlacement() == TickBarPlacement::Bottom;
    const double primary = horizontal ? size.width : size.height;
    const double range = slider.GetMaximum() - slider.GetMinimum();
    if (primary <= 0.0 || range < 0.0) return;

    const Color color = ::Aero::Media::SampleBrush(
        GetFill(), 0.5, ::Aero::Media::SampleBrush(GetForeground()));
    double thumbLength = 4.0;
    if (const Primitives::Track* track = slider.GetTrack()) {
        if (const auto thumb = track->GetThumbElement()) {
            const Size thumbDesired = thumb->GetDesiredSize();
            const double desired = horizontal ? thumbDesired.width : thumbDesired.height;
            if (desired > 0.0) {
                thumbLength = track->GetThumbLength(primary, desired);
            }
        }
    }
    const double start = std::min(
        primary * 0.5, thumbLength * 0.5);
    const double travel = std::max(0.0, primary - thumbLength);

    const bool isRtl = slider.GetFlowDirection() == FlowDirection::RightToLeft;
    const bool invert = horizontal
        ? (isRtl ? !slider.GetIsDirectionReversed() : slider.GetIsDirectionReversed())
        : !slider.GetIsDirectionReversed();

    auto drawTick = [&](double value)
        noexcept -> void {
        if (value < slider.GetMinimum() ||
            value > slider.GetMaximum()) {
            return;
        }
        double normalized = range > 0.0
            ? (value - slider.GetMinimum()) / range
            : 0.0;
        if (invert) {
            normalized = 1.0 - normalized;
        }
        const double position = start +
            std::clamp(normalized, 0.0, 1.0) * travel;
        const float tickPos = static_cast<float>(std::round(position));
        if (horizontal) {
            builder.FillRect({
                tickPos,
                GetPlacement() == TickBarPlacement::Top
                    ? 0.0f
                    : std::max(0.0f, size.height - 4.0f),
                1.0f,
                std::min(4.0f, size.height)}, color);
            return;
        }
        builder.FillRect({
            GetPlacement() == TickBarPlacement::Left
                ? 0.0f
                : std::max(0.0f, size.width - 4.0f),
            tickPos,
            std::min(4.0f, size.width),
            1.0f}, color);
    };

    const Base::Span<const double> ticks = slider.GetTickValues();
    if (!ticks.Empty()) {
        for (double tickValue : ticks) {
            drawTick(tickValue);
        }
        return;
    }

    const double frequency = slider.GetTickFrequency();
    if (frequency <= 0.0) return;
    for (double value = slider.GetMinimum(), count = 0.0;
         value <= slider.GetMaximum() + frequency * 0.000001 &&
         count < 1024.0;
         value += frequency, count += 1.0) {
        drawTick(std::min(value, slider.GetMaximum()));
    }
    return;
}

bool ProgressBar::GetIsIndeterminate() const noexcept {
    return ReadBool(
        *this, IsIndeterminateProperty, false);
}

Orientation ProgressBar::GetOrientation() const noexcept {
    return ReadOrientation(*this, OrientationProperty);
}

void ProgressBar::SetIsIndeterminate(
    bool value) noexcept {
    DependencyObject::SetValue(IsIndeterminateProperty, value);
}

void ProgressBar::SetOrientation(
    Orientation value) noexcept {
    StoreOrientation(*this, OrientationProperty, value);
}

double ProgressBar::GetNormalizedValue() const noexcept {
    const double range = GetMaximum() - GetMinimum();
    return range > 0.0
        ? std::clamp(
            (GetValue() - GetMinimum()) / range,
            0.0,
            1.0)
        : 0.0;
}

void ProgressBar::OnApplyTemplate() noexcept {
    RangeBase::OnApplyTemplate();
    track_ = ::Aero::TryCast<FrameworkElement>(GetTemplateChild(Base::StringView("PART_Track")));
    indicator_ = ::Aero::TryCast<FrameworkElement>(GetTemplateChild(Base::StringView("PART_Indicator")));
    UpdateIndicator(GetRenderSize());
}

void ProgressBar::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    RangeBase::OnPropertyChanged(args);
    const DependencyPropertyHandle prop = args.GetProperty();
    if (prop == ValueProperty.Handle() ||
        prop == MinimumProperty.Handle() ||
        prop == MaximumProperty.Handle() ||
        prop == IsIndeterminateProperty.Handle() ||
        prop == OrientationProperty.Handle()) {
        UpdateIndicator(GetRenderSize());
    }
}

Size ProgressBar::ArrangeOverride(Size finalSize) noexcept {
    UpdateIndicator(finalSize);
    const Size arranged = RangeBase::ArrangeOverride(finalSize);
    UpdateIndicator(arranged);
    return arranged;
}

void ProgressBar::UpdateIndicator(Size fallbackSize) noexcept {
    if (indicator_ == nullptr) return;
    double trackWidth = track_ != nullptr ? track_->GetRenderSize().width : fallbackSize.width;
    if (trackWidth <= 0.0) {
        trackWidth = fallbackSize.width;
    }
    if (track_ != nullptr) {
        if (const auto* border = ::Aero::TryCast<Border>(track_)) {
            const Thickness bt = border->GetBorderThickness();
            const Thickness pad = border->GetPadding();
            trackWidth = std::max(0.0, trackWidth - bt.left - bt.right - pad.left - pad.right);
        }
    }
    const double indicatorWidth = GetIsIndeterminate()
        ? trackWidth
        : std::max(0.0, trackWidth * GetNormalizedValue());
    if (indicator_->GetHorizontalAlignment() == HorizontalAlignment::Stretch) {
        indicator_->SetHorizontalAlignment(HorizontalAlignment::Left);
    }
    indicator_->SetWidth(indicatorWidth);
}

namespace Primitives {

AERO_DESCRIBE(Track) {
    using namespace Aero::Meta;
    Register<Track>(context)
        .Property(Track::OrientationProperty, Orientation::Vertical, AffectsMeasure)
        .Property(Track::MinimumProperty, 0.0, AffectsArrange, &::Aero::Base::Validate::Finite<double>)
        .Property(Track::MaximumProperty, 1.0, AffectsArrange, &::Aero::Base::Validate::Finite<double>)
        .Property(Track::ValueProperty, 0.0, AffectsArrange | BindsTwoWayByDefault, &::Aero::Base::Validate::Finite<double>)
        .Property(Track::ViewportSizeProperty, 0.0, AffectsArrange, &::Aero::Base::Validate::NonNegative<double>)
        .Property(Track::IsDirectionReversedProperty, false, AffectsArrange)
        .Property<Base::Ref<RepeatButton>, &Track::SetDecreaseRepeatButton>("DecreaseRepeatButton", PropertyFlags::Structural)
        .Property<Base::Ref<Thumb>, &Track::SetThumb>("Thumb", PropertyFlags::Structural)
        .Property<Base::Ref<RepeatButton>, &Track::SetIncreaseRepeatButton>("IncreaseRepeatButton", PropertyFlags::Structural)
        .Factory();
}

AERO_DESCRIBE(Thumb) {
    using namespace Aero::Meta;
    Register<Thumb>(context)
        .Property(Thumb::IsDraggingProperty, false, AffectsRender)
        .Factory();
}

AERO_DESCRIBE(RangeBase) {
    using namespace Aero::Meta;
    Register<RangeValueChangedEventArgs>(context);
    Register<RangeBase>(context, TypeFlags::Abstract)
        .Event(RangeBase::ValueChangedEvent)
        .Property(RangeBase::MinimumProperty, FrameworkPropertyMetadata(0.0, AffectsArrange).Validate(&::Aero::Base::Validate::Finite<double>))
        .Property(RangeBase::MaximumProperty, FrameworkPropertyMetadata(100.0, AffectsArrange).Validate(&::Aero::Base::Validate::Finite<double>))
        .Property(RangeBase::ValueProperty, FrameworkPropertyMetadata(0.0, AffectsArrange | BindsTwoWayByDefault).Validate(&::Aero::Base::Validate::Finite<double>));
}

AERO_DESCRIBE(ScrollBar) {
    using namespace Aero::Meta;
    Register<ScrollChangedEventArgs>(context);
    Register<ScrollBar>(context)
        .Property(ScrollBar::OrientationProperty, Orientation::Vertical, AffectsMeasure)
        .Property(ScrollBar::ViewportSizeProperty, 0.0, AffectsArrange, &::Aero::Base::Validate::NonNegative<double>)
        .Property(ScrollBar::SmallChangeProperty, 16.0, FrameworkPropertyMetadataOptions::None, &::Aero::Base::Validate::Positive<double>)
        .Property(ScrollBar::LargeChangeProperty, 0.0, FrameworkPropertyMetadataOptions::None, &::Aero::Base::Validate::NonNegative<double>)
        .TemplatePart("PART_Track", TypeOf<Track>())
        .Factory();
    AERO_ON(ScrollBar, &ScrollBar::OnMouseLeftButtonDown, UIElement::MouseLeftButtonDownEvent);
    AERO_ON(ScrollBar, &ScrollBar::OnMouseLeftButtonUp, UIElement::MouseLeftButtonUpEvent);
    AERO_ON(ScrollBar, &ScrollBar::OnMouseMove, UIElement::MouseMoveEvent);
    AERO_ON(ScrollBar, &ScrollBar::OnKeyDown, UIElement::KeyDownEvent);

    using StaticCommandFn = void (*)(Base::Object*, ExecutedRoutedEventArgs&) noexcept;
    struct CommandEntry {
        Base::StringView primary;
        Base::StringView alias;
        StaticCommandFn fn;
    };
    static constexpr CommandEntry kCommands[] = {
        {"LineUpCommand", "LineUp", &ScrollBar::OnLineUpCommand},
        {"LineDownCommand", "LineDown", &ScrollBar::OnLineDownCommand},
        {"LineLeftCommand", "LineLeft", &ScrollBar::OnLineLeftCommand},
        {"LineRightCommand", "LineRight", &ScrollBar::OnLineRightCommand},
        {"PageUpCommand", "PageUp", &ScrollBar::OnPageUpCommand},
        {"PageDownCommand", "PageDown", &ScrollBar::OnPageDownCommand},
        {"PageLeftCommand", "PageLeft", &ScrollBar::OnPageLeftCommand},
        {"PageRightCommand", "PageRight", &ScrollBar::OnPageRightCommand},
        {"ScrollToTopCommand", "ScrollToTop", &ScrollBar::OnScrollToTopCommand},
        {"ScrollToBottomCommand", "ScrollToBottom", &ScrollBar::OnScrollToBottomCommand},
        {"ScrollToLeftEndCommand", "ScrollToLeftEnd", &ScrollBar::OnScrollToLeftEndCommand},
        {"ScrollToRightEndCommand", "ScrollToRightEnd", &ScrollBar::OnScrollToRightEndCommand},
        {"ScrollToHorizontalOffsetCommand", "ScrollToHorizontalOffset", &ScrollBar::OnScrollToHorizontalOffsetCommand},
        {"ScrollToVerticalOffsetCommand", "ScrollToVerticalOffset", &ScrollBar::OnScrollToVerticalOffsetCommand},
    };
    auto addClassCmd = [](Base::StringView name, const ExecutedRoutedEventHandler& handler) noexcept {
        Input::RoutedCommand::RegisterStatic(ScrollBar::StaticTypeId(), name);
        if (auto cmd = Input::RoutedCommand::ResolveStatic(ScrollBar::StaticTypeId(), name)) {
            Input::CommandBinding::RegisterClassBinding(
                ScrollBar::StaticTypeId(),
                Input::CommandBinding(std::move(cmd), handler));
        }
    };
    for (const CommandEntry& entry : kCommands) {
        const ExecutedRoutedEventHandler handler(entry.fn);
        addClassCmd(entry.primary, handler);
        addClassCmd(entry.alias, handler);
    }
    for (Base::StringView commandName : {
             Base::StringView("DeferScrollToHorizontalOffsetCommand"),
             Base::StringView("DeferScrollToHorizontalOffset"),
             Base::StringView("DeferScrollToVerticalOffsetCommand"),
             Base::StringView("DeferScrollToVerticalOffset")}) {
        (void)Input::RoutedCommand::RegisterStatic(
            ScrollBar::StaticTypeId(), commandName);
    }
}

} // namespace Primitives

AERO_DESCRIBE(Slider) {
    using namespace Aero::Meta;
    using StaticCommandFn = void (*)(Base::Object*, ExecutedRoutedEventArgs&) noexcept;
    struct SliderCommandEntry {
        Base::StringView name;
        StaticCommandFn fn;
    };
    static constexpr SliderCommandEntry kSliderCommands[] = {
        {"DecreaseSmall", &Slider::OnDecreaseSmallCommand},
        {"IncreaseSmall", &Slider::OnIncreaseSmallCommand},
        {"DecreaseLarge", &Slider::OnDecreaseLargeCommand},
        {"IncreaseLarge", &Slider::OnIncreaseLargeCommand},
    };
    for (const SliderCommandEntry& entry : kSliderCommands) {
        Input::RoutedCommand::RegisterStatic(Slider::StaticTypeId(), entry.name);
        if (auto cmd = Input::RoutedCommand::ResolveStatic(Slider::StaticTypeId(), entry.name)) {
            Input::CommandBinding::RegisterClassBinding(
                Slider::StaticTypeId(),
                Input::CommandBinding(std::move(cmd), ExecutedRoutedEventHandler(entry.fn)));
        }
    }

    Register<Slider>(context)
        .Property(Slider::OrientationProperty, Orientation::Horizontal, AffectsMeasure)
        .Property(Slider::SmallChangeProperty, 1.0, FrameworkPropertyMetadataOptions::None, &Base::Validate::Positive<double>)
        .Property(Slider::LargeChangeProperty, 10.0, FrameworkPropertyMetadataOptions::None, &Base::Validate::Positive<double>)
        .Property(Slider::TickPlacementProperty, TickPlacement::None, AffectsRender)
        .Property(Slider::TickFrequencyProperty, 1.0, AffectsRender, &Base::Validate::Positive<double>)
        .Property(Slider::TicksProperty, Base::String{}, AffectsRender)
        .Property(Slider::IsSnapToTickEnabledProperty, false)
        .Property(Slider::IsDirectionReversedProperty, false, AffectsArrange | AffectsRender)
        .Property(Slider::IsMoveToPointEnabledProperty, false)
        .Override(UIElement::IsTabStopProperty, true, FrameworkPropertyMetadataOptions::None)
        .Override(Primitives::RangeBase::MaximumProperty, 10.0, FrameworkPropertyMetadataOptions::AffectsArrange, &::Aero::Base::Validate::Finite<double>)
        .Factory();
    AERO_ON(Slider, &Slider::OnMouseLeftButtonDown, UIElement::MouseLeftButtonDownEvent);
    AERO_ON(Slider, &Slider::OnMouseLeftButtonUp, UIElement::MouseLeftButtonUpEvent);
    AERO_ON(Slider, &Slider::OnMouseMove, UIElement::MouseMoveEvent);
    AERO_ON(Slider, &Slider::OnKeyDown, UIElement::KeyDownEvent);
}

AERO_DESCRIBE(TickBar) {
    using namespace Aero::Meta;
    Register<TickBar>(context)
        .Property(TickBar::FillProperty, Base::Ref<Media::Brush>{}, AffectsRender)
        .Property(TickBar::PlacementProperty, TickBarPlacement::Top, AffectsRender)
        .Factory();
}

AERO_DESCRIBE(ProgressBar) {
    using namespace Aero::Meta;
    Register<ProgressBar>(context)
        .Property(ProgressBar::IsIndeterminateProperty, false, AffectsRender)
        .Property(ProgressBar::OrientationProperty, Orientation::Horizontal, AffectsMeasure)
        .Factory();
}

AERO_DESCRIBE(GridSplitter) {
    using namespace Aero::Meta;
    Register<GridSplitter>(context)
        .Property(GridSplitter::DragIncrementProperty, 1.0, FrameworkPropertyMetadataOptions::None, &::Aero::Base::Validate::Positive<double>)
        .Property(GridSplitter::KeyboardIncrementProperty, 10.0, FrameworkPropertyMetadataOptions::None, &::Aero::Base::Validate::Positive<double>)
        .Property(GridSplitter::ResizeDirectionProperty, GridResizeDirection::Auto)
        .Property(GridSplitter::ResizeBehaviorProperty, GridResizeBehavior::BasedOnAlignment)
        .Property(GridSplitter::ShowsPreviewProperty, false)
        .Property(GridSplitter::PreviewStyleProperty, Base::Ref<Aero::Style>{})
        .Factory();
}

} // namespace Aero::Controls
