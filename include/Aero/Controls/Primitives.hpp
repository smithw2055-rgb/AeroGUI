#pragma once

#include <Aero/Styling.hpp>
#include <Aero/Input.hpp>
#include <Aero/Controls/Core.hpp>
#include <Aero/Controls/Panels.hpp>
#include <Aero/Events/ControlEventArgs.hpp>

namespace Aero::Controls {

using ::Aero::Meta::TypeId;
using ::Aero::Input::ICommand;

enum class ClickMode : std::uint8_t {
    Release = 0U,
    Press,
    Hover,
};

namespace Primitives {

class AERO_API ButtonBase : public ContentControl {
    AERO_DECLARE_TYPE(ButtonBase, ContentControl)
public:
    struct Impl;

    inline static constexpr Members::RoutedEvent<RoutedEventArgs> ClickEvent{"Click"};
    UIElement::Event<RoutedEventArgs> Click() noexcept {
        return GetEvent(ClickEvent);
    }

    ClickMode GetClickMode() const noexcept;
    ICommand* GetCommand() const noexcept;
    Value GetCommandParameter() const noexcept;
    UIElement* GetCommandTarget() const noexcept;
    bool GetIsCommandEnabled() const noexcept {
        return commandEnabled_;
    }

    void SetClickMode(ClickMode value) noexcept;
    void SetCommand(
        Base::Ref<ICommand> command) noexcept;
    void SetCommandParameter(Value parameter) noexcept;
    void SetCommandTarget(
        Base::Ref<UIElement> target) noexcept;

    inline static constexpr Members::Property<ClickMode> ClickModeProperty{"ClickMode"};
    inline static constexpr Members::Property<Base::Ref<ICommand>> CommandProperty{"Command"};
    inline static constexpr Members::Property<Value> CommandParameterProperty{"CommandParameter"};
    inline static constexpr Members::Property<Base::Ref<UIElement>> CommandTargetProperty{"CommandTarget"};

protected:
    explicit ButtonBase(TypeId runtimeType) noexcept
        : ContentControl(runtimeType) {}
    ~ButtonBase() override;
    void OnApplyTemplate() noexcept override;

private:
    friend struct Impl;
    bool commandEnabled_ = true;
};

} // namespace Primitives

class AERO_API Button : public Primitives::ButtonBase {
    AERO_DECLARE_TYPE(Button, Primitives::ButtonBase)
public:
    Button() noexcept : Button(StaticTypeId()) {}
    ~Button() override = default;

protected:
    explicit Button(TypeId runtimeType) noexcept
        : Primitives::ButtonBase(runtimeType) {}
};

namespace Primitives {

class AERO_API RepeatButton : public ButtonBase {
    AERO_DECLARE_TYPE(RepeatButton, ButtonBase)
public:
    RepeatButton() noexcept : RepeatButton(StaticTypeId()) {}
    ~RepeatButton() override = default;

    std::uint32_t GetDelay() const noexcept;
    std::uint32_t GetInterval() const noexcept;
    void SetDelay(std::uint32_t value) noexcept;
    void SetInterval(std::uint32_t value) noexcept;

    inline static constexpr Members::Property<std::uint32_t> DelayProperty{"Delay"};
    inline static constexpr Members::Property<std::uint32_t> IntervalProperty{"Interval"};

protected:
    explicit RepeatButton(TypeId runtimeType) noexcept
        : ButtonBase(runtimeType) {}
};

class AERO_API ToggleButton : public ButtonBase {
    AERO_DECLARE_TYPE(ToggleButton, ButtonBase)
public:
    ToggleButton() noexcept : ToggleButton(StaticTypeId()) {}
    ~ToggleButton() override = default;

    bool GetIsChecked() const noexcept;
    bool GetIsThreeState() const noexcept;
    bool GetIsIndeterminate() const noexcept;
    void SetIsChecked(bool value) noexcept;
    void SetIsThreeState(bool value) noexcept;

