#pragma once

#include <Aero/Gui/ButtonBase.hpp>
#include <Aero/Gui/RepeatButton.hpp>
#include <Aero/Gui/Panel.hpp>
#include <Aero/Gui/Style.hpp>
#include <Aero/Events/ControlEventArgs.hpp>
#include <Aero/Gui/Brush.hpp>

namespace Aero::Controls {
using ::Aero::Meta::TypeId;
enum class TickPlacement : std::uint8_t {
    None = 0U,
    TopLeft,
    BottomRight,
    Both
};

enum class TickBarPlacement : std::uint8_t {
    Top = 0U,
    Bottom,
    Left,
    Right
};

enum class GridResizeDirection : std::uint8_t {
    Auto = 0U,
    Columns,
    Rows
};

enum class GridResizeBehavior : std::uint8_t {
    BasedOnAlignment = 0U,
    CurrentAndNext,
    PreviousAndCurrent,
    PreviousAndNext
};

struct ThumbDragDelta {
    double horizontalChange = 0.0;
    double verticalChange = 0.0;
};
namespace Primitives {
class AERO_API Thumb : public Control {
    AERO_DECLARE_TYPE(Thumb, Control)
public:
    Thumb() noexcept : Control(StaticTypeId()) {}
    ~Thumb() override = default;

    bool GetIsDragging() const noexcept {
        return GetValueOr(
            IsDraggingProperty, false);
    }
    Base::Result<void> BeginDrag(
        std::uint32_t pointerId,
        Point position) noexcept;
    Base::Result<ThumbDragDelta> DragTo(
        std::uint32_t pointerId,
        Point position) noexcept;
    Base::Result<bool> EndDrag(
        std::uint32_t pointerId) noexcept;

    inline static constexpr ReadOnlyDependencyProperty<bool> IsDraggingProperty{"IsDragging"};

private:
    std::uint32_t pointerId_ = 0U;
    Point lastPosition_;
    bool dragging_ = false;
};

class AERO_API Track : public Control {
    AERO_DECLARE_TYPE(Track, Control)
public:
    Track() noexcept : Control(StaticTypeId()) {}
    ~Track() override = default;

    Orientation GetOrientation() const noexcept;
    double GetMinimum() const noexcept;
    double GetMaximum() const noexcept;
    double GetValue() const noexcept;
    double GetViewportSize() const noexcept;
    bool GetIsDirectionReversed() const noexcept;
    Base::Ref<RepeatButton>
    GetDecreaseRepeatButton() const noexcept {
        return decreaseRepeatButton_;
    }
    Base::Ref<Thumb> GetThumbElement() const noexcept {
        return thumb_;
    }
    Base::Ref<RepeatButton>
    GetIncreaseRepeatButton() const noexcept {
        return increaseRepeatButton_;
    }
    void SetOrientation(
        Orientation value) noexcept;
    void SetRange(
        double minimum,
        double maximum) noexcept;
    void SetValue(
        double value) noexcept;
    void SetViewportSize(
        double value) noexcept;
    void SetIsDirectionReversed(
        bool value) noexcept;
    void SetDecreaseRepeatButton(
        Base::Ref<RepeatButton> value) noexcept;
    void SetThumb(
        Base::Ref<Thumb> value) noexcept;
    void SetIncreaseRepeatButton(
        Base::Ref<RepeatButton> value) noexcept;
    double GetThumbLength(
        double trackLength,
        double minimumThumbLength = 8.0) const noexcept;
    double GetThumbOffset(
        double trackLength,
        double minimumThumbLength = 8.0) const noexcept;
    Base::Result<double> ValueFromThumbOffset(
        double offset,
        double trackLength,
        double minimumThumbLength = 8.0) const noexcept;

    inline static constexpr DependencyProperty<Orientation> OrientationProperty{"Orientation"};
    inline static constexpr DependencyProperty<double> MinimumProperty{"Minimum"};
    inline static constexpr DependencyProperty<double> MaximumProperty{"Maximum"};
    inline static constexpr DependencyProperty<double> ValueProperty{"Value"};
    inline static constexpr DependencyProperty<double> ViewportSizeProperty{"ViewportSize"};
    inline static constexpr DependencyProperty<bool> IsDirectionReversedProperty{"IsDirectionReversed"};

protected:
    Size MeasureOverride(
        Size availableSize) noexcept override;
    Size ArrangeOverride(
        Size finalSize) noexcept override;

private:
    Base::Ref<RepeatButton> decreaseRepeatButton_;
    Base::Ref<Thumb> thumb_;
    Base::Ref<RepeatButton> increaseRepeatButton_;
};

class AERO_API RangeBase : public Control {
    AERO_DECLARE_TYPE(RangeBase, Control)
public:
    double GetMinimum() const noexcept;
    double GetMaximum() const noexcept;
    double GetValue() const noexcept;
    void SetMinimum(double value) noexcept;
    void SetMaximum(double value) noexcept;
    void SetRange(
        double minimum,
        double maximum) noexcept;
    void SetValue(double value) noexcept;

