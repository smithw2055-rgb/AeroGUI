#pragma once

// Range controls and the track parts they share. Thumb stays separate so
// GridSplitter does not pull Slider or ScrollBar.
#include <Aero/Base/Vector.hpp>
#include <Aero/CommandBinding.hpp>
#include <Aero/Controls/Buttons.hpp>
#include <Aero/Controls/Control.hpp>
#include <Aero/Controls/Panel.hpp>
#include <Aero/Controls/Primitives/Thumb.hpp>
#include <Aero/Events/CommandEventArgs.hpp>
#include <Aero/Events/ControlEventArgs.hpp>
#include <Aero/Events/EventArgs.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/DrawingContext.hpp>

namespace Aero::Controls::Primitives {

using ::Aero::Meta::DependencyPropertyChangedEventArgs;
using ::Aero::Meta::DependencyPropertyHandle;
using ::Aero::Meta::PropertyValue;
using ::Aero::Meta::TypeId;

class AERO_GUI_API RangeBase : public Control {
    AERO_DECLARE_TYPE(RangeBase, Control)
public:
    using Control::GetValue;
    using Control::SetValue;

    double GetMinimum() const noexcept;
    double GetMaximum() const noexcept;
    double GetValue() const noexcept;
    void SetMinimum(double value) noexcept;
    void SetMaximum(double value) noexcept;
    void SetRange(double minimum, double maximum) noexcept;
    void SetValue(double value) noexcept;

    inline static constexpr RoutedEvent<RangeValueChangedEventArgs> ValueChangedEvent{"ValueChanged"};
    UIElement::Event<RangeValueChangedEventArgs> ValueChanged() noexcept { return GetEvent(ValueChangedEvent); }
    AERO_DEPENDENCY_PROPERTY(double, Minimum);
    AERO_DEPENDENCY_PROPERTY(double, Maximum);
    AERO_DEPENDENCY_PROPERTY(double, Value);

protected:
    explicit RangeBase(TypeId runtimeType) noexcept;
    ~RangeBase() override;
    virtual void OnMinimumChanged(double oldMinimum, double newMinimum) noexcept;
    virtual void OnMaximumChanged(double oldMaximum, double newMaximum) noexcept;
    virtual void OnValueChanged(double oldValue, double newValue) noexcept;
    void OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept override;
    // Replaces the former CoerceRangeMinimum/Maximum/Value metadata delegates.
    PropertyValue CoerceValueCore(DependencyPropertyHandle property, const PropertyValue& baseValue) noexcept override;
};

} // namespace Aero::Controls::Primitives

namespace Aero::Controls::Primitives {

class AERO_GUI_API Track : public Control {
    AERO_DECLARE_TYPE(Track, Control)
public:
    Track() noexcept : Control(StaticTypeId()) {}
    ~Track() override = default;
    using Control::GetValue;
    using Control::SetValue;

    Orientation GetOrientation() const noexcept;
    double GetMinimum() const noexcept;
    double GetMaximum() const noexcept;
    double GetValue() const noexcept;
    double GetViewportSize() const noexcept;
    bool GetIsDirectionReversed() const noexcept;
    Ref<RepeatButton> GetDecreaseRepeatButton() const noexcept { return decreaseRepeatButton_; }
    Ref<Thumb> GetThumbElement() const noexcept { return thumb_; }
    Ref<RepeatButton> GetIncreaseRepeatButton() const noexcept { return increaseRepeatButton_; }
    void SetOrientation(Orientation value) noexcept;
    void SetRange(double minimum, double maximum) noexcept;
    void SetValue(double value) noexcept;
    void SetViewportSize(double value) noexcept;
    void SetIsDirectionReversed(bool value) noexcept;
    void SetDecreaseRepeatButton(Ref<RepeatButton> value) noexcept;
    void SetThumb(Ref<Thumb> value) noexcept;
    void SetIncreaseRepeatButton(Ref<RepeatButton> value) noexcept;
    double GetThumbLength(double trackLength, double minimumThumbLength = 8.0) const noexcept;
    double GetThumbOffset(double trackLength, double minimumThumbLength = 8.0) const noexcept;
    Result<double> ValueFromThumbOffset(double offset, double trackLength,
        double minimumThumbLength = 8.0) const noexcept;

