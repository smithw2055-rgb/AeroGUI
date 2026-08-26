#include "gui/core/State.hpp" 
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include <Aero/Controls.hpp>
#include <Aero/Controls/ItemContainerGenerator.hpp>
#include "gui/controls/State.hpp"
#include "gui/templates/TemplateState.hpp"

#include <algorithm>
#include <limits>

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
    constexpr double Unconstrained = 1.0e12;
    Base::Result<void> measured =
        MeasureChild(*popupChild, Size{Unconstrained, Unconstrained});
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
                ::Aero::TryCast<::Aero::UIElement>(GetVisualParent());
        }
    }
    Size targetSize = finalSize;
    Point targetOrigin{};
    Point targetAbsolute{};
    double rootHeight = 0.0;
    double targetScaleX = 1.0;
    double targetScaleY = 1.0;
    double popupScaleX = 1.0;
    double popupScaleY = 1.0;
    if (placementTarget != nullptr &&
        placementTarget->GetIsArrangeValid()) {
        targetSize = placementTarget->GetRenderSize();
        // The overlay renderer positions Popups in screen space: it accumulates
        // every ancestor's local visual transform (for example a Viewbox
        // scale) plus layout slot, and the arranged child slot is added on top
        // of that origin. Placement must therefore use the same transform-aware
        // origin, otherwise a scaled ancestor displaces the popup and makes the
        // up/down flip decision against the window height wrong.
        auto absoluteOrigin = [](
            UIElement& element,
            UIElement** outRoot,
            double* outScaleX,
            double* outScaleY) noexcept {
            Point result{};
            double scaleX = 1.0;
            double scaleY = 1.0;
            ::Aero::Media::Visual* current = &element;
            UIElement* lastElement = nullptr;
            while (current != nullptr) {
                UIElement* currentElement =
                    ::Aero::TryCast<::Aero::UIElement>(current);
                if (currentElement != nullptr) {
                    lastElement = currentElement;
                    FrameworkElement* currentFramework =
                        ::Aero::TryCast<::Aero::FrameworkElement>(currentElement);
                    if (currentFramework != nullptr) {
                        const Base::ProjectiveTransform2D transform =
                            currentFramework->GetLocalVisualTransform();
                        result = ::Aero::Base::TransformPoint(
                            transform, result);
                        Base::Transform2D affine;
                        if (::Aero::Base::TryToTransform2D(transform, affine) &&
                            affine.m11 > 0.0 &&
                            affine.m22 > 0.0) {
                            scaleX *= affine.m11;
                            scaleY *= affine.m22;
                        }
                    }
                    const Rect slot =
                        currentElement->GetLayoutSlot();
                    result.x += slot.x;
                    result.y += slot.y;
                }
                current = current->GetVisualParent();
            }
            if (outRoot != nullptr) *outRoot = lastElement;
            if (outScaleX != nullptr) *outScaleX = scaleX;
            if (outScaleY != nullptr) *outScaleY = scaleY;
            return result;
        };
        UIElement* rootElement = nullptr;
        targetAbsolute = absoluteOrigin(
            *placementTarget, &rootElement,
            &targetScaleX, &targetScaleY);
        const Point popupAbsolute = absoluteOrigin(
            *this, nullptr, &popupScaleX, &popupScaleY);
        targetOrigin = {
            targetAbsolute.x - popupAbsolute.x,
            targetAbsolute.y - popupAbsolute.y};
        if (rootElement != nullptr) {
            rootHeight = rootElement->GetRenderSize().height;
            if (rootHeight <= 0.0) {
                rootHeight = rootElement->GetLayoutSlot().height;
            }
        }
    }
    const Point targetOriginLocal{
        popupScaleX != 0.0 ? targetOrigin.x / popupScaleX : targetOrigin.x,
        popupScaleY != 0.0 ? targetOrigin.y / popupScaleY : targetOrigin.y};
    if (GetMatchPlacementTargetWidth()) {
        contentSize.width =
            std::max(
                contentSize.width,
                targetSize.width);
    }
    const double targetWidth = targetSize.width;
    const double targetHeight = targetSize.height;
    double x = targetOriginLocal.x + GetHorizontalOffset();
    double y = targetOriginLocal.y + GetVerticalOffset();
    const PlacementMode placement = GetPlacement();
    switch (placement) {
    case PlacementMode::Bottom:
        y += targetHeight;
        break;
    case PlacementMode::Top:
        y -= contentSize.height;
        break;
    case PlacementMode::Left:
        x -= contentSize.width;
        break;
    case PlacementMode::Right:
        x += targetWidth;
        break;
    case PlacementMode::Center:
        x += (targetWidth - contentSize.width) * 0.5;
        y += (targetHeight - contentSize.height) * 0.5;
        break;
    case PlacementMode::Mouse:
        // The popup service supplies a pointer origin when available; the
        // placement target origin remains the deterministic fallback.
        break;
    }

    if (rootHeight > 0.0 && placementTarget != nullptr) {
        const double bottomAbsolute =
            targetAbsolute.y + (y - targetOriginLocal.y + contentSize.height) * targetScaleY;
        const double topAbsolute =
            targetAbsolute.y + (y - targetOriginLocal.y) * targetScaleY;
        if (placement == PlacementMode::Bottom &&
            bottomAbsolute > rootHeight) {
            y = targetOriginLocal.y -
                contentSize.height -
                GetVerticalOffset();
        } else if (placement == PlacementMode::Top &&
                   topAbsolute < 0.0) {
            y = targetOriginLocal.y +
                targetHeight +
                GetVerticalOffset();
        }
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
    : Selector(StaticTypeId()),
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

TabItem* TabControl::GetSelectedTab() const noexcept {
    const std::uint32_t selected = GetSelectedIndex();
    if (selected == UINT32_MAX) return nullptr;
    const Ref<Base::Object> item = GetItem(selected);
    if (item &&
        PropertyRegistry().Types().IsDerivedFrom(
            item->RuntimeType(), TabItem::StaticTypeId())) {
        return static_cast<TabItem*>(item.Get());
    }
    ItemContainerGenerator* generator = AttachedGenerator();
    if (generator == nullptr) return nullptr;
    FrameworkElement* container = generator->ContainerFromIndex(selected);
    if (container != nullptr &&
        PropertyRegistry().Types().IsDerivedFrom(
            container->RuntimeType(), TabItem::StaticTypeId())) {
        return static_cast<TabItem*>(container);
    }
    return nullptr;
}

Base::Result<Ref<FrameworkElement>> TabControl::CreateContainer(
    const Ref<Base::Object>&) noexcept {
    Base::Result<Ref<TabItem>> made = Base::MakeRef<TabItem>();
    if (!made) return made.GetStatus();
    return Ref<FrameworkElement>(std::move(made).Value());
}

Base::Result<void>
TabControl::SynchronizeSelection() noexcept {
    const std::uint32_t value = GetSelectedIndex();
    const std::uint32_t count = GetCount();
    ItemContainerGenerator* generator = AttachedGenerator();
    for (std::uint32_t index = 0U; index < count; ++index) {
        TabItem* tab = nullptr;
        const Ref<Base::Object> item = GetItem(index);
        if (item &&
            PropertyRegistry().Types().IsDerivedFrom(
                item->RuntimeType(), TabItem::StaticTypeId())) {
            tab = static_cast<TabItem*>(item.Get());
        } else if (generator != nullptr) {
            FrameworkElement* container = generator->ContainerFromIndex(index);
            if (container != nullptr &&
                PropertyRegistry().Types().IsDerivedFrom(
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
    Base::Result<void> measure = InvalidateMeasure();
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
    RaiseEvent(SelectionChangedRoutedEvent, &args);
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
            PropertyRegistry().Types().IsDerivedFrom(
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
#include "gui/meta/MetadataState.hpp"
#include "gui/core/State.hpp"
#include "gui/data/BindingEngine.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "render/DisplayList.hpp"
#include <Aero/Controls.hpp>
#include <Aero/Controls/ListBox.hpp>
#include <Aero/Controls/TreeView.hpp>
#include <Aero/Shapes.hpp>
#include <Aero/Media/Transforms.hpp>
#include "gui/media/BrushRendering.hpp"
#include "gui/media/MediaState.hpp"
#include <Aero/Documents.hpp>
#include "RichText.hpp"

#include "TextBlockLayout.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <utility>


namespace Aero::Controls {
using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero::Render;

Stretch Viewbox::GetStretch() const noexcept {
    return GetValueOr(StretchProperty, Stretch::Uniform);
}
StretchDirection
Viewbox::GetStretchDirection() const noexcept {
    return GetValueOr(
        StretchDirectionProperty,
        StretchDirection::Both);
}
void Viewbox::SetStretch(
    Stretch value) noexcept {
    SetValue(StretchProperty, value);
}
void Viewbox::SetStretchDirection(
    StretchDirection value) noexcept {
    SetValue(StretchDirectionProperty, value);
}
Size Viewbox::MeasureOverride(
    Size availableSize) noexcept {
    UIElement* child = GetChild();
    if (child == nullptr) {
        if (!LayoutChildren().Empty()) {
            return Size{};
        }
        return Size{};
    }

    // The layout kernel keeps all constraints finite. A large finite measure
    // gives Viewbox content its natural size while preserving that invariant.
    constexpr double NaturalConstraint = 1.0e12;
    Base::Result<void> measured = MeasureChild(
        *child,
        {NaturalConstraint, NaturalConstraint});
    if (!measured) return Size{};

    const Size natural = child->GetDesiredSize();
    if (natural.width <= 0.0 || natural.height <= 0.0) {
        return Size{};
    }

    double scaleX = availableSize.width / natural.width;
    double scaleY = availableSize.height / natural.height;
    switch (GetStretch()) {
    case Stretch::None:
        scaleX = 1.0;
        scaleY = 1.0;
        break;
    case Stretch::Uniform: {
        const double scale = std::min(scaleX, scaleY);
        scaleX = scale;
        scaleY = scale;
        break;
    }
    case Stretch::UniformToFill: {
        const double scale = std::max(scaleX, scaleY);
        scaleX = scale;
        scaleY = scale;
        break;
    }
    case Stretch::Fill:
        break;
    }

    const StretchDirection direction =
        GetStretchDirection();
    if (direction == StretchDirection::UpOnly) {
        scaleX = std::max(1.0, scaleX);
        scaleY = std::max(1.0, scaleY);
    } else if (
        direction == StretchDirection::DownOnly) {
        scaleX = std::min(1.0, scaleX);
        scaleY = std::min(1.0, scaleY);
    }
    return Size{
        natural.width * scaleX,
        natural.height * scaleY};
}
Base::Result<void> Viewbox::ApplyViewTransform(
    double scaleX,
    double scaleY,
    double offsetX,
    double offsetY) noexcept {
    UIElement* child = GetChild();
    FrameworkElement* framework = child != nullptr
        ? ::Aero::TryCast<::Aero::FrameworkElement>(child)
        : nullptr;
    if (projectedChild_ && projectedChild_.Get() != framework) {
        projectedChild_->ClearViewboxTransform();
        static_cast<void>(
            AeroGuiInternal::InvalidateRenderState(*projectedChild_));
        projectedChild_.Reset();
    }
    if (child == nullptr) {
        viewTransform_.Reset();
        return {};
    }
    if (framework != nullptr) {
        Base::Transform2D matrix;
        matrix.m11 = scaleX;
        matrix.m22 = scaleY;
        matrix.dx = offsetX;
        matrix.dy = offsetY;
        const bool changed = framework->SetViewboxTransform(matrix);
        if (!projectedChild_) {
            projectedChild_ =
                Base::Ref<FrameworkElement>::FromBorrowed(*framework);
        }
        if (changed) {
            static_cast<void>(
                AeroGuiInternal::InvalidateRenderState(*framework));
        }
        return {};
    }
    if (!viewTransform_) {
        Base::Result<Base::Ref<MatrixTransform>> made =
            Base::MakeRef<MatrixTransform>();
        if (!made) return made.GetStatus();
        viewTransform_ = std::move(made).Value();
    }

    Base::Ref<Media::Transform> current =
        child->GetRenderTransform();
    if (!current) {
        child->SetRenderTransform(viewTransform_);
    } else if (current.Get() != viewTransform_.Get()) {
        // A Viewbox contributes an outer scale without replacing the child's
        // authored RenderTransform. Keeping an existing TransformGroup as the
        // root is important: storyboards address its children by index (for
        // example Dialog.RenderTransform.Children[0].ScaleX).
        if (!current->PropertyRegistry().Types().IsDerivedFrom(
                current->RuntimeType(),
                Media::TransformGroup::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Viewbox can only combine its scale with a TransformGroup");
        }
        auto& group = static_cast<Media::TransformGroup&>(*current);
        bool alreadyAppended = false;
        for (const Base::Ref<Media::Transform>& transform :
             group.GetChildren()) {
            if (transform.Get() == viewTransform_.Get()) {
                alreadyAppended = true;
                break;
            }
        }
        if (!alreadyAppended) {
            Base::Result<void> appended = group.AddChild(
                Base::Ref<Media::Transform>(viewTransform_));
            if (!appended) return appended.GetStatus();
        }
    }

    Base::Transform2D matrix;
    matrix.m11 = scaleX;
    matrix.m22 = scaleY;
    // The Viewbox scale is an outer layout transform. When it is composed
    // into the child's authored RenderTransform, compensate the transform
    // origin so the outer scale remains anchored at the child's top-left.
    // Otherwise a non-zero RenderTransformOrigin also pivots the Viewbox
    // scale and visibly displaces centered content.
    const Point origin = child->GetRenderTransformOrigin();
    const Size renderSize = child->GetRenderSize();
    matrix.dx = offsetX +
        origin.x * renderSize.width * (scaleX - 1.0);
    matrix.dy = offsetY +
        origin.y * renderSize.height * (scaleY - 1.0);
    viewTransform_->SetMatrixValue(matrix);
    return {};
}
Size Viewbox::ArrangeOverride(
    Size finalSize) noexcept {
    UIElement* child = GetChild();
    if (child == nullptr) {
        Base::Result<void> reset =
            ApplyViewTransform(1.0, 1.0, 0.0, 0.0);
        if (!reset) return finalSize;
        return finalSize;
    }

    const Size natural = child->GetDesiredSize();
    if (natural.width <= 0.0 || natural.height <= 0.0) {
        Base::Result<void> arranged = ArrangeChild(
            *child, {0.0, 0.0, 0.0, 0.0});
        if (!arranged) return finalSize;
        Base::Result<void> reset =
            ApplyViewTransform(1.0, 1.0, 0.0, 0.0);
        if (!reset) return finalSize;
        return finalSize;
    }

    double scaleX = finalSize.width / natural.width;
    double scaleY = finalSize.height / natural.height;
    switch (GetStretch()) {
    case Stretch::None:
        scaleX = 1.0;
        scaleY = 1.0;
        break;
    case Stretch::Uniform: {
        const double scale = std::min(scaleX, scaleY);
        scaleX = scale;
        scaleY = scale;
        break;
    }
    case Stretch::UniformToFill: {
        const double scale = std::max(scaleX, scaleY);
        scaleX = scale;
        scaleY = scale;
        break;
    }
    case Stretch::Fill:
        break;
    }

    const StretchDirection direction =
        GetStretchDirection();
    if (direction == StretchDirection::UpOnly) {
        scaleX = std::max(1.0, scaleX);
        scaleY = std::max(1.0, scaleY);
    } else if (
        direction == StretchDirection::DownOnly) {
        scaleX = std::min(1.0, scaleX);
        scaleY = std::min(1.0, scaleY);
    }

    const double renderedWidth =
        natural.width * scaleX;
    const double renderedHeight =
        natural.height * scaleY;
    const double offsetX =
        (finalSize.width - renderedWidth) * 0.5;
    const double offsetY =
        (finalSize.height - renderedHeight) * 0.5;
    // A Viewbox scales the complete child footprint, including its margin.
    // RenderTransform is applied after layout translation in the renderer, so
    // an uncompensated FrameworkElement margin would remain in unscaled
    // pixels. That visibly shifts centered reference content (for example the
    // Gallery welcome mark) and diverges from WPF/Noesis layout semantics.
    const FrameworkElement* childFramework =
        ::Aero::TryCast<::Aero::FrameworkElement>(child);
    const Thickness childMargin = childFramework != nullptr
        ? childFramework->GetMargin()
        : Thickness{};
    const double arrangeX = offsetX +
        childMargin.left * (scaleX - 1.0);
    const double arrangeY = offsetY +
        childMargin.top * (scaleY - 1.0);
    Base::Result<void> arranged = ArrangeChild(
        *child,
        {arrangeX, arrangeY,
         natural.width, natural.height});
    if (!arranged) return finalSize;

    Base::Result<void> transformed = ApplyViewTransform(
        scaleX,
        scaleY,
        0.0,
        0.0);
    if (!transformed) return finalSize;
    return finalSize;
}
Border::Border() noexcept : Decorator(StaticTypeId()) {}

Base::Ref<Brush> Border::GetBackground() const noexcept {
    return GetValueOr(
        BackgroundProperty, Base::Ref<Brush>{});
}
Base::Ref<Brush> Border::GetBorderBrush() const noexcept {
    return GetValueOr(
        BorderBrushProperty, Base::Ref<Brush>{});
}
Thickness Border::GetBorderThickness() const noexcept {
    return GetValueOr(
        BorderThicknessProperty, Thickness{});
}
CornerRadius Border::GetCornerRadius() const noexcept {
    return GetValueOr(
        CornerRadiusProperty, CornerRadius{});
}
Thickness Border::GetPadding() const noexcept {
    return GetValueOr(PaddingProperty, Thickness{});
}
void Border::SetBackground(
    Base::Ref<Brush> value) noexcept {
    SetValue(
        BackgroundProperty, std::move(value));
}
void Border::SetBorderBrush(
    Base::Ref<Brush> value) noexcept {
    SetValue(
        BorderBrushProperty, std::move(value));
}
void Border::SetBorderThickness(
    Thickness value) noexcept {
    SetValue(BorderThicknessProperty, value);
}
void Border::SetBorderThickness(
    double value) noexcept {
    SetBorderThickness({value, value, value, value});
}
void Border::SetCornerRadius(
    CornerRadius value) noexcept {
    SetValue(CornerRadiusProperty, value);
}
void Border::SetCornerRadius(
    double value) noexcept {
    SetCornerRadius({value, value, value, value});
}
void Border::SetPadding(Thickness value) noexcept {
    SetValue(PaddingProperty, value);
}
Size Border::MeasureOverride(Size availableSize) noexcept {
    const Thickness border = GetBorderThickness();
    const Thickness padding = GetPadding();
    const Thickness chrome{
        border.left + padding.left,
        border.top + padding.top,
        border.right + padding.right,
        border.bottom + padding.bottom};
    UIElement* child = GetChild();
    if (child == nullptr) return Inflate({}, chrome);
    Base::Result<void> measured = MeasureChild(
        *child, Deflate(availableSize, chrome));
    if (!measured) return Size{};
    return Inflate(child->GetDesiredSize(), chrome);
}
Size Border::ArrangeOverride(Size finalSize) noexcept {
    UIElement* child = GetChild();
    if (child == nullptr) return finalSize;
    const Thickness border = GetBorderThickness();
    const Thickness padding = GetPadding();
    const Thickness chrome{
        border.left + padding.left,
        border.top + padding.top,
        border.right + padding.right,
        border.bottom + padding.bottom};
    const Size childSize = Deflate(finalSize, chrome);
    Base::Result<void> arranged = ArrangeChild(*child,
        {chrome.left, chrome.top,
         childSize.width, childSize.height});
    if (!arranged) return finalSize;
    return finalSize;
}
void Border::OnRender(
    ::Aero::Media::DrawingContext& context) noexcept {
    auto& builder = Aero::Render::DrawingPrivate::Builder(context);
    const Rect bounds{0.0, 0.0, GetRenderSize().width, GetRenderSize().height};
    if (bounds.width <= 0.0 ||
        bounds.height <= 0.0) {
        return;
    }
    const Base::Ref<Brush> background = GetBackground();
    const bool paintedBackground = background &&
        (background->RuntimeType() == Media::ImageBrush::StaticTypeId() ||
         background->RuntimeType() == Media::LinearGradientBrush::StaticTypeId() ||
         background->RuntimeType() == Media::RadialGradientBrush::StaticTypeId());
    const CornerRadius radii = GetCornerRadius();
    const double radius = std::min(
        std::max(
            std::max(
                radii.topLeft,
                radii.topRight),
            std::max(
                radii.bottomRight,
                radii.bottomLeft)),
        std::min(
            bounds.width,
            bounds.height) * 0.5);
    const Color brush = ::Aero::Media::SampleBrush(GetBorderBrush());
    const Thickness thickness = GetBorderThickness();
    const bool uniform =
        thickness.left == thickness.top &&
        thickness.left == thickness.right &&
        thickness.left == thickness.bottom;
    const double uniformThickness =
        uniform ? thickness.left : 0.0;
    const Color backgroundColor =
        ::Aero::Media::SampleBrush(background);
    if (radius > 0.0 && uniformThickness > 0.0 &&
        brush.alpha > 0.0F && backgroundColor.alpha <= 0.0F) {
        // Filling an outer rounded rectangle and then drawing transparent
        // into its center cannot punch a hole with source-over blending.
        // Preserve transparent rounded-border templates as an outline; the
        // square-corner approximation is preferable to an opaque block.
        static_cast<void>(builder.StrokeRect(
            bounds, brush, uniformThickness, radius));
        return;
    }
    if (radius > 0.0 && uniformThickness > 0.0 &&
        brush.alpha > 0.0F) {
        Base::Result<void> border =
            builder.FillRoundedRect(
                bounds, brush, radius);
        if (!border) return;
        const double inset = std::min(
            uniformThickness,
            std::min(
                bounds.width * 0.5,
                bounds.height * 0.5));
        const Rect inner{
            inset,
            inset,
            std::max(0.0, bounds.width -
                inset * 2.0),
            std::max(0.0, bounds.height -
                inset * 2.0)};
        if (inner.width <= 0.0 ||
            inner.height <= 0.0) {
            return;
        }
        const bool isRtl = GetFlowDirection() == FlowDirection::RightToLeft;
        if (paintedBackground) {
            static_cast<void>(PaintBrushRect(builder, background, inner, 0.0, isRtl));
        } else {
            static_cast<void>(builder.FillRoundedRect(
                inner,
                ::Aero::Media::SampleBrush(background),
                std::min(
                    std::max(0.0, radius - inset),
                    std::min(
                        inner.width,
                        inner.height) * 0.5)));
        }
        return;
    }
    const bool isRtl = GetFlowDirection() == FlowDirection::RightToLeft;
    Base::Result<void> fill = paintedBackground
        ? PaintBrushRect(builder, background, bounds, radius, isRtl)
        : (radius > 0.0
            ? builder.FillRoundedRect(
                  bounds, backgroundColor, radius)
            : builder.FillRect(
                  bounds, backgroundColor));
    if (!fill) return;
    if (uniformThickness > 0.0 &&
        brush.alpha > 0.0F) {
        static_cast<void>(builder.StrokeRect(
            bounds, brush, uniformThickness, radius));
        return;
    }
    if (!uniform && brush.alpha > 0.0F) {
        const auto fillSide =
            [&](Rect side) noexcept -> Base::Result<void> {
                return side.width > 0.0 && side.height > 0.0
                    ? PaintBrushRect(builder, GetBorderBrush(), side)
                    : Base::Result<void>();
            };
        Base::Result<void> side = fillSide({
            0.0, 0.0,
            std::min(bounds.width, thickness.left),
            bounds.height});
        if (!side) return;
        side = fillSide({
            std::max(0.0, bounds.width - thickness.right),
            0.0,
            std::min(bounds.width, thickness.right),
            bounds.height});
        if (!side) return;
        side = fillSide({
            0.0, 0.0,
            bounds.width,
            std::min(bounds.height, thickness.top)});
        if (!side) return;
        static_cast<void>(fillSide({
            0.0,
            std::max(0.0, bounds.height - thickness.bottom),
            bounds.width,
            std::min(bounds.height, thickness.bottom)}));
        return;
    }
    return;
}
ContentPresenter::ContentPresenter() noexcept
    : FrameworkElement(StaticTypeId()) {}

void ContentPresenter::OnContentPropertyChanged(
    ::Aero::DependencyObject& object,
    const Meta::DependencyPropertyChangedEventArgs&
        change) noexcept {
    auto& presenter =
        static_cast<ContentPresenter&>(object);
    presenter.contentValue_ = change.GetNewValue();
    static_cast<void>(
        presenter.UpdatePresentedText());
}
Base::Result<void>
ContentPresenter::UpdatePresentedText() noexcept {
    if (content_ == nullptr ||
        !PropertyRegistry().Types().IsDerivedFrom(
            content_->RuntimeType(),
            TextBlock::StaticTypeId())) {
        return {};
    }
    Base::String text;
    switch (contentValue_.Kind()) {
    case Meta::ValueKind::String:
        {
            Base::Result<void> assigned =
                text.Assign(
                    contentValue_.AsString());
            if (!assigned) {
                return assigned.GetStatus();
            }
        }
        break;
    case Meta::ValueKind::Boolean:
        {
            Base::Result<void> assigned =
                text.Assign(
                    contentValue_.AsBoolean()
                    ? Base::StringView("True")
                    : Base::StringView("False"));
            if (!assigned) {
                return assigned.GetStatus();
            }
        }
        break;
    case Meta::ValueKind::SignedInteger:
    case Meta::ValueKind::UnsignedInteger:
    case Meta::ValueKind::Double:
        {
            char raw[64]{};
            if (contentValue_.Kind() ==
                Meta::ValueKind::SignedInteger) {
                std::snprintf(
                    raw, sizeof(raw), "%lld",
                    static_cast<long long>(
                        contentValue_.
                            AsSignedInteger()));
            } else if (contentValue_.Kind() ==
                       Meta::ValueKind::
                           UnsignedInteger) {
                std::snprintf(
                    raw, sizeof(raw), "%llu",
                    static_cast<
                        unsigned long long>(
                            contentValue_.
                                AsUnsignedInteger()));
            } else {
                std::snprintf(
                    raw, sizeof(raw), "%.15g",
                    contentValue_.AsDouble());
            }
            Base::Result<void> assigned =
                text.Assign(raw);
            if (!assigned) {
                return assigned.GetStatus();
            }
        }
        break;
    case Meta::ValueKind::Object:
        if (!contentValue_.IsNullObject()) {
            return {};
        }
        break;
    default:
        return {};
    }
    auto* textBlock = static_cast<TextBlock*>(content_);
    textBlock->SetValue(RichText::TextProperty, text.View());
    textBlock->SetText(text.View());
    return {};
}
void ContentPresenter::SetContentSource(
    Base::StringView value) noexcept {
    SetValue(
        ContentSourceProperty, value);
}
bool ContentPresenter::IsOnlyAttachedContent(
    const UIElement& content) const noexcept {
    const UIElementChildRange children = LayoutChildren();
    return children.Size() == 1U && children[0] == &content;
}
void ContentPresenter::SetContent(UIElement* content) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return;
    Base::Result<void> validated = ValidateContent(content);
    if (!validated) return;
    if (content == content_) return;
    content_ = content;
    if (content == nullptr) ownedContent_.Reset();
    (void)InvalidateMeasure();
}
void ContentPresenter::SetOwnedContent(
    const Base::Ref<Base::Object>& contentObject,
    UIElement& content) noexcept {
    if (!contentObject || contentObject.Get() != &content) {
        return;
    }
    Base::Result<void> access = VerifyAccess();
    if (!access) return;
    Base::Result<void> validated = ValidateContent(&content);
    if (!validated) return;
    content_ = &content;
    ownedContent_ = contentObject;
    (void)UpdatePresentedText();
    (void)InvalidateMeasure();
}
Base::Result<void> ContentPresenter::ValidateContent(
    UIElement* content) const noexcept {
    if (content == nullptr) {
        if (!LayoutChildren().Empty()) {
            return Base::Status::Failure(Base::ErrorCode::InvalidState,
                "ContentPresenter content must be detached before clearing it");
        }
    } else if (!LayoutChildren().Empty() && !IsOnlyAttachedContent(*content)) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "ContentPresenter content must be its only attached layout child");
    }
    return {};
}
Size ContentPresenter::MeasureOverride(
    Size availableSize) noexcept {
    if (content_ == nullptr) {
        if (!LayoutChildren().Empty()) {
            return Size{};
        }
        return Size{};
    }
    if (!IsOnlyAttachedContent(*content_)) {
        return Size{};
    }
    Base::Result<void> measured = MeasureChild(*content_, availableSize);
    if (!measured) return Size{};
    return content_->GetDesiredSize();
}
Size ContentPresenter::ArrangeOverride(Size finalSize) noexcept {
    if (content_ == nullptr) return finalSize;
    if (!IsOnlyAttachedContent(*content_)) {
        return finalSize;
    }
    Base::Result<void> arranged = ArrangeChild(*content_,
        {0.0, 0.0, finalSize.width, finalSize.height});
    if (!arranged) return finalSize;
    return finalSize;
}

} // namespace Aero
