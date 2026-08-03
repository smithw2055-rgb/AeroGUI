#include "gui/MetadataInternal.hpp"
#include <Aero/Controls/Common.hpp>

#include <utility>
#include "gui/RoutedEventInternal.hpp"
#include "ControlBehavior.hpp"

namespace Aero::Controls {
using Aero::Internal::MenuBehavior;

using namespace Primitives;
using namespace Meta;

MenuItem::MenuItem() noexcept
    : HeaderedItemsControl(StaticTypeId()),
      menuPropertyChangedHandler_(
          this,
          &MenuItem::OnMenuPropertyChanged) {
    static_cast<void>(AddValueChangedHandlerChecked(
        InputGestureTextProperty,
        menuPropertyChangedHandler_));
    static_cast<void>(AddValueChangedHandlerChecked(
        IsCheckableProperty,
        menuPropertyChangedHandler_));
    static_cast<void>(AddValueChangedHandlerChecked(
        IsCheckedProperty,
        menuPropertyChangedHandler_));
    static_cast<void>(AddValueChangedHandlerChecked(
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
MenuItem::GetInputGestureText() const noexcept {
    return GetValueOr(
        InputGestureTextProperty,
        Base::StringView{});
}

void
MenuItem::SetInputGestureText(
    Base::StringView value) noexcept {
    SetValue(
        InputGestureTextProperty, value);
}

bool MenuItem::GetIsCheckable() const noexcept {
    return GetValueOr(
        IsCheckableProperty, false);
}

void MenuItem::SetIsCheckable(
    bool value) noexcept {
    SetValue(
        IsCheckableProperty, value);
}

bool MenuItem::GetIsChecked() const noexcept {
    return GetValueOr(
        IsCheckedProperty, false);
}

void MenuItem::SetIsChecked(
    bool value) noexcept {
    SetCurrentValue(
        IsCheckedProperty, value);
}

bool MenuItem::GetIsHighlighted() const noexcept {
    return GetValueOr(IsHighlightedProperty, false);
}

bool MenuItem::GetIsSubmenuOpen() const noexcept {
    return GetValueOr(IsSubmenuOpenProperty, false);
}

void MenuItem::SetIsSubmenuOpen(
    bool value) noexcept {
    SetCurrentValue(IsSubmenuOpenProperty, value);
}

MenuItemRole MenuItem::GetRole() const noexcept {
    return GetValueOr(RoleProperty, MenuItemRole::TopLevelItem);
}

ICommand* MenuItem::GetCommand() const noexcept {
    return GetValueOr(
        CommandProperty,
        Base::Ref<ICommand>{}).Get();
}

void MenuItem::SetCommand(
    Base::Ref<ICommand> command) noexcept {
    SetValue(
        CommandProperty, std::move(command));
}

Base::Ref<Base::Object>
MenuItem::GetCommandParameter() const noexcept {
    return GetValueOr(
        CommandParameterProperty,
        Base::Ref<Base::Object>{});
}

void
MenuItem::SetCommandParameter(
    Base::Ref<Base::Object> value) noexcept {
    SetValue(
        CommandParameterProperty,
        std::move(value));
}

void
MenuItem::OnApplyTemplate() noexcept {
    HeaderedItemsControl::OnApplyTemplate();
    static_cast<void>(SetRoleState(
        GetCount() != 0U
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
        return;
    }
    static_cast<void>(SynchronizeMenuTemplate());
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
    HeaderedItemsControl::OnTemplateDetached();
}

Base::Result<void>
MenuItem::SynchronizeMenuTemplate() noexcept {
    if (gestureText_ != nullptr) {
        gestureText_->SetText(GetInputGestureText());
    }
    if (checkGlyph_ != nullptr) {
        checkGlyph_->SetText(
            GetIsCheckable() && GetIsChecked()
            ? Base::StringView("x")
            : Base::StringView(""));
    }
    if (submenuPopup_ != nullptr) {
        if (GetIsSubmenuOpen()) {
            Base::Ref<UIElement> target =
                Base::Ref<UIElement>::
                    TryFromBorrowed(*this);
            if (target) {
                submenuPopup_->SetPlacementTarget(std::move(target));
            }
        }
        submenuPopup_->SetIsOpen(GetIsSubmenuOpen());
        if (!GetIsSubmenuOpen()) {
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

void MenuItem::SetHighlightedState(
    bool value) noexcept {
    SetReadOnlyCurrentValue(IsHighlightedProperty, value);
}

void MenuItem::SetRoleState(
    MenuItemRole value) noexcept {
    SetReadOnlyCurrentValue(RoleProperty, value);
}

Menu::~Menu() {
    if (interactions_ != nullptr) {
        static_cast<void>(
            static_cast<MenuBehavior*>(
                interactions_)->Detach(*this));
    }
}

Base::Result<Base::Ref<FrameworkElement>>
Menu::CreateContainer(
    const Base::Ref<Base::Object>&) noexcept {
    Base::Result<Base::Ref<MenuItem>> made =
        Base::MakeRef<MenuItem>();
    if (!made) return made.GetStatus();
    return Base::Ref<FrameworkElement>(
        std::move(made).Value());
}

ContextMenu::ContextMenu() noexcept
    : Menu(StaticTypeId()),
      openChangedHandler_(
          this,
          &ContextMenu::OnOpenChanged) {
    static_cast<void>(AddValueChangedHandlerChecked(
        IsOpenProperty,
        openChangedHandler_));
}

ContextMenu::~ContextMenu() {
    static_cast<void>(RemoveValueChangedHandler(
        IsOpenProperty,
        openChangedHandler_));
}

bool ContextMenu::GetIsOpen() const noexcept {
    return GetValueOr(
        IsOpenProperty, false);
}

void ContextMenu::SetIsOpen(
    bool value) noexcept {
    SetCurrentValue(
        IsOpenProperty, value);
}

Base::Ref<UIElement>
ContextMenu::GetPlacementTarget() const noexcept {
    return GetValueOr(
        PlacementTargetProperty,
        Base::Ref<UIElement>{});
}

void
ContextMenu::SetPlacementTarget(
    Base::Ref<UIElement> value) noexcept {
    SetValue(
        PlacementTargetProperty,
        std::move(value));
}

void
ContextMenu::OnApplyTemplate() noexcept {
    Menu::OnApplyTemplate();
    SetVisibility(
        GetIsOpen()
        ? Visibility::Visible
        : Visibility::Collapsed);
}

void ContextMenu::OnOpenChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs&
        args) noexcept {
    const bool opened =
        args.GetNewValue().AsBoolean();
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

void
ContextMenuService::SetContextMenu(
    DependencyObject& target,
    Base::Ref<ContextMenu> value) noexcept {
    target.SetValue(
        ContextMenuProperty,
        std::move(value));
}

} // namespace Aero::Controls

namespace Aero::Controls {

using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero::Controls;
using namespace ::Aero::Internal;

Menu::Impl::
Impl(
    ElementTree& tree,
    EventRouter& events,
    InputRouter& input) noexcept
    : tree_(&tree),
      events_(&events),
      input_(&input),
      mouseDownHandler_(
          this,
          &Menu::Impl::
              OnMouseDown),
      keyDownHandler_(
          this,
          &Menu::Impl::
              OnKeyDown) {}

Menu::Impl::
~Impl() noexcept {
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
Menu::Impl::FindMenu(
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

Menu* Menu::Impl::ResolveMenu(
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
Menu::Impl::Attach(
    Menu& menu) noexcept {
    if (menu.interactions_ != nullptr ||
        Aero::Internal::ElementPrivate::Tree(menu) != tree_ ||
        FindMenu(menu) != UINT32_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Menu interaction attach state is invalid");
    }
    Base::Result<VisualHandle> handle =
        tree_->GetHandle(menu);
    if (!handle) return handle.GetStatus();
    menu.AddHandlerChecked(UIElement::MouseDownEvent, mouseDownHandler_);
    menu.AddHandlerChecked(UIElement::KeyDownEvent, keyDownHandler_);
    Base::Result<void> stored =
        records_.PushBack(handle.Value());
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
Menu::Impl::Detach(
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

MenuItem* Menu::Impl::FindItem(
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
    Menu::Impl::Invoke(
    Menu& menu,
    MenuItem& item) noexcept {
    if (item.GetCount() != 0U) {
        item.SetIsSubmenuOpen(!item.GetIsSubmenuOpen());
        return {};
    }
    if (item.GetIsCheckable()) {
        item.SetIsChecked(!item.GetIsChecked());
    }
    RoutedEventArgs event;
    Base::Result<void> raised =
        events_->RaiseEvent(
            item, MenuItem::ClickEvent, &event);
    if (!raised) return raised.GetStatus();
    ICommand* command = item.GetCommand();
    if (command != nullptr) {
        Base::Ref<Base::Object> parameter =
            item.GetCommandParameter();
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

void Menu::Impl::OnMouseDown(
    Base::Object* sender,
    MouseButtonEventArgs& args)
    noexcept {
    if (args.GetChangedButton() !=
        MouseButton::Left) {
        return;
    }
    auto& menu =
        *static_cast<Menu*>(sender);
    MenuItem* item =
        FindItem(
            menu, args.GetOriginalSource());
    if (item == nullptr) return;
    ::Aero::Visual::Impl::SetMenuItemHighlighted(*item, true);
    Base::Result<void> invoked =
        Invoke(menu, *item);
    if (!invoked) return;
    static_cast<void>(
        input_->SetFocus(item));
    args.SetHandled(true);
}

void Menu::Impl::OnKeyDown(
    Base::Object* sender,
    KeyEventArgs& args) noexcept {
    if (args.GetKey() != KeyboardKeyEnter &&
        args.GetKey() != KeyboardKeySpace &&
        args.GetKey() != KeyboardKeyRight &&
        args.GetKey() != KeyboardKeyLeft &&
        args.GetKey() != KeyboardKeyEscape) {
        return;
    }
    auto& menu =
        *static_cast<Menu*>(sender);
    MenuItem* item =
        FindItem(
            menu, args.GetOriginalSource());
    if (item == nullptr) return;
    if (args.GetKey() == KeyboardKeyEscape ||
        args.GetKey() == KeyboardKeyLeft) {
        static_cast<void>(
            item->SetIsSubmenuOpen(false));
    } else if (
        args.GetKey() == KeyboardKeyRight) {
        if (item->GetCount() != 0U) {
            static_cast<void>(
                item->SetIsSubmenuOpen(true));
        }
    } else {
        static_cast<void>(
            Invoke(menu, *item));
    }
    args.SetHandled(true);
}

} // namespace Aero::Controls
