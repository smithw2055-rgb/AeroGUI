#include <Aero/Controls/Standard.hpp>

#include <utility>
#include "gui/RoutedEventInternal.hpp"
#include "RuntimeManagers.hpp"

namespace Aero::Controls {

using namespace Primitives;
using namespace Core;

MenuItem::MenuItem() noexcept
    : TreeViewItem(StaticTypeId()),
      menuPropertyChangedHandler_(
          this,
          &MenuItem::OnMenuPropertyChanged) {
    static_cast<void>(TryAddValueChangedHandler(
        InputGestureTextProperty,
        menuPropertyChangedHandler_));
    static_cast<void>(TryAddValueChangedHandler(
        IsCheckableProperty,
        menuPropertyChangedHandler_));
    static_cast<void>(TryAddValueChangedHandler(
        IsCheckedProperty,
        menuPropertyChangedHandler_));
    static_cast<void>(TryAddValueChangedHandler(
        IsSubmenuOpenProperty,
        menuPropertyChangedHandler_));
}

MenuItem::~MenuItem() {
    static_cast<void>(RemoveValueChangedHandler(
        InputGestureTextProperty,
        menuPropertyChangedHandler_));
    static_cast<void>(RemoveValueChangedHandler(
        IsCheckableProperty,
        menuPropertyChangedHandler_));
    static_cast<void>(RemoveValueChangedHandler(
        IsCheckedProperty,
        menuPropertyChangedHandler_));
    static_cast<void>(RemoveValueChangedHandler(
        IsSubmenuOpenProperty,
        menuPropertyChangedHandler_));
}

Base::StringView
MenuItem::InputGestureText() const noexcept {
    return GetValueOr(
        InputGestureTextProperty,
        Base::StringView{});
}

Base::Result<void>
MenuItem::SetInputGestureText(
    Base::StringView value) noexcept {
    return SetValue(
        InputGestureTextProperty, value);
}

bool MenuItem::IsCheckable() const noexcept {
    return GetValueOr(
        IsCheckableProperty, false);
}

Base::Result<void> MenuItem::SetIsCheckable(
    bool value) noexcept {
    return SetValue(
        IsCheckableProperty, value);
}

bool MenuItem::IsChecked() const noexcept {
    return GetValueOr(
        IsCheckedProperty, false);
}

Base::Result<void> MenuItem::SetIsChecked(
    bool value) noexcept {
    return SetCurrentValue(
        IsCheckedProperty, value);
}

bool MenuItem::IsHighlighted() const noexcept {
    return GetValueOr(IsHighlightedProperty, false);
}

bool MenuItem::IsSubmenuOpen() const noexcept {
    return GetValueOr(IsSubmenuOpenProperty, false);
}

Base::Result<void> MenuItem::SetIsSubmenuOpen(
    bool value) noexcept {
    return SetCurrentValue(IsSubmenuOpenProperty, value);
}

MenuItemRole MenuItem::Role() const noexcept {
    return GetValueOr(RoleProperty, MenuItemRole::TopLevelItem);
}

ICommand* MenuItem::GetCommand() const noexcept {
    return GetValueOr(
        CommandProperty,
        Base::Ref<ICommand>{}).Get();
}

Base::Result<void> MenuItem::SetCommand(
    Base::Ref<ICommand> command) noexcept {
    return SetValue(
        CommandProperty, std::move(command));
}

Base::Ref<Base::Object>
MenuItem::CommandParameter() const noexcept {
    return GetValueOr(
        CommandParameterProperty,
        Base::Ref<Base::Object>{});
}

Base::Result<void>
MenuItem::SetCommandParameter(
    Base::Ref<Base::Object> value) noexcept {
    return SetValue(
        CommandParameterProperty,
        std::move(value));
}

Base::Result<void>
MenuItem::OnApplyTemplate() noexcept {
    Base::Result<void> applied =
        TreeViewItem::OnApplyTemplate();
    if (!applied) return applied.GetStatus();
    static_cast<void>(SetRoleState(
        Count() != 0U
            ? MenuItemRole::TopLevelHeader
            : MenuItemRole::TopLevelItem));
    DependencyObject* gesture =
        GetTemplateChild("GestureText");
    gestureText_ =
        gesture != nullptr &&
        PropertyRegistry().Types().IsDerivedFrom(
            gesture->RuntimeType(),
            TextBlock::StaticTypeId())
        ? static_cast<TextBlock*>(gesture)
        : nullptr;
    DependencyObject* check =
        GetTemplateChild("CheckGlyph");
    checkGlyph_ =
        check != nullptr &&
        PropertyRegistry().Types().IsDerivedFrom(
            check->RuntimeType(),
            TextBlock::StaticTypeId())
        ? static_cast<TextBlock*>(check)
        : nullptr;
    DependencyObject* submenu =
        GetTemplateChild("SubmenuPopup");
    submenuPopup_ =
        submenu != nullptr &&
        PropertyRegistry().Types().IsDerivedFrom(
            submenu->RuntimeType(),
            Popup::StaticTypeId())
        ? static_cast<Popup*>(submenu)
        : nullptr;
    if (gestureText_ == nullptr ||
        checkGlyph_ == nullptr ||
        submenuPopup_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "MenuItem template requires GestureText, CheckGlyph, and SubmenuPopup");
    }
    return SynchronizeMenuTemplate();
}

