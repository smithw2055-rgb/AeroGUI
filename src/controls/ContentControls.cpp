#include <Aero/Controls/Items.hpp>
#include "ContentControlAccess.hpp"

#include <algorithm>

namespace Aero::Controls {

using namespace Primitives;

using namespace Aero::Core;


Popup::Popup() noexcept
    : Popup(StaticTypeId()) {}

Popup::Popup(TypeId runtimeType) noexcept
    : ContentControl(runtimeType),
      openChangedHandler_(
          this,
          &Popup::OnOpenPropertyChanged) {
    static_cast<void>(SetHitTestVisible(false));
    static_cast<void>(TryAddValueChangedHandler(
        IsOpenProperty,
        openChangedHandler_));
}

Popup::~Popup() {
    static_cast<void>(RemoveValueChangedHandler(
        IsOpenProperty,
        openChangedHandler_));
}

bool Popup::IsOpen() const noexcept {
    return GetValueOr(
        IsOpenProperty, false);
}

Base::Result<void> Popup::SetIsOpen(
    bool value) noexcept {
    return SetValue(IsOpenProperty, value);
}

PlacementMode Popup::Placement() const noexcept {
    return GetValueOr(
        PlacementProperty,
        PlacementMode::Bottom);
}

Base::Result<void> Popup::SetPlacement(
    PlacementMode value) noexcept {
    return SetValue(
        PlacementProperty, value);
}

double Popup::HorizontalOffset() const noexcept {
    return GetValueOr(
        HorizontalOffsetProperty, 0.0);
}

Base::Result<void> Popup::SetHorizontalOffset(
    double value) noexcept {
    return SetValue(
        HorizontalOffsetProperty, value);
}

double Popup::VerticalOffset() const noexcept {
    return GetValueOr(
        VerticalOffsetProperty, 0.0);
}

Base::Result<void> Popup::SetVerticalOffset(
    double value) noexcept {
    return SetValue(
        VerticalOffsetProperty, value);
}

bool Popup::StaysOpen() const noexcept {
    return GetValueOr(
        StaysOpenProperty, true);
}

Base::Result<void> Popup::SetStaysOpen(
    bool value) noexcept {
    return SetValue(
        StaysOpenProperty, value);
}

bool Popup::MatchPlacementTargetWidth() const noexcept {
    return GetValueOr(
        MatchPlacementTargetWidthProperty, false);
}

Base::Result<void>
Popup::SetMatchPlacementTargetWidth(
    bool value) noexcept {
    return SetValue(
        MatchPlacementTargetWidthProperty,
        value);
}

Base::Ref<UIElement>
Popup::PlacementTarget() const noexcept {
    return GetValueOr(
        PlacementTargetProperty,
        Base::Ref<UIElement>{});
}

Base::Result<void> Popup::SetPlacementTarget(
    Base::Ref<UIElement> value) noexcept {
    return SetValue(
        PlacementTargetProperty,
        std::move(value));
}

PopupAnimation Popup::GetPopupAnimation() const noexcept {
    return GetValueOr(
        PopupAnimationProperty,
        PopupAnimation::None);
}

Base::Result<void> Popup::SetPopupAnimation(
    PopupAnimation value) noexcept {
    return SetValue(
        PopupAnimationProperty, value);
}

bool Popup::AllowsTransparency() const noexcept {
    return GetValueOr(
        AllowsTransparencyProperty, false);
}

Base::Result<void> Popup::SetAllowsTransparency(
    bool value) noexcept {
    return SetValue(
        AllowsTransparencyProperty, value);
}

void Popup::OnOpenPropertyChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs&
        args) noexcept {
    static_cast<void>(
        SetHitTestVisible(
            args.newValue.AsBoolean()));
    static_cast<void>(InvalidateMeasure());
    RoutedEventArgs eventArgs;
    Base::Result<void> raised =
        RaiseEvent(
            args.newValue.AsBoolean()
                ? OpenedEvent
                : ClosedEvent,
            &eventArgs);
    if (!raised &&
        raised.GetStatus().code !=
            Base::ErrorCode::NotInitialized) {
        static_cast<void>(raised);
    }
}

