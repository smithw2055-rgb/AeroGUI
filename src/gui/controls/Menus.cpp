#include "gui/core/ElementTree.hpp"
#include "gui/core/Describe.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/input/InputManager.hpp" 
#include <Aero/Controls.hpp>
#include <Aero/Controls/Panels.hpp>
#include <Aero/ClassHandler.hpp>

#include <utility>
#include "gui/core/DependencyObjectAccess.hpp"

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
    UpdateRole();
    DependencyObject* gesture =
        GetTemplateChild("GestureText");
    gestureText_ =
        gesture != nullptr &&
        DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
            gesture->RuntimeType(),
            TextBlock::StaticTypeId())
        ? static_cast<TextBlock*>(gesture)
        : nullptr;
    DependencyObject* check =
        GetTemplateChild("CheckGlyph");
    checkGlyph_ =
        check != nullptr &&
        DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
            check->RuntimeType(),
            TextBlock::StaticTypeId())
        ? static_cast<TextBlock*>(check)
        : nullptr;
    DependencyObject* submenu =
        GetTemplateChild("SubmenuPopup");
    submenuPopup_ =
        submenu != nullptr &&
        DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
            submenu->RuntimeType(),
            Popup::StaticTypeId())
        ? static_cast<Popup*>(submenu)
        : nullptr;
    SynchronizeMenuTemplate();
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

void
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
}

static ItemsControl* GetParentItemsControl(MenuItem& item) noexcept {
    Media::Visual* curr = item.GetVisualParent();
    if (curr == nullptr) {
        curr = ::Aero::TryCast<Media::Visual>(item.GetLogicalParent());
    }
    while (curr != nullptr) {
        if (ItemsControl* ctrl = ::Aero::TryCast<ItemsControl>(curr)) {
            return ctrl;
        }
        curr = curr->GetVisualParent() != nullptr
            ? curr->GetVisualParent()
            : ::Aero::TryCast<Media::Visual>(curr->GetLogicalParent());
    }
    return nullptr;
}

static MenuItem* GetMenuItemAt(ItemsControl& host, std::uint32_t i) noexcept {
    Base::Ref<Base::Object> obj = host.GetItem(i);
    if (MenuItem* item = ::Aero::TryCast<MenuItem>(obj.Get())) {
        return item;
    }
    if (ItemContainerGenerator* gen = host.GetItemContainerGenerator()) {
        return ::Aero::TryCast<MenuItem>(gen->ContainerFromIndex(i));
    }
    return nullptr;
}

static bool HasAnySubmenuOpen(ItemsControl& host) noexcept {
    const std::uint32_t count = host.GetCount();
    for (std::uint32_t i = 0; i < count; ++i) {
        if (MenuItem* item = GetMenuItemAt(host, i)) {
            if (item->GetIsSubmenuOpen()) {
                return true;
            }
        }
    }
    return false;
}

static void CloseAllSubmenus(ItemsControl& host) noexcept {
    const std::uint32_t count = host.GetCount();
    for (std::uint32_t i = 0; i < count; ++i) {
        if (MenuItem* item = GetMenuItemAt(host, i)) {
            item->SetIsSubmenuOpen(false);
            item->SetHighlightedState(false);
            CloseAllSubmenus(*item);
        }
    }
}

static void CloseSiblingSubmenus(MenuItem& item) noexcept {
    ItemsControl* parent = GetParentItemsControl(item);
    if (parent == nullptr) return;
    const std::uint32_t count = parent->GetCount();
    for (std::uint32_t i = 0; i < count; ++i) {
        if (MenuItem* sibling = GetMenuItemAt(*parent, i)) {
            if (sibling != &item) {
                sibling->SetIsSubmenuOpen(false);
                sibling->SetHighlightedState(false);
                CloseAllSubmenus(*sibling);
            }
        }
    }
}