    inline static constexpr Members::RoutedEvent<RoutedEventArgs> CheckedEvent{"Checked"};
    inline static constexpr Members::RoutedEvent<RoutedEventArgs> UncheckedEvent{"Unchecked"};
    inline static constexpr Members::RoutedEvent<RoutedEventArgs> IndeterminateEvent{"Indeterminate"};
    UIElement::Event<RoutedEventArgs> Checked() noexcept {
        return GetEvent(CheckedEvent);
    }
    UIElement::Event<RoutedEventArgs> Unchecked() noexcept {
        return GetEvent(UncheckedEvent);
    }
    UIElement::Event<RoutedEventArgs> Indeterminate() noexcept {
        return GetEvent(IndeterminateEvent);
    }

    inline static constexpr Members::Property<bool> IsCheckedProperty{"IsChecked"};
    inline static constexpr Members::Property<bool> IsThreeStateProperty{"IsThreeState"};
    inline static constexpr Members::ReadOnlyProperty<bool> IsIndeterminateProperty{"IsIndeterminate"};

protected:
    explicit ToggleButton(TypeId runtimeType) noexcept
        : ButtonBase(runtimeType) {}

private:
    friend struct ::Aero::Controls::Primitives::ButtonBase::Impl;
    void SetToggleState(
        std::uint8_t value) noexcept;
};

} // namespace Primitives

class AERO_API CheckBox : public Primitives::ToggleButton {
    AERO_DECLARE_TYPE(CheckBox, Primitives::ToggleButton)
public:
    CheckBox() noexcept : CheckBox(StaticTypeId()) {}
    ~CheckBox() override = default;

protected:
    explicit CheckBox(TypeId runtimeType) noexcept
        : Primitives::ToggleButton(runtimeType) {}
};

class AERO_API RadioButton : public Primitives::ToggleButton {
    AERO_DECLARE_TYPE(RadioButton, Primitives::ToggleButton)
public:
    RadioButton() noexcept : RadioButton(StaticTypeId()) {}
    ~RadioButton() override = default;

    Base::StringView GetGroupName() const noexcept;
    void SetGroupName(
        Base::StringView value) noexcept;

    inline static constexpr Members::Property<Base::String> GroupNameProperty{"GroupName"};

protected:
    explicit RadioButton(TypeId runtimeType) noexcept
        : Primitives::ToggleButton(runtimeType) {}
};


} // namespace Aero::Controls

AERO_DECLARE_TYPE_ENUM(Aero::Controls::ClickMode)