    // Control only exposes TemplateRoot. Track's Thumb / RepeatButtons are
    // assigned as structural properties; the first attached child would
    // otherwise steal TemplateRoot and hide the gold indicator.
    std::uint32_t GetVisualChildrenCount() const noexcept override {
        std::uint32_t count = 0U;
        if (decreaseRepeatButton_ && decreaseRepeatButton_->GetVisualParent() == this) { ++count; }
        if (thumb_ && thumb_->GetVisualParent() == this) { ++count; }
        if (increaseRepeatButton_ && increaseRepeatButton_->GetVisualParent() == this) { ++count; }
        return count;
    }
    ::Aero::Media::Visual* GetVisualChild(std::uint32_t index) const noexcept override {
        std::uint32_t current = 0U;
        const auto take = [&](::Aero::Media::Visual* child) noexcept -> ::Aero::Media::Visual* {
            if (child == nullptr || child->GetVisualParent() != this) { return nullptr; }
            if (current == index) return child;
            ++current;
            return nullptr;
        };
        if (::Aero::Media::Visual* found = take(decreaseRepeatButton_.Get())) { return found; }
        if (::Aero::Media::Visual* found = take(thumb_.Get())) { return found; }
        return take(increaseRepeatButton_.Get());
    }

    AERO_DEPENDENCY_PROPERTY(Orientation, Orientation);
    AERO_DEPENDENCY_PROPERTY(double, Minimum);
    AERO_DEPENDENCY_PROPERTY(double, Maximum);
    AERO_DEPENDENCY_PROPERTY(double, Value);
    AERO_DEPENDENCY_PROPERTY(double, ViewportSize);
    AERO_DEPENDENCY_PROPERTY(bool, IsDirectionReversed);

protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;

private:
    Ref<RepeatButton> decreaseRepeatButton_;
    Ref<Thumb> thumb_;
    Ref<RepeatButton> increaseRepeatButton_;
};

} // namespace Aero::Controls::Primitives

namespace Aero::Controls::Primitives {

class AERO_GUI_API ScrollBar : public RangeBase {
    AERO_DECLARE_TYPE(ScrollBar, RangeBase)
public:
    ScrollBar() noexcept;
    ~ScrollBar() override;

    Orientation GetOrientation() const noexcept;
    double GetViewportSize() const noexcept;
    double GetSmallChange() const noexcept;
    double GetLargeChange() const noexcept;
    void SetOrientation(Orientation value) noexcept;
    void SetViewportSize(double value) noexcept;
    void SetSmallChange(double value) noexcept;
    void SetLargeChange(double value) noexcept;
    Result<bool> LineDecrement() noexcept;
    Result<bool> LineIncrement() noexcept;
    Result<bool> PageDecrement() noexcept;
    Result<bool> PageIncrement() noexcept;
    Result<bool> DragThumb(double thumbOffset, double trackLength, double minimumThumbLength = 8.0) noexcept;

    AERO_DEPENDENCY_PROPERTY(Orientation, Orientation);
    AERO_DEPENDENCY_PROPERTY(double, ViewportSize);
    AERO_DEPENDENCY_PROPERTY(double, SmallChange);
    AERO_DEPENDENCY_PROPERTY(double, LargeChange);

protected:
    void OnApplyTemplate() noexcept override;
    void OnTemplateDetached() noexcept override;
    void OnVisualParentChanged(Visual* oldParent) noexcept override;

    void OnMouseLeftButtonDown(MouseButtonEventArgs& args);
    void OnMouseLeftButtonUp(MouseButtonEventArgs& args);
    void OnMouseMove(MouseEventArgs& args);
    void OnKeyDown(KeyEventArgs& args);
    void OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept override;

private:
    Track* track_ = nullptr;
    std::uint32_t pointerId_ = 0U;
    bool dragging_ = false;
    Point dragOrigin_{};
    double dragStartValue_ = 0.0;

