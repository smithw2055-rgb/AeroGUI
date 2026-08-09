#include "gui/metadata/MetadataRuntime.hpp"
#include "gui/property/PropertyRuntime.hpp"
#include "gui/base/FreezableRuntime.hpp"
#include "gui/base/ElementRuntime.hpp"
#include "gui/base/RoutedEventRuntime.hpp"
#include "gui/input/InputRuntime.hpp"
#include "gui/layout/LayoutRuntime.hpp"
#include "gui/binding/BindingRuntime.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/resources/StyleRuntime.hpp"
#include <Aero/Controls.hpp>
#include "gui/metadata/MetadataRuntime.hpp"
#include "gui/property/PropertyRuntime.hpp"
#include "gui/base/FreezableRuntime.hpp"
#include "gui/base/ElementRuntime.hpp"
#include "gui/base/RoutedEventRuntime.hpp"
#include "gui/input/InputRuntime.hpp"
#include "gui/layout/LayoutRuntime.hpp"
#include "gui/binding/BindingRuntime.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/resources/StyleRuntime.hpp"
#include "gui/controls/ControlRuntime.hpp"
#include "gui/controls/ItemsRuntime.hpp"
#include "gui/controls/TemplateRuntime.hpp"

#include <algorithm>

namespace Aero::Controls {

using namespace Primitives;

using namespace Aero::Meta;
using namespace Aero::Threading;


Popup::Popup() noexcept
    : Popup(StaticTypeId()) {}

Popup::Popup(TypeId runtimeType) noexcept
    : ContentControl(runtimeType),
      openChangedHandler_(
          this,
          &Popup::OnOpenPropertyChanged) {
    static_cast<void>(SetIsHitTestVisible(false));
    static_cast<void>(AddValueChangedHandlerChecked(
        IsOpenProperty,
        openChangedHandler_));
}

Popup::~Popup() {
    static_cast<void>(RemoveValueChangedHandler(
        IsOpenProperty,
        openChangedHandler_));
}

bool Popup::GetIsOpen() const noexcept {
    return GetValueOr(
        IsOpenProperty, false);
}

void Popup::SetIsOpen(
    bool value) noexcept {
    SetValue(IsOpenProperty, value);
}

PlacementMode Popup::GetPlacement() const noexcept {
    return GetValueOr(
        PlacementProperty,
        PlacementMode::Bottom);
}

void Popup::SetPlacement(
    PlacementMode value) noexcept {
    SetValue(
        PlacementProperty, value);
}

double Popup::GetHorizontalOffset() const noexcept {
    return GetValueOr(
        HorizontalOffsetProperty, 0.0);
}

void Popup::SetHorizontalOffset(
    double value) noexcept {
    SetValue(
        HorizontalOffsetProperty, value);
}

double Popup::GetVerticalOffset() const noexcept {
    return GetValueOr(
        VerticalOffsetProperty, 0.0);
}

void Popup::SetVerticalOffset(
    double value) noexcept {
    SetValue(
        VerticalOffsetProperty, value);
}

bool Popup::GetStaysOpen() const noexcept {
    return GetValueOr(
        StaysOpenProperty, true);
}

void Popup::SetStaysOpen(
    bool value) noexcept {
    SetValue(
        StaysOpenProperty, value);
}

bool Popup::GetMatchPlacementTargetWidth() const noexcept {
    return GetValueOr(
        MatchPlacementTargetWidthProperty, false);
}

void
Popup::SetMatchPlacementTargetWidth(
    bool value) noexcept {
    SetValue(
        MatchPlacementTargetWidthProperty,
        value);
}

Base::Ref<UIElement>
Popup::GetPlacementTarget() const noexcept {
    return GetValueOr(
        PlacementTargetProperty,
        Base::Ref<UIElement>{});
}

void Popup::SetPlacementTarget(
    Base::Ref<UIElement> value) noexcept {
    SetValue(
        PlacementTargetProperty,
        std::move(value));
}

PopupAnimation Popup::GetPopupAnimation() const noexcept {
    return GetValueOr(
        PopupAnimationProperty,
        PopupAnimation::None);
}

void Popup::SetPopupAnimation(
    PopupAnimation value) noexcept {
    SetValue(
        PopupAnimationProperty, value);
}

bool Popup::GetAllowsTransparency() const noexcept {
    return GetValueOr(
        AllowsTransparencyProperty, false);
}

void Popup::SetAllowsTransparency(
    bool value) noexcept {
    SetValue(
        AllowsTransparencyProperty, value);
}

void Popup::OnOpenPropertyChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs&
        args) noexcept {
    static_cast<void>(
        SetIsHitTestVisible(
            args.GetNewValue().AsBoolean()));
    static_cast<void>(InvalidateMeasure());
    RoutedEventArgs eventArgs;
    RaiseEvent(
        args.GetNewValue().AsBoolean()
            ? OpenedEvent
            : ClosedEvent,
        &eventArgs);
}

