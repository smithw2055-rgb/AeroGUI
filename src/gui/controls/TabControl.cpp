#include "gui/core/ElementTree.hpp"
#include "gui/core/Describe.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/media/BrushRendering.hpp"
#include "render/DisplayList.hpp"
#include <Aero/Controls.hpp>
#include <Aero/Controls/ItemContainerGenerator.hpp>
#include <Aero/Controls/Decorator.hpp>
#include <Aero/Controls/ControlTemplate.hpp>
#include <Aero/DataTemplate.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Media/Transform2D.hpp>
#include <Aero/Media/Transform3D.hpp>
#include <Aero/Shapes.hpp>
#include <Aero/Documents.hpp>
#include "gui/core/TypeRegistryCore.hpp"
#include "gui/core/ValueConversion.hpp"
#include "ControlPropertyValidators.hpp"
#include "gui/templates/TemplateInstance.hpp"
#include "gui/data/BindingEngine.hpp"
#include <Aero/TryCast.hpp>
#include <Aero/VisualTreeHelper.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <utility>
#include "gui/core/DependencyObjectAccess.hpp"

namespace Aero::Controls {

using namespace Primitives;

using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero::Render;


bool TabItem::GetIsSelected() const noexcept {
    return GetValue(IsSelectedProperty);
}

void TabItem::SetIsSelected(
    bool value) noexcept {
    SetValue(
        IsSelectedProperty, value);
}

TabControl* TabItem::FindParentTabControl() const noexcept {
    const DependencyObject* current = GetLogicalParent();
    if (current == nullptr) current = GetVisualParent();
    while (current != nullptr) {
        if (const auto* tc = TryCast<TabControl>(current)) {
            return const_cast<TabControl*>(tc);
        }
        if (const auto* fe = TryCast<FrameworkElement>(current)) {
            current = fe->GetLogicalParent() ? fe->GetLogicalParent() : fe->GetTemplatedParent();
        } else if (const auto* vis = TryCast<Media::Visual>(current)) {
            current = vis->GetVisualParent();
        } else {
            break;
        }
    }
    return nullptr;
}

void TabItem::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    HeaderedContentControl::OnPropertyChanged(args);
    if (args.GetProperty() == ContentProperty.Handle()) {
        if (GetIsSelected()) {
            if (auto* tc = FindParentTabControl()) {
                tc->SynchronizeSelection();
            }
        }
    } else if (args.GetProperty() == IsSelectedProperty.Handle()) {
        if (args.GetNewValue().Kind() == ValueKind::Boolean && args.GetNewValue().AsBoolean()) {
            if (auto* tc = FindParentTabControl()) {
                const std::uint32_t count = tc->GetCount();
                ItemContainerGenerator* gen = tc->GetItemContainerGenerator();
                for (std::uint32_t i = 0U; i < count; ++i) {
                    if (tc->GetItem(i).Get() == this ||
                        (gen != nullptr && gen->ContainerFromIndex(i) == this)) {
                        if (tc->GetSelectedIndex() != i) {
                            tc->SetSelectedIndex(i);
                        }
                        break;
                    }
                }
            }
        }
    }
}

void TabItem::OnMouseLeftButtonDown(MouseButtonEventArgs& args) {
    SetIsSelected(true);
    args.SetHandled(true);
}

TabControl::TabControl() noexcept
    : Selector(StaticTypeId()) {}

TabControl::~TabControl() = default;

TabItem* TabControl::GetSelectedTab() const noexcept {
    const std::uint32_t selected = GetSelectedIndex();
    if (selected == UINT32_MAX) return nullptr;
    const Ref<Base::Object> item = GetItem(selected);
    if (item &&
        DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
            item->RuntimeType(), TabItem::StaticTypeId())) {
        return static_cast<TabItem*>(item.Get());
    }
    ItemContainerGenerator* generator = AttachedGenerator();
    if (generator == nullptr) return nullptr;
    FrameworkElement* container = generator->ContainerFromIndex(selected);
    if (container != nullptr &&
        DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
            container->RuntimeType(), TabItem::StaticTypeId())) {
        return static_cast<TabItem*>(container);
    }
    return nullptr;
}

Base::Result<Ref<FrameworkElement>> TabControl::GetContainerForItemOverride() const noexcept {
    Base::Result<Ref<TabItem>> made = Base::MakeRef<TabItem>();
    if (!made) return made.GetStatus();
    return Ref<FrameworkElement>(std::move(made).Value());
}

Base::Result<void> TabControl::PrepareContainerForItemOverride(
    FrameworkElement& container,
    const Base::Ref<Base::Object>& item,
    std::uint32_t index) noexcept {
    Base::Result<void> prepared =
        Selector::PrepareContainerForItemOverride(container, item, index);
    if (!prepared) return prepared.GetStatus();

    if (DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
            container.RuntimeType(), TabItem::StaticTypeId())) {
        auto& tabItem = static_cast<TabItem&>(container);
        if (item && item.Get() != &container) {
            tabItem.SetHeader(Value::FromObject(item->RuntimeType(), item));
            if (const DataTemplate* tpl = GetItemTemplate()) {
                tabItem.SetHeaderTemplate(Ref<DataTemplate>::FromBorrowed(
                    const_cast<DataTemplate&>(*tpl)));
            }
            tabItem.SetContent(item.Get());
            tabItem.SetValue(ContentControl::ContentProperty,
                Value::FromObject(item->RuntimeType(), item));
        }
        tabItem.SetIsSelected(index == GetSelectedIndex());
    }
    return {};
}

