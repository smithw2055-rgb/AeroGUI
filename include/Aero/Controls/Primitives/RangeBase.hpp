#pragma once

#include <Aero/Controls/Control.hpp>
#include <Aero/Events/ControlEventArgs.hpp>

namespace Aero::Controls::Primitives {

class AERO_GUI_API RangeBase : public Control {
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

} // namespace Aero::Controls::Primitives