Size Popup::MeasureOverride(
    Size availableSize) noexcept {
    popupDesiredSize_ = {};
    UIElement* popupChild =
        GetTemplateRoot() != nullptr
            ? GetTemplateRoot()
            : ContentElement();
    if (!GetIsOpen() || popupChild == nullptr) {
        return Size{};
    }
    Base::Result<void> measured =
        MeasureChild(*popupChild, availableSize);
    if (!measured) return Size{};
    popupDesiredSize_ =
        popupChild->GetDesiredSize();
    // Popup content participates in rendering and input, but never consumes
    // space in its placement target's layout.
    return Size{};
}

Size Popup::ArrangeOverride(
    Size finalSize) noexcept {
    UIElement* popupChild =
        GetTemplateRoot() != nullptr
            ? GetTemplateRoot()
            : ContentElement();
    if (popupChild == nullptr) return finalSize;
    if (!GetIsOpen()) {
        Base::Result<void> hidden =
            ArrangeChild(*popupChild, {});
        (void)hidden;
        return finalSize;
    }

    Size contentSize = popupDesiredSize_;
    Base::Ref<UIElement> explicitPlacementTarget =
        GetPlacementTarget();
    UIElement* placementTarget =
        explicitPlacementTarget.Get();
    if (placementTarget == nullptr) {
        DependencyObject* templatedParent =
            GetTemplatedParent();
        if (templatedParent != nullptr &&
            PropertyRegistry().Types().IsDerivedFrom(
                templatedParent->RuntimeType(),
                UIElement::StaticTypeId())) {
            placementTarget =
                static_cast<UIElement*>(
                    templatedParent);
        } else if (GetVisualParent() != nullptr) {
            placementTarget =
                GetVisualParent()->AsUIElement();
        }
    }
    Size targetSize = finalSize;
    Point targetOrigin{};
    if (placementTarget != nullptr &&
        placementTarget->GetIsArrangeValid()) {
        targetSize = placementTarget->GetRenderSize();
        auto absoluteOrigin = [](UIElement& element) noexcept {
            Point result{};
            ::Aero::Media::Visual* current = &element;
            while (current != nullptr) {
                UIElement* currentElement =
                    current->AsUIElement();
                if (currentElement != nullptr) {
                    const Rect slot =
                        currentElement->GetLayoutSlot();
                    result.x += slot.x;
                    result.y += slot.y;
                }
                current = current->GetVisualParent();
            }
            return result;
        };
        const Point targetAbsolute =
            absoluteOrigin(*placementTarget);
        const Point popupAbsolute =
            absoluteOrigin(*this);
        targetOrigin = {
            targetAbsolute.x - popupAbsolute.x,
            targetAbsolute.y - popupAbsolute.y};
    }
    if (GetMatchPlacementTargetWidth()) {
        contentSize.width =
            std::max(
                contentSize.width,
                targetSize.width);
    }
    double x = targetOrigin.x + GetHorizontalOffset();
    double y = targetOrigin.y + GetVerticalOffset();
    switch (GetPlacement()) {
    case PlacementMode::Bottom:
        y += targetSize.height;
        break;
    case PlacementMode::Top:
        y -= contentSize.height;
        break;
    case PlacementMode::Left:
        x -= contentSize.width;
        break;
    case PlacementMode::Right:
        x += targetSize.width;
        break;
    case PlacementMode::Center:
        x += (targetSize.width -
            contentSize.width) * 0.5;
        y += (targetSize.height -
            contentSize.height) * 0.5;
        break;
    case PlacementMode::Mouse:
        // The popup service supplies a pointer origin when available; the
        // placement target origin remains the deterministic fallback.
        break;
    }
    Base::Result<void> arranged =
        ArrangeChild(
            *popupChild,
            {x, y,
             contentSize.width,
             contentSize.height});
    (void)arranged;
    return finalSize;
}

