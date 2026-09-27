#include "gui/core/TypeRegistryDetail.hpp"
#include "gui/core/Describe.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/input/InputManager.hpp" 
#include "gui/controls/ItemsContainers.hpp"
#include "gui/templates/TemplateInstance.hpp"
#include <Aero/VisualStateManager.hpp>
#include <Aero/Controls.hpp>
#include <Aero/ClassHandler.hpp>
#include <Aero/TryCast.hpp>
#include <Aero/Controls/ControlTemplate.hpp>
#include <Aero/Controls/TextBoxBase.hpp>
#include <Aero/Controls/TextBox.hpp>
#include <Aero/Controls/Popup.hpp>
#include <Aero/Data/CollectionView.hpp>


#include <Aero/Base/String.hpp>
#include "gui/core/ValueConversion.hpp"
#include <algorithm>
#include <utility>
#include "gui/core/DependencyObjectAccess.hpp"

namespace Aero::Controls {

using namespace Primitives;

ListBoxItem::ListBoxItem() noexcept
    : ListBoxItem(StaticTypeId()) {}

ListBoxItem::ListBoxItem(TypeId runtimeType) noexcept
    : ContentControl(runtimeType) {}

ListBoxItem::~ListBoxItem() = default;

bool ListBoxItem::GetIsSelected() const noexcept {
    return GetValue(IsSelectedProperty);
}

void ListBoxItem::SetIsSelected(
    bool value) noexcept {
    SetCurrentValue(IsSelectedProperty, value);
}

void ListBoxItem::UpdateVisualState(bool useTransitions) noexcept {
    Base::StringView common = "Normal";
    if (!GetIsEnabled()) {
        common = "Disabled";
    } else if (GetIsMouseOver()) {
        common = "MouseOver";
    }
    static_cast<void>(
        VisualStateManager::GoToState(
            *this,
            common,
            useTransitions));
    const bool selected = GetIsSelected();
    static_cast<void>(
        VisualStateManager::GoToState(
            *this,
            selected
                ? Base::StringView("Selected")
                : Base::StringView("Unselected"),
            useTransitions));
}

void ListBoxItem::OnSelected(RoutedEventArgs& e) {
    RaiseEvent(SelectedEvent, &e);
}

void ListBoxItem::OnUnselected(RoutedEventArgs& e) {
    RaiseEvent(UnselectedEvent, &e);
}

void ListBoxItem::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    ContentControl::OnPropertyChanged(args);
    const DependencyPropertyHandle prop = args.GetProperty();
    if (prop == IsSelectedProperty) {
        UpdateVisualState(true);
        const bool selected =
            args.GetNewValue().Kind() == Meta::ValueKind::Boolean &&
            args.GetNewValue().AsBoolean();
        RoutedEventArgs eventArgs;
        if (selected) {
            OnSelected(eventArgs);
        } else {
            OnUnselected(eventArgs);
        }
        if (!selected) return;
        ::Aero::Media::Visual* visual = this;
        while (visual != nullptr) {
            UIElement* element = ::Aero::TryCast<UIElement>(visual);
            if (element != nullptr &&
                DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
                    element->RuntimeType(), ListBox::StaticTypeId())) {
                auto& listBox = *static_cast<ListBox*>(element);
                ItemContainerGenerator* generator =
                    listBox.GetItemContainerGenerator();
                if (generator == nullptr) return;
                const std::uint32_t index =
                    generator->IndexFromContainer(*this);
                if (index != UINT32_MAX &&
                    listBox.GetSelectedIndex() != index) {
                    listBox.SetSelectedIndex(index);
                }
                return;
            }
            visual = visual->GetVisualParent();
        }
    } else if (prop == UIElement::IsMouseOverProperty) {
        UpdateVisualState(true);
        if (GetIsMouseOver() && GetIsEnabled()) {
            ::Aero::Media::Visual* visual = this;
            while (visual != nullptr) {
                UIElement* element = ::Aero::TryCast<UIElement>(visual);
                if (element != nullptr &&
                    DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
                        element->RuntimeType(), ListBox::StaticTypeId())) {
                    auto& listBox = *static_cast<ListBox*>(element);
                    if (listBox.GetSelectionMode() == SelectionMode::Single) {
                        ItemContainerGenerator* generator =
                            listBox.GetItemContainerGenerator();
                        if (generator != nullptr) {
                            const std::uint32_t index =
                                generator->IndexFromContainer(*this);
                            if (index != UINT32_MAX &&
                                listBox.GetSelectedIndex() != index) {
                                listBox.SetSelectedIndex(index);
                            }
                        }
                    }
                    break;
                }
                visual = visual->GetVisualParent();
            }
        }
    } else if (prop == UIElement::IsEnabledProperty) {
        UpdateVisualState(true);
    }
}


