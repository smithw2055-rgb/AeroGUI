#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/internal/AeroGuiInternal.hpp"
#include "gui/input/InputManager.hpp" 
#include "gui/media/AnimationEngine.hpp"
#include "gui/meta/TypeRegistryDetail.hpp"
#include <Aero/Controls.hpp>
#include <Aero/Controls/ContextMenuService.hpp>

#include <utility>

namespace Aero::Controls {

using namespace Primitives;
using namespace Meta;

MenuItem::MenuItem() noexcept
    : HeaderedItemsControl(StaticTypeId()) {}

MenuItem::~MenuItem() = default;

Base::StringView
MenuItem::GetInputGestureText() const noexcept {
    return GetValue(InputGestureTextProperty);
}

void
MenuItem::SetInputGestureText(
    Base::StringView value) noexcept {
    SetValue(
        InputGestureTextProperty, value);
}

bool MenuItem::GetIsCheckable() const noexcept {
    return GetValue(IsCheckableProperty);
}

void MenuItem::SetIsCheckable(
    bool value) noexcept {
    SetValue(
        IsCheckableProperty, value);
}

bool MenuItem::GetIsChecked() const noexcept {
    return GetValue(IsCheckedProperty);
}

void MenuItem::SetIsChecked(
    bool value) noexcept {
    SetCurrentValue(
        IsCheckedProperty, value);
}

bool MenuItem::GetIsHighlighted() const noexcept {
    return GetValue(IsHighlightedProperty);
}

bool MenuItem::GetIsSubmenuOpen() const noexcept {
    return GetValue(IsSubmenuOpenProperty);
}

void MenuItem::SetIsSubmenuOpen(
    bool value) noexcept {
    SetCurrentValue(IsSubmenuOpenProperty, value);
}

MenuItemRole MenuItem::GetRole() const noexcept {
    return GetValue(RoleProperty);
}

ICommand* MenuItem::GetCommand() const noexcept {
    return GetValue(CommandProperty).Get();
}

void MenuItem::SetCommand(
    Base::Ref<ICommand> command) noexcept {
    SetValue(
        CommandProperty, std::move(command));
}

Value
MenuItem::GetCommandParameter() const noexcept {
    return GetValue(CommandParameterProperty);
}

void
MenuItem::SetCommandParameter(
    Value value) noexcept {
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
        AeroGuiInternal::PropertyRegistry(*this).Types().IsDerivedFrom(
            gesture->RuntimeType(),
            TextBlock::StaticTypeId())
        ? static_cast<TextBlock*>(gesture)
        : nullptr;
    DependencyObject* check =
        GetTemplateChild("CheckGlyph");
    checkGlyph_ =
        check != nullptr &&
        AeroGuiInternal::PropertyRegistry(*this).Types().IsDerivedFrom(
            check->RuntimeType(),
            TextBlock::StaticTypeId())
        ? static_cast<TextBlock*>(check)
        : nullptr;
    DependencyObject* submenu =
        GetTemplateChild("SubmenuPopup");
    submenuPopup_ =
        submenu != nullptr &&
        AeroGuiInternal::PropertyRegistry(*this).Types().IsDerivedFrom(
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

void MenuItem::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    HeaderedItemsControl::OnPropertyChanged(args);
    const DependencyPropertyHandle prop = args.GetProperty();
    if (prop == InputGestureTextProperty ||
        prop == IsCheckableProperty ||
        prop == IsCheckedProperty ||
        prop == IsSubmenuOpenProperty) {
        static_cast<void>(
            SynchronizeMenuTemplate());
    }
}

void MenuItem::SetHighlightedState(
    bool value) noexcept {
    SetReadOnlyCurrentValue(IsHighlightedProperty, value);
}

void MenuItem::SetRoleState(
    MenuItemRole value) noexcept {
    SetReadOnlyCurrentValue(RoleProperty, value);
}

Menu::Menu() noexcept
    : Menu(StaticTypeId()) {}

Menu::Menu(TypeId runtimeType) noexcept
    : ItemsControl(runtimeType) {}

Menu::~Menu() = default;

MenuItem* Menu::FindItem(Base::Object* source) const noexcept {
    if (source == nullptr ||
        !AeroGuiInternal::PropertyRegistry(*this).Types().
            IsDerivedFrom(
                source->RuntimeType(),
                UIElement::StaticTypeId())) {
        return nullptr;
    }
    ::Aero::Media::Visual* visual =
        static_cast<UIElement*>(source);
    while (visual != nullptr &&
        visual != this) {
        UIElement* element =
            ::Aero::TryCast<::Aero::UIElement>(visual);
        if (element != nullptr &&
            AeroGuiInternal::PropertyRegistry(*this).Types().
                IsDerivedFrom(
                    element->RuntimeType(),
                    MenuItem::StaticTypeId())) {
            return static_cast<MenuItem*>(element);
        }
        visual = visual->GetVisualParent();
    }
    return nullptr;
}

Base::Result<void> Menu::Invoke(MenuItem& item) noexcept {
    if (item.GetCount() != 0U) {
        item.SetIsSubmenuOpen(!item.GetIsSubmenuOpen());
        return {};
    }
    if (item.GetIsCheckable()) {
        item.SetIsChecked(!item.GetIsChecked());
    }
    RoutedEventArgs event;
    if (auto* events = AeroGuiInternal::EventRouterOf(*this)) {
        Base::Result<void> raised =
            events->RaiseEvent(item, MenuItem::ClickEvent, &event);
        if (!raised) return raised.GetStatus();
    }
    ICommand* command = item.GetCommand();
    if (command != nullptr) {
        const Value parameter = item.GetCommandParameter();
        Aero::InputRouter* input = AeroGuiInternal::InputRouterOf(*this);
        if (input != nullptr) {
            Base::Result<bool> executed =
                input->Execute(*command, parameter, item);
            if (!executed) {
                return executed.GetStatus();
            }
        } else {
            command->Execute(parameter, &item);
        }
    }
    if (AeroGuiInternal::PropertyRegistry(*this).Types().
        IsDerivedFrom(
            RuntimeType(),
            ContextMenu::StaticTypeId())) {
        static_cast<void>(
            static_cast<ContextMenu&>(
                *this).SetIsOpen(false));
    }
    return {};
}

void Menu::OnMouseLeftButtonDown(MouseButtonEventArgs& args) {
    if (args.GetChangedButton() != MouseButton::Left) {
        return;
    }
    MenuItem* item = FindItem(args.GetOriginalSource());
    if (item == nullptr) return;
    AeroGuiInternal::SetMenuItemHighlighted(*item, true);
    Base::Result<void> invoked = Invoke(*item);
    if (!invoked) return;
    static_cast<void>(item->Focus());
    args.SetHandled(true);
}

void Menu::OnKeyDown(KeyEventArgs& args) {
    if (args.GetKey() != KeyboardKeyEnter &&
        args.GetKey() != KeyboardKeySpace &&
        args.GetKey() != KeyboardKeyRight &&
        args.GetKey() != KeyboardKeyLeft &&
        args.GetKey() != KeyboardKeyEscape) {
        return;
    }
    MenuItem* item = FindItem(args.GetOriginalSource());
    if (item == nullptr) return;
    if (args.GetKey() == KeyboardKeyEscape ||
        args.GetKey() == KeyboardKeyLeft) {
        static_cast<void>(
            item->SetIsSubmenuOpen(false));
    } else if (args.GetKey() == KeyboardKeyRight) {
        if (item->GetCount() != 0U) {
            static_cast<void>(
                item->SetIsSubmenuOpen(true));
        }
    } else {
        static_cast<void>(
            Invoke(*item));
    }
    args.SetHandled(true);
}

Base::Result<Base::Ref<FrameworkElement>>
Menu::GetContainerForItemOverride() const noexcept {
    Base::Result<Base::Ref<MenuItem>> made =
        Base::MakeRef<MenuItem>();
    if (!made) return made.GetStatus();
    return Base::Ref<FrameworkElement>(
        std::move(made).Value());
}

ContextMenu::ContextMenu() noexcept
    : Menu(StaticTypeId()) {}

ContextMenu::~ContextMenu() = default;

bool ContextMenu::GetIsOpen() const noexcept {
    return GetValue(IsOpenProperty);
}

void ContextMenu::SetIsOpen(
    bool value) noexcept {
    SetCurrentValue(
        IsOpenProperty, value);
}

Base::Ref<UIElement>
ContextMenu::GetPlacementTarget() const noexcept {
    return GetValue(PlacementTargetProperty);
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

void ContextMenu::OnOpened(RoutedEventArgs& e) {
    static_cast<void>(RaiseEvent(OpenedEvent, &e));
}

void ContextMenu::OnClosed(RoutedEventArgs& e) {
    static_cast<void>(RaiseEvent(ClosedEvent, &e));
}

void ContextMenu::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    Menu::OnPropertyChanged(args);
    if (args.GetProperty() == IsOpenProperty) {
        const bool opened = args.GetNewValue().AsBoolean();
        static_cast<void>(SetVisibility(
            opened ? Visibility::Visible : Visibility::Collapsed));
        RoutedEventArgs event;
        if (opened) {
            OnOpened(event);
        } else {
            OnClosed(event);
        }
    }
}

Base::Ref<ContextMenu>
ContextMenuService::GetContextMenu(
    const DependencyObject& target) noexcept {
    return target.GetValue(ContextMenuProperty);
}

void
ContextMenuService::SetContextMenu(
    DependencyObject& target,
    Base::Ref<ContextMenu> value) noexcept {
    target.SetValue(
        ContextMenuProperty,
        std::move(value));
}

void Menu::RegisterMetadata(::Aero::Meta::Registration& context) noexcept {
    using namespace Aero::Meta;
    Register<Menu>(context)
        .Factory();
}

void MenuItem::RegisterMetadata(::Aero::Meta::Registration& context) noexcept {
    using namespace Aero::Meta;
    Register<MenuItem>(context)
        .Event(MenuItem::ClickEvent)
        .Property(MenuItem::InputGestureTextProperty, Base::String{}, AffectsMeasure)
        .Property(MenuItem::IsCheckableProperty, false, AffectsMeasure)
        .Property(MenuItem::IsCheckedProperty, false, AffectsRender | BindsTwoWayByDefault)
        .Property(MenuItem::IsHighlightedProperty, false, AffectsRender)
        .Property(MenuItem::IsSubmenuOpenProperty, false, AffectsMeasure | AffectsRender | BindsTwoWayByDefault)
        .Property(MenuItem::RoleProperty, MenuItemRole::TopLevelItem, AffectsMeasure | AffectsRender)
        .Property(MenuItem::CommandProperty, Base::Ref<ICommand>{})
        .Property(MenuItem::CommandParameterProperty, Value::NullObject(TypeOf<Base::Object>()))
        .Property(MenuItem::IconProperty, Value::NullObject(TypeOf<Base::Object>()), AffectsMeasure)
        .Factory();
}

void ContextMenu::RegisterMetadata(::Aero::Meta::Registration& context) noexcept {
    using namespace Aero::Meta;
    Register<ContextMenu>(context)
        .Event(ContextMenu::OpenedEvent)
        .Event(ContextMenu::ClosedEvent)
        .Property(ContextMenu::IsOpenProperty, false, AffectsMeasure | AffectsRender | BindsTwoWayByDefault)
        .Property(ContextMenu::PlacementTargetProperty, Base::Ref<UIElement>{})
        .Factory();
}

void ContextMenuService::RegisterMetadata(::Aero::Meta::Registration& context) noexcept {
    using namespace Aero::Meta;
    Register<ContextMenuService>(context, TypeFlags::Abstract)
        .Property(ContextMenuService::ContextMenuProperty, Base::Ref<ContextMenu>{});
}

void Separator::RegisterMetadata(::Aero::Meta::Registration& context) noexcept {
    using namespace Aero::Meta;
    Register<Separator>(context)
        .Factory();
}

} // namespace Aero::Controls