Meta::Value
HeaderedContentControl::GetHeader() const noexcept {
    return GetValueOr(
        HeaderProperty,
        Meta::Value::NullObject(
            Meta::TypeOf<Base::Object>()));
}

void HeaderedContentControl::SetHeader(
    const Meta::Value& value) noexcept {
    SetValue(HeaderProperty, value);
}

Base::Result<void> HeaderedContentControl::SetHeader(
    Base::StringView value) noexcept {
    Base::Result<Value> boxed = Value::TryFromString(
        Meta::TypeOf<Base::String>(), value);
    if (!boxed) return boxed.GetStatus();
    SetHeader(std::move(boxed).Value());
    return {};
}

Base::Ref<DataTemplate>
HeaderedContentControl::GetHeaderTemplate() const noexcept {
    return GetValueOr(
        HeaderTemplateProperty,
        Base::Ref<DataTemplate>{});
}

void
HeaderedContentControl::SetHeaderTemplate(
    Base::Ref<DataTemplate> value) noexcept {
    SetValue(
        HeaderTemplateProperty,
        std::move(value));
}

Expander::Expander() noexcept
    : HeaderedContentControl(StaticTypeId()),
      expandedChangedHandler_(
          this,
          &Expander::OnExpandedPropertyChanged) {
    static_cast<void>(AddValueChangedHandlerChecked(
        IsExpandedProperty,
        expandedChangedHandler_));
}

Expander::~Expander() {
    static_cast<void>(RemoveValueChangedHandler(
        IsExpandedProperty,
        expandedChangedHandler_));
}

bool Expander::GetIsExpanded() const noexcept {
    return GetValueOr(
        IsExpandedProperty, false);
}

void Expander::SetIsExpanded(
    bool value) noexcept {
    const bool old = GetIsExpanded();
    if (old == value) return;
    SetValue(IsExpandedProperty, value);
}

void Expander::OnExpandedPropertyChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs&
        change) noexcept {
    static_cast<void>(InvalidateMeasure());
    RoutedEventArgs eventArgs;
    RaiseEvent(
        change.GetNewValue().AsBoolean()
            ? ExpandedEvent
            : CollapsedEvent,
        &eventArgs);
}

ExpandDirection Expander::GetDirection() const noexcept {
    return GetValueOr(
        ExpandDirectionProperty,
        ExpandDirection::Down);
}

void Expander::SetDirection(
    ExpandDirection value) noexcept {
    SetValue(
        ExpandDirectionProperty, value);
}

Size Expander::MeasureOverride(
    Size availableSize) noexcept {
    if (GetTemplateRoot() != nullptr) {
        return ContentControl::MeasureOverride(
            availableSize);
    }
    constexpr double HeaderExtent = 24.0;
    if (!GetIsExpanded() || ContentElement() == nullptr) {
        return GetDirection() == ExpandDirection::Left ||
                GetDirection() == ExpandDirection::Right
            ? Size{HeaderExtent, 0.0}
            : Size{0.0, HeaderExtent};
    }
    Size childAvailable = availableSize;
    if (GetDirection() == ExpandDirection::Left ||
        GetDirection() == ExpandDirection::Right) {
        childAvailable.width =
            std::max(0.0, childAvailable.width - HeaderExtent);
    } else {
        childAvailable.height =
            std::max(0.0, childAvailable.height - HeaderExtent);
    }
    Base::Result<void> measured =
        MeasureChild(*ContentElement(), childAvailable);
    if (!measured) return Size{};
    const Size desired = ContentElement()->GetDesiredSize();
    return GetDirection() == ExpandDirection::Left ||
            GetDirection() == ExpandDirection::Right
        ? Size{desired.width + HeaderExtent, desired.height}
        : Size{desired.width, desired.height + HeaderExtent};
}

Size Expander::ArrangeOverride(
    Size finalSize) noexcept {
    if (GetTemplateRoot() != nullptr) {
        return ContentControl::ArrangeOverride(
            finalSize);
    }
    if (!GetIsExpanded() || ContentElement() == nullptr) {
        return finalSize;
    }
    constexpr double HeaderExtent = 24.0;
    Rect slot{0.0, 0.0, finalSize.width, finalSize.height};
    switch (GetDirection()) {
    case ExpandDirection::Down:
        slot.y += HeaderExtent;
        slot.height = std::max(
            0.0, slot.height - HeaderExtent);
        break;
    case ExpandDirection::Up:
        slot.height = std::max(
            0.0, slot.height - HeaderExtent);
        break;
    case ExpandDirection::Right:
        slot.x += HeaderExtent;
        slot.width = std::max(
            0.0, slot.width - HeaderExtent);
        break;
    case ExpandDirection::Left:
        slot.width = std::max(
            0.0, slot.width - HeaderExtent);
        break;
    }
    Base::Result<void> arranged =
        ArrangeChild(*ContentElement(), slot);
    if (!arranged) return finalSize;
    return finalSize;
}

