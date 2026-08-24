#pragma once

#include <Aero/Controls/ButtonBase.hpp>

namespace Aero::Controls::Primitives {

class AERO_GUI_API ToggleButton : public ButtonBase {
    AERO_DECLARE_TYPE(ToggleButton, ButtonBase)
public:
    ToggleButton() noexcept : ToggleButton(StaticTypeId()) {}
    ~ToggleButton() override = default;

    Nullable<bool> GetIsChecked() const noexcept;
    bool GetIsThreeState() const noexcept;
    void SetIsChecked(Nullable<bool> value) noexcept;
    void SetIsThreeState(bool value) noexcept;

    inline static constexpr RoutedEvent<RoutedEventArgs> CheckedEvent{"Checked"};
    inline static constexpr RoutedEvent<RoutedEventArgs> UncheckedEvent{"Unchecked"};
    inline static constexpr RoutedEvent<RoutedEventArgs> IndeterminateEvent{"Indeterminate"};
    UIElement::Event<RoutedEventArgs> Checked() noexcept {
        return GetEvent(CheckedEvent);
    }
    UIElement::Event<RoutedEventArgs> Unchecked() noexcept {
        return GetEvent(UncheckedEvent);
    }
    UIElement::Event<RoutedEventArgs> Indeterminate() noexcept {
        return GetEvent(IndeterminateEvent);
    }

    inline static constexpr DependencyProperty<Nullable<bool>> IsCheckedProperty{"IsChecked"};
    inline static constexpr DependencyProperty<bool> IsThreeStateProperty{"IsThreeState"};

protected:
    explicit ToggleButton(TypeId runtimeType) noexcept
        : ButtonBase(runtimeType) {}

private:
    friend class ::Aero::Controls::ButtonBehavior;
    void SetToggleState(std::uint8_t value) noexcept;
};

} // namespace Aero::Controls::Primitives
