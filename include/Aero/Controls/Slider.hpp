#pragma once

#include <Aero/Controls/Panel.hpp>
#include <Aero/Controls/Primitives/RangeBase.hpp>
#include <Aero/Media/DrawingContext.hpp>
#include <Aero/Events/EventArgs.hpp>
#include <Aero/Events/CommandEventArgs.hpp>
#include <Aero/CommandBinding.hpp>

namespace Aero::Controls {
namespace Primitives { class Track; }

enum class TickPlacement : std::uint8_t {
    None = 0U,
    TopLeft,
    BottomRight,
    Both
};

class AERO_GUI_API Slider : public Primitives::RangeBase {
    AERO_DECLARE_TYPE(Slider, Primitives::RangeBase)
public:
    static void RegisterMetadata(::Aero::Meta::Registration& context) noexcept;

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
    Size ArrangeOverride(
        Size finalSize) noexcept override;
    void OnRender(
        ::Aero::Media::DrawingContext& context) noexcept override;

    void OnMouseLeftButtonDown(MouseButtonEventArgs& args) override;
    void OnMouseLeftButtonUp(MouseButtonEventArgs& args) override;
    void OnMouseMove(MouseEventArgs& args) override;
    void OnKeyDown(KeyEventArgs& args) override;
    bool ValidateValueCore(
        Meta::DependencyPropertyHandle property,
        const PropertyValue& value) const noexcept override;
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