void MenuItem::DismissMenuTree(MenuItem& item) noexcept {
    item.SetIsSubmenuOpen(false);
    item.SetHighlightedState(false);
    CloseAllSubmenus(item);

    ItemsControl* curr = GetParentItemsControl(item);
    ItemsControl* root = nullptr;
    while (curr != nullptr) {
        root = curr;
        if (auto* mi = ::Aero::TryCast<MenuItem>(curr)) {
            mi->SetIsSubmenuOpen(false);
            mi->SetHighlightedState(false);
            curr = GetParentItemsControl(*mi);
        } else {
            break;
        }
    }
    if (root != nullptr) {
        CloseAllSubmenus(*root);
    }
}

void MenuItem::UpdateRole() noexcept {
    const bool hasItems = GetCount() != 0U;
    ItemsControl* parent = GetParentItemsControl(*this);
    const bool isTopLevel = parent != nullptr && ::Aero::TryCast<Menu>(parent) != nullptr;
    if (isTopLevel) {
        SetRoleState(hasItems ? MenuItemRole::TopLevelHeader : MenuItemRole::TopLevelItem);
    } else {
        SetRoleState(hasItems ? MenuItemRole::SubmenuHeader : MenuItemRole::SubmenuItem);
    }
}

Size MenuItem::MeasureOverride(Size availableSize) noexcept {
    UpdateRole();
    return HeaderedItemsControl::MeasureOverride(availableSize);
}

void MenuItem::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    HeaderedItemsControl::OnPropertyChanged(args);
    const DependencyPropertyHandle prop = args.GetProperty();
    if (prop == InputGestureTextProperty ||
        prop == IsCheckableProperty ||
        prop == IsCheckedProperty ||
        prop == IsSubmenuOpenProperty) {
        SynchronizeMenuTemplate();
        if (prop == IsSubmenuOpenProperty && !args.GetNewValue().AsBoolean()) {
            CloseAllSubmenus(*this);
            if (!GetIsMouseOver()) {
                SetHighlightedState(false);
            }
        }
    }
}

void MenuItem::OnMouseEnter(MouseEventArgs&) {
    SetHighlightedState(true);
    ItemsControl* parent = GetParentItemsControl(*this);
    UpdateRole();
    const MenuItemRole role = GetRole();
    if (role == MenuItemRole::SubmenuHeader) {
        CloseSiblingSubmenus(*this);
        SetIsSubmenuOpen(true);
    } else if (role == MenuItemRole::SubmenuItem) {
        CloseSiblingSubmenus(*this);
    } else if (role == MenuItemRole::TopLevelHeader) {
        if (parent != nullptr && HasAnySubmenuOpen(*parent)) {
            CloseSiblingSubmenus(*this);
            SetIsSubmenuOpen(true);
        }
    } else if (role == MenuItemRole::TopLevelItem) {
        if (parent != nullptr && HasAnySubmenuOpen(*parent)) {
            CloseSiblingSubmenus(*this);
        }
    }
}

