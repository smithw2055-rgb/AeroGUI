#pragma once

#include <Aero/Controls/ContentControl.hpp>
#include <Aero/Input.hpp>
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

    inline static constexpr RoutedEvent<RoutedEventArgs> ClickEvent{"Click"};
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

    inline static constexpr DependencyProperty<ClickMode> ClickModeProperty{"ClickMode"};
    inline static constexpr DependencyProperty<Base::Ref<ICommand>> CommandProperty{"Command"};
    inline static constexpr DependencyProperty<Value> CommandParameterProperty{"CommandParameter"};
    inline static constexpr DependencyProperty<Base::Ref<UIElement>> CommandTargetProperty{"CommandTarget"};

protected:
    explicit ButtonBase(TypeId runtimeType) noexcept
        : ContentControl(runtimeType) {}
    ~ButtonBase() override;
    void OnApplyTemplate() noexcept override;

private:
    friend struct Impl;
    bool commandEnabled_ = true;
};

class AERO_API RepeatButton : public ButtonBase {
    AERO_DECLARE_TYPE(RepeatButton, ButtonBase)
public:
    RepeatButton() noexcept : RepeatButton(StaticTypeId()) {}
    ~RepeatButton() override = default;

    std::uint32_t GetDelay() const noexcept;
    std::uint32_t GetInterval() const noexcept;
    void SetDelay(std::uint32_t value) noexcept;
    void SetInterval(std::uint32_t value) noexcept;

    inline static constexpr DependencyProperty<std::uint32_t> DelayProperty{"Delay"};
    inline static constexpr DependencyProperty<std::uint32_t> IntervalProperty{"Interval"};

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
    void SetIsIndeterminate() noexcept;
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

    inline static constexpr DependencyProperty<bool> IsCheckedProperty{"IsChecked"};
    inline static constexpr DependencyProperty<bool> IsThreeStateProperty{"IsThreeState"};
    inline static constexpr ReadOnlyDependencyProperty<bool> IsIndeterminateProperty{"IsIndeterminate"};

protected:
    explicit ToggleButton(TypeId runtimeType) noexcept
        : ButtonBase(runtimeType) {}

private:
    friend struct ::Aero::Controls::Primitives::ButtonBase::Impl;
    void SetToggleState(
        std::uint8_t value) noexcept;
};
} // namespace Primitives
} // namespace Aero::Controls
AERO_DECLARE_TYPE_ENUM(Aero::Controls::ClickMode)