    inline static constexpr RoutedEvent<RangeValueChangedEventArgs> ValueChangedEvent{"ValueChanged"};
    UIElement::Event<RangeValueChangedEventArgs>
        ValueChanged() noexcept {
        return GetEvent(ValueChangedEvent);
    }
    inline static constexpr DependencyProperty<double> MinimumProperty{"Minimum"};
    inline static constexpr DependencyProperty<double> MaximumProperty{"Maximum"};
    inline static constexpr DependencyProperty<double> ValueProperty{"Value"};

protected:
    explicit RangeBase(TypeId runtimeType) noexcept;
    ~RangeBase() override;
    virtual void OnValueChanged(
        double oldValue,
        double newValue) noexcept;

private:
    DependencyPropertyChangedEventHandler
        rangeChangedHandler_;
    void OnRangePropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs&
            args) noexcept;
};

class AERO_API ScrollBar : public RangeBase {
    AERO_DECLARE_TYPE(ScrollBar, RangeBase)
public:
    ScrollBar() noexcept;
    ~ScrollBar() override;

    Orientation GetOrientation() const noexcept;
    double GetViewportSize() const noexcept;
    double GetSmallChange() const noexcept;
    double GetLargeChange() const noexcept;
    void SetOrientation(
        Orientation value) noexcept;
    void SetViewportSize(
        double value) noexcept;
    void SetSmallChange(
        double value) noexcept;
    void SetLargeChange(
        double value) noexcept;
    Base::Result<bool> LineDecrement() noexcept;
    Base::Result<bool> LineIncrement() noexcept;
    Base::Result<bool> PageDecrement() noexcept;
    Base::Result<bool> PageIncrement() noexcept;
    Base::Result<bool> DragThumb(
        double thumbOffset,
        double trackLength,
        double minimumThumbLength = 8.0) noexcept;

    inline static constexpr DependencyProperty<Orientation> OrientationProperty{"Orientation"};
    inline static constexpr DependencyProperty<double> ViewportSizeProperty{"ViewportSize"};
    inline static constexpr DependencyProperty<double> SmallChangeProperty{"SmallChange"};
    inline static constexpr DependencyProperty<double> LargeChangeProperty{"LargeChange"};

protected:
    void OnApplyTemplate() noexcept override;
    void OnTemplateDetached() noexcept override;

private:
    Track* track_ = nullptr;
    DependencyPropertyChangedEventHandler
        trackPropertyChangedHandler_;
    void OnTrackPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs&
            args) noexcept;
    void SynchronizeTrack() noexcept;
};
} // namespace Primitives

// WPF-compatible GridSplitter surface. The splitter carries the full
// resize-policy state even when the hosting grid chooses to apply the delta
// through a custom interaction adapter.
class AERO_API GridSplitter : public Control {
    AERO_DECLARE_TYPE(GridSplitter, Control)
public:
    GridSplitter() noexcept : Control(StaticTypeId()) {}
    ~GridSplitter() override = default;

    double GetDragIncrement() const noexcept;
    double GetKeyboardIncrement() const noexcept;
    GridResizeDirection GetResizeDirection() const noexcept;
    GridResizeBehavior GetResizeBehavior() const noexcept;
    bool GetShowsPreview() const noexcept;
    Base::Ref<Aero::Style> GetPreviewStyle() const noexcept;
    void SetDragIncrement(double value) noexcept;
    void SetKeyboardIncrement(double value) noexcept;
    void SetResizeDirection(
        GridResizeDirection value) noexcept;
    void SetResizeBehavior(
        GridResizeBehavior value) noexcept;
    void SetShowsPreview(bool value) noexcept;
    void SetPreviewStyle(
        Base::Ref<Aero::Style> value) noexcept;

    inline static constexpr DependencyProperty<double> DragIncrementProperty{"DragIncrement"};
    inline static constexpr DependencyProperty<double> KeyboardIncrementProperty{"KeyboardIncrement"};
    inline static constexpr DependencyProperty<GridResizeDirection> ResizeDirectionProperty{"ResizeDirection"};
    inline static constexpr DependencyProperty<GridResizeBehavior> ResizeBehaviorProperty{"ResizeBehavior"};
    inline static constexpr DependencyProperty<bool> ShowsPreviewProperty{"ShowsPreview"};
    inline static constexpr DependencyProperty<Base::Ref<Aero::Style>> PreviewStyleProperty{"PreviewStyle"};
};

class AERO_API Slider : public Primitives::RangeBase {
    AERO_DECLARE_TYPE(Slider, Primitives::RangeBase)
public:
    struct Impl;

    Slider() noexcept;
    ~Slider() override;

