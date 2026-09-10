#pragma once

#include <Aero/Controls/ContentControl.hpp>
#include <Aero/Input.hpp>
#include <Aero/ICommand.hpp>
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

    virtual void OnMouseLeftButtonDown(MouseButtonEventArgs& args);
    virtual void OnMouseLeftButtonUp(MouseButtonEventArgs& args);
    virtual void OnKeyDown(KeyEventArgs& args);
    virtual void OnKeyUp(KeyEventArgs& args);

    void OnApplyTemplate() noexcept override;

private:
    struct State;
    State* state_ = nullptr;
};

} // namespace Primitives
} // namespace Aero::Controls
AERO_DECLARE_TYPE_ENUM(Aero::Controls::ClickMode)
