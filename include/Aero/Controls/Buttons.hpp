#pragma once

#include <Aero/Controls/Control.hpp>
#include <Aero/Input.hpp>
#include <Aero/ICommand.hpp>
#include <Aero/Events.hpp>

namespace Aero::Controls {

using ::Aero::Meta::DependencyPropertyChangedEventArgs;
using ::Aero::Meta::DependencyPropertyHandle;
using ::Aero::Meta::PropertyValue;
using ::Aero::Meta::TypeId;
using ::Aero::Input::ICommand;

enum class ClickMode : std::uint8_t {
    Release = 0U,
    Press,
    Hover,
};

namespace Primitives {

class AERO_GUI_API ButtonBase : public ContentControl {
    AERO_DECLARE_TYPE(ButtonBase, ContentControl)

public:
    ClickMode GetClickMode() const noexcept;
    void SetClickMode(ClickMode value) noexcept;
    ICommand* GetCommand() const noexcept;
    void SetCommand(Ref<ICommand> command) noexcept;
    Value GetCommandParameter() const noexcept;
    void SetCommandParameter(Value parameter) noexcept;
    UIElement* GetCommandTarget() const noexcept;
    void SetCommandTarget(Ref<UIElement> target) noexcept;
    bool GetIsCommandEnabled() const noexcept;

    inline static constexpr RoutedEvent<RoutedEventArgs> ClickEvent{"Click"};
    UIElement::Event<RoutedEventArgs> Click() noexcept { return GetEvent(ClickEvent); }

    AERO_DEPENDENCY_PROPERTY(ClickMode, ClickMode);
    AERO_DEPENDENCY_PROPERTY(Ref<ICommand>, Command);
    AERO_DEPENDENCY_PROPERTY(Value, CommandParameter);
    AERO_DEPENDENCY_PROPERTY(Ref<UIElement>, CommandTarget);

protected:
    explicit ButtonBase(TypeId runtimeType) noexcept;
    ~ButtonBase() override;

    virtual void OnClick();
    virtual void UpdateVisualState(bool useTransitions = true) noexcept;
    void OnMouseLeftButtonDown(MouseButtonEventArgs& args);
    void OnMouseLeftButtonUp(MouseButtonEventArgs& args);
    void OnKeyDown(KeyEventArgs& args);
    void OnKeyUp(KeyEventArgs& args);
    void OnGotKeyboardFocus(KeyboardFocusChangedEventArgs& args);
    void OnLostKeyboardFocus(KeyboardFocusChangedEventArgs& args);
    void OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept override;
    void OnApplyTemplate() noexcept override;
    PropertyValue CoerceValueCore(DependencyPropertyHandle property, const PropertyValue& baseValue) noexcept override;

private:
    void HookCommand(ICommand* command) noexcept;
    void UnhookCommand() noexcept;
    void RefreshCanExecute() noexcept;
    void OnCanExecuteChanged() noexcept;

    Base::Ref<ICommand> hookedCommand_;
    Base::Delegate<void()> canExecuteChangedHandler_;
    std::uint32_t pointerId_ = 0U;
    bool pointerDown_ = false;
    bool keyboardDown_ = false;
    bool wasMouseOver_ = false;
    bool commandEnabled_ = true;
};

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

    bool HitTestCore() const noexcept override { return true; }

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

} // namespace Primitives

class AERO_GUI_API Button : public Primitives::ButtonBase {
    AERO_DECLARE_TYPE(Button, Primitives::ButtonBase)

public:
    Button() noexcept : Button(StaticTypeId()) {}
    ~Button() override = default;

protected:
    explicit Button(TypeId runtimeType) noexcept : Primitives::ButtonBase(runtimeType) {}
};

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

AERO_DECLARE_TYPE_ENUM(Aero::Controls::ClickMode)