void MenuItem::OnTemplateDetached() noexcept {
    if (submenuPopup_ != nullptr) {
        static_cast<void>(
            submenuPopup_->SetIsOpen(false));
        static_cast<void>(
            submenuPopup_->
                SetPlacementTarget({}));
    }
    gestureText_ = nullptr;
    checkGlyph_ = nullptr;
    submenuPopup_ = nullptr;
    TreeViewItem::OnTemplateDetached();
}

Base::Result<void>
MenuItem::SynchronizeMenuTemplate() noexcept {
    if (gestureText_ != nullptr) {
        Base::Result<void> gesture =
            gestureText_->SetText(
                InputGestureText());
        if (!gesture) return gesture.GetStatus();
    }
    if (checkGlyph_ != nullptr) {
        Base::Result<void> checked =
            checkGlyph_->SetText(
            IsCheckable() && IsChecked()
            ? Base::StringView("x")
            : Base::StringView(""));
        if (!checked) return checked.GetStatus();
    }
    if (submenuPopup_ != nullptr) {
        if (IsSubmenuOpen()) {
            Base::Ref<UIElement> target =
                Base::Ref<UIElement>::
                    TryFromBorrowed(*this);
            if (target) {
                Base::Result<void> placed =
                    submenuPopup_->
                        SetPlacementTarget(
                            std::move(target));
                if (!placed) {
                    return placed.GetStatus();
                }
            }
        }
        Base::Result<void> opened =
            submenuPopup_->SetIsOpen(
                IsSubmenuOpen());
        if (!opened) return opened.GetStatus();
        if (!IsSubmenuOpen()) {
            static_cast<void>(
                submenuPopup_->
                    SetPlacementTarget({}));
        }
    }
    return {};
}

void MenuItem::OnMenuPropertyChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs&)
    noexcept {
    static_cast<void>(
        SynchronizeMenuTemplate());
}

Base::Result<void> MenuItem::SetHighlightedState(
    bool value) noexcept {
    return SetReadOnlyCurrentValue(
        IsHighlightedProperty, value);
}

Base::Result<void> MenuItem::SetRoleState(
    MenuItemRole value) noexcept {
    return SetReadOnlyCurrentValue(RoleProperty, value);
}

Menu::~Menu() {
    if (interactions_ != nullptr) {
        static_cast<void>(
            static_cast<MenuInteractionManager*>(
                interactions_)->Detach(*this));
    }
}

Base::Result<Base::Ref<ItemContainer>>
Menu::CreateContainer(
    const Base::Ref<Base::Object>&) noexcept {
    Base::Result<Base::Ref<MenuItem>> made =
        Base::MakeRef<MenuItem>();
    if (!made) return made.GetStatus();
    return Base::Ref<ItemContainer>(
        std::move(made).Value());
}

ContextMenu::ContextMenu() noexcept
    : Menu(StaticTypeId()),
      openChangedHandler_(
          this,
          &ContextMenu::OnOpenChanged) {
    static_cast<void>(TryAddValueChangedHandler(
        IsOpenProperty,
        openChangedHandler_));
}

ContextMenu::~ContextMenu() {
    static_cast<void>(RemoveValueChangedHandler(
        IsOpenProperty,
        openChangedHandler_));
}

bool ContextMenu::IsOpen() const noexcept {
    return GetValueOr(
        IsOpenProperty, false);
}

Base::Result<void> ContextMenu::SetIsOpen(
    bool value) noexcept {
    return SetCurrentValue(
        IsOpenProperty, value);
}

