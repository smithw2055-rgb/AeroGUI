#pragma once

#include <Aero/ICommand.hpp>
#include <Aero/Controls/HeaderedItemsControl.hpp>
#include <Aero/Controls/Popup.hpp>
#include <Aero/Controls/TextBlock.hpp>

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
    MenuItem() noexcept;
    ~MenuItem() override;

    StringView GetInputGestureText()
        const noexcept;
    void SetInputGestureText(
        StringView value) noexcept;
    bool GetIsCheckable() const noexcept;
    void SetIsCheckable(
        bool value) noexcept;
    bool GetIsChecked() const noexcept;
    void SetIsChecked(
        bool value) noexcept;
    bool GetIsHighlighted() const noexcept;
    bool GetIsSubmenuOpen() const noexcept;
    void SetIsSubmenuOpen(
        bool value) noexcept;
    MenuItemRole GetRole() const noexcept;
    ICommand* GetCommand() const noexcept;
    void SetCommand(
        Ref<ICommand> command) noexcept;
    Value GetCommandParameter() const noexcept;
    void SetCommandParameter(Value value) noexcept;
    Value GetIcon() const noexcept {
        return GetValue(IconProperty);
    }
    void SetIcon(Value value) noexcept {
        SetValue(IconProperty, std::move(value));
    }

    inline static constexpr DependencyProperty<String> InputGestureTextProperty{"InputGestureText"};
    inline static constexpr DependencyProperty<bool> IsCheckableProperty{"IsCheckable"};
    inline static constexpr DependencyProperty<bool> IsCheckedProperty{"IsChecked"};
    inline static constexpr ReadOnlyDependencyProperty<bool> IsHighlightedProperty{"IsHighlighted"};
    inline static constexpr DependencyProperty<bool> IsSubmenuOpenProperty{"IsSubmenuOpen"};
    inline static constexpr ReadOnlyDependencyProperty<MenuItemRole> RoleProperty{"Role"};
    inline static constexpr DependencyProperty<Ref<ICommand>> CommandProperty{"Command"};
    inline static constexpr DependencyProperty<Value> CommandParameterProperty{"CommandParameter"};
    inline static constexpr DependencyProperty<Value> IconProperty{"Icon"};
    inline static constexpr RoutedEvent<RoutedEventArgs> ClickEvent{"Click"};

protected:
    void
        OnApplyTemplate() noexcept override;
    void OnTemplateDetached() noexcept override;

private:
#if defined(AERO_GUI_IMPLEMENTATION)
    friend class ::Aero::AeroGuiInternal;
#endif
    TextBlock* gestureText_ = nullptr;
    TextBlock* checkGlyph_ = nullptr;
    Primitives::Popup* submenuPopup_ = nullptr;
    DependencyPropertyChangedEventHandler
        menuPropertyChangedHandler_;
    void OnMenuPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs&
            args) noexcept;
    Result<void>
        SynchronizeMenuTemplate() noexcept;
    void SetHighlightedState(bool value) noexcept;
    void SetRoleState(MenuItemRole value) noexcept;
};
} // namespace Aero::Controls
AERO_DECLARE_TYPE_ENUM(Aero::Controls::MenuItemRole)
