#pragma once

#include <Aero/Controls/Primitives/ButtonBase.hpp>

namespace Aero::Controls::Primitives {

class AERO_GUI_API ToggleButton : public ButtonBase {
    AERO_DECLARE_TYPE(ToggleButton, ButtonBase)
public:
    ToggleButton() noexcept : ToggleButton(StaticTypeId()) {}
    ~ToggleButton() override;

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

    AERO_DEPENDENCY_PROPERTY(Nullable<bool>, IsChecked);
    AERO_DEPENDENCY_PROPERTY(bool, IsThreeState);

protected:
    explicit ToggleButton(TypeId runtimeType) noexcept;

    void OnClick() override;
    virtual void OnToggle() noexcept;
    virtual void OnChecked(RoutedEventArgs& e);
    virtual void OnUnchecked(RoutedEventArgs& e);
    virtual void OnIndeterminate(RoutedEventArgs& e);
    void UpdateVisualState(bool useTransitions = true) noexcept override;
    void OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept override;
};

} // namespace Aero::Controls::Primitives
