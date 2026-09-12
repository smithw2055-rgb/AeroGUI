#pragma once

#include <Aero/Controls/Control.hpp>
#include <Aero/Events/ControlEventArgs.hpp>

namespace Aero::Controls::Primitives {

using ::Aero::Meta::DependencyPropertyChangedEventArgs;
using ::Aero::Meta::DependencyPropertyHandle;
using ::Aero::Meta::PropertyValue;
using ::Aero::Meta::TypeId;

class AERO_GUI_API RangeBase : public Control {
    AERO_DECLARE_TYPE(RangeBase, Control)
public:
    static void RegisterMetadata(::Aero::Meta::Registration& context) noexcept;

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
    UIElement::Event<RangeValueChangedEventArgs>
        ValueChanged() noexcept {
        return GetEvent(ValueChangedEvent);
    }
    AERO_DEPENDENCY_PROPERTY(double, Minimum);
    AERO_DEPENDENCY_PROPERTY(double, Maximum);
    AERO_DEPENDENCY_PROPERTY(double, Value);

protected:
    explicit RangeBase(TypeId runtimeType) noexcept;
    ~RangeBase() override;
    virtual void OnMinimumChanged(
        double oldMinimum,
        double newMinimum) noexcept;
    virtual void OnMaximumChanged(
        double oldMaximum,
        double newMaximum) noexcept;
    virtual void OnValueChanged(
        double oldValue,
        double newValue) noexcept;
    void OnPropertyChanged(
        const DependencyPropertyChangedEventArgs& args) noexcept override;
    // Replaces the former CoerceRangeMinimum/Maximum/Value metadata delegates.
    PropertyValue CoerceValueCore(
        DependencyPropertyHandle property,
        const PropertyValue& baseValue) noexcept override;
};

} // namespace Aero::Controls::Primitives