    ExecutedRoutedEventHandler lineUpHandler_;
    ExecutedRoutedEventHandler lineDownHandler_;
    ExecutedRoutedEventHandler lineLeftHandler_;
    ExecutedRoutedEventHandler lineRightHandler_;
    ExecutedRoutedEventHandler pageUpHandler_;
    ExecutedRoutedEventHandler pageDownHandler_;
    ExecutedRoutedEventHandler pageLeftHandler_;
    ExecutedRoutedEventHandler pageRightHandler_;
    ExecutedRoutedEventHandler scrollToTopHandler_;
    ExecutedRoutedEventHandler scrollToBottomHandler_;
    ExecutedRoutedEventHandler scrollToLeftEndHandler_;
    ExecutedRoutedEventHandler scrollToRightEndHandler_;
    ExecutedRoutedEventHandler scrollToHorizontalOffsetHandler_;
    ExecutedRoutedEventHandler scrollToVerticalOffsetHandler_;
    Base::Vector<Input::CommandBindingHandle> commandHandles_;

    static void OnLineUpCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept;
    static void OnLineDownCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept;
    static void OnLineLeftCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept;
    static void OnLineRightCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept;
    static void OnPageUpCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept;
    static void OnPageDownCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept;
    static void OnPageLeftCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept;
    static void OnPageRightCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept;
    static void OnScrollToTopCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept;
    static void OnScrollToBottomCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept;
    static void OnScrollToLeftEndCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept;
    static void OnScrollToRightEndCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept;
    static void OnScrollToHorizontalOffsetCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept;
    static void OnScrollToVerticalOffsetCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept;

    void EnsureCommands() noexcept;
    void UnregisterCommands() noexcept;

    void SynchronizeTrack() noexcept;
};

} // namespace Aero::Controls::Primitives

namespace Aero::Controls {

enum class TickBarPlacement : std::uint8_t {
    Top = 0U,
    Bottom,
    Left,
    Right
};

class AERO_GUI_API TickBar : public Control {
    AERO_DECLARE_TYPE(TickBar, Control)
public:
    TickBar() noexcept : Control(StaticTypeId()) {}
    ~TickBar() override = default;

    Ref<Aero::Media::Brush> GetFill() const noexcept;
    TickBarPlacement GetPlacement() const noexcept;
    void SetFill(Ref<Aero::Media::Brush> value) noexcept;
    void SetPlacement(TickBarPlacement value) noexcept;

    AERO_DEPENDENCY_PROPERTY(Ref<Aero::Media::Brush>, Fill);
    AERO_DEPENDENCY_PROPERTY(TickBarPlacement, Placement);

protected:
    void OnRender(Aero::Media::DrawingContext& context) noexcept override;
};

} // namespace Aero::Controls
AERO_DECLARE_TYPE_ENUM(Aero::Controls::TickBarPlacement)

namespace Aero::Controls {
enum class TickPlacement : std::uint8_t {
    None = 0U,
    TopLeft,
    BottomRight,
    Both
};

class AERO_GUI_API Slider : public Primitives::RangeBase {
    AERO_DECLARE_TYPE(Slider, Primitives::RangeBase)
public:
    Slider() noexcept;
    ~Slider() override;

    Orientation GetOrientation() const noexcept;
    double GetSmallChange() const noexcept;
    double GetLargeChange() const noexcept;
    TickPlacement GetTickPlacement() const noexcept;
    double GetTickFrequency() const noexcept;
    StringView GetTicks() const noexcept;
    bool GetIsSnapToTickEnabled() const noexcept;
    bool GetIsDirectionReversed() const noexcept;
    bool GetIsMoveToPointEnabled() const noexcept;
    void SetOrientation(Orientation value) noexcept;
    void SetSmallChange(double value) noexcept;
    void SetLargeChange(double value) noexcept;
    void SetTickPlacement(TickPlacement value) noexcept;
    void SetTickFrequency(double value) noexcept;
    void SetTicks(StringView value) noexcept;
    void SetIsSnapToTickEnabled(bool value) noexcept;
    void SetIsDirectionReversed(bool value) noexcept;
    void SetIsMoveToPointEnabled(bool value) noexcept;
    Result<bool> DecreaseSmall() noexcept;
    Result<bool> IncreaseSmall() noexcept;
    Result<bool> DecreaseLarge() noexcept;
    Result<bool> IncreaseLarge() noexcept;
    void SetValueFromPosition(double position, double trackLength) noexcept;
    void SetValueFromTrackPoint(Point local) noexcept;

