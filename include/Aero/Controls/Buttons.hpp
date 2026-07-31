#pragma once

#include <Aero/Detail/RuntimeManagersFwd.hpp>

#include <Aero/Controls/Templates.hpp>
#include <Aero/Input/Commands.hpp>
#include <Aero/Input/Navigation.hpp>

namespace Aero::Controls {

enum class ClickMode : std::uint8_t {
    Release = 0U,
    Press,
    Hover,
};

enum class ToggleState : std::uint8_t {
    Unchecked = 0U,
    Checked,
    Indeterminate,
};


class AERO_API ButtonBase : public ContentControl {
    AERO_DECLARE_TYPE(ButtonBase, ContentControl)
public:
    inline static constexpr Members::RoutedEvent<
        RoutedEventArgs> ClickEvent{"Click"};
    UIElement::RoutedEvent_<RoutedEventHandler> Click() noexcept {
        return Event(ClickEvent);
    }

    ClickMode GetClickMode() const noexcept;
    ICommand* Command() const noexcept;
    Base::Ref<Base::Object> CommandParameter() const noexcept;
    UIElement* CommandTarget() const noexcept;
    bool IsCommandEnabled() const noexcept {
        return commandEnabled_;
    }

    Base::Result<void> SetClickMode(ClickMode value) noexcept;
    Base::Result<void> SetCommand(
        Base::Ref<ICommand> command) noexcept;
    Base::Result<void> SetCommandParameter(
        Base::Ref<Base::Object> parameter) noexcept;
    Base::Result<void> SetCommandTarget(
        Base::Ref<UIElement> target) noexcept;

    inline static constexpr Members::Property<ClickMode>
        ClickModeProperty{"ClickMode"};
    inline static constexpr Members::Property<Base::Ref<ICommand>>
        CommandProperty{"Command"};
    inline static constexpr Members::Property<
        Base::Ref<Base::Object>>
        CommandParameterProperty{"CommandParameter"};
    inline static constexpr Members::Property<Base::Ref<UIElement>>
        CommandTargetProperty{"CommandTarget"};

protected:
    explicit ButtonBase(TypeId runtimeType) noexcept
        : ContentControl(runtimeType) {}
    ~ButtonBase() override = default;
    Base::Result<void> OnApplyTemplate() noexcept override;

private:
    friend class Aero::Detail::ControlRuntimeAccess;
    void* interactionManager_ = nullptr;
    bool commandEnabled_ = true;
};

class AERO_API Button : public ButtonBase {
    AERO_DECLARE_TYPE(Button, ButtonBase)
public:
    Button() noexcept : Button(StaticTypeId()) {}
    ~Button() override = default;

protected:
    explicit Button(TypeId runtimeType) noexcept
        : ButtonBase(runtimeType) {}
};

class AERO_API RepeatButton : public ButtonBase {
    AERO_DECLARE_TYPE(RepeatButton, ButtonBase)
public:
    RepeatButton() noexcept : RepeatButton(StaticTypeId()) {}
    ~RepeatButton() override = default;

    std::uint32_t Delay() const noexcept;
    std::uint32_t Interval() const noexcept;
    Base::Result<void> SetDelay(std::uint32_t value) noexcept;
    Base::Result<void> SetInterval(std::uint32_t value) noexcept;

    inline static constexpr Members::Property<std::uint32_t>
        DelayProperty{"Delay"};
    inline static constexpr Members::Property<std::uint32_t>
        IntervalProperty{"Interval"};

protected:
    explicit RepeatButton(TypeId runtimeType) noexcept
        : ButtonBase(runtimeType) {}
};

class AERO_API ToggleButton : public ButtonBase {
    AERO_DECLARE_TYPE(ToggleButton, ButtonBase)
public:
    ToggleButton() noexcept : ToggleButton(StaticTypeId()) {}
    ~ToggleButton() override = default;

    bool IsChecked() const noexcept;
    bool IsThreeState() const noexcept;
    bool IsIndeterminate() const noexcept;
    ToggleState GetToggleState() const noexcept;
    Base::Result<void> SetIsChecked(bool value) noexcept;
    Base::Result<void> SetIsThreeState(bool value) noexcept;

    inline static constexpr Members::RoutedEvent<
        RoutedEventArgs> CheckedEvent{"Checked"};
    inline static constexpr Members::RoutedEvent<
        RoutedEventArgs> UncheckedEvent{"Unchecked"};
    inline static constexpr Members::RoutedEvent<
        RoutedEventArgs> IndeterminateEvent{"Indeterminate"};
    UIElement::RoutedEvent_<RoutedEventHandler> Checked() noexcept {
        return Event(CheckedEvent);
    }
    UIElement::RoutedEvent_<RoutedEventHandler> Unchecked() noexcept {
        return Event(UncheckedEvent);
    }
    UIElement::RoutedEvent_<RoutedEventHandler> Indeterminate() noexcept {
        return Event(IndeterminateEvent);
    }

    inline static constexpr Members::Property<bool>
        IsCheckedProperty{"IsChecked"};
    inline static constexpr Members::Property<bool>
        IsThreeStateProperty{"IsThreeState"};
    inline static constexpr Members::ReadOnlyProperty<bool>
        IsIndeterminateProperty{"IsIndeterminate"};

protected:
    explicit ToggleButton(TypeId runtimeType) noexcept
        : ButtonBase(runtimeType) {}

private:
    friend class Aero::Detail::ControlRuntimeAccess;
    Base::Result<void> SetToggleState(
        ToggleState value) noexcept;
};

class AERO_API CheckBox : public ToggleButton {
    AERO_DECLARE_TYPE(CheckBox, ToggleButton)
public:
    CheckBox() noexcept : CheckBox(StaticTypeId()) {}
    ~CheckBox() override = default;

protected:
    explicit CheckBox(TypeId runtimeType) noexcept
        : ToggleButton(runtimeType) {}
};

class AERO_API RadioButton : public ToggleButton {
    AERO_DECLARE_TYPE(RadioButton, ToggleButton)
public:
    RadioButton() noexcept : RadioButton(StaticTypeId()) {}
    ~RadioButton() override = default;

    Base::StringView GroupName() const noexcept;
    Base::Result<void> SetGroupName(
        Base::StringView value) noexcept;

    inline static constexpr Members::Property<Base::String>
        GroupNameProperty{"GroupName"};

protected:
    explicit RadioButton(TypeId runtimeType) noexcept
        : ToggleButton(runtimeType) {}
};


} // namespace Aero::Controls

namespace Aero::Core {

template<>
struct MetaTypeTraits<Controls::ClickMode> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("ClickMode");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "ClickMode";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

} // namespace Aero::Core