ListBox::ListBox() noexcept
    : ListBox(StaticTypeId()) {}

ListBox::ListBox(TypeId runtimeType) noexcept
    : Selector(runtimeType) {
    (*this).SyncContainers();
}

ListBox::~ListBox() = default;

std::uint32_t ListBox::FindContainerIndex(Base::Object* source) const noexcept {
    if (source == nullptr ||
        !DependencyObjectAccess::PropertyRegistry((*this)).Types()
            .IsDerivedFrom(
                source->RuntimeType(),
                UIElement::StaticTypeId())) {
        return UINT32_MAX;
    }
    ::Aero::Media::Visual* visual =
        static_cast<UIElement*>(source);
    while (visual != nullptr &&
        visual != this) {
        UIElement* element =
            ::Aero::TryCast<::Aero::UIElement>(visual);
        if (element != nullptr &&
            DependencyObjectAccess::PropertyRegistry((*this)).Types()
                .IsDerivedFrom(
                    element->RuntimeType(),
                    ListBoxItem::StaticTypeId())) {
            ItemContainerGenerator* generator =
                GetItemContainerGenerator();
            return generator != nullptr
                ? generator->IndexFromContainer(
                    static_cast<ListBoxItem&>(
                        *element))
                : UINT32_MAX;
        }
        visual = visual->GetVisualParent();
    }
    return UINT32_MAX;
}

Base::Result<bool> ListBox::ApplyUserSelection(
    std::uint32_t index,
    std::uint32_t modifiers) noexcept {
    const SelectionMode mode = GetSelectionMode();
    if (mode == SelectionMode::Single) {
        anchorIndex_ = index;
        SetSelectedIndex(index);
        if (!LastSelectionError().IsOk()) {
            return LastSelectionError();
        }
        return true;
    }
    if (mode == SelectionMode::Multiple) {
        anchorIndex_ = index;
        const bool changed = Toggle(index);
        return LastSelectionError().IsOk()
            ? Base::Result<bool>(changed)
            : Base::Result<bool>(LastSelectionError());
    }
    const bool shift = HasKeyboardModifier(
        modifiers, KeyboardModifiers::Shift);
    const bool control = HasKeyboardModifier(
        modifiers, KeyboardModifiers::Control);
    if (shift) {
        if (anchorIndex_ == UINT32_MAX ||
            anchorIndex_ >= GetCount()) {
            anchorIndex_ =
                GetSelectedIndex() != UINT32_MAX
                ? GetSelectedIndex()
                : index;
        }
        const bool changed = SelectRange(
                anchorIndex_,
                index,
                control);
        return LastSelectionError().IsOk()
            ? Base::Result<bool>(changed)
            : Base::Result<bool>(LastSelectionError());
    }
    anchorIndex_ = index;
    if (control) {
        const bool changed = Toggle(index);
        return LastSelectionError().IsOk()
            ? Base::Result<bool>(changed)
            : Base::Result<bool>(LastSelectionError());
    }
    SetSelectedIndex(index);
    if (!LastSelectionError().IsOk()) {
        return LastSelectionError();
    }
    return true;
}

void ListBox::OnMouseLeftButtonDown(MouseButtonEventArgs& args) {
    if (args.GetChangedButton() != MouseButton::Left) {
        return;
    }
    if (!GetIsEnabled()) return;
    const std::uint32_t index = FindContainerIndex(args.GetOriginalSource());
    if (index == UINT32_MAX) return;
    Base::Result<bool> selected = ApplyUserSelection(index, args.GetModifiers());
    if (!selected) return;
    ItemContainerGenerator* generator = GetItemContainerGenerator();
    if (generator != nullptr) {
        FrameworkElement* container = generator->ContainerFromIndex(index);
        if (container != nullptr) {
            static_cast<void>(container->Focus());
        }
    }
    static_cast<void>(BringIntoView(index));
    args.SetHandled(true);
}