Base::Result<Size> Popup::MeasureOverride(
    Size availableSize) noexcept {
    popupDesiredSize_ = {};
    UIElement* popupChild =
        TemplateChild() != nullptr
            ? TemplateChild()
            : ContentElement();
    if (!IsOpen() || popupChild == nullptr) {
        return Size{};
    }
    Base::Result<void> measured =
        MeasureChild(*popupChild, availableSize);
    if (!measured) return measured.GetStatus();
    popupDesiredSize_ =
        popupChild->DesiredSize();
    // Popup content participates in rendering and input, but never consumes
    // space in its placement target's layout.
    return Size{};
}

Base::Result<Size> Popup::ArrangeOverride(
    Size finalSize) noexcept {
    UIElement* popupChild =
        TemplateChild() != nullptr
            ? TemplateChild()
            : ContentElement();
    if (popupChild == nullptr) return finalSize;
    if (!IsOpen()) {
        Base::Result<void> hidden =
            ArrangeChild(*popupChild, {});
        return hidden
            ? finalSize
            : Base::Result<Size>(
                  hidden.GetStatus());
    }

    Size contentSize = popupDesiredSize_;
    Base::Ref<UIElement> explicitPlacementTarget =
        PlacementTarget();
    UIElement* placementTarget =
        explicitPlacementTarget.Get();
    if (placementTarget == nullptr) {
        DependencyObject* templatedParent =
            TemplatedParent();
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
        placementTarget->IsArrangeValid()) {
        targetSize = placementTarget->RenderSize();
        auto absoluteOrigin = [](UIElement& element) noexcept {
            Point result{};
            Visual* current = &element;
            while (current != nullptr) {
                UIElement* currentElement =
                    current->AsUIElement();
                if (currentElement != nullptr) {
                    const Rect slot =
                        currentElement->LayoutSlot();
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
    if (MatchPlacementTargetWidth()) {
        contentSize.width =
            std::max(
                contentSize.width,
                targetSize.width);
    }
    double x = targetOrigin.x + HorizontalOffset();
    double y = targetOrigin.y + VerticalOffset();
    switch (Placement()) {
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
    return arranged
        ? finalSize
        : Base::Result<Size>(
              arranged.GetStatus());
}

Core::Value
HeaderedContentControl::Header() const noexcept {
    return GetValueOr(
        HeaderProperty,
        Core::Value::NullObject(
            Core::TypeOf<Base::Object>()));
}

Base::Result<void> HeaderedContentControl::SetHeader(
    const Core::Value& value) noexcept {
    return SetValue(HeaderProperty, value);
}

Base::Ref<DataTemplate>
HeaderedContentControl::HeaderTemplate() const noexcept {
    return GetValueOr(
        HeaderTemplateProperty,
        Base::Ref<DataTemplate>{});
}

Base::Result<void>
HeaderedContentControl::SetHeaderTemplate(
    Base::Ref<DataTemplate> value) noexcept {
    return SetValue(
        HeaderTemplateProperty,
        std::move(value));
}

Expander::Expander() noexcept
    : HeaderedContentControl(StaticTypeId()),
      expandedChangedHandler_(
          this,
          &Expander::OnExpandedPropertyChanged) {
    static_cast<void>(TryAddValueChangedHandler(
        IsExpandedProperty,
        expandedChangedHandler_));
}

Expander::~Expander() {
    static_cast<void>(RemoveValueChangedHandler(
        IsExpandedProperty,
        expandedChangedHandler_));
}

bool Expander::IsExpanded() const noexcept {
    return GetValueOr(
        IsExpandedProperty, false);
}

Base::Result<void> Expander::SetIsExpanded(
    bool value) noexcept {
    const bool old = IsExpanded();
    if (old == value) return {};
    Base::Result<void> stored =
        SetValue(IsExpandedProperty, value);
    return stored;
}

void Expander::OnExpandedPropertyChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs&
        change) noexcept {
    static_cast<void>(InvalidateMeasure());
    RoutedEventArgs eventArgs;
    Base::Result<void> raised =
        RaiseEvent(
            change.newValue.AsBoolean()
                ? ExpandedEvent
                : CollapsedEvent,
            &eventArgs);
    if (!raised &&
        raised.GetStatus().code !=
            Base::ErrorCode::NotInitialized) {
        static_cast<void>(raised);
    }
}

ExpandDirection Expander::Direction() const noexcept {
    return GetValueOr(
        ExpandDirectionProperty,
        ExpandDirection::Down);
}

Base::Result<void> Expander::SetDirection(
    ExpandDirection value) noexcept {
    return SetValue(
        ExpandDirectionProperty, value);
}

Base::Result<Size> Expander::MeasureOverride(
    Size availableSize) noexcept {
    if (TemplateChild() != nullptr) {
        return ContentControl::MeasureOverride(
            availableSize);
    }
    constexpr double HeaderExtent = 24.0;
    if (!IsExpanded() || ContentElement() == nullptr) {
        return Direction() == ExpandDirection::Left ||
                Direction() == ExpandDirection::Right
            ? Size{HeaderExtent, 0.0}
            : Size{0.0, HeaderExtent};
    }
    Size childAvailable = availableSize;
    if (Direction() == ExpandDirection::Left ||
        Direction() == ExpandDirection::Right) {
        childAvailable.width =
            std::max(0.0, childAvailable.width - HeaderExtent);
    } else {
        childAvailable.height =
            std::max(0.0, childAvailable.height - HeaderExtent);
    }
    Base::Result<void> measured =
        MeasureChild(*ContentElement(), childAvailable);
    if (!measured) return measured.GetStatus();
    const Size desired = ContentElement()->DesiredSize();
    return Direction() == ExpandDirection::Left ||
            Direction() == ExpandDirection::Right
        ? Size{desired.width + HeaderExtent, desired.height}
        : Size{desired.width, desired.height + HeaderExtent};
}

Base::Result<Size> Expander::ArrangeOverride(
    Size finalSize) noexcept {
    if (TemplateChild() != nullptr) {
        return ContentControl::ArrangeOverride(
            finalSize);
    }
    if (!IsExpanded() || ContentElement() == nullptr) {
        return finalSize;
    }
    constexpr double HeaderExtent = 24.0;
    Rect slot{0.0, 0.0, finalSize.width, finalSize.height};
    switch (Direction()) {
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
    if (!arranged) return arranged.GetStatus();
    return finalSize;
}

bool TabItem::IsSelected() const noexcept {
    return GetValueOr(
        IsSelectedProperty, false);
}

Base::Result<void> TabItem::SetIsSelected(
    bool value) noexcept {
    return SetValue(
        IsSelectedProperty, value);
}

TabControl::TabControl() noexcept
    : Control(StaticTypeId()),
      selectionChangedHandler_(
          this,
          &TabControl::OnSelectionPropertyChanged) {
    static_cast<void>(TryAddValueChangedHandler(
        SelectedIndexProperty,
        selectionChangedHandler_));
}

TabControl::~TabControl() {
    static_cast<void>(RemoveValueChangedHandler(
        SelectedIndexProperty,
        selectionChangedHandler_));
}

std::uint32_t TabControl::SelectedIndex() const noexcept {
    return GetValueOr(
        SelectedIndexProperty,
        UINT32_MAX);
}

TabItem* TabControl::SelectedTab() const noexcept {
    const std::uint32_t selected =
        SelectedIndex();
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
        tabs_.TryPushBack(std::move(tab));
    if (!added) return added.GetStatus();
    if (tabs_.Size() == 1U &&
        SelectedIndex() == UINT32_MAX) {
        Base::Result<bool> selected =
            SetSelectedIndex(0U);
        if (!selected) return selected.GetStatus();
    } else {
        Base::Result<void> synchronized =
            SynchronizeSelection();
        if (!synchronized) {
            return synchronized.GetStatus();
        }
    }
    return InvalidateMeasure();
}

Base::Result<void> TabControl::ClearOwnedTabs() noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return access.GetStatus();
    if (!LayoutChildren().Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "TabControl tabs must be detached before releasing ownership");
    }
    tabs_.Clear();
    Base::Result<void> selected =
        SetValue(
            SelectedIndexProperty,
            UINT32_MAX);
    return selected
        ? InvalidateMeasure()
        : selected;
}

Base::Result<bool> TabControl::SetSelectedIndex(
    std::uint32_t value) noexcept {
    if (value != UINT32_MAX &&
        value >= tabs_.Size()) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "TabControl selected index is outside the tab range");
    }
    const std::uint32_t old =
        SelectedIndex();
    if (old == value) return false;
    Base::Result<void> stored =
        SetValue(
            SelectedIndexProperty,
            value);
    if (!stored) return stored.GetStatus();
    return true;
}

Base::Result<void>
TabControl::SynchronizeSelection() noexcept {
    const std::uint32_t value =
        SelectedIndex();
    for (std::uint32_t index = 0U;
         index < tabs_.Size();
         ++index) {
        Base::Result<void> selected =
            tabs_[index]->SetIsSelected(
                index == value);
        if (!selected) return selected.GetStatus();
    }
    const Core::Value selectedContent =
        value < tabs_.Size()
        ? tabs_[value]->GetContent()
        : Core::Value::NullObject(
              Core::TypeOf<Base::Object>());
    Base::Result<void> content = SetReadOnlyCurrentValue(
        SelectedContentProperty, selectedContent);
    if (!content) return content.GetStatus();
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
    Base::Result<void> raised =
        RaiseEvent(
            SelectionChangedEvent, &args);
    if (!raised &&
        raised.GetStatus().code !=
            Base::ErrorCode::NotInitialized) {
        return;
    }
}

Base::Result<Size> TabControl::MeasureOverride(
    Size availableSize) noexcept {
    constexpr double HeaderExtent = 28.0;
    const bool verticalStrip =
        TabStripPlacement() == Dock::Left ||
        TabStripPlacement() == Dock::Right;
    TabItem* selected = SelectedTab();
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
    if (!measured) return measured.GetStatus();
    const Size desired = selected->DesiredSize();
    return verticalStrip
        ? Size{desired.width + HeaderExtent, desired.height}
        : Size{desired.width, desired.height + HeaderExtent};
}

Base::Result<Size> TabControl::ArrangeOverride(
    Size finalSize) noexcept {
    constexpr double HeaderExtent = 28.0;
    const Dock placement = TabStripPlacement();
    const bool verticalStrip =
        placement == Dock::Left || placement == Dock::Right;
    TabItem* selected = SelectedTab();
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
        if (!arranged) return arranged.GetStatus();
    }
    return finalSize;
}