Base::Ref<UIElement>
ContextMenu::PlacementTarget() const noexcept {
    return GetValueOr(
        PlacementTargetProperty,
        Base::Ref<UIElement>{});
}

Base::Result<void>
ContextMenu::SetPlacementTarget(
    Base::Ref<UIElement> value) noexcept {
    return SetValue(
        PlacementTargetProperty,
        std::move(value));
}

Base::Result<void>
ContextMenu::OnApplyTemplate() noexcept {
    Base::Result<void> applied =
        Menu::OnApplyTemplate();
    if (!applied) return applied.GetStatus();
    return SetVisibility(
        IsOpen()
        ? Visibility::Visible
        : Visibility::Collapsed);
}

void ContextMenu::OnOpenChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs&
        args) noexcept {
    const bool opened =
        args.newValue.AsBoolean();
    static_cast<void>(SetVisibility(
        opened
        ? Visibility::Visible
        : Visibility::Collapsed));
    RoutedEventArgs event;
    static_cast<void>(RaiseEvent(
        opened ? OpenedEvent : ClosedEvent,
        &event));
}

Base::Ref<ContextMenu>
ContextMenuService::GetContextMenu(
    const DependencyObject& target) noexcept {
    return target.GetValueOr(
        ContextMenuProperty,
        Base::Ref<ContextMenu>{});
}

Base::Result<void>
ContextMenuService::SetContextMenu(
    DependencyObject& target,
    Base::Ref<ContextMenu> value) noexcept {
    return target.SetValue(
        ContextMenuProperty,
        std::move(value));
}

} // namespace Aero::Controls