    Orientation GetOrientation() const noexcept;
    double GetSmallChange() const noexcept;
    double GetLargeChange() const noexcept;
    TickPlacement GetTickPlacement() const noexcept;
    double GetTickFrequency() const noexcept;
    Base::StringView GetTicks() const noexcept;
    bool GetIsSnapToTickEnabled() const noexcept;
    bool GetIsDirectionReversed() const noexcept;
    bool GetIsMoveToPointEnabled() const noexcept;
    void SetOrientation(
        Orientation value) noexcept;
    void SetSmallChange(
        double value) noexcept;
    void SetLargeChange(
        double value) noexcept;
    void SetTickPlacement(
        TickPlacement value) noexcept;
    void SetTickFrequency(
        double value) noexcept;
    void SetTicks(
        Base::StringView value) noexcept;
    void SetIsSnapToTickEnabled(
        bool value) noexcept;
    void SetIsDirectionReversed(
        bool value) noexcept;
    void SetIsMoveToPointEnabled(
        bool value) noexcept;
    Base::Result<bool> DecreaseSmall() noexcept;
    Base::Result<bool> IncreaseSmall() noexcept;
    Base::Result<bool> DecreaseLarge() noexcept;
    Base::Result<bool> IncreaseLarge() noexcept;
    void SetValueFromPosition(
        double position,
        double trackLength) noexcept;

    inline static constexpr DependencyProperty<Orientation> OrientationProperty{"Orientation"};
    inline static constexpr DependencyProperty<double> SmallChangeProperty{"SmallChange"};
    inline static constexpr DependencyProperty<double> LargeChangeProperty{"LargeChange"};
    inline static constexpr DependencyProperty<TickPlacement> TickPlacementProperty{"TickPlacement"};
    inline static constexpr DependencyProperty<double> TickFrequencyProperty{"TickFrequency"};
    inline static constexpr DependencyProperty<Base::String> TicksProperty{"Ticks"};
    inline static constexpr DependencyProperty<bool> IsSnapToTickEnabledProperty{"IsSnapToTickEnabled"};
    inline static constexpr DependencyProperty<bool> IsDirectionReversedProperty{"IsDirectionReversed"};
    inline static constexpr DependencyProperty<bool> IsMoveToPointEnabledProperty{"IsMoveToPointEnabled"};

protected:
    void OnApplyTemplate() noexcept override;
    void OnTemplateDetached() noexcept override;
    Size ArrangeOverride(
        Size finalSize) noexcept override;
    void OnRender(
        ::Aero::Media::DrawingContext& context) noexcept override;

private:
    Primitives::Track* track_ = nullptr;
    DependencyPropertyChangedEventHandler trackPropertyChangedHandler_;
    void OnTrackPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs& args) noexcept;
    void SynchronizeTrack() noexcept;
    double GetNormalizedValueForLayout() const noexcept;
    double GetSnapValue(double value) const noexcept;
};

class AERO_API TickBar : public Control {
    AERO_DECLARE_TYPE(TickBar, Control)
public:
    TickBar() noexcept : Control(StaticTypeId()) {}
    ~TickBar() override = default;

    Base::Ref<Aero::Media::Brush> GetFill() const noexcept;
    TickBarPlacement GetPlacement() const noexcept;
    void SetFill(
        Base::Ref<Aero::Media::Brush> value) noexcept;
    void SetPlacement(
        TickBarPlacement value) noexcept;

    inline static constexpr DependencyProperty<Base::Ref<Aero::Media::Brush>> FillProperty{"Fill"};
    inline static constexpr DependencyProperty<TickBarPlacement> PlacementProperty{"Placement"};

protected:
    void OnRender(
        Aero::Media::DrawingContext& context) noexcept override;
};

class AERO_API ProgressBar : public Primitives::RangeBase {
    AERO_DECLARE_TYPE(ProgressBar, Primitives::RangeBase)
public:
    ProgressBar() noexcept : Primitives::RangeBase(StaticTypeId()) {}
    ~ProgressBar() override = default;

    bool GetIsIndeterminate() const noexcept;
    Orientation GetOrientation() const noexcept;
    void SetIsIndeterminate(
        bool value) noexcept;
    void SetOrientation(
        Orientation value) noexcept;
    double GetNormalizedValue() const noexcept;

    inline static constexpr DependencyProperty<bool> IsIndeterminateProperty{"IsIndeterminate"};
    inline static constexpr DependencyProperty<Orientation> OrientationProperty{"Orientation"};
};
} // namespace Aero::Controls
AERO_DECLARE_TYPE_ENUM(Aero::Controls::TickPlacement)
AERO_DECLARE_TYPE_ENUM(Aero::Controls::TickBarPlacement)
AERO_DECLARE_TYPE_ENUM(Aero::Controls::GridResizeDirection)
AERO_DECLARE_TYPE_ENUM(Aero::Controls::GridResizeBehavior)
