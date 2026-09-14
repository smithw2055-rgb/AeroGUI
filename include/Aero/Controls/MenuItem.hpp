#pragma once

#include <Aero/ICommand.hpp>
#include <Aero/Controls/HeaderedItemsControl.hpp>
#include <Aero/Controls/Popup.hpp>
#include <Aero/Controls/TextBlock.hpp>

namespace Aero { class AeroGuiInternal; }
namespace Aero::Controls {
using ::Aero::Meta::DependencyPropertyChangedEventArgs;
using ::Aero::Meta::DependencyPropertyChangedEventHandler;
using ::Aero::Meta::TypeId;
using ::Aero::Input::ICommand;
enum class MenuItemRole : std::uint8_t {
    TopLevelItem = 0U,
    TopLevelHeader,
    SubmenuItem,
    SubmenuHeader
};

class AERO_GUI_API MenuItem
    : public HeaderedItemsControl {
    AERO_DECLARE_TYPE(MenuItem, HeaderedItemsControl)
public:
    static void RegisterMetadata(::Aero::Meta::Registration& context) noexcept;

    MenuItem() noexcept;
    ~MenuItem() override;

    StringView GetInputGestureText()
        const noexcept;
    void SetInputGestureText(StringView value) noexcept;
    bool GetIsCheckable() const noexcept;
    void SetIsCheckable(bool value) noexcept;
    bool GetIsChecked() const noexcept;
    void SetIsChecked(bool value) noexcept;
    bool GetIsHighlighted() const noexcept;
    bool GetIsSubmenuOpen() const noexcept;
    void SetIsSubmenuOpen(bool value) noexcept;
    MenuItemRole GetRole() const noexcept;
    ICommand* GetCommand() const noexcept;
    void SetCommand(Ref<ICommand> command) noexcept;
    Value GetCommandParameter() const noexcept;
    void SetCommandParameter(Value value) noexcept;
    Value GetIcon() const noexcept {
        return GetValue(IconProperty);
    }
    void SetIcon(Value value) noexcept {
        SetValue(IconProperty, std::move(value));
    }

    AERO_DEPENDENCY_PROPERTY(String, InputGestureText);
    AERO_DEPENDENCY_PROPERTY(bool, IsCheckable);
    AERO_DEPENDENCY_PROPERTY(bool, IsChecked);
    AERO_READONLY_PROPERTY(bool, IsHighlighted);
    AERO_DEPENDENCY_PROPERTY(bool, IsSubmenuOpen);
    AERO_READONLY_PROPERTY(MenuItemRole, Role);
    AERO_DEPENDENCY_PROPERTY(Ref<ICommand>, Command);
    AERO_DEPENDENCY_PROPERTY(Value, CommandParameter);
    AERO_DEPENDENCY_PROPERTY(Value, Icon);
    inline static constexpr RoutedEvent<RoutedEventArgs> ClickEvent{"Click"};

protected:
    void
        OnApplyTemplate() noexcept override;
    void OnTemplateDetached() noexcept override;
    void OnPropertyChanged(
        const DependencyPropertyChangedEventArgs& args) noexcept override;

private:
    friend class ::Aero::AeroGuiInternal;
    TextBlock* gestureText_ = nullptr;
    TextBlock* checkGlyph_ = nullptr;
    Primitives::Popup* submenuPopup_ = nullptr;
    Result<void>
        SynchronizeMenuTemplate() noexcept;
    void SetHighlightedState(bool value) noexcept;
    void SetRoleState(MenuItemRole value) noexcept;
};
} // namespace Aero::Controls
AERO_DECLARE_TYPE_ENUM(Aero::Controls::MenuItemRole)
