#pragma once

#include <Aero/Detail/RuntimeManagersFwd.hpp>

#include <Aero/Controls/ControlPrimitives.hpp>
#include <Aero/Controls/Controls.hpp>
#include <Aero/Presentation/Input.hpp>

namespace Aero::Presentation {
}

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

enum class ScrollInputKind : std::uint8_t {
    Line = 0U,
    Page,
    Wheel,
    Thumb,
    Touch,
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

struct ScrollData final {
    double horizontalOffset = 0.0;
    double verticalOffset = 0.0;
    double extentWidth = 0.0;
    double extentHeight = 0.0;
    double viewportWidth = 0.0;
    double viewportHeight = 0.0;
};

class AERO_API IScrollInfo {
public:
    virtual ~IScrollInfo() = default;
    virtual ScrollData Data() const noexcept = 0;
    virtual Base::Result<bool> SetViewport(
        Size viewport) noexcept = 0;
    virtual Base::Result<bool> SetHorizontalOffset(
        double value) noexcept = 0;
    virtual Base::Result<bool> SetVerticalOffset(
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


struct ScrollChangedEventArgs final : RoutedEventArgs {
    AERO_DECLARE_TYPE(ScrollChangedEventArgs, RoutedEventArgs)
    ScrollChangedEventArgs() noexcept
        : RoutedEventArgs(StaticTypeId()) {}
    ScrollData oldData;
    ScrollData newData;
    ScrollInputKind inputKind = ScrollInputKind::Line;
};

using ScrollChangedEventHandler =
    Base::Delegate<void(
        Base::Object*, const ScrollChangedEventArgs&)>;

class AERO_API ScrollContentPresenter
    : public ContentControl,
      public IScrollInfo {
    AERO_DECLARE_TYPE(ScrollContentPresenter, ContentControl)
public:
    ScrollContentPresenter() noexcept;
    ~ScrollContentPresenter() override = default;

    ScrollData Data() const noexcept override;
    IScrollInfo* ContentScrollInfo() const noexcept {
        return contentScrollInfo_;
    }
    Base::Result<void> SetContentScrollInfo(
        IScrollInfo* value) noexcept;

    bool CanHorizontallyScroll() const noexcept;
    bool CanVerticallyScroll() const noexcept;
    bool CanContentScroll() const noexcept;
    Base::Result<void> SetCanHorizontallyScroll(
        bool value) noexcept;
    Base::Result<void> SetCanVerticallyScroll(
        bool value) noexcept;
    Base::Result<void> SetCanContentScroll(
        bool value) noexcept;
    inline static constexpr Members::Property<bool>
        CanContentScrollProperty{
            "CanContentScroll"};

    Base::Result<bool> SetViewport(
        Size viewport) noexcept override;
    Base::Result<bool> SetHorizontalOffset(
        double value) noexcept override;
    Base::Result<bool> SetVerticalOffset(
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

    double LineScrollAmount() const noexcept {
        return lineScrollAmount_;
    }
    Base::Result<void> SetLineScrollAmount(
        double value) noexcept;

protected:
    explicit ScrollContentPresenter(
        TypeId runtimeType) noexcept;
    Base::Result<Size> MeasureOverride(
        Size availableSize) noexcept override;
    Base::Result<Size> ArrangeOverride(
        Size finalSize) noexcept override;
    virtual void OnScrollDataChanged(
        const ScrollData& oldData,
        const ScrollData& newData,
        ScrollInputKind kind) noexcept;
    virtual bool AllowsHorizontalScroll() const noexcept;
    virtual bool AllowsVerticalScroll() const noexcept;
    virtual bool UsesContentScrolling() const noexcept;
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

class AERO_API ScrollViewer final
    : public ScrollContentPresenter {
    AERO_DECLARE_TYPE(ScrollViewer, ScrollContentPresenter)
public:
    ScrollViewer() noexcept;
    ~ScrollViewer() override;

    inline static constexpr Members::RoutedEvent<
        ScrollChangedEventArgs>
        ScrollChangedEvent{"ScrollChanged"};
    UIElement::RoutedEvent_<ScrollChangedEventHandler>
        ScrollChanged() noexcept {
        return Event(ScrollChangedEvent);
    }

    double HorizontalOffset() const noexcept;
    double VerticalOffset() const noexcept;
    double ExtentWidth() const noexcept;
    double ExtentHeight() const noexcept;
    double ViewportWidth() const noexcept;
    double ViewportHeight() const noexcept;
    double ScrollableWidth() const noexcept;
    double ScrollableHeight() const noexcept;
    ScrollBarVisibility
    HorizontalScrollBarVisibility() const noexcept;
    ScrollBarVisibility
    VerticalScrollBarVisibility() const noexcept;
    Visibility
    ComputedHorizontalScrollBarVisibility() const noexcept;
    Visibility
    ComputedVerticalScrollBarVisibility() const noexcept;

    Base::Result<void> SetCanHorizontallyScroll(
        bool value) noexcept;
    Base::Result<void> SetCanVerticallyScroll(
        bool value) noexcept;
    Base::Result<void> SetCanContentScroll(
        bool value) noexcept;
    Base::Result<void>
    SetHorizontalScrollBarVisibility(
        ScrollBarVisibility value) noexcept;
    Base::Result<void>
    SetVerticalScrollBarVisibility(
        ScrollBarVisibility value) noexcept;
    PanningMode GetPanningMode() const noexcept;
    Base::Result<void> SetPanningMode(
        PanningMode value) noexcept;
    Base::Result<bool> SetHorizontalOffset(
        double value) noexcept override;
    Base::Result<bool> SetVerticalOffset(
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
    static Base::Result<void>
    SetHorizontalScrollBarVisibility(
        DependencyObject& element,
        ScrollBarVisibility value) noexcept;
    static Base::Result<void>
    SetVerticalScrollBarVisibility(
        DependencyObject& element,
        ScrollBarVisibility value) noexcept;

    inline static constexpr Members::ReadOnlyProperty<double>
        HorizontalOffsetProperty{"HorizontalOffset"};
    inline static constexpr Members::ReadOnlyProperty<double>
        VerticalOffsetProperty{"VerticalOffset"};
    inline static constexpr Members::ReadOnlyProperty<double>
        ExtentWidthProperty{"ExtentWidth"};
    inline static constexpr Members::ReadOnlyProperty<double>
        ExtentHeightProperty{"ExtentHeight"};
    inline static constexpr Members::ReadOnlyProperty<double>
        ViewportWidthProperty{"ViewportWidth"};
    inline static constexpr Members::ReadOnlyProperty<double>
        ViewportHeightProperty{"ViewportHeight"};
    inline static constexpr Members::ReadOnlyProperty<double>
        ScrollableWidthProperty{"ScrollableWidth"};
    inline static constexpr Members::ReadOnlyProperty<double>
        ScrollableHeightProperty{"ScrollableHeight"};
    inline static constexpr Members::ReadOnlyProperty<
        Visibility>
        ComputedHorizontalScrollBarVisibilityProperty{
            "ComputedHorizontalScrollBarVisibility"};
    inline static constexpr Members::ReadOnlyProperty<
        Visibility>
        ComputedVerticalScrollBarVisibilityProperty{
            "ComputedVerticalScrollBarVisibility"};
    inline static constexpr Members::AttachedProperty<
        ScrollBarVisibility>
        HorizontalScrollBarVisibilityProperty{
            "HorizontalScrollBarVisibility"};
    inline static constexpr Members::AttachedProperty<
        ScrollBarVisibility>
        VerticalScrollBarVisibilityProperty{
            "VerticalScrollBarVisibility"};
    inline static constexpr Members::Property<bool>
        CanHorizontallyScrollProperty{"CanHorizontallyScroll"};
    inline static constexpr Members::Property<bool>
        CanVerticallyScrollProperty{"CanVerticallyScroll"};
    inline static constexpr Members::AttachedProperty<bool>
        CanContentScrollProperty{"CanContentScroll"};
    inline static constexpr Members::AttachedProperty<PanningMode>
        PanningModeProperty{"PanningMode"};

protected:
    Base::Result<void> OnApplyTemplate() noexcept override;
    Base::Result<Size> MeasureOverride(
        Size availableSize) noexcept override;
    void OnScrollDataChanged(
        const ScrollData& oldData,
        const ScrollData& newData,
        ScrollInputKind kind) noexcept override;
    bool AllowsHorizontalScroll() const noexcept override;
    bool AllowsVerticalScroll() const noexcept override;
    bool UsesContentScrolling() const noexcept override;
    void OnTemplateDetached() noexcept override;

private:
    friend class ScrollContentPresenter;
    friend class Aero::Detail::ControlRuntimeAccess;
    RoutedEventManager* events_ = nullptr;
    ScrollInteractionManager* interactions_ = nullptr;
    ScrollContentPresenter* contentPresenter_ = nullptr;
    void AdoptPresenterData(
        ScrollContentPresenter& presenter,
        const ScrollData& data,
        ScrollInputKind kind) noexcept;
    void UpdateComputedScrollBarVisibility(
        const ScrollData& data) noexcept;
};

struct ThumbDragDelta final {
    double horizontalChange = 0.0;
    double verticalChange = 0.0;
};

class AERO_API Thumb final : public Control {
    AERO_DECLARE_TYPE(Thumb, Control)
public:
    Thumb() noexcept : Control(StaticTypeId()) {}
    ~Thumb() override = default;

    bool IsDragging() const noexcept {
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

    inline static constexpr Members::ReadOnlyProperty<bool>
        IsDraggingProperty{"IsDragging"};

private:
    std::uint32_t pointerId_ = 0U;
    Point lastPosition_;
    bool dragging_ = false;
};

class AERO_API Track final : public Control {
    AERO_DECLARE_TYPE(Track, Control)
public:
    Track() noexcept : Control(StaticTypeId()) {}
    ~Track() override = default;

    Orientation GetOrientation() const noexcept;
    double Minimum() const noexcept;
    double Maximum() const noexcept;
    double Value() const noexcept;
    double ViewportSize() const noexcept;
    bool IsDirectionReversed() const noexcept;
    Base::Ref<RepeatButton>
    DecreaseRepeatButton() const noexcept {
        return decreaseRepeatButton_;
    }
    Base::Ref<Thumb> ThumbElement() const noexcept {
        return thumb_;
    }
    Base::Ref<RepeatButton>
    IncreaseRepeatButton() const noexcept {
        return increaseRepeatButton_;
    }
    Base::Result<void> SetOrientation(
        Orientation value) noexcept;
    Base::Result<void> SetRange(
        double minimum,
        double maximum) noexcept;
    Base::Result<void> SetValue(
        double value) noexcept;
    Base::Result<void> SetViewportSize(
        double value) noexcept;
    Base::Result<void> SetIsDirectionReversed(
        bool value) noexcept;
    Base::Result<void> SetDecreaseRepeatButton(
        Base::Ref<RepeatButton> value) noexcept;
    Base::Result<void> SetThumb(
        Base::Ref<Thumb> value) noexcept;
    Base::Result<void> SetIncreaseRepeatButton(
        Base::Ref<RepeatButton> value) noexcept;
    double ThumbLength(
        double trackLength,
        double minimumThumbLength = 8.0) const noexcept;
    double ThumbOffset(
        double trackLength,
        double minimumThumbLength = 8.0) const noexcept;
    Base::Result<double> ValueFromThumbOffset(
        double offset,
        double trackLength,
        double minimumThumbLength = 8.0) const noexcept;

    inline static constexpr Members::Property<Orientation>
        OrientationProperty{"Orientation"};
    inline static constexpr Members::Property<double>
        MinimumProperty{"Minimum"};
    inline static constexpr Members::Property<double>
        MaximumProperty{"Maximum"};
    inline static constexpr Members::Property<double>
        ValueProperty{"Value"};
    inline static constexpr Members::Property<double>
        ViewportSizeProperty{"ViewportSize"};
    inline static constexpr Members::Property<bool>
        IsDirectionReversedProperty{
            "IsDirectionReversed"};

protected:
    Base::Result<Size> MeasureOverride(
        Size availableSize) noexcept override;
    Base::Result<Size> ArrangeOverride(
        Size finalSize) noexcept override;

private:
    Base::Ref<RepeatButton> decreaseRepeatButton_;
    Base::Ref<Thumb> thumb_;
    Base::Ref<RepeatButton> increaseRepeatButton_;
};

// WPF-compatible GridSplitter surface. The splitter carries the full
// resize-policy state even when the hosting grid chooses to apply the delta
// through a custom interaction adapter.
class AERO_API GridSplitter final : public Control {
    AERO_DECLARE_TYPE(GridSplitter, Control)
public:
    GridSplitter() noexcept : Control(StaticTypeId()) {}
    ~GridSplitter() override = default;

    double DragIncrement() const noexcept;
    double KeyboardIncrement() const noexcept;
    GridResizeDirection ResizeDirection() const noexcept;
    GridResizeBehavior ResizeBehavior() const noexcept;
    bool ShowsPreview() const noexcept;
    Base::Ref<Presentation::Style> PreviewStyle() const noexcept;
    Base::Result<void> SetDragIncrement(double value) noexcept;
    Base::Result<void> SetKeyboardIncrement(double value) noexcept;
    Base::Result<void> SetResizeDirection(
        GridResizeDirection value) noexcept;
    Base::Result<void> SetResizeBehavior(
        GridResizeBehavior value) noexcept;
    Base::Result<void> SetShowsPreview(bool value) noexcept;
    Base::Result<void> SetPreviewStyle(
        Base::Ref<Presentation::Style> value) noexcept;

    inline static constexpr Members::Property<double>
        DragIncrementProperty{"DragIncrement"};
    inline static constexpr Members::Property<double>
        KeyboardIncrementProperty{"KeyboardIncrement"};
    inline static constexpr Members::Property<GridResizeDirection>
        ResizeDirectionProperty{"ResizeDirection"};
    inline static constexpr Members::Property<GridResizeBehavior>
        ResizeBehaviorProperty{"ResizeBehavior"};
    inline static constexpr Members::Property<bool>
        ShowsPreviewProperty{"ShowsPreview"};
    inline static constexpr Members::Property<
        Base::Ref<Presentation::Style>>
        PreviewStyleProperty{"PreviewStyle"};
};

struct RangeValueChangedEventArgs final : RoutedEventArgs {
    AERO_DECLARE_TYPE(
        RangeValueChangedEventArgs,
        RoutedEventArgs)
    RangeValueChangedEventArgs() noexcept
        : RoutedEventArgs(StaticTypeId()) {}
    double oldValue = 0.0;
    double newValue = 0.0;
};

using RangeValueChangedEventHandler =
    Base::Delegate<void(
        Base::Object*,
        const RangeValueChangedEventArgs&)>;

class AERO_API RangeBase : public Control {
    AERO_DECLARE_TYPE(RangeBase, Control)
public:
    double Minimum() const noexcept;
    double Maximum() const noexcept;
    double Value() const noexcept;
    Base::Result<void> SetMinimum(double value) noexcept;
    Base::Result<void> SetMaximum(double value) noexcept;
    Base::Result<void> SetRange(
        double minimum,
        double maximum) noexcept;
    Base::Result<bool> SetValue(double value) noexcept;

    inline static constexpr Members::RoutedEvent<
        RangeValueChangedEventArgs>
        ValueChangedEvent{"ValueChanged"};
    UIElement::RoutedEvent_<
        RangeValueChangedEventHandler>
        ValueChanged() noexcept {
        return Event(ValueChangedEvent);
    }
    inline static constexpr Members::Property<double>
        MinimumProperty{"Minimum"};
    inline static constexpr Members::Property<double>
        MaximumProperty{"Maximum"};
    inline static constexpr Members::Property<double>
        ValueProperty{"Value"};

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

class AERO_API ScrollBar final : public RangeBase {
    AERO_DECLARE_TYPE(ScrollBar, RangeBase)
public:
    ScrollBar() noexcept;
    ~ScrollBar() override;

    Orientation GetOrientation() const noexcept;
    double ViewportSize() const noexcept;
    double SmallChange() const noexcept;
    double LargeChange() const noexcept;
    Base::Result<void> SetOrientation(
        Orientation value) noexcept;
    Base::Result<void> SetViewportSize(
        double value) noexcept;
    Base::Result<void> SetSmallChange(
        double value) noexcept;
    Base::Result<void> SetLargeChange(
        double value) noexcept;
    Base::Result<bool> LineDecrement() noexcept;
    Base::Result<bool> LineIncrement() noexcept;
    Base::Result<bool> PageDecrement() noexcept;
    Base::Result<bool> PageIncrement() noexcept;
    Base::Result<bool> DragThumb(
        double thumbOffset,
        double trackLength,
        double minimumThumbLength = 8.0) noexcept;

    inline static constexpr Members::Property<Orientation>
        OrientationProperty{"Orientation"};
    inline static constexpr Members::Property<double>
        ViewportSizeProperty{"ViewportSize"};
    inline static constexpr Members::Property<double>
        SmallChangeProperty{"SmallChange"};
    inline static constexpr Members::Property<double>
        LargeChangeProperty{"LargeChange"};

protected:
    Base::Result<void> OnApplyTemplate() noexcept override;
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

class AERO_API Slider final : public RangeBase {
    AERO_DECLARE_TYPE(Slider, RangeBase)
public:
    Slider() noexcept : RangeBase(StaticTypeId()) {}
    ~Slider() override = default;

    Orientation GetOrientation() const noexcept;
    double SmallChange() const noexcept;
    double LargeChange() const noexcept;
    TickPlacement GetTickPlacement() const noexcept;
    double TickFrequency() const noexcept;
    Base::StringView Ticks() const noexcept;
    bool IsSnapToTickEnabled() const noexcept;
    bool IsDirectionReversed() const noexcept;
    bool IsMoveToPointEnabled() const noexcept;
    Base::Result<void> SetOrientation(
        Orientation value) noexcept;
    Base::Result<void> SetSmallChange(
        double value) noexcept;
    Base::Result<void> SetLargeChange(
        double value) noexcept;
    Base::Result<void> SetTickPlacement(
        TickPlacement value) noexcept;
    Base::Result<void> SetTickFrequency(
        double value) noexcept;
    Base::Result<void> SetTicks(
        Base::StringView value) noexcept;
    Base::Result<void> SetIsSnapToTickEnabled(
        bool value) noexcept;
    Base::Result<void> SetIsDirectionReversed(
        bool value) noexcept;
    Base::Result<void> SetIsMoveToPointEnabled(
        bool value) noexcept;
    Base::Result<bool> DecreaseSmall() noexcept;
    Base::Result<bool> IncreaseSmall() noexcept;
    Base::Result<bool> DecreaseLarge() noexcept;
    Base::Result<bool> IncreaseLarge() noexcept;
    Base::Result<bool> SetValueFromPosition(
        double position,
        double trackLength) noexcept;

    inline static constexpr Members::Property<Orientation>
        OrientationProperty{"Orientation"};
    inline static constexpr Members::Property<double>
        SmallChangeProperty{"SmallChange"};
    inline static constexpr Members::Property<double>
        LargeChangeProperty{"LargeChange"};
    inline static constexpr Members::Property<TickPlacement>
        TickPlacementProperty{"TickPlacement"};
    inline static constexpr Members::Property<double>
        TickFrequencyProperty{"TickFrequency"};
    inline static constexpr Members::Property<Base::String>
        TicksProperty{"Ticks"};
    inline static constexpr Members::Property<bool>
        IsSnapToTickEnabledProperty{"IsSnapToTickEnabled"};
    inline static constexpr Members::Property<bool>
        IsDirectionReversedProperty{"IsDirectionReversed"};
    inline static constexpr Members::Property<bool>
        IsMoveToPointEnabledProperty{"IsMoveToPointEnabled"};

protected:
    Base::Result<Size> ArrangeOverride(
        Size finalSize) noexcept override;
    Base::Result<void> BuildDisplayList(
        DisplayListBuilder& builder) noexcept override;

private:
    double NormalizedValueForLayout() const noexcept;
    double SnapValue(double value) const noexcept;
};

class AERO_API TickBar final : public Control {
    AERO_DECLARE_TYPE(TickBar, Control)
public:
    TickBar() noexcept : Control(StaticTypeId()) {}
    ~TickBar() override = default;

    Base::Ref<Presentation::Brush> Fill() const noexcept;
    TickBarPlacement Placement() const noexcept;
    Base::Result<void> SetFill(
        Base::Ref<Presentation::Brush> value) noexcept;
    Base::Result<void> SetPlacement(
        TickBarPlacement value) noexcept;

    inline static constexpr Members::Property<
        Base::Ref<Presentation::Brush>>
        FillProperty{"Fill"};
    inline static constexpr Members::Property<TickBarPlacement>
        PlacementProperty{"Placement"};

protected:
    Base::Result<void> BuildDisplayList(
        Presentation::DisplayListBuilder& builder) noexcept override;
};

class AERO_API ProgressBar final : public RangeBase {
    AERO_DECLARE_TYPE(ProgressBar, RangeBase)
public:
    ProgressBar() noexcept : RangeBase(StaticTypeId()) {}
    ~ProgressBar() override = default;

    bool IsIndeterminate() const noexcept;
    Orientation GetOrientation() const noexcept;
    Base::Result<void> SetIsIndeterminate(
        bool value) noexcept;
    Base::Result<void> SetOrientation(
        Orientation value) noexcept;
    double NormalizedValue() const noexcept;

    inline static constexpr Members::Property<bool>
        IsIndeterminateProperty{"IsIndeterminate"};
    inline static constexpr Members::Property<Orientation>
        OrientationProperty{"Orientation"};
};



} // namespace Aero::Controls

namespace Aero::Core {

template<>
struct MetaTypeTraits<Controls::TickPlacement> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("TickPlacement");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "TickPlacement";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

template<>
struct MetaTypeTraits<Controls::TickBarPlacement> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("TickBarPlacement");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "TickBarPlacement";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

template<>
struct MetaTypeTraits<Controls::ScrollBarVisibility> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("ScrollBarVisibility");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "ScrollBarVisibility";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

template<>
struct MetaTypeTraits<Controls::PanningMode> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("PanningMode");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "PanningMode";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

template<>
struct MetaTypeTraits<Controls::GridResizeDirection> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("GridResizeDirection");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "GridResizeDirection";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

template<>
struct MetaTypeTraits<Controls::GridResizeBehavior> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("GridResizeBehavior");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "GridResizeBehavior";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

} // namespace Aero::Core
