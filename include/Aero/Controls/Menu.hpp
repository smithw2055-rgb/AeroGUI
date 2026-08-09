#pragma once

#include <Aero/Input.hpp>
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
        return GetValueOr(
            IconProperty,
            Value::NullObject(Meta::TypeOf<Base::Object>()));
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
    friend struct ::Aero::Media::Visual::Access;
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

class AERO_GUI_API Menu : public ItemsControl {
    AERO_DECLARE_TYPE(Menu, ItemsControl)
#if defined(AERO_GUI_IMPLEMENTATION)
public:
#else
private:
#endif
    struct Access;

public:

    Menu() noexcept
        : Menu(StaticTypeId()) {}
    ~Menu() override;

protected:
    explicit Menu(TypeId runtimeType) noexcept
        : ItemsControl(runtimeType) {}
    Result<Ref<FrameworkElement>>
        CreateContainer(
            const Ref<Base::Object>& item)
            noexcept override;

private:
    friend struct Access;
};

class AERO_GUI_API ContextMenu
    : public Menu {
    AERO_DECLARE_TYPE(ContextMenu, Menu)
public:
    ContextMenu() noexcept;
    ~ContextMenu() override;

    bool GetIsOpen() const noexcept;
    void SetIsOpen(
        bool value) noexcept;
    Ref<UIElement>
        GetPlacementTarget() const noexcept;
    void SetPlacementTarget(
        Ref<UIElement> value) noexcept;

    inline static constexpr DependencyProperty<bool> IsOpenProperty{"IsOpen"};
    inline static constexpr DependencyProperty<Ref<UIElement>> PlacementTargetProperty{"PlacementTarget"};
    inline static constexpr RoutedEvent<RoutedEventArgs> OpenedEvent{"Opened"};
    inline static constexpr RoutedEvent<RoutedEventArgs> ClosedEvent{"Closed"};

protected:
    void
        OnApplyTemplate() noexcept override;

private:
    DependencyPropertyChangedEventHandler
        openChangedHandler_;
    void OnOpenChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs&
            args) noexcept;
};

class AERO_GUI_API ContextMenuService
    : public Base::Object {
    AERO_DECLARE_TYPE(
        ContextMenuService, Base::Object)
public:
    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    static Ref<ContextMenu> GetContextMenu(
        const DependencyObject& target) noexcept;
    static void SetContextMenu(
        DependencyObject& target,
        Ref<ContextMenu> value) noexcept;

    inline static constexpr AttachedProperty<Ref<ContextMenu>> ContextMenuProperty{"ContextMenu"};
};
} // namespace Aero::Controls
AERO_DECLARE_TYPE_ENUM(Aero::Controls::MenuItemRole)