void MenuItem::OnMouseLeave(MouseEventArgs&) {
    if (!GetIsSubmenuOpen()) {
        SetHighlightedState(false);
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

MenuBase::MenuBase(TypeId runtimeType) noexcept
    : ItemsControl(runtimeType) {}

Menu::Menu() noexcept
    : MenuBase(StaticTypeId()) {}

Menu::~Menu() = default;

MenuItem* MenuBase::FindItem(Base::Object* source) const noexcept {
    if (source == nullptr ||
        !DependencyObjectAccess::PropertyRegistry((*this)).Types().
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
            DependencyObjectAccess::PropertyRegistry((*this)).Types().
                IsDerivedFrom(
                    element->RuntimeType(),
                    MenuItem::StaticTypeId())) {
            return static_cast<MenuItem*>(element);
        }
        visual = visual->GetVisualParent();
    }
    return nullptr;
}

void MenuBase::Invoke(MenuItem& item) noexcept {
    if (item.GetCount() != 0U) {
        const bool willOpen = !item.GetIsSubmenuOpen();
        if (willOpen) {
            CloseSiblingSubmenus(item);
        }
        item.SetIsSubmenuOpen(willOpen);
        return;
    }
    if (item.GetIsCheckable()) {
        item.SetIsChecked(!item.GetIsChecked());
    }
    RoutedEventArgs event;
    if (auto* events = ElementTree::EventsOf(*this)) {
        static_cast<void>(events->RaiseEvent(item, MenuItem::ClickEvent, &event));
    }
    ICommand* command = item.GetCommand();
    if (command != nullptr) {
        const Value parameter = item.GetCommandParameter();
        Aero::InputRouter* input = ElementTree::InputOf(*this);
        if (input != nullptr) {
            static_cast<void>(input->Execute(*command, parameter, item));
        } else {
            command->Execute(parameter, &item);
        }
    }
    if (DependencyObjectAccess::PropertyRegistry((*this)).Types().
        IsDerivedFrom(
            RuntimeType(),
            ContextMenu::StaticTypeId())) {
        static_cast<void>(
            static_cast<ContextMenu&>(
                *this).SetIsOpen(false));
    } else {
        CloseAllSubmenus(*this);
    }
}

void MenuBase::OnMouseLeftButtonDown(MouseButtonEventArgs& args) {
    if (args.GetChangedButton() != MouseButton::Left) {
        return;
    }
    MenuItem* item = FindItem(args.GetOriginalSource());
    if (item == nullptr) return;
    (*item).SetHighlightedState(true);
    Invoke(*item);
    static_cast<void>(item->Focus());
    args.SetHandled(true);
}

void MenuBase::OnKeyDown(KeyEventArgs& args) {
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
        Invoke(*item);
    }
    args.SetHandled(true);
}

Base::Ref<FrameworkElement>
MenuBase::GetContainerForItemOverride() const noexcept {
    return Base::MakeRef<MenuItem>();
}

ContextMenu::ContextMenu() noexcept
    : MenuBase(StaticTypeId()) {
    SetOverlayHostFlag(true);
}

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
    ItemsControl::OnApplyTemplate();
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
    ItemsControl::OnPropertyChanged(args);
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

AERO_DESCRIBE(MenuBase) {
    using namespace Aero::Meta;
    Register<MenuBase>(context, TypeFlags::Abstract);
    AERO_ON(MenuBase, &MenuBase::OnMouseLeftButtonDown, UIElement::MouseLeftButtonDownEvent);
    AERO_ON(MenuBase, &MenuBase::OnKeyDown, UIElement::KeyDownEvent);
}

AERO_DESCRIBE(Menu) {
    using namespace Aero::Meta;
    Register<Menu>(context)
        .Factory();
}

AERO_DESCRIBE(MenuItem) {
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
    AERO_ON(MenuItem, &MenuItem::OnMouseEnter, UIElement::MouseEnterEvent);
    AERO_ON(MenuItem, &MenuItem::OnMouseLeave, UIElement::MouseLeaveEvent);
}

AERO_DESCRIBE(ContextMenu) {
    using namespace Aero::Meta;
    Register<ContextMenu>(context)
        .Event(ContextMenu::OpenedEvent)
        .Event(ContextMenu::ClosedEvent)
        .Property(ContextMenu::IsOpenProperty, false, AffectsMeasure | AffectsRender | BindsTwoWayByDefault)
        .Property(ContextMenu::PlacementTargetProperty, Base::Ref<UIElement>{})
        .Factory();
}

AERO_DESCRIBE(ContextMenuService) {
    using namespace Aero::Meta;
    Register<ContextMenuService>(context, TypeFlags::Abstract)
        .Property(ContextMenuService::ContextMenuProperty, Base::Ref<ContextMenu>{});
}



AERO_DESCRIBE(Separator) {
    using namespace Aero::Meta;
    Register<Separator>(context)
        .Factory();
}