bool TabItem::GetIsSelected() const noexcept {
    return GetValueOr(
        IsSelectedProperty, false);
}

void TabItem::SetIsSelected(
    bool value) noexcept {
    SetValue(
        IsSelectedProperty, value);
}

TabControl::TabControl() noexcept
    : Control(StaticTypeId()),
      selectionChangedHandler_(
          this,
          &TabControl::OnSelectionPropertyChanged) {
    static_cast<void>(AddValueChangedHandlerChecked(
        SelectedIndexProperty,
        selectionChangedHandler_));
}

TabControl::~TabControl() {
    static_cast<void>(RemoveValueChangedHandler(
        SelectedIndexProperty,
        selectionChangedHandler_));
}

std::uint32_t TabControl::GetSelectedIndex() const noexcept {
    return GetValueOr(
        SelectedIndexProperty,
        UINT32_MAX);
}

TabItem* TabControl::GetSelectedTab() const noexcept {
    const std::uint32_t selected =
        GetSelectedIndex();
    return selected < tabs_.Size()
        ? tabs_[selected].Get()
        : nullptr;
}

Base::Result<void> TabControl::AddOwnedTab(
    Base::Ref<TabItem> tab) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return access.GetStatus();
    if (!tab) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "TabControl tab is null");
    }
    for (const Base::Ref<TabItem>& current : tabs_) {
        if (current.Get() == tab.Get()) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "TabControl already owns this tab");
        }
    }
    Base::Result<void> added =
        tabs_.PushBack(std::move(tab));
    if (!added) return added.GetStatus();
    if (tabs_.Size() == 1U &&
        GetSelectedIndex() == UINT32_MAX) {
        SetSelectedIndex(0U);
    } else {
        Base::Result<void> synchronized =
            SynchronizeSelection();
        if (!synchronized) {
            return synchronized.GetStatus();
        }
    }
    return InvalidateMeasure();
}

void TabControl::ClearOwnedTabs() noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return;
    if (!LayoutChildren().Empty()) {
        return;
    }
    tabs_.Clear();
    SetValue(SelectedIndexProperty, UINT32_MAX);
    (void)InvalidateMeasure();
}

void TabControl::SetSelectedIndex(
    std::uint32_t value) noexcept {
    if (value != UINT32_MAX &&
        value >= tabs_.Size()) {
        return;
    }
    const std::uint32_t old =
        GetSelectedIndex();
    if (old == value) return;
    SetValue(SelectedIndexProperty, value);
}

Base::Result<void>
TabControl::SynchronizeSelection() noexcept {
    const std::uint32_t value =
        GetSelectedIndex();
    for (std::uint32_t index = 0U;
         index < tabs_.Size();
         ++index) {
        tabs_[index]->SetIsSelected(index == value);
    }
    const Meta::Value selectedContent =
        value < tabs_.Size()
        ? tabs_[value]->GetContent()
        : Meta::Value::NullObject(
              Meta::TypeOf<Base::Object>());
    SetReadOnlyCurrentValue(SelectedContentProperty, selectedContent);
    Base::Result<void> measure =
        InvalidateMeasure();
    if (!measure) return measure.GetStatus();
    return {};
}

void TabControl::OnSelectionPropertyChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs&)
        noexcept {
    const Base::Result<void> synchronized =
        SynchronizeSelection();
    if (!synchronized) return;
    RoutedEventArgs args;
    RaiseEvent(SelectionChangedEvent, &args);
}

Size TabControl::MeasureOverride(
    Size availableSize) noexcept {
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
    constexpr double HeaderExtent = 28.0;
    const Dock placement = GetTabStripPlacement();
    const bool verticalStrip =
        placement == Dock::Left || placement == Dock::Right;
    TabItem* selected = GetSelectedTab();
    for (const Base::Ref<TabItem>& tab : tabs_) {
        if (!tab) continue;
        Rect slot{};
        if (tab.Get() == selected) {
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
        PropertyRegistry().Types().IsDerivedFrom(
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

} // namespace Aero::Controls