void ListBox::OnKeyDown(KeyEventArgs& args) {
    if (args.GetKey() != KeyboardKeyUp &&
        args.GetKey() != KeyboardKeyDown &&
        args.GetKey() != KeyboardKeyHome &&
        args.GetKey() != KeyboardKeyEnd) {
        return;
    }
    if (!GetIsEnabled() || GetCount() == 0U) {
        return;
    }
    std::uint32_t current = FindContainerIndex(args.GetOriginalSource());
    if (current == UINT32_MAX) {
        current =
            GetSelectedIndex() != UINT32_MAX
            ? GetSelectedIndex()
            : 0U;
    }
    std::uint32_t target = current;
    if (args.GetKey() == KeyboardKeyUp && target > 0U) {
        --target;
    } else if (args.GetKey() == KeyboardKeyDown && target + 1U < GetCount()) {
        ++target;
    } else if (args.GetKey() == KeyboardKeyHome) {
        target = 0U;
    } else if (args.GetKey() == KeyboardKeyEnd) {
        target = GetCount() - 1U;
    }
    const bool control = HasKeyboardModifier(
        args.GetModifiers(), KeyboardModifiers::Control);
    const bool shift = HasKeyboardModifier(
        args.GetModifiers(), KeyboardModifiers::Shift);
    if (!control || GetSelectionMode() != SelectionMode::Extended || shift) {
        Base::Result<bool> selected = ApplyUserSelection(target, args.GetModifiers());
        if (!selected) return;
    }
    ItemContainerGenerator* generator = GetItemContainerGenerator();
    if (generator != nullptr) {
        FrameworkElement* container = generator->ContainerFromIndex(target);
        if (container != nullptr) {
            static_cast<void>(container->Focus());
        }
    }
    static_cast<void>(BringIntoView(target));
    args.SetHandled(true);
}

Base::Result<Base::Ref<FrameworkElement>>
ListBox::GetContainerForItemOverride() const noexcept {
    Base::Result<Base::Ref<ListBoxItem>> made =
        Base::MakeRef<ListBoxItem>();
    if (!made) return made.GetStatus();
    return Base::Ref<FrameworkElement>(
        std::move(made).Value());
}

Base::Result<bool> ListBox::BringIntoView(
    std::uint32_t index) noexcept {
    ItemContainerGenerator* generator =
        AttachedGenerator();
    if (generator == nullptr) {
        return false;
    }
    FrameworkElement* container =
        generator->ContainerFromIndex(index);
    if (container == nullptr) return false;
    double x = 0.0;
    double y = 0.0;
    UIElement* node = container;
    ScrollViewer* viewer = nullptr;
    while (node != nullptr) {
        const Rect slot = node->GetLayoutSlot();
        x += slot.x;
        y += slot.y;
        ::Aero::Media::Visual* parent = node->GetVisualParent();
        if (parent == nullptr) break;
        UIElement* parentElement =
            ::Aero::TryCast<::Aero::UIElement>(parent);
        if (parentElement != nullptr &&
            DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
                parentElement->RuntimeType(),
                ScrollViewer::StaticTypeId())) {
            viewer =
                static_cast<ScrollViewer*>(
                    parentElement);
            break;
        }
        node = parentElement;
    }
    if (viewer == nullptr) return false;

    bool changed = false;
    const double width =
        container->GetRenderSize().width;
    const double height =
        container->GetRenderSize().height;
    double horizontal =
        viewer->GetHorizontalOffset();
    double vertical =
        viewer->GetVerticalOffset();
    if (x < 0.0) horizontal += x;
    else if (x + width >
        viewer->GetViewportWidth()) {
        horizontal += x + width -
            viewer->GetViewportWidth();
    }
    if (y < 0.0) vertical += y;
    else if (y + height >
        viewer->GetViewportHeight()) {
        vertical += y + height -
            viewer->GetViewportHeight();
    }
    const double oldHorizontal = viewer->GetHorizontalOffset();
    const double oldVertical = viewer->GetVerticalOffset();
    viewer->SetHorizontalOffset(std::max(0.0, horizontal));
    viewer->SetVerticalOffset(std::max(0.0, vertical));
    changed = oldHorizontal != viewer->GetHorizontalOffset();
    return changed || oldVertical != viewer->GetVerticalOffset();
}

AERO_DESCRIBE(ListBox) {
    using namespace Aero::Meta;
    Register<ListBox>(context)
        .Factory();
    AERO_ON(ListBox, &ListBox::OnMouseLeftButtonDown, UIElement::MouseLeftButtonDownEvent);
    AERO_ON(ListBox, &ListBox::OnKeyDown, UIElement::KeyDownEvent);
}

AERO_DESCRIBE(ListBoxItem) {
    using namespace Aero::Meta;
    Register<ListBoxItem>(context)
        .Property(ListBoxItem::IsSelectedProperty, false, AffectsRender | BindsTwoWayByDefault)
        .Override(Aero::UIElement::IsTabStopProperty, true, FrameworkPropertyMetadataOptions::None)
        .Factory();
}



} // namespace Aero::Controls