namespace Aero::Controls {

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

enum class ScrollBarVisibility : std::uint8_t {
    Disabled = 0U,
    Auto,
    Hidden,
    Visible,
};

// Mirrors WPF's gesture-direction policy. Pointer/touch routing can use this
// value without changing the established mouse-wheel scroll behavior.
enum class PanningMode : std::uint8_t {
    None = 0U,
    HorizontalOnly,
    VerticalOnly,
    Both,
    HorizontalFirst,
    VerticalFirst,
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

class AERO_API IScrollInfo {
public:
    virtual ~IScrollInfo() = default;
    virtual ScrollData GetData() const noexcept = 0;
    virtual void SetViewport(
        Size viewport) noexcept = 0;
    virtual void SetHorizontalOffset(
        double value) noexcept = 0;
    virtual void SetVerticalOffset(
        double value) noexcept = 0;
    virtual Base::Result<bool> LineHorizontal(
        double direction) noexcept = 0;
    virtual Base::Result<bool> LineVertical(
        double direction) noexcept = 0;
    virtual Base::Result<bool> PageHorizontal(
        double direction) noexcept = 0;
    virtual Base::Result<bool> PageVertical(
        double direction) noexcept = 0;
};


class AERO_API ScrollContentPresenter
    : public ContentControl,
      public IScrollInfo {
    AERO_DECLARE_TYPE(ScrollContentPresenter, ContentControl)
public:
    ScrollContentPresenter() noexcept;
    ~ScrollContentPresenter() override = default;

    ScrollData GetData() const noexcept override;
    IScrollInfo* GetContentScrollInfo() const noexcept {
        return contentScrollInfo_;
    }
    void SetContentScrollInfo(
        IScrollInfo* value) noexcept;

    bool GetCanHorizontallyScroll() const noexcept;
    bool GetCanVerticallyScroll() const noexcept;
    bool GetCanContentScroll() const noexcept;
    void SetCanHorizontallyScroll(
        bool value) noexcept;
    void SetCanVerticallyScroll(
        bool value) noexcept;
    void SetCanContentScroll(
        bool value) noexcept;
    inline static constexpr Members::Property<bool> CanContentScrollProperty{"CanContentScroll"};

    void SetViewport(
        Size viewport) noexcept override;
    void SetHorizontalOffset(
        double value) noexcept override;
    void SetVerticalOffset(
        double value) noexcept override;
    Base::Result<bool> LineHorizontal(
        double direction) noexcept override;
    Base::Result<bool> LineVertical(
        double direction) noexcept override;
    Base::Result<bool> PageHorizontal(
        double direction) noexcept override;
    Base::Result<bool> PageVertical(
        double direction) noexcept override;
    Base::Result<bool> ApplyScrollDelta(
        double deltaX,
        double deltaY,
        ScrollInputKind kind) noexcept;

    double GetLineScrollAmount() const noexcept {
        return lineScrollAmount_;
    }
    void SetLineScrollAmount(
        double value) noexcept;

protected:
    explicit ScrollContentPresenter(
        TypeId runtimeType) noexcept;
    Size MeasureOverride(
        Size availableSize) noexcept override;
    Size ArrangeOverride(
        Size finalSize) noexcept override;
    virtual void OnScrollDataChanged(
        const ScrollData& oldData,
        const ScrollData& newData,
        ScrollInputKind kind) noexcept;
    virtual bool GetAllowsHorizontalScroll() const noexcept;
    virtual bool GetAllowsVerticalScroll() const noexcept;
    virtual bool GetUsesContentScrolling() const noexcept;
    Base::Result<bool> UpdateData(
        ScrollData value,
        ScrollInputKind kind,
        bool invalidateArrange) noexcept;

private:
    ScrollData data_;
    IScrollInfo* contentScrollInfo_ = nullptr;
    double lineScrollAmount_ = 16.0;
    bool canHorizontallyScroll_ = false;
    bool canVerticallyScroll_ = true;
    ScrollInputKind pendingInputKind_ = ScrollInputKind::Line;

    Base::Result<bool> SyncLogicalData(
        ScrollInputKind kind) noexcept;
    IScrollInfo* ActiveContentScrollInfo() const noexcept;
};

class AERO_API ScrollViewer
    : public ScrollContentPresenter {
    AERO_DECLARE_TYPE(ScrollViewer, ScrollContentPresenter)
public:
    struct Impl;

    ScrollViewer() noexcept;
    ~ScrollViewer() override;

    inline static constexpr Members::RoutedEvent<ScrollChangedEventArgs> ScrollChangedEvent{"ScrollChanged"};
    UIElement::Event<ScrollChangedEventArgs>
        ScrollChanged() noexcept {
        return GetEvent(ScrollChangedEvent);
    }

    double GetHorizontalOffset() const noexcept;
    double GetVerticalOffset() const noexcept;
    double GetExtentWidth() const noexcept;
    double GetExtentHeight() const noexcept;
    double GetViewportWidth() const noexcept;
    double GetViewportHeight() const noexcept;
    double GetScrollableWidth() const noexcept;
    double GetScrollableHeight() const noexcept;
    ScrollBarVisibility
    GetHorizontalScrollBarVisibility() const noexcept;
    ScrollBarVisibility
    GetVerticalScrollBarVisibility() const noexcept;
    Visibility
    GetComputedHorizontalScrollBarVisibility() const noexcept;
    Visibility
    GetComputedVerticalScrollBarVisibility() const noexcept;

    void SetCanHorizontallyScroll(
        bool value) noexcept;
    void SetCanVerticallyScroll(
        bool value) noexcept;
    void SetCanContentScroll(
        bool value) noexcept;
    void SetHorizontalScrollBarVisibility(
        ScrollBarVisibility value) noexcept;
    void SetVerticalScrollBarVisibility(
        ScrollBarVisibility value) noexcept;
    PanningMode GetPanningMode() const noexcept;
    void SetPanningMode(
        PanningMode value) noexcept;
    void SetHorizontalOffset(
        double value) noexcept override;
    void SetVerticalOffset(
        double value) noexcept override;
    Base::Result<bool> LineHorizontal(
        double direction) noexcept override;
    Base::Result<bool> LineVertical(
        double direction) noexcept override;
    Base::Result<bool> PageHorizontal(
        double direction) noexcept override;
    Base::Result<bool> PageVertical(
        double direction) noexcept override;

    static ScrollBarVisibility
    GetHorizontalScrollBarVisibility(
        const DependencyObject& element) noexcept;
    static ScrollBarVisibility
    GetVerticalScrollBarVisibility(
        const DependencyObject& element) noexcept;
    static void SetHorizontalScrollBarVisibility(
        DependencyObject& element,
        ScrollBarVisibility value) noexcept;
    static void SetVerticalScrollBarVisibility(
        DependencyObject& element,
        ScrollBarVisibility value) noexcept;

    inline static constexpr Members::ReadOnlyProperty<double> HorizontalOffsetProperty{"HorizontalOffset"};
    inline static constexpr Members::ReadOnlyProperty<double> VerticalOffsetProperty{"VerticalOffset"};
    inline static constexpr Members::ReadOnlyProperty<double> ExtentWidthProperty{"ExtentWidth"};
    inline static constexpr Members::ReadOnlyProperty<double> ExtentHeightProperty{"ExtentHeight"};
    inline static constexpr Members::ReadOnlyProperty<double> ViewportWidthProperty{"ViewportWidth"};
    inline static constexpr Members::ReadOnlyProperty<double> ViewportHeightProperty{"ViewportHeight"};
    inline static constexpr Members::ReadOnlyProperty<double> ScrollableWidthProperty{"ScrollableWidth"};
    inline static constexpr Members::ReadOnlyProperty<double> ScrollableHeightProperty{"ScrollableHeight"};
    inline static constexpr Members::ReadOnlyProperty<Visibility> ComputedHorizontalScrollBarVisibilityProperty{"ComputedHorizontalScrollBarVisibility"};
    inline static constexpr Members::ReadOnlyProperty<Visibility> ComputedVerticalScrollBarVisibilityProperty{"ComputedVerticalScrollBarVisibility"};
    inline static constexpr Members::AttachedProperty<ScrollBarVisibility> HorizontalScrollBarVisibilityProperty{"HorizontalScrollBarVisibility"};
    inline static constexpr Members::AttachedProperty<ScrollBarVisibility> VerticalScrollBarVisibilityProperty{"VerticalScrollBarVisibility"};
    inline static constexpr Members::Property<bool> CanHorizontallyScrollProperty{"CanHorizontallyScroll"};
    inline static constexpr Members::Property<bool> CanVerticallyScrollProperty{"CanVerticallyScroll"};
    inline static constexpr Members::AttachedProperty<bool> CanContentScrollProperty{"CanContentScroll"};
    inline static constexpr Members::AttachedProperty<PanningMode> PanningModeProperty{"PanningMode"};

protected:
    void OnApplyTemplate() noexcept override;
    Size MeasureOverride(
        Size availableSize) noexcept override;
    void OnScrollDataChanged(
        const ScrollData& oldData,
        const ScrollData& newData,
        ScrollInputKind kind) noexcept override;
    bool GetAllowsHorizontalScroll() const noexcept override;
    bool GetAllowsVerticalScroll() const noexcept override;
    bool GetUsesContentScrolling() const noexcept override;
    void OnTemplateDetached() noexcept override;

private:
    friend class ScrollContentPresenter;
    friend struct Impl;
    ScrollContentPresenter* contentPresenter_ = nullptr;
    void AdoptPresenterData(
        ScrollContentPresenter& presenter,
        const ScrollData& data,
        ScrollInputKind kind) noexcept;
    void UpdateComputedScrollBarVisibility(
        const ScrollData& data) noexcept;
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

    inline static constexpr Members::ReadOnlyProperty<bool> IsDraggingProperty{"IsDragging"};

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

    inline static constexpr Members::Property<Orientation> OrientationProperty{"Orientation"};
    inline static constexpr Members::Property<double> MinimumProperty{"Minimum"};
    inline static constexpr Members::Property<double> MaximumProperty{"Maximum"};
    inline static constexpr Members::Property<double> ValueProperty{"Value"};
    inline static constexpr Members::Property<double> ViewportSizeProperty{"ViewportSize"};
    inline static constexpr Members::Property<bool> IsDirectionReversedProperty{"IsDirectionReversed"};

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

    inline static constexpr Members::Property<double> DragIncrementProperty{"DragIncrement"};
    inline static constexpr Members::Property<double> KeyboardIncrementProperty{"KeyboardIncrement"};
    inline static constexpr Members::Property<GridResizeDirection> ResizeDirectionProperty{"ResizeDirection"};
    inline static constexpr Members::Property<GridResizeBehavior> ResizeBehaviorProperty{"ResizeBehavior"};
    inline static constexpr Members::Property<bool> ShowsPreviewProperty{"ShowsPreview"};
    inline static constexpr Members::Property<Base::Ref<Aero::Style>> PreviewStyleProperty{"PreviewStyle"};
};

namespace Primitives {

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