namespace Aero::Detail {

using namespace Aero::Core;
using namespace Aero::Controls;

MenuInteractionManager::
MenuInteractionManager(
    GuiContext& tree,
    EventRouter& events,
    InputService& input) noexcept
    : tree_(&tree),
      events_(&events),
      input_(&input),
      mouseDownHandler_(
          this,
          &MenuInteractionManager::
              OnMouseDown),
      keyDownHandler_(
          this,
          &MenuInteractionManager::
              OnKeyDown) {}

MenuInteractionManager::
~MenuInteractionManager() noexcept {
    while (!records_.Empty()) {
        Menu* menu =
            ResolveMenu(records_.Size() - 1U);
        if (menu == nullptr) {
            records_.PopBack();
        } else {
            static_cast<void>(Detach(*menu));
        }
    }
}

std::uint32_t
MenuInteractionManager::FindMenu(
    const Menu& menu) const noexcept {
    for (std::uint32_t index = 0U;
        index < records_.Size(); ++index) {
        if (tree_->ResolveHandle(
                records_[index]) == &menu) {
            return index;
        }
    }
    return UINT32_MAX;
}

Menu* MenuInteractionManager::ResolveMenu(
    std::uint32_t index) noexcept {
    Visual* visual =
        index < records_.Size()
        ? tree_->ResolveHandle(records_[index])
        : nullptr;
    return visual != nullptr
        ? static_cast<Menu*>(
            visual->AsUIElement())
        : nullptr;
}

Base::Result<void>
MenuInteractionManager::Attach(
    Menu& menu) noexcept {
    if (menu.interactions_ != nullptr ||
        Aero::Detail::VisualAccess::Tree(menu) != tree_ ||
        FindMenu(menu) != UINT32_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Menu interaction attach state is invalid");
    }
    Base::Result<VisualHandle> handle =
        tree_->GetHandle(menu);
    if (!handle) return handle.GetStatus();
    Base::Result<void> mouse =
        menu.TryAddHandler(
            UIElement::MouseDownEvent,
            mouseDownHandler_);
    if (!mouse) return mouse.GetStatus();
    Base::Result<void> key =
        menu.TryAddHandler(
            UIElement::KeyDownEvent,
            keyDownHandler_);
    if (!key) {
        static_cast<void>(menu.RemoveHandler(
            UIElement::MouseDownEvent,
            mouseDownHandler_));
        return key.GetStatus();
    }
    Base::Result<void> stored =
        records_.TryPushBack(handle.Value());
    if (!stored) {
        static_cast<void>(menu.RemoveHandler(
            UIElement::KeyDownEvent,
            keyDownHandler_));
        static_cast<void>(menu.RemoveHandler(
            UIElement::MouseDownEvent,
            mouseDownHandler_));
        return stored.GetStatus();
    }
    menu.interactions_ = this;
    return {};
}

Base::Result<bool>
MenuInteractionManager::Detach(
    Menu& menu) noexcept {
    const std::uint32_t index =
        FindMenu(menu);
    if (index == UINT32_MAX) return false;
    static_cast<void>(menu.RemoveHandler(
        UIElement::MouseDownEvent,
        mouseDownHandler_));
    static_cast<void>(menu.RemoveHandler(
        UIElement::KeyDownEvent,
        keyDownHandler_));
    for (std::uint32_t current = index;
        current + 1U < records_.Size();
        ++current) {
        records_[current] =
            records_[current + 1U];
    }
    records_.PopBack();
    menu.interactions_ = nullptr;
    return true;
}

MenuItem* MenuInteractionManager::FindItem(
    Menu& menu,
    Base::Object* source) const noexcept {
    if (source == nullptr ||
        !menu.PropertyRegistry().Types().
            IsDerivedFrom(
                source->RuntimeType(),
                UIElement::StaticTypeId())) {
        return nullptr;
    }
    Visual* visual =
        static_cast<UIElement*>(source);
    while (visual != nullptr &&
        visual != &menu) {
        UIElement* element =
            visual->AsUIElement();
        if (element != nullptr &&
            menu.PropertyRegistry().Types().
                IsDerivedFrom(
                    element->RuntimeType(),
                    MenuItem::StaticTypeId())) {
            return static_cast<MenuItem*>(
                element);
        }
        visual = visual->GetVisualParent();
    }
    return nullptr;
}

Base::Result<void>
MenuInteractionManager::Invoke(
    Menu& menu,
    MenuItem& item) noexcept {
    if (item.Count() != 0U) {
        return item.SetIsSubmenuOpen(
            !item.IsSubmenuOpen());
    }
    if (item.IsCheckable()) {
        Base::Result<void> checked =
            item.SetIsChecked(
                !item.IsChecked());
        if (!checked) return checked.GetStatus();
    }
    RoutedEventArgs event;
    Base::Result<void> raised =
        events_->RaiseEvent(
            item, MenuItem::ClickEvent, &event);
    if (!raised) return raised.GetStatus();
    ICommand* command = item.GetCommand();
    if (command != nullptr) {
        Base::Ref<Base::Object> parameter =
            item.CommandParameter();
        const Value value = Value::FromObject(
            TypeOf<Base::Object>(),
            std::move(parameter));
        Base::Result<bool> executed =
            input_->Execute(*command, value, item);
        if (!executed) {
            return executed.GetStatus();
        }
    }
    if (menu.PropertyRegistry().Types().
        IsDerivedFrom(
            menu.RuntimeType(),
            ContextMenu::StaticTypeId())) {
        static_cast<void>(
            static_cast<ContextMenu&>(
                menu).SetIsOpen(false));
    }
    return {};
}

void MenuInteractionManager::OnMouseDown(
    Base::Object* sender,
    MouseButtonEventArgs& args)
    noexcept {
    if (args.changedButton !=
        MouseButton::Left) {
        return;
    }
    auto& menu =
        *static_cast<Menu*>(sender);
    MenuItem* item =
        FindItem(
            menu, args.originalSource);
    if (item == nullptr) return;
    static_cast<void>(item->SetHighlightedState(true));
    Base::Result<void> invoked =
        Invoke(menu, *item);
    if (!invoked) return;
    static_cast<void>(
        input_->SetFocus(item));
    args.handled = true;
}

void MenuInteractionManager::OnKeyDown(
    Base::Object* sender,
    KeyEventArgs& args) noexcept {
    if (args.key != KeyboardKeyEnter &&
        args.key != KeyboardKeySpace &&
        args.key != KeyboardKeyRight &&
        args.key != KeyboardKeyLeft &&
        args.key != KeyboardKeyEscape) {
        return;
    }
    auto& menu =
        *static_cast<Menu*>(sender);
    MenuItem* item =
        FindItem(
            menu, args.originalSource);
    if (item == nullptr) return;
    if (args.key == KeyboardKeyEscape ||
        args.key == KeyboardKeyLeft) {
        static_cast<void>(
            item->SetIsExpanded(false));
    } else if (
        args.key == KeyboardKeyRight) {
        if (item->Count() != 0U) {
            static_cast<void>(
                item->SetIsExpanded(true));
        }
    } else {
        static_cast<void>(
            Invoke(menu, *item));
    }
    args.handled = true;
}

} // namespace Aero::Detail