bool TabPanel::IsVertical() const noexcept {
    const DependencyObject* parent = TemplatedParent();
    return parent != nullptr &&
        PropertyRegistry().Types().IsDerivedFrom(
            parent->RuntimeType(), TabControl::StaticTypeId()) &&
        (static_cast<const TabControl*>(parent)->TabStripPlacement() ==
             Dock::Left ||
         static_cast<const TabControl*>(parent)->TabStripPlacement() ==
             Dock::Right);
}

Base::Result<Size> TabPanel::MeasureOverride(
    Size availableSize) noexcept {
    const bool vertical = IsVertical();
    Size desired{};
    double linePrimary = 0.0;
    double lineCross = 0.0;
    const double limit = vertical
        ? availableSize.height
        : availableSize.width;
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        Base::Result<void> measured = MeasureChild(*child, availableSize);
        if (!measured) return measured.GetStatus();
        const Size size = child->DesiredSize();
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

Base::Result<Size> TabPanel::ArrangeOverride(
    Size finalSize) noexcept {
    const bool vertical = IsVertical();
    const double limit = vertical ? finalSize.height : finalSize.width;
    double x = 0.0;
    double y = 0.0;
    double lineCross = 0.0;
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        const Size size = child->DesiredSize();
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
        if (!arranged) return arranged.GetStatus();
        if (vertical) y += size.height;
        else x += size.width;
        lineCross = std::max(lineCross, cross);
    }
    return finalSize;
}

} // namespace Aero::Controls