void TabControl::OnApplyTemplate() noexcept {
    Selector::OnApplyTemplate();
    EnsureDefaultSelection();
}

void TabControl::SynchronizeSelection() noexcept {
    const std::uint32_t value = GetSelectedIndex();
    const std::uint32_t count = GetCount();
    ItemContainerGenerator* generator = AttachedGenerator();
    for (std::uint32_t index = 0U; index < count; ++index) {
        TabItem* tab = nullptr;
        const Ref<Base::Object> item = GetItem(index);
        if (item &&
            DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
                item->RuntimeType(), TabItem::StaticTypeId())) {
            tab = static_cast<TabItem*>(item.Get());
        } else if (generator != nullptr) {
            FrameworkElement* container = generator->ContainerFromIndex(index);
            if (container != nullptr &&
                DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
                    container->RuntimeType(), TabItem::StaticTypeId())) {
                tab = static_cast<TabItem*>(container);
            }
        }
        if (tab != nullptr) {
            tab->SetIsSelected(index == value);
        }
    }
    TabItem* selected = GetSelectedTab();
    Meta::Value selectedContent =
        selected != nullptr
        ? selected->GetContent()
        : Meta::Value::NullObject(
              Meta::TypeOf<Base::Object>());
    if ((selectedContent.Kind() == Meta::ValueKind::Unset ||
         selectedContent.IsNullObject()) &&
        value < count) {
        const Ref<Base::Object> item = GetItem(value);
        if (item && item.Get() != selected) {
            selectedContent = Value::FromObject(item->RuntimeType(), item);
        }
    }
    SetReadOnlyCurrentValue(SelectedContentProperty, selectedContent);
    std::fprintf(stderr, "[TabControl %p] Sync: selIdx=%u selectedTab=%p selectedContentKind=%d isNull=%d\n",
        (void*)this, value, (void*)selected, static_cast<int>(selectedContent.Kind()),
        selectedContent.IsNullObject());

    Ref<Base::Object> selectedContentTemplate;
    if (selected != nullptr && selected->GetContentTemplate()) {
        selectedContentTemplate = selected->GetContentTemplate();
    } else if (Ref<DataTemplate> ct = GetContentTemplate()) {
        selectedContentTemplate = std::move(ct);
    }
    SetReadOnlyCurrentValue(SelectedContentTemplateProperty, selectedContentTemplate);
    InvalidateMeasure();
}

void TabControl::OnSelectionChanged(
    const SelectionChangedEvent& event) {
    Selector::OnSelectionChanged(event);
    SynchronizeSelection();
}

void TabControl::EnsureDefaultSelection() noexcept {
    // WPF/Noesis parity: a TabControl with items but no selection
    // automatically selects the first tab. Without this, SelectedContent
    // stays null and the content host renders an empty grey box
    // (gallery TabControl sample).
    if (GetSelectedIndex() == UINT32_MAX && GetCount() > 0U) {
        SetSelectedIndex(0U);
    } else {
        SynchronizeSelection();
    }
}

void TabControl::OnItemsChanged(
    const ItemsChangedEvent& event) noexcept {
    Selector::OnItemsChanged(event);
    static_cast<void>(event);
    EnsureDefaultSelection();
}

void TabControl::OnContainersChanged() noexcept {
    Selector::OnContainersChanged();
    EnsureDefaultSelection();
}

void TabControl::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    Selector::OnPropertyChanged(args);
    if (args.GetProperty() == SelectedIndexProperty ||
        args.GetProperty() == ContentTemplateProperty) {
        SynchronizeSelection();
    }
}

Size TabControl::MeasureOverride(
    Size availableSize) noexcept {
    if (GetTemplateRoot() != nullptr) {
        return Control::MeasureOverride(availableSize);
    }
    constexpr double HeaderExtent = 28.0;
    const bool verticalStrip =
        GetTabStripPlacement() == Dock::Left ||
        GetTabStripPlacement() == Dock::Right;
    TabItem* selected = GetSelectedTab();
    if (selected == nullptr) {
        return verticalStrip
            ? Size{HeaderExtent, 0.0}
            : Size{0.0, HeaderExtent};
    }
    MeasureChild(
            *selected,
            verticalStrip
                ? Size{std::max(0.0, availableSize.width - HeaderExtent),
                    availableSize.height}
                : Size{availableSize.width,
                    std::max(0.0, availableSize.height - HeaderExtent)});
    const Size desired = selected->GetDesiredSize();
    return verticalStrip
        ? Size{desired.width + HeaderExtent, desired.height}
        : Size{desired.width, desired.height + HeaderExtent};
}