    AERO_DEPENDENCY_PROPERTY(Orientation, Orientation);
    AERO_DEPENDENCY_PROPERTY(double, SmallChange);
    AERO_DEPENDENCY_PROPERTY(double, LargeChange);
    AERO_DEPENDENCY_PROPERTY(TickPlacement, TickPlacement);
    AERO_DEPENDENCY_PROPERTY(double, TickFrequency);
    AERO_DEPENDENCY_PROPERTY(String, Ticks);
    AERO_DEPENDENCY_PROPERTY(bool, IsSnapToTickEnabled);
    AERO_DEPENDENCY_PROPERTY(bool, IsDirectionReversed);
    AERO_DEPENDENCY_PROPERTY(bool, IsMoveToPointEnabled);

protected:
    void OnApplyTemplate() noexcept override;
    void OnTemplateDetached() noexcept override;
    void OnVisualParentChanged(Visual* oldParent) noexcept override;
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;
    void OnRender(::Aero::Media::DrawingContext& context) noexcept override;

    void OnMouseLeftButtonDown(MouseButtonEventArgs& args);
    void OnMouseLeftButtonUp(MouseButtonEventArgs& args);
    void OnMouseMove(MouseEventArgs& args);
    void OnKeyDown(KeyEventArgs& args);
    bool ValidateValueCore(Meta::DependencyPropertyHandle property, const PropertyValue& value) const noexcept override;
    void OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept override;

private:
    Primitives::Track* track_ = nullptr;
    std::uint32_t pointerId_ = 0U;
    bool dragging_ = false;

    ExecutedRoutedEventHandler decreaseSmallHandler_;
    ExecutedRoutedEventHandler increaseSmallHandler_;
    ExecutedRoutedEventHandler decreaseLargeHandler_;
    ExecutedRoutedEventHandler increaseLargeHandler_;
    Input::CommandBindingHandle decreaseSmallCommand_;
    Input::CommandBindingHandle increaseSmallCommand_;
    Input::CommandBindingHandle decreaseLargeCommand_;
    Input::CommandBindingHandle increaseLargeCommand_;

    void OnDecreaseSmallCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept;
    void OnIncreaseSmallCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept;
    void OnDecreaseLargeCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept;
    void OnIncreaseLargeCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept;
    void SetFromPoint() noexcept;
    void EnsureCommands() noexcept;
    void UnregisterCommands() noexcept;

    void SynchronizeTrack() noexcept;
    double GetNormalizedValueForLayout() const noexcept;
    double GetSnapValue(double value) const noexcept;
};

} // namespace Aero::Controls
AERO_DECLARE_TYPE_ENUM(Aero::Controls::TickPlacement)

namespace Aero::Controls {

class AERO_GUI_API ProgressBar : public Primitives::RangeBase {
    AERO_DECLARE_TYPE(ProgressBar, Primitives::RangeBase)
public:
    ProgressBar() noexcept : Primitives::RangeBase(StaticTypeId()) {}
    ~ProgressBar() override = default;

    bool GetIsIndeterminate() const noexcept;
    Orientation GetOrientation() const noexcept;
    void SetIsIndeterminate(bool value) noexcept;
    void SetOrientation(Orientation value) noexcept;
    double GetNormalizedValue() const noexcept;

    AERO_DEPENDENCY_PROPERTY(bool, IsIndeterminate);
    AERO_DEPENDENCY_PROPERTY(Orientation, Orientation);
};

} // namespace Aero::Controls
