#pragma once

#include <Aero/Controls/ContentControl.hpp>
#include <Aero/Input.hpp>
#include <Aero/ICommand.hpp>
#include <Aero/Events/ControlEventArgs.hpp>

namespace Aero::Controls {
class ButtonBehavior;
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
        Ref<ICommand> command) noexcept;
    void SetCommandParameter(Value parameter) noexcept;
    void SetCommandTarget(
        Ref<UIElement> target) noexcept;

    inline static constexpr DependencyProperty<ClickMode> ClickModeProperty{"ClickMode"};
    inline static constexpr DependencyProperty<Ref<ICommand>> CommandProperty{"Command"};
    inline static constexpr DependencyProperty<Value> CommandParameterProperty{"CommandParameter"};
    inline static constexpr DependencyProperty<Ref<UIElement>> CommandTargetProperty{"CommandTarget"};

protected:
    explicit ButtonBase(TypeId runtimeType) noexcept
        : ContentControl(runtimeType) {}
    ~ButtonBase() override;
    void OnApplyTemplate() noexcept override;

private:
    friend class ::Aero::Controls::ButtonBehavior;
    bool commandEnabled_ = true;
};

} // namespace Primitives
} // namespace Aero::Controls
AERO_DECLARE_TYPE_ENUM(Aero::Controls::ClickMode)
