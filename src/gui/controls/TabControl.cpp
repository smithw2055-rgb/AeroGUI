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
#include "gui/core/TypeRegistryDetail.hpp"
#include "gui/core/ValueConversion.hpp"
#include "ControlsMetadata.hpp"
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

TabControl::TabControl() noexcept
    : Selector(StaticTypeId()) {}

TabControl::~TabControl() = default;

TabItem* TabControl::GetSelectedTab() const noexcept {
    const std::uint32_t selected = GetSelectedIndex();
    if (selected == UINT32_MAX) return nullptr;
    const Ref<Base::Object> item = GetItem(selected);
    if (item &&
        (*this).PropertyRegistry().Types().IsDerivedFrom(
            item->RuntimeType(), TabItem::StaticTypeId())) {
        return static_cast<TabItem*>(item.Get());
    }
    ItemContainerGenerator* generator = AttachedGenerator();
    if (generator == nullptr) return nullptr;
    FrameworkElement* container = generator->ContainerFromIndex(selected);
    if (container != nullptr &&
        (*this).PropertyRegistry().Types().IsDerivedFrom(
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

void TabControl::SynchronizeSelection() noexcept {
    const std::uint32_t value = GetSelectedIndex();
    const std::uint32_t count = GetCount();
    ItemContainerGenerator* generator = AttachedGenerator();
    for (std::uint32_t index = 0U; index < count; ++index) {
        TabItem* tab = nullptr;
        const Ref<Base::Object> item = GetItem(index);
        if (item &&
            (*this).PropertyRegistry().Types().IsDerivedFrom(
                item->RuntimeType(), TabItem::StaticTypeId())) {
            tab = static_cast<TabItem*>(item.Get());
        } else if (generator != nullptr) {
            FrameworkElement* container = generator->ContainerFromIndex(index);
            if (container != nullptr &&
                (*this).PropertyRegistry().Types().IsDerivedFrom(
                    container->RuntimeType(), TabItem::StaticTypeId())) {
                tab = static_cast<TabItem*>(container);
            }
        }
        if (tab != nullptr) {
            tab->SetIsSelected(index == value);
        }
    }
    TabItem* selected = GetSelectedTab();
    const Meta::Value selectedContent =
        selected != nullptr
        ? selected->GetContent()
        : Meta::Value::NullObject(
              Meta::TypeOf<Base::Object>());
    SetReadOnlyCurrentValue(SelectedContentProperty, selectedContent);
    InvalidateMeasure();
}

void TabControl::OnSelectionChanged(
    const SelectionChangedEvent& event) {
    Selector::OnSelectionChanged(event);
    SynchronizeSelection();
}

void TabControl::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    Selector::OnPropertyChanged(args);
    if (args.GetProperty() == SelectedIndexProperty) {
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
    Base::Result<void> measured =
        MeasureChild(
            *selected,
            verticalStrip
                ? Size{std::max(0.0, availableSize.width - HeaderExtent),
                    availableSize.height}
                : Size{availableSize.width,
                    std::max(0.0, availableSize.height - HeaderExtent)});
    if (!measured) return Size{};
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
            (*this).PropertyRegistry().Types().IsDerivedFrom(
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
        Base::Result<void> arranged =
            ArrangeChild(*tab, slot);
        if (!arranged) return finalSize;
    }
    return finalSize;
}

bool TabPanel::GetIsVertical() const noexcept {
    const DependencyObject* parent = GetTemplatedParent();
    return parent != nullptr &&
        (*this).PropertyRegistry().Types().IsDerivedFrom(
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
        Base::Result<void> measured = MeasureChild(*child, availableSize);
        if (!measured) return Size{};
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
        Base::Result<void> arranged = ArrangeChild(*child, {
            x, y, size.width, size.height});
        if (!arranged) return finalSize;
        if (vertical) y += size.height;
        else x += size.width;
        lineCross = std::max(lineCross, cross);
    }
    return finalSize;
}

AERO_DESCRIBE(TabItem) {
    using namespace Aero::Meta;
    Register<TabItem>(context)
        .Property(TabItem::IsSelectedProperty, false, AffectsRender)
        .Factory();
}

AERO_DESCRIBE(TabControl) {
    using namespace Aero::Meta;
    Register<TabControl>(context)
        .Property(TabControl::SelectedContentProperty, Meta::Value::NullObject(Meta::TypeOf<Base::Object>()))
        .Property(TabControl::ContentTemplateProperty, Base::Ref<DataTemplate>{}, AffectsMeasure)
        .Property(TabControl::TabStripPlacementProperty, Dock::Top, AffectsMeasure)
        .Factory();
}

} // namespace Aero::Controls