Size ToolBarPanel::MeasureOverride(Size availableSize) noexcept {
    Size desired{};
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        MeasureChild(
            *child, {1.0e12, availableSize.height});
        const Size childSize = child->GetDesiredSize();
        desired.width += childSize.width;
        desired.height = std::max(desired.height, childSize.height);
    }
    return desired;
}

Size ToolBarPanel::ArrangeOverride(Size finalSize) noexcept {
    double offset = 0.0;
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        const Size desired = child->GetDesiredSize();
        ArrangeChild(
            *child, {offset, 0.0, desired.width, finalSize.height});
        offset += desired.width;
    }
    return finalSize;
}

Size ToolBarOverflowPanel::MeasureOverride(Size availableSize) noexcept {
    Size desired{};
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        MeasureChild(
            *child, {availableSize.width, 1.0e12});
        const Size childSize = child->GetDesiredSize();
        desired.width = std::max(desired.width, childSize.width);
        desired.height += childSize.height;
    }
    return desired;
}

Size ToolBarOverflowPanel::ArrangeOverride(Size finalSize) noexcept {
    double offset = 0.0;
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        const Size desired = child->GetDesiredSize();
        ArrangeChild(
            *child, {0.0, offset, finalSize.width, desired.height});
        offset += desired.height;
    }
    return finalSize;
}

ToolBar::ToolBar() noexcept
    : ItemsControl(StaticTypeId()) {}

ToolBar::~ToolBar() = default;

Meta::Value ToolBar::GetHeader()
    const noexcept {
    return GetValue(HeaderProperty);
}

void ToolBar::SetHeader(
    const Meta::Value& value) noexcept {
    SetValue(HeaderProperty, value);
}

void ToolBar::SetHeader(
    Base::StringView value) noexcept {
    Base::Result<Value> boxed = Value::TryFromString(
        Meta::TypeOf<Base::String>(), value);
    if (!boxed) { AERO_ASSERT(false); return; }
    SetHeader(std::move(boxed).Value());
}

Base::Ref<DataTemplate>
ToolBar::GetHeaderTemplate() const noexcept {
    return GetValue(HeaderTemplateProperty);
}

void ToolBar::SetHeaderTemplate(
    Base::Ref<DataTemplate> value) noexcept {
    SetValue(
        HeaderTemplateProperty, std::move(value));
}

Orientation ToolBar::GetOrientation()
    const noexcept {
    return GetValue(OrientationProperty);
}

void ToolBar::SetOrientation(
    Orientation value) noexcept {
    SetValue(OrientationProperty, value);
}

std::uint32_t ToolBar::GetOverflowCapacity()
    const noexcept {
    return GetValue(OverflowCapacityProperty);
}

void ToolBar::SetOverflowCapacity(
    std::uint32_t value) noexcept {
    SetValue(OverflowCapacityProperty, value);
}

bool ToolBar::GetIsOverflowOpen()
    const noexcept {
    return GetValue(IsOverflowOpenProperty);
}

void ToolBar::SetIsOverflowOpen(
    bool value) noexcept {
    SetValue(IsOverflowOpenProperty, value);
}

bool ToolBar::GetHasOverflowItems()
    const noexcept {
    return GetValue(HasOverflowItemsProperty);
}

std::uint32_t ToolBar::GetOverflowItemCount()
    const noexcept {
    return GetValue(OverflowItemCountProperty);
}

void ToolBar::OnContainersChanged() noexcept {
    ItemsControl::OnContainersChanged();
    SynchronizeToolBar();
}

void ToolBar::OnApplyTemplate() noexcept {
    ItemsControl::OnApplyTemplate();
    headerText_ = TryCast<TextBlock>(
        GetTemplateChild(Base::StringView("PART_Header")));
    overflowGlyph_ = TryCast<TextBlock>(
        GetTemplateChild(Base::StringView("PART_OverflowGlyph")));
    SynchronizeToolBar();
}

void ToolBar::OnTemplateDetached() noexcept {
    headerText_ = nullptr;
    overflowGlyph_ = nullptr;
    ItemsControl::OnTemplateDetached();
}

