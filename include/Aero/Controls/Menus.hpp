#pragma once

// Menu, its item, the context-menu host, and the attached-property service.
#include <Aero/ICommand.hpp>
#include <Aero/Controls/HeaderedItemsControl.hpp>
#include <Aero/Controls/ItemsControl.hpp>
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

class AERO_GUI_API MenuItem : public HeaderedItemsControl {
    AERO_DECLARE_TYPE(MenuItem, HeaderedItemsControl)

public:
    MenuItem() noexcept;
    ~MenuItem() override;

    StringView GetInputGestureText() const noexcept;
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
    Value GetIcon() const noexcept { return GetValue(IconProperty); }
    void SetIcon(Value value) noexcept { SetValue(IconProperty, std::move(value)); }

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

    void SetHighlightedState(bool value) noexcept;

protected:
    void OnApplyTemplate() noexcept override;
    void OnTemplateDetached() noexcept override;
    void OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept override;

private:
    TextBlock* gestureText_ = nullptr;
    TextBlock* checkGlyph_ = nullptr;
    Primitives::Popup* submenuPopup_ = nullptr;
    Result<void> SynchronizeMenuTemplate() noexcept;
    void SetRoleState(MenuItemRole value) noexcept;
};

class AERO_GUI_API MenuBase : public ItemsControl {
    AERO_DECLARE_TYPE(MenuBase, ItemsControl)

public:
    ~MenuBase() override = default;

protected:
    explicit MenuBase(TypeId runtimeType) noexcept;
    Result<Ref<FrameworkElement>> GetContainerForItemOverride() const noexcept override;
    void OnMouseLeftButtonDown(MouseButtonEventArgs& args);
    void OnKeyDown(KeyEventArgs& args);

private:
    MenuItem* FindItem(Base::Object* source) const noexcept;
    Result<void> Invoke(MenuItem& item) noexcept;
};

class AERO_GUI_API Menu : public MenuBase {
    AERO_DECLARE_TYPE(Menu, MenuBase)

public:
    Menu() noexcept;
    ~Menu() override;
};

class AERO_GUI_API ContextMenu : public MenuBase {
    AERO_DECLARE_TYPE(ContextMenu, MenuBase)

public:
    ContextMenu() noexcept;
    ~ContextMenu() override;

    bool GetIsOpen() const noexcept;
    void SetIsOpen(bool value) noexcept;
    Ref<UIElement> GetPlacementTarget() const noexcept;
    void SetPlacementTarget(Ref<UIElement> value) noexcept;

    AERO_DEPENDENCY_PROPERTY(bool, IsOpen);
    AERO_DEPENDENCY_PROPERTY(Ref<UIElement>, PlacementTarget);
    inline static constexpr RoutedEvent<RoutedEventArgs> OpenedEvent{"Opened"};
    inline static constexpr RoutedEvent<RoutedEventArgs> ClosedEvent{"Closed"};

protected:
    virtual void OnOpened(RoutedEventArgs& e);
    virtual void OnClosed(RoutedEventArgs& e);
    void OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept override;
    void OnApplyTemplate() noexcept override;
};

class AERO_GUI_API ContextMenuService : public Base::Object {
    AERO_DECLARE_TYPE(ContextMenuService, Base::Object)

public:
    TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    static Ref<ContextMenu> GetContextMenu(const DependencyObject& target) noexcept;
    static void SetContextMenu(DependencyObject& target, Ref<ContextMenu> value) noexcept;

    AERO_ATTACHED_PROPERTY(Ref<ContextMenu>, ContextMenu);
};

} // namespace Aero::Controls

AERO_DECLARE_TYPE_ENUM(Aero::Controls::MenuItemRole)