    inline static constexpr Members::RoutedEvent<RangeValueChangedEventArgs> ValueChangedEvent{"ValueChanged"};
    UIElement::Event<RangeValueChangedEventArgs>
        ValueChanged() noexcept {
        return GetEvent(ValueChangedEvent);
    }
    inline static constexpr Members::Property<double> MinimumProperty{"Minimum"};
    inline static constexpr Members::Property<double> MaximumProperty{"Maximum"};
    inline static constexpr Members::Property<double> ValueProperty{"Value"};

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

    inline static constexpr Members::Property<Orientation> OrientationProperty{"Orientation"};
    inline static constexpr Members::Property<double> ViewportSizeProperty{"ViewportSize"};
    inline static constexpr Members::Property<double> SmallChangeProperty{"SmallChange"};
    inline static constexpr Members::Property<double> LargeChangeProperty{"LargeChange"};

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

    inline static constexpr Members::Property<Orientation> OrientationProperty{"Orientation"};
    inline static constexpr Members::Property<double> SmallChangeProperty{"SmallChange"};
    inline static constexpr Members::Property<double> LargeChangeProperty{"LargeChange"};
    inline static constexpr Members::Property<TickPlacement> TickPlacementProperty{"TickPlacement"};
    inline static constexpr Members::Property<double> TickFrequencyProperty{"TickFrequency"};
    inline static constexpr Members::Property<Base::String> TicksProperty{"Ticks"};
    inline static constexpr Members::Property<bool> IsSnapToTickEnabledProperty{"IsSnapToTickEnabled"};
    inline static constexpr Members::Property<bool> IsDirectionReversedProperty{"IsDirectionReversed"};
    inline static constexpr Members::Property<bool> IsMoveToPointEnabledProperty{"IsMoveToPointEnabled"};

protected:
    void OnApplyTemplate() noexcept override;
    void OnTemplateDetached() noexcept override;
    Size ArrangeOverride(
        Size finalSize) noexcept override;
    void OnRender(
        DrawingContext& context) noexcept override;

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

    inline static constexpr Members::Property<Base::Ref<Aero::Media::Brush>> FillProperty{"Fill"};
    inline static constexpr Members::Property<TickBarPlacement> PlacementProperty{"Placement"};

protected:
    void OnRender(
        Aero::DrawingContext& context) noexcept override;
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

    inline static constexpr Members::Property<bool> IsIndeterminateProperty{"IsIndeterminate"};
    inline static constexpr Members::Property<Orientation> OrientationProperty{"Orientation"};
};



} // namespace Aero::Controls

AERO_DECLARE_TYPE_ENUM(Aero::Controls::TickPlacement)
AERO_DECLARE_TYPE_ENUM(Aero::Controls::TickBarPlacement)
AERO_DECLARE_TYPE_ENUM(Aero::Controls::ScrollBarVisibility)
AERO_DECLARE_TYPE_ENUM(Aero::Controls::PanningMode)
AERO_DECLARE_TYPE_ENUM(Aero::Controls::GridResizeDirection)
AERO_DECLARE_TYPE_ENUM(Aero::Controls::GridResizeBehavior)