void ToolBar::SynchronizeToolBar() noexcept {
    if (headerText_ != nullptr) {
        const Meta::Value headerValue = GetHeader();
        headerText_->SetText(
            headerValue.Kind() == Meta::ValueKind::String
            ? headerValue.AsString()
            : Base::StringView{});
    }
    Panel* host = GetItemsHost();
    if (host != nullptr &&
        DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
            host->RuntimeType(),
            StackPanel::StaticTypeId())) {
        static_cast<StackPanel*>(host)->
            SetOrientation(GetOrientation());
    }
    const std::uint32_t capacity =
        GetOverflowCapacity();
    const std::uint32_t count =
        GetCount();
    const std::uint32_t overflowCount =
        count > capacity
        ? count - capacity
        : 0U;
    SetValue(
        HasOverflowItemsProperty,
        overflowCount != 0U);
    SetValue(
        OverflowItemCountProperty,
        overflowCount);
    if (host != nullptr) {
        const auto children = (*host).RenderChildren();
        for (std::uint32_t index = 0U;
             index < children.Size();
             ++index) {
            UIElement* child =
                children[index] != nullptr
                ? ::Aero::TryCast<::Aero::UIElement>(children[index])
                : nullptr;
            if (child == nullptr) continue;
            child->SetVisibility(
                index < capacity
                ? Visibility::Visible
                : Visibility::Collapsed);
        }
    }
    if (overflowGlyph_ != nullptr) {
        overflowGlyph_->SetText(
            overflowCount != 0U
            ? Base::StringView("...")
            : Base::StringView(""));
    }
}

void ToolBar::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    ItemsControl::OnPropertyChanged(args);
    if (args.GetProperty() == HeaderProperty.Handle() ||
        args.GetProperty() == OverflowCapacityProperty.Handle() ||
        args.GetProperty() == IsOverflowOpenProperty.Handle()) {
        SynchronizeToolBar();
    }
}

Base::Ref<FrameworkElement>
StatusBar::GetContainerForItemOverride() const noexcept {
    return Base::MakeRef<StatusBarItem>();
}

AERO_DESCRIBE(ToolBar) {
    using namespace Aero::Meta;
    Register<ToolBar>(context)
        .Property(ToolBar::HeaderProperty, Value::NullObject(TypeOf<Base::Object>()), AffectsMeasure)
        .Property(ToolBar::HeaderTemplateProperty, Base::Ref<DataTemplate>{}, AffectsMeasure)
        .Property(ToolBar::OrientationProperty, Orientation::Horizontal, AffectsMeasure)
        .Property(ToolBar::OverflowCapacityProperty, std::uint32_t{4U})
        .Property(ToolBar::IsOverflowOpenProperty, false)
        .Property(ToolBar::HasOverflowItemsProperty, false)
        .Property(ToolBar::OverflowItemCountProperty, std::uint32_t{0U})
        .Factory();
}

AERO_DESCRIBE(ToolBarTray) {
    using namespace Aero::Meta;
    Register<ToolBarTray>(context, TypeFlags::Abstract)
        .Property(ToolBarTray::IsLockedProperty, false);
}

AERO_DESCRIBE(StatusBar) {
    using namespace Aero::Meta;
    Register<StatusBar>(context)
        .Property(StatusBar::IsSizingGripVisibleProperty, true, AffectsMeasure)
        .Factory();
}

AERO_DESCRIBE(ToolBarPanel) {
    using namespace Aero::Meta;
    Register<ToolBarPanel>(context)
        .Factory();
}

AERO_DESCRIBE(ToolBarOverflowPanel) {
    using namespace Aero::Meta;
    Register<ToolBarOverflowPanel>(context)
        .Factory();
}

AERO_DESCRIBE(StatusBarItem) {
    using namespace Aero::Meta;
    Register<StatusBarItem>(context)
        .Factory();
}

} // namespace Aero::Controls

