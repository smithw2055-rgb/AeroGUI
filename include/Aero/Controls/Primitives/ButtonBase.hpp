#pragma once

#include <Aero/Controls/ContentControl.hpp>
#include <Aero/Input.hpp>
#include <Aero/ICommand.hpp>
#include <Aero/Events/ControlEventArgs.hpp>

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

    inline static constexpr RoutedEvent<RoutedEventArgs> ClickEvent{"Click"};
    UIElement::Event<RoutedEventArgs> Click() noexcept {
        return GetEvent(ClickEvent);
    }

    ClickMode GetClickMode() const noexcept;
    ICommand* GetCommand() const noexcept;
    Value GetCommandParameter() const noexcept;
    UIElement* GetCommandTarget() const noexcept;
    bool GetIsCommandEnabled() const noexcept;

    void SetClickMode(ClickMode value) noexcept;
    void SetCommand(Ref<ICommand> command) noexcept;
    void SetCommandParameter(Value parameter) noexcept;
    void SetCommandTarget(Ref<UIElement> target) noexcept;

    AERO_DEPENDENCY_PROPERTY(ClickMode, ClickMode);
    AERO_DEPENDENCY_PROPERTY(Ref<ICommand>, Command);
    AERO_DEPENDENCY_PROPERTY(Value, CommandParameter);
    AERO_DEPENDENCY_PROPERTY(Ref<UIElement>, CommandTarget);

protected:
    explicit ButtonBase(TypeId runtimeType) noexcept;
    ~ButtonBase() override;

    virtual void OnClick();
    virtual void UpdateVisualState(bool useTransitions = true) noexcept;

    void OnMouseLeftButtonDown(MouseButtonEventArgs& args) override;
    void OnMouseLeftButtonUp(MouseButtonEventArgs& args) override;
    void OnKeyDown(KeyEventArgs& args) override;
    void OnKeyUp(KeyEventArgs& args) override;
    void OnGotKeyboardFocus(KeyboardFocusChangedEventArgs& args) override;
    void OnLostKeyboardFocus(KeyboardFocusChangedEventArgs& args) override;

    void OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept override;
    void OnApplyTemplate() noexcept override;
    // Replaces the former CoerceButtonEnabled metadata delegate on IsEnabled.
    PropertyValue CoerceValueCore(
        DependencyPropertyHandle property,
        const PropertyValue& baseValue) noexcept override;

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

} // namespace Primitives
} // namespace Aero::Controls
AERO_DECLARE_TYPE_ENUM(Aero::Controls::ClickMode)
