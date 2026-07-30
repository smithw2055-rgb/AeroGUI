#pragma once

#include <Aero/Controls/ContentControls.hpp>
#include <Aero/Controls/Trees.hpp>
#include <Aero/Presentation/Commands.hpp>

namespace Aero::Controls {

enum class MenuItemRole : std::uint8_t {
    TopLevelItem = 0U,
    TopLevelHeader,
    SubmenuItem,
    SubmenuHeader
};

class MenuInteractionManager;

class AERO_API MenuItem final
    : public TreeViewItem {
    AERO_DECLARE_TYPE(MenuItem, TreeViewItem)
public:
    MenuItem() noexcept;
    ~MenuItem() override;

    Base::StringView InputGestureText()
        const noexcept;
    Base::Result<void> SetInputGestureText(
        Base::StringView value) noexcept;
    bool IsCheckable() const noexcept;
    Base::Result<void> SetIsCheckable(
        bool value) noexcept;
    bool IsChecked() const noexcept;
    Base::Result<void> SetIsChecked(
        bool value) noexcept;
    bool IsHighlighted() const noexcept;
    bool IsSubmenuOpen() const noexcept;
    Base::Result<void> SetIsSubmenuOpen(
        bool value) noexcept;
    MenuItemRole Role() const noexcept;
    ICommand* Command() const noexcept;
    Base::Result<void> SetCommand(
        Base::Ref<ICommand> command) noexcept;
    Base::Ref<Base::Object>
        CommandParameter() const noexcept;
    Base::Result<void> SetCommandParameter(
        Base::Ref<Base::Object> value) noexcept;

    inline static constexpr Members::Property<
        Base::String>
        InputGestureTextProperty{
            "InputGestureText"};
    inline static constexpr Members::Property<bool>
        IsCheckableProperty{"IsCheckable"};
    inline static constexpr Members::Property<bool>
        IsCheckedProperty{"IsChecked"};
    inline static constexpr Members::ReadOnlyProperty<bool>
        IsHighlightedProperty{"IsHighlighted"};
    inline static constexpr Members::Property<bool>
        IsSubmenuOpenProperty{"IsSubmenuOpen"};
    inline static constexpr Members::ReadOnlyProperty<MenuItemRole>
        RoleProperty{"Role"};
    inline static constexpr Members::Property<
        Base::Ref<ICommand>>
        CommandProperty{"Command"};
    inline static constexpr Members::Property<
        Base::Ref<Base::Object>>
        CommandParameterProperty{
            "CommandParameter"};
    inline static constexpr Members::RoutedEvent<
        RoutedEventArgs> ClickEvent{"Click"};

protected:
    Base::Result<void>
        OnApplyTemplate() noexcept override;
    void OnTemplateDetached() noexcept override;

private:
    friend class MenuInteractionManager;
    TextBlock* gestureText_ = nullptr;
    TextBlock* checkGlyph_ = nullptr;
    Popup* submenuPopup_ = nullptr;
    DependencyPropertyChangedEventHandler
        menuPropertyChangedHandler_;
    void OnMenuPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs&
            args) noexcept;
    Base::Result<void>
        SynchronizeMenuTemplate() noexcept;
    Base::Result<void> SetHighlightedState(bool value) noexcept;
    Base::Result<void> SetRoleState(MenuItemRole value) noexcept;
};

class AERO_API Menu : public ItemsControl {
    AERO_DECLARE_TYPE(Menu, ItemsControl)
public:
    Menu() noexcept
        : Menu(StaticTypeId()) {}
    ~Menu() override;

protected:
    explicit Menu(TypeId runtimeType) noexcept
        : ItemsControl(runtimeType) {}
    Base::Result<Base::Ref<ItemContainer>>
        CreateContainer(
            const Base::Ref<Base::Object>& item)
            noexcept override;

private:
    friend class MenuInteractionManager;
    MenuInteractionManager* interactions_ =
        nullptr;
};

class AERO_API ContextMenu final
    : public Menu {
    AERO_DECLARE_TYPE(ContextMenu, Menu)
public:
    ContextMenu() noexcept;
    ~ContextMenu() override;

    bool IsOpen() const noexcept;
    Base::Result<void> SetIsOpen(
        bool value) noexcept;
    Base::Ref<UIElement>
        PlacementTarget() const noexcept;
    Base::Result<void> SetPlacementTarget(
        Base::Ref<UIElement> value) noexcept;

    inline static constexpr Members::Property<bool>
        IsOpenProperty{"IsOpen"};
    inline static constexpr Members::Property<
        Base::Ref<UIElement>>
        PlacementTargetProperty{
            "PlacementTarget"};
    inline static constexpr Members::RoutedEvent<
        RoutedEventArgs> OpenedEvent{"Opened"};
    inline static constexpr Members::RoutedEvent<
        RoutedEventArgs> ClosedEvent{"Closed"};

protected:
    Base::Result<void>
        OnApplyTemplate() noexcept override;

private:
    DependencyPropertyChangedEventHandler
        openChangedHandler_;
    void OnOpenChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs&
            args) noexcept;
};

class AERO_API ContextMenuService final
    : public Base::Object {
    AERO_DECLARE_TYPE(
        ContextMenuService, Base::Object)
public:
    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    static Base::Ref<ContextMenu> GetContextMenu(
        const DependencyObject& target) noexcept;
    static Base::Result<void> SetContextMenu(
        DependencyObject& target,
        Base::Ref<ContextMenu> value) noexcept;

    inline static constexpr Members::AttachedProperty<
        Base::Ref<ContextMenu>>
        ContextMenuProperty{"ContextMenu"};
};


} // namespace Aero::Controls

namespace Aero::Core {

template<>
struct MetaTypeTraits<Controls::MenuItemRole> {
    static constexpr TypeId Id() noexcept { return MakeTypeId("MenuItemRole"); }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept { return "MenuItemRole"; }
    static constexpr TypeId BaseType() noexcept { return InvalidTypeId; }
};

} // namespace Aero::Core
