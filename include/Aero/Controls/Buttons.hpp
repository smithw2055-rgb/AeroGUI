#pragma once

// Button leaves. Button and ButtonBase stay in their own headers.
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
    UIElement::Event<RoutedEventArgs> Checked() noexcept { return GetEvent(CheckedEvent); }
    UIElement::Event<RoutedEventArgs> Unchecked() noexcept { return GetEvent(UncheckedEvent); }
    UIElement::Event<RoutedEventArgs> Indeterminate() noexcept { return GetEvent(IndeterminateEvent); }

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

class AERO_GUI_API RepeatButton : public ButtonBase {
    AERO_DECLARE_TYPE(RepeatButton, ButtonBase)

public:
    RepeatButton() noexcept : RepeatButton(StaticTypeId()) {}
    ~RepeatButton() override;

    std::uint32_t GetDelay() const noexcept;
    std::uint32_t GetInterval() const noexcept;
    void SetDelay(std::uint32_t value) noexcept;
    void SetInterval(std::uint32_t value) noexcept;
    std::uint32_t AdvanceTime(
        std::uint32_t elapsedMilliseconds,
        std::uint64_t& repeatElapsed,
        std::uint64_t& nextRepeat) noexcept;

    AERO_DEPENDENCY_PROPERTY(std::uint32_t, Delay);
    AERO_DEPENDENCY_PROPERTY(std::uint32_t, Interval);

protected:
    explicit RepeatButton(TypeId runtimeType) noexcept;

    void OnMouseLeftButtonDown(MouseButtonEventArgs& args);
    void OnMouseLeftButtonUp(MouseButtonEventArgs& args);
    void OnKeyDown(KeyEventArgs& args);
    void OnKeyUp(KeyEventArgs& args);
};

} // namespace Aero::Controls::Primitives

namespace Aero::Controls {

class AERO_GUI_API CheckBox : public Primitives::ToggleButton {
    AERO_DECLARE_TYPE(CheckBox, Primitives::ToggleButton)

public:
    CheckBox() noexcept : CheckBox(StaticTypeId()) {}
    ~CheckBox() override = default;

protected:
    explicit CheckBox(TypeId runtimeType) noexcept : Primitives::ToggleButton(runtimeType) {}
};

class AERO_GUI_API RadioButton : public Primitives::ToggleButton {
    AERO_DECLARE_TYPE(RadioButton, Primitives::ToggleButton)

public:
    RadioButton() noexcept : RadioButton(StaticTypeId()) {}
    ~RadioButton() override;

    StringView GetGroupName() const noexcept;
    void SetGroupName(StringView value) noexcept;

    AERO_DEPENDENCY_PROPERTY(String, GroupName);

protected:
    explicit RadioButton(TypeId runtimeType) noexcept;

    void OnClick() override;
    void OnToggle() noexcept override;
    void OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept override;

private:
    void UncheckRadioPeers() noexcept;
};

} // namespace Aero::Controls
