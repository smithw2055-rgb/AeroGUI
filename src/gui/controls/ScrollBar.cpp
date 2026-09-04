#include "gui/controls/ScrollCommon.hpp"
#include "gui/meta/ValueConversion.hpp"
#include "gui/core/State.hpp"
#include "gui/input/InputState.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "render/DisplayList.hpp"
#include <Aero/Controls.hpp>
#include "gui/media/MediaState.hpp"
#include <Aero/Input/Mouse.hpp>
#include <Aero/TryCast.hpp>
#include <Aero/Value.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include "ControlBehavior.hpp"
#include "gui/templates/TemplateState.hpp"

namespace Aero::Controls {
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
    decreaseRepeatButton_ = std::move(value);
    (void)InvalidateMeasure();
}

void Track::SetThumb(
    Base::Ref<Thumb> value) noexcept {
    thumb_ = std::move(value);
    (void)InvalidateMeasure();
}

void Track::SetIncreaseRepeatButton(
    Base::Ref<RepeatButton> value) noexcept {
    increaseRepeatButton_ = std::move(value);
    (void)InvalidateMeasure();
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

Base::Result<double> Track::ValueFromThumbOffset(
    double offset,
    double trackLength,
    double minimumThumbLength) const noexcept {
    if (!std::isfinite(offset) ||
        !ValidNonnegative(trackLength) ||
        !ValidNonnegative(minimumThumbLength)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Track thumb geometry is invalid");
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
    Size desired{};
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        Base::Result<void> measured =
            MeasureChild(*child, availableSize);
        if (!measured) return Size{};
        if (GetOrientation() == Orientation::Vertical) {
            desired.width = std::max(
                desired.width,
                child->GetDesiredSize().width);
            desired.height += child->GetDesiredSize().height;
        } else {
            desired.width += child->GetDesiredSize().width;
            desired.height = std::max(
                desired.height,
                child->GetDesiredSize().height);
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
            ? thumb_->GetDesiredSize().height
            : thumb_->GetDesiredSize().width)
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
        return ArrangeChild(
            *child,
            vertical
                ? Rect{0.0, offset,
                       finalSize.width, extent}
                : Rect{offset, 0.0,
                       extent, finalSize.height});
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
    : RangeBase(StaticTypeId()),
      trackPropertyChangedHandler_(
          this,
          &ScrollBar::OnTrackPropertyChanged) {
    static_cast<void>(AddValueChangedHandler(
        OrientationProperty,
        trackPropertyChangedHandler_));
    static_cast<void>(AddValueChangedHandler(
        MinimumProperty,
        trackPropertyChangedHandler_));
    static_cast<void>(AddValueChangedHandler(
        MaximumProperty,
        trackPropertyChangedHandler_));
    static_cast<void>(AddValueChangedHandler(
        ValueProperty,
        trackPropertyChangedHandler_));
    static_cast<void>(AddValueChangedHandler(
        ViewportSizeProperty,
        trackPropertyChangedHandler_));
}

ScrollBar::~ScrollBar() {
    static_cast<void>(RemoveValueChangedHandler(
        OrientationProperty,
        trackPropertyChangedHandler_));
    static_cast<void>(RemoveValueChangedHandler(
        MinimumProperty,
        trackPropertyChangedHandler_));
    static_cast<void>(RemoveValueChangedHandler(
        MaximumProperty,
        trackPropertyChangedHandler_));
    static_cast<void>(RemoveValueChangedHandler(
        ValueProperty,
        trackPropertyChangedHandler_));
    static_cast<void>(RemoveValueChangedHandler(
        ViewportSizeProperty,
        trackPropertyChangedHandler_));
}

void ScrollBar::OnApplyTemplate()
    noexcept {
    Control::OnApplyTemplate();
    DependencyObject* part =
        GetTemplateChild("PART_Track");
    track_ =
        part != nullptr &&
        PropertyRegistry().Types().IsDerivedFrom(
            part->RuntimeType(),
            Track::StaticTypeId())
        ? static_cast<Track*>(part)
        : nullptr;
    SynchronizeTrack();
    return;
}

void ScrollBar::OnTemplateDetached() noexcept {
    track_ = nullptr;
    Control::OnTemplateDetached();
}

void ScrollBar::OnTrackPropertyChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs&)
        noexcept {
    SynchronizeTrack();
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
    : Control(runtimeType),
      rangeChangedHandler_(
          this,
          &RangeBase::OnRangePropertyChanged) {
    static_cast<void>(AddValueChangedHandler(
        MinimumProperty, rangeChangedHandler_));
    static_cast<void>(AddValueChangedHandler(
        MaximumProperty, rangeChangedHandler_));
    static_cast<void>(AddValueChangedHandler(
        ValueProperty, rangeChangedHandler_));
}

RangeBase::~RangeBase() {
    static_cast<void>(RemoveValueChangedHandler(
        MinimumProperty, rangeChangedHandler_));
    static_cast<void>(RemoveValueChangedHandler(
        MaximumProperty, rangeChangedHandler_));
    static_cast<void>(RemoveValueChangedHandler(
        ValueProperty, rangeChangedHandler_));
}

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

void RangeBase::OnRangePropertyChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs&
        args) noexcept {
    if (args.GetProperty() == ValueProperty) {
        OnValueChanged(
            args.GetOldValue().AsDouble(),
            args.GetNewValue().AsDouble());
    } else if (
        args.GetProperty() == MinimumProperty ||
        args.GetProperty() == MaximumProperty) {
        SetValue(GetValue());
    }
}

void RangeBase::OnValueChanged(
    double oldValue,
    double newValue) noexcept {
    RangeValueChangedEventArgs args(oldValue, newValue);
    RaiseEvent(ValueChangedEvent, &args);
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

Base::Result<bool> ScrollBar::LineDecrement() noexcept {
    const double oldValue = GetValue();
    SetValue(oldValue - GetSmallChange());
    return !Same(oldValue, GetValue());
}

Base::Result<bool> ScrollBar::LineIncrement() noexcept {
    const double oldValue = GetValue();
    SetValue(oldValue + GetSmallChange());
    return !Same(oldValue, GetValue());
}

Base::Result<bool> ScrollBar::PageDecrement() noexcept {
    const double oldValue = GetValue();
    SetValue(oldValue - GetLargeChange());
    return !Same(oldValue, GetValue());
}

Base::Result<bool> ScrollBar::PageIncrement() noexcept {
    const double oldValue = GetValue();
    SetValue(oldValue + GetLargeChange());
    return !Same(oldValue, GetValue());
}

Base::Result<bool> ScrollBar::DragThumb(
    double thumbOffset,
    double trackLength,
    double minimumThumbLength) noexcept {
    Track track;
    track.SetOrientation(GetOrientation());
    track.SetRange(GetMinimum(), GetMaximum());
    track.SetViewportSize(GetViewportSize());
    Base::Result<double> value =
        track.ValueFromThumbOffset(
            thumbOffset,
            trackLength,
            minimumThumbLength);
    if (!value) return value.GetStatus();
    const double oldValue = GetValue();
    SetValue(value.Value());
    return !Same(oldValue, GetValue());
}

Slider::Slider() noexcept
    : Primitives::RangeBase(StaticTypeId()),
      trackPropertyChangedHandler_(
          this, &Slider::OnTrackPropertyChanged) {
    static_cast<void>(AddValueChangedHandler(
        OrientationProperty, trackPropertyChangedHandler_));
    static_cast<void>(AddValueChangedHandler(
        MinimumProperty, trackPropertyChangedHandler_));
    static_cast<void>(AddValueChangedHandler(
        MaximumProperty, trackPropertyChangedHandler_));
    static_cast<void>(AddValueChangedHandler(
        ValueProperty, trackPropertyChangedHandler_));
    static_cast<void>(AddValueChangedHandler(
        IsDirectionReversedProperty, trackPropertyChangedHandler_));
}

Slider::~Slider() {
    static_cast<void>(RemoveValueChangedHandler(
        OrientationProperty, trackPropertyChangedHandler_));
    static_cast<void>(RemoveValueChangedHandler(
        MinimumProperty, trackPropertyChangedHandler_));
    static_cast<void>(RemoveValueChangedHandler(
        MaximumProperty, trackPropertyChangedHandler_));
    static_cast<void>(RemoveValueChangedHandler(
        ValueProperty, trackPropertyChangedHandler_));
    static_cast<void>(RemoveValueChangedHandler(
        IsDirectionReversedProperty, trackPropertyChangedHandler_));
}

void Slider::OnApplyTemplate() noexcept {
    Control::OnApplyTemplate();
    DependencyObject* part = GetTemplateChild("PART_Track");
    track_ = part != nullptr &&
        PropertyRegistry().Types().IsDerivedFrom(
            part->RuntimeType(), Track::StaticTypeId())
        ? static_cast<Track*>(part)
        : nullptr;
    SynchronizeTrack();
}

void Slider::OnTemplateDetached() noexcept {
    track_ = nullptr;
    Control::OnTemplateDetached();
}

Size Slider::MeasureOverride(Size availableSize) noexcept {
    Size desired = Control::MeasureOverride(availableSize);
    // BlendTutorial ColorSelector / Position sliders omit Height. An empty
    // default template Grid measures 0x0, which collapses Grid Height="*"
    // rows inside a StackPanel. Keep a theme-like minimum.
    constexpr double kMinThickness = 18.0;
    constexpr double kMinLength = 32.0;
    if (GetOrientation() == Orientation::Horizontal) {
        desired.height = std::max(desired.height, kMinThickness);
        desired.width = std::max(desired.width, kMinLength);
    } else {
        desired.width = std::max(desired.width, kMinThickness);
        desired.height = std::max(desired.height, kMinLength);
    }
    return desired;
}

void Slider::OnTrackPropertyChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs&) noexcept {
    SynchronizeTrack();
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

    const Base::StringView ticks = GetTicks();
    bool found = false;
    double nearest = value;
    double distance =
        std::numeric_limits<double>::infinity();
    std::uint32_t start = 0U;
    while (start < ticks.SizeBytes()) {
        while (start < ticks.SizeBytes() &&
            (ticks[start] == ' ' ||
             ticks[start] == '\t' ||
             ticks[start] == ',' ||
             ticks[start] == ';')) {
            ++start;
        }
        if (start >= ticks.SizeBytes()) break;
        std::uint32_t end = start;
        while (end < ticks.SizeBytes() &&
            ticks[end] != ' ' &&
            ticks[end] != '\t' &&
            ticks[end] != ',' &&
            ticks[end] != ';') {
            ++end;
        }
        Base::Result<double> parsed =
            ::Aero::Base::ValueConversion::ParseDouble(
                ticks.Substr(start, end - start));
        if (parsed &&
            parsed.Value() >= GetMinimum() &&
            parsed.Value() <= GetMaximum()) {
            const double candidateDistance =
                std::fabs(parsed.Value() - value);
            if (!found ||
                candidateDistance < distance) {
                found = true;
                nearest = parsed.Value();
                distance = candidateDistance;
            }
        }
        start = end;
    }
    if (found) return nearest;

    const double frequency = GetTickFrequency();
    const double step = std::round(
        (value - GetMinimum()) / frequency);
    return std::clamp(
        GetMinimum() + step * frequency,
        GetMinimum(), GetMaximum());
}

Base::Result<bool> Slider::DecreaseSmall() noexcept {
    const double oldValue = GetValue();
    SetValue(GetSnapValue(oldValue - GetSmallChange()));
    return !Same(oldValue, GetValue());
}

Base::Result<bool> Slider::IncreaseSmall() noexcept {
    const double oldValue = GetValue();
    SetValue(GetSnapValue(oldValue + GetSmallChange()));
    return !Same(oldValue, GetValue());
}

Base::Result<bool> Slider::DecreaseLarge() noexcept {
    const double oldValue = GetValue();
    SetValue(GetSnapValue(oldValue - GetLargeChange()));
    return !Same(oldValue, GetValue());
}

Base::Result<bool> Slider::IncreaseLarge() noexcept {
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
    const double thumbLength = track_->GetThumbLength(length);
    const double position =
        (horizontal ? local.x : local.y) - thumbLength * 0.5;
    Base::Result<double> value =
        track_->ValueFromThumbOffset(position, length, thumbLength);
    if (!value) return;
    SetValue(GetSnapValue(value.Value()));
}

double Slider::GetNormalizedValueForLayout() const noexcept {
    const double range = GetMaximum() - GetMinimum();
    double normalized = range > 0.0
        ? std::clamp(
            (GetValue() - GetMinimum()) / range,
            0.0, 1.0)
        : 0.0;
    if (GetIsDirectionReversed()) {
        normalized = 1.0 - normalized;
    }
    return normalized;
}

Size Slider::ArrangeOverride(
    Size finalSize) noexcept {
    return Control::ArrangeOverride(finalSize);
}

void Slider::OnRender(
    ::Aero::Media::DrawingContext& context) noexcept {
    auto& builder = Aero::Render::DrawingPrivate::Builder(context);
    const TickPlacement placement =
        GetTickPlacement();
    const Size size = GetRenderSize();
    if (size.width <= 0.0 ||
        size.height <= 0.0) {
        return;
    }
    const bool first =
        placement == TickPlacement::TopLeft ||
        placement == TickPlacement::Both;
    const bool second =
        placement == TickPlacement::BottomRight ||
        placement == TickPlacement::Both;
    const bool horizontal =
        GetOrientation() ==
        Orientation::Horizontal;
    const double primary =
        horizontal ? size.width : size.height;
    constexpr double thumbLength = 14.0;
    const double startPixel =
        std::min(primary * 0.5, thumbLength * 0.5);
    const double travel =
        std::max(0.0,
            primary - thumbLength);
    const Color color = ::Aero::Media::SampleBrush(GetForeground());
    if (AeroGuiInternal::RenderChildren(*this).Empty()) {
        Color trackColor = color;
        trackColor.alpha *= 0.35F;
        const double normalized =
            GetNormalizedValueForLayout();
        Base::Result<void> chrome;
        if (horizontal) {
            chrome = builder.FillRoundedRect(
                {
                    startPixel,
                    std::max(0.0,
                        (size.height - 4.0) * 0.5),
                    travel,
                    std::min(4.0, size.height)},
                trackColor,
                2.0);
            if (chrome) {
                chrome = builder.FillRoundedRect(
                    {
                        std::clamp(normalized, 0.0, 1.0) *
                            travel,
                        std::max(0.0,
                            (size.height -
                                thumbLength) * 0.5),
                        std::min(thumbLength, size.width),
                        std::min(thumbLength, size.height)},
                    color,
                    thumbLength * 0.5);
            }
        } else {
            chrome = builder.FillRoundedRect(
                {
                    std::max(0.0,
                        (size.width - 4.0) * 0.5),
                    startPixel,
                    std::min(4.0, size.width),
                    travel},
                trackColor,
                2.0);
            if (chrome) {
                chrome = builder.FillRoundedRect(
                    {
                        std::max(0.0,
                            (size.width -
                                thumbLength) * 0.5),
                        std::clamp(normalized, 0.0, 1.0) *
                            travel,
                        std::min(thumbLength, size.width),
                        std::min(thumbLength, size.height)},
                    color,
                    thumbLength * 0.5);
            }
        }
        if (!chrome) return;
    }
    if (placement == TickPlacement::None) {
        return;
    }
    auto drawTick = [&](double value)
        noexcept -> Base::Result<void> {
        if (value < GetMinimum() ||
            value > GetMaximum()) {
            return {};
        }
        const double range =
            GetMaximum() - GetMinimum();
        double normalized = range > 0.0
            ? (value - GetMinimum()) / range
            : 0.0;
        if (GetIsDirectionReversed()) {
            normalized = 1.0 - normalized;
        }
        const double pixel =
            startPixel +
            std::clamp(normalized, 0.0, 1.0) *
                travel;
        Base::Result<void> drawn;
        if (horizontal) {
            const double center =
                size.height * 0.5;
            if (first) {
                drawn = builder.FillRect(
                    {pixel, std::max(0.0, center - 9.0),
                     1.0, std::min(5.0, size.height)},
                    color);
                if (!drawn) return {};
            }
            if (second) {
                drawn = builder.FillRect(
                    {pixel, std::min(size.height, center + 4.0),
                     1.0, std::min(5.0, size.height)},
                    color);
            }
        } else {
            const double center =
                size.width * 0.5;
            if (first) {
                drawn = builder.FillRect(
                    {std::max(0.0, center - 9.0), pixel,
                     std::min(5.0, size.width), 1.0},
                    color);
                if (!drawn) return {};
            }
            if (second) {
                drawn = builder.FillRect(
                    {std::min(size.width, center + 4.0), pixel,
                     std::min(5.0, size.width), 1.0},
                    color);
            }
        }
        return drawn;
    };

    const Base::StringView ticks = GetTicks();
    if (!ticks.Empty()) {
        std::uint32_t start = 0U;
        std::uint32_t count = 0U;
        while (start < ticks.SizeBytes() &&
            count < 1024U) {
            while (start < ticks.SizeBytes() &&
                (ticks[start] == ' ' ||
                 ticks[start] == '\t' ||
                 ticks[start] == ',' ||
                 ticks[start] == ';')) {
                ++start;
            }
            if (start >= ticks.SizeBytes()) break;
            std::uint32_t end = start;
            while (end < ticks.SizeBytes() &&
                ticks[end] != ' ' &&
                ticks[end] != '\t' &&
                ticks[end] != ',' &&
                ticks[end] != ';') {
                ++end;
            }
            Base::Result<double> parsed =
                ::Aero::Base::ValueConversion::ParseDouble(
                    ticks.Substr(start, end - start));
            if (parsed) {
                Base::Result<void> drawn =
                    drawTick(parsed.Value());
                if (!drawn) {
                    return;
                }
            }
            start = end;
            ++count;
        }
        return;
    }

    const double frequency = GetTickFrequency();
    std::uint32_t count = 0U;
    for (double value = GetMinimum();
         value <= GetMaximum() +
            frequency * 0.000001 &&
         count < 1024U;
         value += frequency, ++count) {
        Base::Result<void> drawn =
            drawTick(std::min(value, GetMaximum()));
        if (!drawn) return;
    }
    return;
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
    auto& builder = Aero::Render::DrawingPrivate::Builder(context);
    DependencyObject* parent = GetTemplatedParent();
    if (parent == nullptr ||
        !PropertyRegistry().Types().IsDerivedFrom(
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
    constexpr double thumbLength = 14.0;
    const double start = std::min(
        primary * 0.5, thumbLength * 0.5);
    const double travel = std::max(0.0, primary - thumbLength);
    auto drawTick = [&](double value)
        noexcept -> Base::Result<void> {
        if (value < slider.GetMinimum() ||
            value > slider.GetMaximum()) {
            return {};
        }
        double normalized = range > 0.0
            ? (value - slider.GetMinimum()) / range
            : 0.0;
        if (slider.GetIsDirectionReversed()) {
            normalized = 1.0 - normalized;
        }
        const double position = start +
            std::clamp(normalized, 0.0, 1.0) * travel;
        if (horizontal) {
            return builder.FillRect({
                position,
                GetPlacement() == TickBarPlacement::Top
                    ? 0.0
                    : std::max(0.0, size.height - 4.0),
                1.0,
                std::min(4.0, size.height)}, color);
        }
        return builder.FillRect({
            GetPlacement() == TickBarPlacement::Left
                ? 0.0
                : std::max(0.0, size.width - 4.0),
            position,
            std::min(4.0, size.width),
            1.0}, color);
    };

    const Base::StringView ticks = slider.GetTicks();
    if (!ticks.Empty()) {
        std::uint32_t begin = 0U;
        std::uint32_t count = 0U;
        while (begin < ticks.SizeBytes() && count < 1024U) {
            while (begin < ticks.SizeBytes() &&
                (ticks[begin] == ' ' || ticks[begin] == '\t' ||
                 ticks[begin] == ',' || ticks[begin] == ';')) {
                ++begin;
            }
            if (begin >= ticks.SizeBytes()) break;
            std::uint32_t end = begin;
            while (end < ticks.SizeBytes() &&
                ticks[end] != ' ' && ticks[end] != '\t' &&
                ticks[end] != ',' && ticks[end] != ';') {
                ++end;
            }
            Base::Result<double> parsed =
                ::Aero::Base::ValueConversion::ParseDouble(
                    ticks.Substr(begin, end - begin));
            if (parsed) {
                Base::Result<void> rendered = drawTick(parsed.Value());
                if (!rendered) return;
            }
            begin = end;
            ++count;
        }
        return;
    }

    const double frequency = slider.GetTickFrequency();
    for (double value = slider.GetMinimum(), count = 0.0;
         value <= slider.GetMaximum() + frequency * 0.000001 &&
         count < 1024.0;
         value += frequency, count += 1.0) {
        Base::Result<void> rendered = drawTick(
            std::min(value, slider.GetMaximum()));
        if (!rendered) return;
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

} // namespace Aero::Controls