Size TabControl::ArrangeOverride(
    Size finalSize) noexcept {
    if (GetTemplateRoot() != nullptr) {
        return Control::ArrangeOverride(finalSize);
    }
    constexpr double HeaderExtent = 28.0;
    const Dock placement = GetTabStripPlacement();
    const bool verticalStrip =
        placement == Dock::Left || placement == Dock::Right;
    TabItem* selected = GetSelectedTab();
    const std::uint32_t count = GetCount();
    for (std::uint32_t index = 0U; index < count; ++index) {
        TabItem* tab = nullptr;
        const Ref<Base::Object> item = GetItem(index);
        if (item &&
            DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
                item->RuntimeType(), TabItem::StaticTypeId())) {
            tab = static_cast<TabItem*>(item.Get());
        }
        if (tab == nullptr) continue;
        Rect slot{};
        if (tab == selected) {
            if (verticalStrip) {
                slot = {placement == Dock::Left ? HeaderExtent : 0.0,
                    0.0, std::max(0.0, finalSize.width - HeaderExtent),
                    finalSize.height};
            } else {
                slot = {0.0, placement == Dock::Top ? HeaderExtent : 0.0,
                    finalSize.width,
                    std::max(0.0, finalSize.height - HeaderExtent)};
            }
        }
        ArrangeChild(*tab, slot);
    }
    return finalSize;
}

bool TabPanel::GetIsVertical() const noexcept {
    const DependencyObject* parent = GetTemplatedParent();
    return parent != nullptr &&
        DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
            parent->RuntimeType(), TabControl::StaticTypeId()) &&
        (static_cast<const TabControl*>(parent)->GetTabStripPlacement() ==
             Dock::Left ||
         static_cast<const TabControl*>(parent)->GetTabStripPlacement() ==
             Dock::Right);
}

Size TabPanel::MeasureOverride(
    Size availableSize) noexcept {
    const bool vertical = GetIsVertical();
    Size desired{};
    double linePrimary = 0.0;
    double lineCross = 0.0;
    const double limit = vertical
        ? availableSize.height
        : availableSize.width;
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        MeasureChild(*child, availableSize);
        const Size size = child->GetDesiredSize();
        const double primary = vertical ? size.height : size.width;
        const double cross = vertical ? size.width : size.height;
        if (linePrimary > 0.0 && limit < 1.0e11 &&
            linePrimary + primary > limit) {
            if (vertical) {
                desired.width += lineCross;
                desired.height = std::max(desired.height, linePrimary);
            } else {
                desired.width = std::max(desired.width, linePrimary);
                desired.height += lineCross;
            }
            linePrimary = 0.0;
            lineCross = 0.0;
        }
        linePrimary += primary;
        lineCross = std::max(lineCross, cross);
    }
    if (vertical) {
        desired.width += lineCross;
        desired.height = std::max(desired.height, linePrimary);
    } else {
        desired.width = std::max(desired.width, linePrimary);
        desired.height += lineCross;
    }
    return desired;
}

Size TabPanel::ArrangeOverride(
    Size finalSize) noexcept {
    const bool vertical = GetIsVertical();
    const double limit = vertical ? finalSize.height : finalSize.width;
    double x = 0.0;
    double y = 0.0;
    double lineCross = 0.0;
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        const Size size = child->GetDesiredSize();
        const double primary = vertical ? size.height : size.width;
        const double cross = vertical ? size.width : size.height;
        if ((vertical ? y : x) > 0.0 &&
            (vertical ? y : x) + primary > limit) {
            if (vertical) {
                x += lineCross;
                y = 0.0;
            } else {
                y += lineCross;
                x = 0.0;
            }
            lineCross = 0.0;
        }
        ArrangeChild(*child, {
            x, y, size.width, size.height});
        if (vertical) y += size.height;
        else x += size.width;
        lineCross = std::max(lineCross, cross);
    }
    return finalSize;
}

AERO_DESCRIBE(TabItem) {
    using namespace Aero::Meta;
    Register<TabItem>(context)
        .Property(TabItem::IsSelectedProperty, false, AffectsRender | BindsTwoWayByDefault)
        .Factory();
    AERO_ON(TabItem, &TabItem::OnMouseLeftButtonDown, UIElement::MouseLeftButtonDownEvent);
}

AERO_DESCRIBE(TabControl) {
    using namespace Aero::Meta;
    Register<TabControl>(context)
        .Property(TabControl::SelectedContentProperty, Meta::Value::NullObject(Meta::TypeOf<Base::Object>()))
        .Property(TabControl::SelectedContentTemplateProperty, Base::Ref<Base::Object>{})
        .Property(TabControl::ContentTemplateProperty, Base::Ref<DataTemplate>{}, AffectsMeasure)
        .Property(TabControl::TabStripPlacementProperty, Dock::Top, AffectsMeasure)
        .Factory();
}


AERO_DESCRIBE(TabPanel) {
    using namespace Aero::Meta;
    Register<TabPanel>(context)
        .Factory();
}

} // namespace Aero::Controls
