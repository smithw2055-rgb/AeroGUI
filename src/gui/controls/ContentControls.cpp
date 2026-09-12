#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/internal/AeroGuiInternal.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/media/BrushRendering.hpp"
#include "gui/styles/StyleEngine.hpp"
#include "render/DisplayList.hpp"
#include <Aero/Controls.hpp>
#include <Aero/Controls/ItemContainerGenerator.hpp>
#include <Aero/Controls/BulletDecorator.hpp>
#include <Aero/Controls/ControlTemplate.hpp>
#include <Aero/DataTemplate.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Media/Transforms.hpp>
#include <Aero/Shapes.hpp>
#include <Aero/Documents.hpp>
#include "gui/meta/TypeRegistryDetail.hpp"
#include "gui/meta/ValueConversion.hpp"
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


Popup::Popup() noexcept
    : Popup(StaticTypeId()) {}

Popup::Popup(TypeId runtimeType) noexcept
    : ContentControl(runtimeType) {
    static_cast<void>(SetIsHitTestVisible(false));
}

Popup::~Popup() = default;

bool Popup::GetIsOpen() const noexcept {
    return GetValue(IsOpenProperty);
}

void Popup::SetIsOpen(
    bool value) noexcept {
    SetValue(IsOpenProperty, value);
}

PlacementMode Popup::GetPlacement() const noexcept {
    return GetValue(PlacementProperty);
}

void Popup::SetPlacement(
    PlacementMode value) noexcept {
    SetValue(
        PlacementProperty, value);
}

double Popup::GetHorizontalOffset() const noexcept {
    return GetValue(HorizontalOffsetProperty);
}

void Popup::SetHorizontalOffset(
    double value) noexcept {
    SetValue(
        HorizontalOffsetProperty, value);
}

double Popup::GetVerticalOffset() const noexcept {
    return GetValue(VerticalOffsetProperty);
}

void Popup::SetVerticalOffset(
    double value) noexcept {
    SetValue(
        VerticalOffsetProperty, value);
}

bool Popup::GetStaysOpen() const noexcept {
    return GetValue(StaysOpenProperty);
}

void Popup::SetStaysOpen(
    bool value) noexcept {
    SetValue(
        StaysOpenProperty, value);
}

bool Popup::GetMatchPlacementTargetWidth() const noexcept {
    return GetValue(MatchPlacementTargetWidthProperty);
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
    return GetValue(PlacementTargetProperty);
}

void Popup::SetPlacementTarget(
    Base::Ref<UIElement> value) noexcept {
    SetValue(
        PlacementTargetProperty,
        std::move(value));
}

PopupAnimation Popup::GetPopupAnimation() const noexcept {
    return GetValue(PopupAnimationProperty);
}

void Popup::SetPopupAnimation(
    PopupAnimation value) noexcept {
    SetValue(
        PopupAnimationProperty, value);
}

bool Popup::GetAllowsTransparency() const noexcept {
    return GetValue(AllowsTransparencyProperty);
}

void Popup::SetAllowsTransparency(
    bool value) noexcept {
    SetValue(
        AllowsTransparencyProperty, value);
}

void Popup::OnOpened(RoutedEventArgs& e) {
    RaiseEvent(OpenedEvent, &e);
}

void Popup::OnClosed(RoutedEventArgs& e) {
    RaiseEvent(ClosedEvent, &e);
}

void Popup::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    ContentControl::OnPropertyChanged(args);
    if (args.GetProperty() == IsOpenProperty) {
        const bool open = args.GetNewValue().AsBoolean();
        bool hitTest = open;
        if (open) {
            UIElement* popupChild =
                GetTemplateRoot() != nullptr
                    ? GetTemplateRoot()
                    : GetContentElement();
            // Tooltips set IsHitTestVisible=False on the content so the pointer
            // can keep hitting the placement target. Forcing the Popup itself
            // hittable would steal MouseEnter/Leave from the planet underneath.
            if (popupChild != nullptr &&
                !popupChild->GetIsHitTestVisible()) {
                hitTest = false;
            }
        }
        static_cast<void>(SetIsHitTestVisible(hitTest));
        InvalidateMeasure();
        RoutedEventArgs eventArgs;
        if (open) {
            OnOpened(eventArgs);
        } else {
            OnClosed(eventArgs);
        }
    }
}

Size Popup::MeasureOverride(
    Size availableSize) noexcept {
    (void)availableSize;
    popupDesiredSize_ = {};
    UIElement* popupChild =
        GetTemplateRoot() != nullptr
            ? GetTemplateRoot()
            : GetContentElement();
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
            : GetContentElement();
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
            AeroGuiInternal::PropertyRegistry(*this).Types().IsDerivedFrom(
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
    Point popupScreenOrigin{};
    double rootHeight = 0.0;
    double screenRootWidth = 0.0;
    double screenRootHeight = 0.0;
    double targetScaleX = 1.0;
    double targetScaleY = 1.0;
    double popupScaleX = 1.0;
    double popupScaleY = 1.0;
    if (placementTarget != nullptr) {
        targetSize = placementTarget->GetRenderSize();
        if (!(targetSize.width > 0.0 && targetSize.height > 0.0)) {
            targetSize = placementTarget->GetDesiredSize();
        }
        if (!(targetSize.width > 0.0 && targetSize.height > 0.0)) {
            targetSize = finalSize;
        }
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
        popupScreenOrigin = popupAbsolute;
        targetOrigin = {
            targetAbsolute.x - popupAbsolute.x,
            targetAbsolute.y - popupAbsolute.y};
        if (rootElement != nullptr) {
            rootHeight = rootElement->GetRenderSize().height;
            if (rootHeight <= 0.0) {
                rootHeight = rootElement->GetLayoutSlot().height;
            }
            screenRootWidth = rootElement->GetRenderSize().width;
            if (screenRootWidth <= 0.0) {
                screenRootWidth = rootElement->GetLayoutSlot().width;
            }
            screenRootHeight = rootHeight;
        }
        // A Viewbox letter-boxes its child inside the window. WPF still uses
        // the window for popup flip, which opens a bottom ComboBox into the
        // empty margin. Flip against the scaled content box instead so a
        // control at the bottom of the Viewbox child opens upward.
        ::Aero::Media::Visual* walk = placementTarget;
        while (walk != nullptr) {
            ::Aero::Media::Visual* parentVisual = walk->GetVisualParent();
            if (parentVisual != nullptr &&
                ::Aero::TryCast<Viewbox>(parentVisual) != nullptr) {
                UIElement* viewboxChild =
                    ::Aero::TryCast<UIElement>(walk);
                if (viewboxChild != nullptr) {
                    double childScaleX = 1.0;
                    double childScaleY = 1.0;
                    const Point childOrigin = absoluteOrigin(
                        *viewboxChild, nullptr,
                        &childScaleX, &childScaleY);
                    const Size childSize = viewboxChild->GetRenderSize();
                    const double clipTop = childOrigin.y;
                    const double clipBottom =
                        childOrigin.y + childSize.height * childScaleY;
                    if (clipBottom > clipTop) {
                        targetAbsolute.y -= clipTop;
                        rootHeight = clipBottom - clipTop;
                    }
                }
                break;
            }
            walk = parentVisual;
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
        const double popupHeightAbs =
            contentSize.height * targetScaleY;
        const double targetTopAbs = targetAbsolute.y;
        const double targetBottomAbs =
            targetAbsolute.y + targetHeight * targetScaleY;
        const double spaceBelow = rootHeight - targetBottomAbs;
        const double spaceAbove = targetTopAbs;
        if (placement == PlacementMode::Bottom) {
            const double bottomAbsolute =
                targetBottomAbs +
                GetVerticalOffset() * targetScaleY +
                popupHeightAbs;
            if (bottomAbsolute > rootHeight &&
                spaceAbove > spaceBelow) {
                y = targetOriginLocal.y -
                    contentSize.height -
                    GetVerticalOffset();
            }
        } else if (placement == PlacementMode::Top) {
            const double topAbsolute =
                targetTopAbs -
                GetVerticalOffset() * targetScaleY -
                popupHeightAbs;
            if (topAbsolute < 0.0 &&
                spaceBelow > spaceAbove) {
                y = targetOriginLocal.y +
                    targetHeight +
                    GetVerticalOffset();
            }
        }
    }

    if (screenRootWidth > 0.0 || screenRootHeight > 0.0) {
        double screenX =
            popupScreenOrigin.x + x * popupScaleX;
        double screenY =
            popupScreenOrigin.y + y * popupScaleY;
        const double screenW = contentSize.width * popupScaleX;
        const double screenH = contentSize.height * popupScaleY;
        if (screenRootWidth > 0.0 &&
            screenX + screenW > screenRootWidth) {
            screenX = screenRootWidth - screenW;
        }
        if (screenX < 0.0) {
            screenX = 0.0;
        }
        if (screenRootHeight > 0.0 &&
            screenY + screenH > screenRootHeight) {
            screenY = screenRootHeight - screenH;
        }
        if (screenY < 0.0) {
            screenY = 0.0;
        }
        x = popupScaleX != 0.0
            ? (screenX - popupScreenOrigin.x) / popupScaleX
            : screenX;
        y = popupScaleY != 0.0
            ? (screenY - popupScreenOrigin.y) / popupScaleY
            : screenY;
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

HeaderedContentControl::HeaderedContentControl(
    TypeId runtimeType) noexcept
    : ContentControl(runtimeType) {}

HeaderedContentControl::~HeaderedContentControl() = default;

void HeaderedContentControl::OnHeaderChanged(
    const Value&,
    const Value&) {
    ProjectHeaderContent();
}

void HeaderedContentControl::OnHeaderTemplateChanged(
    const Ref<DataTemplate>&,
    const Ref<DataTemplate>&) {}

void HeaderedContentControl::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    ContentControl::OnPropertyChanged(args);
    const DependencyPropertyHandle prop = args.GetProperty();
    if (prop == HeaderProperty) {
        OnHeaderChanged(args.GetOldValue(), args.GetNewValue());
    } else if (prop == HeaderTemplateProperty) {
        const auto toTemplate = [](const Value& v) -> Ref<DataTemplate> {
            if (v.Kind() == ValueKind::Object && v.AsObject()) {
                if (auto* dt = TryCast<DataTemplate>(v.AsObject().Get())) {
                    return Ref<DataTemplate>::FromBorrowed(*dt);
                }
            }
            return {};
        };
        OnHeaderTemplateChanged(
            toTemplate(args.GetOldValue()),
            toTemplate(args.GetNewValue()));
    }
}

Meta::Value
HeaderedContentControl::GetHeader() const noexcept {
    return GetValue(HeaderProperty);
}

void HeaderedContentControl::SetHeader(
    const Meta::Value& value) noexcept {
    SetValue(HeaderProperty, value);
}

void HeaderedContentControl::SetHeader(
    Base::StringView value) noexcept {
    Base::Result<Value> boxed = Value::TryFromString(
        Meta::TypeOf<Base::String>(), value);
    if (!boxed) { AERO_ASSERT(false); return; }
    SetHeader(std::move(boxed).Value());
}

Base::Ref<DataTemplate>
HeaderedContentControl::GetHeaderTemplate() const noexcept {
    return GetValue(HeaderTemplateProperty);
}

void
HeaderedContentControl::SetHeaderTemplate(
    Base::Ref<DataTemplate> value) noexcept {
    SetValue(
        HeaderTemplateProperty,
        std::move(value));
}

void HeaderedContentControl::OnApplyTemplate() noexcept {
    Control::OnApplyTemplate();
    ProjectHeaderContent();
}

void HeaderedContentControl::ProjectHeaderContent() noexcept {
    const Value header = GetHeader();
    if (header.Kind() != ValueKind::Object ||
        header.IsNullObject() ||
        !header.AsObject()) {
        return;
    }
    Base::Object* obj = header.AsObject().Get();
    if (!AeroGuiInternal::PropertyRegistry(*this).Types().IsDerivedFrom(
            obj->RuntimeType(), UIElement::StaticTypeId())) {
        return;
    }
    auto* element = static_cast<UIElement*>(obj);
    DependencyObject* part = GetTemplateChild(Base::StringView("HeaderHost"));
    if (part == nullptr) {
        part = GetTemplateChild(Base::StringView("PART_Header"));
    }
    auto* presenter =
        part != nullptr ? ::Aero::TryCast<ContentPresenter>(part) : nullptr;
    if (presenter == nullptr) {
        return;
    }
    presenter->HostUiElement(header.AsObject(), *element);
}

Expander::Expander() noexcept
    : HeaderedContentControl(StaticTypeId()),
      headerCheckedHandler_(
          this,
          &Expander::OnHeaderCheckedChanged) {}

Expander::~Expander() {
    UnbindHeaderToggle();
}

bool Expander::GetIsExpanded() const noexcept {
    return GetValue(IsExpandedProperty);
}

void Expander::SetIsExpanded(
    bool value) noexcept {
    const bool old = GetIsExpanded();
    if (old == value) return;
    SetValue(IsExpandedProperty, value);
}

void Expander::OnExpanded() {
    RoutedEventArgs eventArgs;
    RaiseEvent(ExpandedEvent, &eventArgs);
}

void Expander::OnCollapsed() {
    RoutedEventArgs eventArgs;
    RaiseEvent(CollapsedEvent, &eventArgs);
}

void Expander::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    HeaderedContentControl::OnPropertyChanged(args);
    if (args.GetProperty() == IsExpandedProperty) {
        const bool expanded = args.GetNewValue().AsBoolean();
        if (!synchronizingHeader_ && headerToggle_ != nullptr) {
            const Nullable<bool> isChecked = headerToggle_->GetIsChecked();
            const bool checked =
                isChecked.GetHasValue() ? isChecked.GetValue() : false;
            if (checked != expanded) {
                synchronizingHeader_ = true;
                headerToggle_->SetIsChecked(Nullable<bool>{expanded});
                synchronizingHeader_ = false;
            }
        }
        InvalidateMeasure();
        if (expanded) {
            OnExpanded();
        } else {
            OnCollapsed();
        }
    }
}

void Expander::OnHeaderCheckedChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs&) noexcept {
    if (synchronizingHeader_ || headerToggle_ == nullptr) {
        return;
    }
    const Nullable<bool> isChecked = headerToggle_->GetIsChecked();
    const bool checked =
        isChecked.GetHasValue() ? isChecked.GetValue() : false;
    if (checked == GetIsExpanded()) {
        return;
    }
    synchronizingHeader_ = true;
    SetIsExpanded(checked);
    synchronizingHeader_ = false;
}

void Expander::UnbindHeaderToggle() noexcept {
    if (headerToggle_ == nullptr) {
        return;
    }
    static_cast<void>(headerToggle_->RemoveValueChangedHandler(
        ToggleButton::IsCheckedProperty,
        headerCheckedHandler_));
    headerToggle_ = nullptr;
}

void Expander::BindHeaderToggle() noexcept {
    UnbindHeaderToggle();
    UIElement* root = GetTemplateRoot();
    if (root == nullptr) {
        return;
    }
    const auto findHeader = [this](auto& self, Aero::Media::Visual& visual)
        -> ToggleButton* {
        if (auto* toggle = TryCast<ToggleButton>(&visual)) {
            if (toggle->GetTemplatedParent() == this) {
                return toggle;
            }
        }
        const std::uint32_t count =
            Aero::Media::VisualTreeHelper::GetChildrenCount(visual);
        for (std::uint32_t index = 0U; index < count; ++index) {
            Aero::Media::Visual* child =
                Aero::Media::VisualTreeHelper::GetChild(visual, index);
            if (child == nullptr) {
                continue;
            }
            if (ToggleButton* found = self(self, *child)) {
                return found;
            }
        }
        return nullptr;
    };
    headerToggle_ = findHeader(findHeader, *root);
    if (headerToggle_ == nullptr) {
        return;
    }
    static_cast<void>(headerToggle_->AddValueChangedHandler(
        ToggleButton::IsCheckedProperty,
        headerCheckedHandler_));
    const Nullable<bool> isChecked = headerToggle_->GetIsChecked();
    const bool checked =
        isChecked.GetHasValue() ? isChecked.GetValue() : false;
    if (checked != GetIsExpanded()) {
        synchronizingHeader_ = true;
        headerToggle_->SetIsChecked(Nullable<bool>{GetIsExpanded()});
        synchronizingHeader_ = false;
    }
}

void Expander::OnApplyTemplate() noexcept {
    HeaderedContentControl::OnApplyTemplate();
    BindHeaderToggle();
}

void Expander::OnTemplateDetached() noexcept {
    UnbindHeaderToggle();
    HeaderedContentControl::OnTemplateDetached();
}

ExpandDirection Expander::GetDirection() const noexcept {
    return GetValue(ExpandDirectionProperty);
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
    if (!GetIsExpanded() || GetContentElement() == nullptr) {
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
        MeasureChild(*GetContentElement(), childAvailable);
    if (!measured) return Size{};
    const Size desired = GetContentElement()->GetDesiredSize();
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
    if (!GetIsExpanded() || GetContentElement() == nullptr) {
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
        ArrangeChild(*GetContentElement(), slot);
    if (!arranged) return finalSize;
    return finalSize;
}

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
        AeroGuiInternal::PropertyRegistry(*this).Types().IsDerivedFrom(
            item->RuntimeType(), TabItem::StaticTypeId())) {
        return static_cast<TabItem*>(item.Get());
    }
    ItemContainerGenerator* generator = AttachedGenerator();
    if (generator == nullptr) return nullptr;
    FrameworkElement* container = generator->ContainerFromIndex(selected);
    if (container != nullptr &&
        AeroGuiInternal::PropertyRegistry(*this).Types().IsDerivedFrom(
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

Base::Result<void>
TabControl::SynchronizeSelection() noexcept {
    const std::uint32_t value = GetSelectedIndex();
    const std::uint32_t count = GetCount();
    ItemContainerGenerator* generator = AttachedGenerator();
    for (std::uint32_t index = 0U; index < count; ++index) {
        TabItem* tab = nullptr;
        const Ref<Base::Object> item = GetItem(index);
        if (item &&
            AeroGuiInternal::PropertyRegistry(*this).Types().IsDerivedFrom(
                item->RuntimeType(), TabItem::StaticTypeId())) {
            tab = static_cast<TabItem*>(item.Get());
        } else if (generator != nullptr) {
            FrameworkElement* container = generator->ContainerFromIndex(index);
            if (container != nullptr &&
                AeroGuiInternal::PropertyRegistry(*this).Types().IsDerivedFrom(
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
    return {};
}

void TabControl::OnSelectionChanged(
    const SelectionChangedEvent& event) {
    Selector::OnSelectionChanged(event);
    static_cast<void>(SynchronizeSelection());
}

void TabControl::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    Selector::OnPropertyChanged(args);
    if (args.GetProperty() == SelectedIndexProperty) {
        static_cast<void>(SynchronizeSelection());
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
            AeroGuiInternal::PropertyRegistry(*this).Types().IsDerivedFrom(
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
        AeroGuiInternal::PropertyRegistry(*this).Types().IsDerivedFrom(
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

Stretch Viewbox::GetStretch() const noexcept {
    return GetValue(StretchProperty);
}
StretchDirection
Viewbox::GetStretchDirection() const noexcept {
    return GetValue(StretchDirectionProperty);
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
void Viewbox::ApplyViewTransform(
    double scaleX,
    double scaleY,
    double offsetX,
    double offsetY) noexcept {
    UIElement* child = GetChild();
    FrameworkElement* framework = child != nullptr
        ? ::Aero::TryCast<::Aero::FrameworkElement>(child)
        : nullptr;
    auto clearStretch = [](FrameworkElement* element) noexcept {
        if (element == nullptr) return;
        Base::Transform2D leftover{};
        if (!element->TryGetViewboxTransform(leftover)) return;
        element->ClearViewboxTransform();
        static_cast<void>(
            AeroGuiInternal::InvalidateRenderState(*element));
    };
    // Stretch stays on this Viewbox (AeroGUI wrapper Decorator), never on the
    // child: Hexagon grids have ScaleTransform 1.2, Board has RotationY.
    // Putting stretch on those nodes composed it after their own transforms
    // around the unscaled origin, which shifted score digits and collapsed
    // a 90° flip. DropShadow offscreen bakes this matrix in FrameEncoder.
    if (projectedChild_ && projectedChild_.Get() != this) {
        clearStretch(projectedChild_.Get());
    }
    if (child == nullptr) {
        clearStretch(this);
        viewTransform_.Reset();
        projectedChild_.Reset();
        return;
    }
    Base::Transform2D matrix;
    matrix.m11 = scaleX;
    matrix.m22 = scaleY;
    matrix.dx = offsetX;
    matrix.dy = offsetY;
    clearStretch(framework);
    const bool changed = SetViewboxTransform(matrix);
    projectedChild_ = framework != nullptr
        ? Base::Ref<FrameworkElement>::FromBorrowed(*framework)
        : Base::Ref<FrameworkElement>{};
    if (changed) {
        static_cast<void>(
            AeroGuiInternal::InvalidateRenderState(*this));
    }
}
Size Viewbox::ArrangeOverride(
    Size finalSize) noexcept {
    UIElement* child = GetChild();
    if (child == nullptr) {
        ApplyViewTransform(1.0, 1.0, 0.0, 0.0);
        return finalSize;
    }

    const Size natural = child->GetDesiredSize();
    if (natural.width <= 0.0 || natural.height <= 0.0) {
        Base::Result<void> arranged = ArrangeChild(
            *child, {0.0, 0.0, 0.0, 0.0});
        if (!arranged) return finalSize;
        ApplyViewTransform(1.0, 1.0, 0.0, 0.0);
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
    // Child layout stays in unscaled local pixels (Transform3D CenterX/Y,
    // ScaleTransform origin). Stretch lives on this Viewbox. Centering
    // offset is part of that matrix, not the child slot.
    ApplyViewTransform(
        scaleX,
        scaleY,
        offsetX,
        offsetY);
    Base::Result<void> arranged = ArrangeChild(
        *child,
        {0.0, 0.0, natural.width, natural.height});
    if (!arranged) return finalSize;
    // Non-FrameworkElement children compensate RenderTransformOrigin from
    // the arranged RenderSize; re-apply so that origin stays correct.
    ApplyViewTransform(
        scaleX,
        scaleY,
        offsetX,
        offsetY);
    return finalSize;
}
Border::Border() noexcept : Decorator(StaticTypeId()) {}

Base::Ref<Brush> Border::GetBackground() const noexcept {
    return GetValue(BackgroundProperty);
}
Base::Ref<Brush> Border::GetBorderBrush() const noexcept {
    return GetValue(BorderBrushProperty);
}
Thickness Border::GetBorderThickness() const noexcept {
    return GetValue(BorderThicknessProperty);
}
CornerRadius Border::GetCornerRadius() const noexcept {
    return GetValue(CornerRadiusProperty);
}
Thickness Border::GetPadding() const noexcept {
    return GetValue(PaddingProperty);
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
    auto& builder = Aero::Render::DrawingBridge::Builder(context);
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


namespace {

void AttachOwnedContentSubtree(
    ElementTree& tree,
    UIElement& parent) noexcept {
    const auto attachChild = [&](UIElement& child) noexcept {
        if (child.GetVisualParent() == &parent &&
            VisualTree(child) == &tree &&
            child.GetIsLayoutAttached()) {
            AttachOwnedContentSubtree(tree, child);
            return;
        }
        if (child.GetVisualParent() != nullptr &&
            child.GetVisualParent() != &parent) {
            static_cast<void>(tree.DetachVisual(
                *child.GetVisualParent(),
                static_cast<::Aero::Media::Visual&>(child)));
        }
        if (VisualTree(child) == nullptr &&
            child.GetLogicalParent() == nullptr) {
            static_cast<void>(tree.AttachElement(parent, child));
        } else if (child.GetVisualParent() != &parent ||
                   !child.GetIsLayoutAttached()) {
            static_cast<void>(tree.AttachVisualChild(parent, child));
        }
        if (Aero::BindingEngine* bindings =
                Aero::AeroGuiInternal::BindingEngineOf(child)) {
            static_cast<void>(bindings->ActivateDeferredWhenReady(child));
        }
        AttachOwnedContentSubtree(tree, child);
    };

    if (AeroGuiInternal::PropertyRegistry(parent).Types().IsDerivedFrom(
            parent.RuntimeType(), Controls::Panel::StaticTypeId())) {
        auto& panel = static_cast<Controls::Panel&>(parent);
        const std::uint32_t count = AeroGuiInternal::PanelChildCount(panel);
        for (std::uint32_t index = 0U; index < count; ++index) {
            const Base::Ref<Base::Object> owned =
                AeroGuiInternal::PanelChildAt(panel, index);
            if (!owned ||
                !AeroGuiInternal::PropertyRegistry(parent).Types().IsDerivedFrom(
                    owned->RuntimeType(), UIElement::StaticTypeId())) {
                continue;
            }
            attachChild(*static_cast<UIElement*>(owned.Get()));
        }
        return;
    }
    if (AeroGuiInternal::PropertyRegistry(parent).Types().IsDerivedFrom(
            parent.RuntimeType(), Controls::Decorator::StaticTypeId())) {
        const Base::Ref<Base::Object>& owned =
            AeroGuiInternal::DecoratorOwnedChild(
                static_cast<Controls::Decorator&>(parent));
        if (owned &&
            AeroGuiInternal::PropertyRegistry(parent).Types().IsDerivedFrom(
                owned->RuntimeType(), UIElement::StaticTypeId())) {
            attachChild(*static_cast<UIElement*>(owned.Get()));
        }
        return;
    }
    if (AeroGuiInternal::PropertyRegistry(parent).Types().IsDerivedFrom(
            parent.RuntimeType(), ContentPresenter::StaticTypeId())) {
        auto& presenter = static_cast<ContentPresenter&>(parent);
        const Base::Ref<Base::Object>& owned = presenter.GetOwnedContent();
        if (owned &&
            AeroGuiInternal::PropertyRegistry(parent).Types().IsDerivedFrom(
                owned->RuntimeType(), UIElement::StaticTypeId())) {
            attachChild(*static_cast<UIElement*>(owned.Get()));
        }
        return;
    }
    if (AeroGuiInternal::PropertyRegistry(parent).Types().IsDerivedFrom(
            parent.RuntimeType(), Controls::ContentControl::StaticTypeId())) {
        const Base::Ref<Base::Object>& owned =
            AeroGuiInternal::OwnedContent(
                static_cast<Controls::ContentControl&>(parent));
        if (owned &&
            AeroGuiInternal::PropertyRegistry(parent).Types().IsDerivedFrom(
                owned->RuntimeType(), UIElement::StaticTypeId())) {
            attachChild(*static_cast<UIElement*>(owned.Get()));
        }
    }
}

} // namespace

void ContentPresenter::HostUiElement(
    const Base::Ref<Base::Object>& owner,
    UIElement& element) noexcept {
    if (!owner || owner.Get() != &element) {
        return;
    }
    ElementTree* tree = VisualTree(this);
    const auto detachHosted = [&](UIElement& hosted) noexcept {
        if (tree == nullptr) {
            return;
        }
        ::Aero::VisualAttachment state;
        state.visualParent =
            hosted.GetVisualParent() != nullptr
            ? hosted.GetVisualParent()
            : static_cast<::Aero::Media::Visual*>(this);
        state.child = &hosted;
        state.visualAttached = hosted.GetVisualParent() != nullptr;
        state.layoutAttached =
            hosted.GetIsLayoutAttached() &&
            hosted.LayoutParent() != nullptr;
        state.renderAttached = false;
        if (state.IsAttached()) {
            static_cast<void>(tree->DetachVisual(state));
        }
    };

    UIElement* existing = content_;
    if (existing != nullptr && existing != &element) {
        detachHosted(*existing);
        SetContent(nullptr);
        if (content_ == existing) {
            content_ = nullptr;
            ownedContent_.Reset();
        }
    }
    if (tree != nullptr) {
        const UIElementChildRange children = LayoutChildren();
        for (std::uint32_t index = children.Size(); index > 0U; --index) {
            UIElement* child = children[index - 1U];
            if (child == nullptr || child == &element) {
                continue;
            }
            detachHosted(*child);
        }
    }
    if (element.GetVisualParent() != nullptr &&
        element.GetVisualParent() != this) {
        detachHosted(element);
    }
    if (tree != nullptr &&
        (element.GetVisualParent() != this ||
         !element.GetIsLayoutAttached())) {
        // AttachVisual requires the child to already be a tree member.
        // Authored Header visuals and DataTemplate roots often are not;
        // AttachElement joins them first. LoadComponent can also leave the
        // visual parent set while layout is still detached.
        if (VisualTree(element) == nullptr &&
            element.GetLogicalParent() == nullptr) {
            static_cast<void>(tree->AttachElement(*this, element));
        } else if (element.GetVisualParent() == nullptr ||
                   element.GetVisualParent() == this) {
            static_cast<void>(tree->AttachVisualChild(*this, element));
        }
    }
    SetOwnedContent(owner, element);
    if (content_ != &element) {
        content_ = &element;
        ownedContent_ = owner;
        InvalidateMeasure();
    }
    if (tree != nullptr) {
        AttachOwnedContentSubtree(*tree, element);
    }
}

void ContentPresenter::OnContentPropertyChanged(
    ::Aero::DependencyObject& object,
    const Meta::DependencyPropertyChangedEventArgs&
        change) noexcept {
    auto& presenter =
        static_cast<ContentPresenter&>(object);
    presenter.contentValue_ = change.GetNewValue();
    const Value& value = presenter.contentValue_;
    if (value.Kind() == Meta::ValueKind::Object &&
        !value.IsNullObject() &&
        value.AsObject()) {
        Base::Object* obj = value.AsObject().Get();
        if (AeroGuiInternal::PropertyRegistry(presenter).Types().IsDerivedFrom(
                obj->RuntimeType(), UIElement::StaticTypeId())) {
            auto* element = static_cast<UIElement*>(obj);
            presenter.HostUiElement(value.AsObject(), *element);
            return;
        }
    }
    static_cast<void>(
        presenter.UpdatePresentedText());
}

void ContentPresenter::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    if (args.GetProperty() == ContentProperty.Handle()) {
        OnContentPropertyChanged(*this, args);
    }
    FrameworkElement::OnPropertyChanged(args);
}
Base::Result<void>
ContentPresenter::UpdatePresentedText() noexcept {
    if (content_ == nullptr ||
        !AeroGuiInternal::PropertyRegistry(*this).Types().IsDerivedFrom(
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
    InvalidateMeasure();
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
    InvalidateMeasure();
    if (ElementTree* tree = VisualTree(this)) {
        AttachOwnedContentSubtree(*tree, content);
    }
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
        return Size{};
    }
    // WPF ContentPresenter measures its content regardless of whether the
    // layout-child table still lists it as the only child. Returning an empty
    // size here collapses UniformGrid rows whose cells bind Height to
    // ActualWidth (Inventory slots).
    Base::Result<void> measured = MeasureChild(*content_, availableSize);
    if (!measured) return Size{};
    return content_->GetDesiredSize();
}
Size ContentPresenter::ArrangeOverride(Size finalSize) noexcept {
    if (content_ == nullptr) return finalSize;
    Base::Result<void> arranged = ArrangeChild(*content_,
        {0.0, 0.0, finalSize.width, finalSize.height});
    if (!arranged) return finalSize;
    return finalSize;
}

namespace {

class BasicControl : public Control {
public:
    BasicControl() noexcept : Control(Control::StaticTypeId()) {}
};

class BasicContentControl : public ContentControl {
public:
    BasicContentControl() noexcept
        : ContentControl(ContentControl::StaticTypeId()) {}
};

class BasicHeaderedContentControl : public HeaderedContentControl {
public:
    BasicHeaderedContentControl() noexcept
        : HeaderedContentControl(HeaderedContentControl::StaticTypeId()) {}
};

void SetDecoratorContent(
    Base::Object& owner,
    const Base::Ref<Base::Object>& child,
    void*) noexcept {
    if (!child) {
        return;
    }
    (void)AeroGuiInternal::DecoratorSetOwnedChild(
        static_cast<Decorator&>(owner), child, *static_cast<Aero::UIElement*>(child.Get()));
}

void ClearDecoratorContent(
    Base::Object& owner,
    void*) noexcept {
    static_cast<Decorator&>(owner).SetChild(nullptr);
}

void AddBulletDecoratorContent(
    Base::Object& owner,
    const Base::Ref<Base::Object>& child,
    void*) noexcept {
    if (!child) return;
    auto& decorator = static_cast<BulletDecorator&>(owner);
    Base::Ref<UIElement> retained =
        Base::Ref<UIElement>::TryFromBorrowed(
            *static_cast<UIElement*>(child.Get()));
    if (!retained) return;
    if (decorator.GetBullet() == nullptr) {
        decorator.SetBullet(std::move(retained));
    } else {
        decorator.SetChild(std::move(retained));
    }
}

void ClearBulletDecoratorContent(
    Base::Object& owner,
    void*) noexcept {
    auto& decorator = static_cast<BulletDecorator&>(owner);
    decorator.SetBullet({});
    decorator.SetChild({});
}

void SetContentControlContent(
    Base::Object& owner,
    const Base::Ref<Base::Object>& child,
    void*) noexcept {
    if (!child) {
        return;
    }
    (void)AeroGuiInternal::SetContentValue(
        static_cast<ContentControl&>(owner), child);
}

void ClearContentControlContent(
    Base::Object& owner,
    void*) noexcept {
    (void)AeroGuiInternal::SetContentValue(
        static_cast<ContentControl&>(owner), Meta::Value::NullObject(Meta::TypeOf<Base::Object>()));
}

void SetContentPresenterContent(
    Base::Object& owner,
    const Base::Ref<Base::Object>& child,
    void*) noexcept {
    if (!child) {
        return;
    }
    static_cast<ContentPresenter&>(owner).SetOwnedContent(
        child, *static_cast<Aero::UIElement*>(child.Get()));
}

void ClearContentPresenterContent(
    Base::Object& owner,
    void*) noexcept {
    static_cast<ContentPresenter&>(owner).SetContent(nullptr);
}

bool ValidateCornerRadiusValue(
    const Aero::CornerRadius& radius) noexcept {
    return std::isfinite(radius.topLeft) &&
        std::isfinite(radius.topRight) &&
        std::isfinite(radius.bottomRight) &&
        std::isfinite(radius.bottomLeft) &&
        radius.topLeft >= 0.0 &&
        radius.topRight >= 0.0 &&
        radius.bottomRight >= 0.0 &&
        radius.bottomLeft >= 0.0;
}

} // namespace

void Control::RegisterMetadata(::Aero::Meta::Registration& context) noexcept {
    using namespace Aero::Meta;
    Register<Control>(context)
        .Event(Control::PreviewMouseDoubleClickEvent, RoutingStrategy::Tunnel)
        .Event(Control::MouseDoubleClickEvent)
        .Override(UIElement::FocusableProperty, true, FrameworkPropertyMetadataOptions::None)
        .Override(UIElement::IsTabStopProperty, true, FrameworkPropertyMetadataOptions::None)
        .Property(Control::BackgroundProperty, Base::Ref<Media::Brush>{}, AffectsRender)
        .Property(Control::BorderBrushProperty, Base::Ref<Media::Brush>{}, AffectsRender)
        .Property(Control::BorderThicknessProperty, Aero::Thickness{}, AffectsMeasure | AffectsRender, &ValidateThicknessValue)
        .Property(Control::PaddingProperty, Aero::Thickness{}, AffectsMeasure, &ValidateThicknessValue)
        .Property(Control::FontWeightProperty, FontWeight::Normal, AffectsMeasure)
        .Property(Control::HorizontalContentAlignmentProperty, Aero::HorizontalAlignment::Left, AffectsArrange)
        .Property(Control::VerticalContentAlignmentProperty, Aero::VerticalAlignment::Top, AffectsArrange)
        .Property(Control::FontSizeProperty, 15.0, Inherits | AffectsMeasure, &ValidatePositiveFiniteDouble)
        .Property(Control::FocusVisualStyleProperty, Base::Ref<Aero::Style>{})
        .Property(Control::OverridesDefaultStyleProperty, false)
        .Property(Control::TemplateProperty, Base::Ref<ControlTemplate>{}, AffectsMeasure)
        .Factory<BasicControl>();
}

void ContentControl::RegisterMetadata(::Aero::Meta::Registration& context) noexcept {
    using namespace Aero::Meta;
    Register<ContentControl>(context)
        .Property(ContentControl::ContentProperty, FrameworkPropertyMetadata(Meta::Value::NullObject(Meta::TypeOf<Base::Object>()), AffectsMeasure).Structural())
        .Property(ContentControl::ContentTemplateProperty, Base::Ref<Base::Object>{}, AffectsMeasure)
        .Property(ContentControl::ContentTemplateSelectorProperty, Base::Ref<Base::Object>{}, AffectsMeasure)
        .ContentAccessor(MakeMemberId(ContentControl::StaticTypeId(), MemberKind::Property, "Content"), ContentKind::Single, &SetContentControlContent, &ClearContentControlContent, ContentFlags::Visual)
        .Factory<BasicContentControl>();
}

void HeaderedContentControl::RegisterMetadata(::Aero::Meta::Registration& context) noexcept {
    using namespace Aero::Meta;
    Register<HeaderedContentControl>(context)
        .Property(HeaderedContentControl::HeaderProperty, Meta::Value::NullObject(Meta::TypeOf<Base::Object>()), AffectsMeasure)
        .Property(HeaderedContentControl::HeaderTemplateProperty, Base::Ref<DataTemplate>{}, AffectsMeasure)
        .Factory<BasicHeaderedContentControl>();
}

void Decorator::RegisterMetadata(::Aero::Meta::Registration& context) noexcept {
    using namespace Aero::Meta;
    Register<Decorator>(context)
        .Content<Aero::UIElement>("Content", ContentKind::Single, &SetDecoratorContent, &ClearDecoratorContent, ContentFlags::Visual)
        .Factory();
}

void BulletDecorator::RegisterMetadata(::Aero::Meta::Registration& context) noexcept {
    using namespace Aero::Meta;
    Register<BulletDecorator>(context)
        .Property(BulletDecorator::BackgroundProperty, Base::Ref<Aero::Media::Brush>{}, AffectsRender)
        .Content<Aero::UIElement>("Bullet", ContentKind::Collection, &AddBulletDecoratorContent, &ClearBulletDecoratorContent, ContentFlags::Visual)
        .Factory();
}

void Viewbox::RegisterMetadata(::Aero::Meta::Registration& context) noexcept {
    using namespace Aero::Meta;
    Register<Viewbox>(context)
        .Property(Viewbox::StretchProperty, Stretch::Uniform, AffectsMeasure)
        .Property(Viewbox::StretchDirectionProperty, StretchDirection::Both, AffectsMeasure)
        .Content<Aero::UIElement>("Content", ContentKind::Single, &SetDecoratorContent, &ClearDecoratorContent, ContentFlags::Visual)
        .Factory();
}

void Border::RegisterMetadata(::Aero::Meta::Registration& context) noexcept {
    using namespace Aero::Meta;
    Register<Border>(context)
        .Property(Border::BackgroundProperty, Base::Ref<Media::Brush>{}, AffectsRender)
        .Property(Border::BorderBrushProperty, Base::Ref<Media::Brush>{}, AffectsRender)
        .Property(Border::BorderThicknessProperty, Aero::Thickness{}, AffectsMeasure | AffectsRender, &ValidateThicknessValue)
        .Property(Border::CornerRadiusProperty, Aero::CornerRadius{}, AffectsRender, &ValidateCornerRadiusValue)
        .Property(Border::PaddingProperty, Aero::Thickness{}, AffectsMeasure, &ValidateThicknessValue)
        .Factory();
}

void ContentPresenter::RegisterMetadata(::Aero::Meta::Registration& context) noexcept {
    using namespace Aero::Meta;
    Base::String defaultContentSource;
    (void)defaultContentSource.Assign(Base::StringView("Content"));

    Register<ContentPresenter>(context)
        .Property(ContentPresenter::ContentProperty, FrameworkPropertyMetadata(Meta::Value::NullObject(Meta::TypeOf<Base::Object>()), AffectsMeasure).Structural())
        .Property(ContentPresenter::ContentTemplateProperty, Base::Ref<Base::Object>{}, AffectsMeasure)
        .Property(ContentPresenter::ContentSourceProperty, std::move(defaultContentSource))
        .ContentAccessor(MakeMemberId(ContentPresenter::StaticTypeId(), MemberKind::Property, "Content"), ContentKind::Single, &SetContentPresenterContent, &ClearContentPresenterContent, ContentFlags::Visual)
        .Factory();
}

void UserControl::RegisterMetadata(::Aero::Meta::Registration& context) noexcept {
    using namespace Aero::Meta;
    Register<UserControl>(context)
        .Factory();
}

void Page::RegisterMetadata(::Aero::Meta::Registration& context) noexcept {
    using namespace Aero::Meta;
    Register<Page>(context)
        .Factory();
}

void GroupBox::RegisterMetadata(::Aero::Meta::Registration& context) noexcept {
    using namespace Aero::Meta;
    Register<GroupBox>(context)
        .Factory();
}

void Label::RegisterMetadata(::Aero::Meta::Registration& context) noexcept {
    using namespace Aero::Meta;
    Register<Label>(context)
        .Factory();
}

void Expander::RegisterMetadata(::Aero::Meta::Registration& context) noexcept {
    using namespace Aero::Meta;
    Register<Expander>(context)
        .Event(Expander::ExpandedEvent)
        .Event(Expander::CollapsedEvent)
        .Property(Expander::IsExpandedProperty, false, AffectsMeasure)
        .Property(Expander::ExpandDirectionProperty, ExpandDirection::Down, AffectsMeasure)
        .Factory();
}

void TabItem::RegisterMetadata(::Aero::Meta::Registration& context) noexcept {
    using namespace Aero::Meta;
    Register<TabItem>(context)
        .Property(TabItem::IsSelectedProperty, false, AffectsRender)
        .Factory();
}

void TabControl::RegisterMetadata(::Aero::Meta::Registration& context) noexcept {
    using namespace Aero::Meta;
    Register<TabControl>(context)
        .Property(TabControl::SelectedContentProperty, Meta::Value::NullObject(Meta::TypeOf<Base::Object>()))
        .Property(TabControl::ContentTemplateProperty, Base::Ref<DataTemplate>{}, AffectsMeasure)
        .Property(TabControl::TabStripPlacementProperty, Dock::Top, AffectsMeasure)
        .Factory();
}

void TabPanel::RegisterMetadata(::Aero::Meta::Registration& context) noexcept {
    using namespace Aero::Meta;
    Register<TabPanel>(context)
        .Factory();
}

namespace Primitives {

void Popup::RegisterMetadata(::Aero::Meta::Registration& context) noexcept {
    using namespace Aero::Meta;
    Register<Popup>(context)
        .Event(Popup::OpenedEvent)
        .Event(Popup::ClosedEvent)
        .Property(Popup::IsOpenProperty, false, AffectsMeasure | AffectsRender | BindsTwoWayByDefault)
        .Property(Popup::PlacementProperty, PlacementMode::Bottom, AffectsArrange)
        .Property(Popup::HorizontalOffsetProperty, 0.0, AffectsArrange, &Base::Validate::Finite<double>)
        .Property(Popup::VerticalOffsetProperty, 0.0, AffectsArrange, &Base::Validate::Finite<double>)
        .Property(Popup::StaysOpenProperty, true)
        .Property(Popup::MatchPlacementTargetWidthProperty, false, AffectsArrange)
        .Property(Popup::PlacementTargetProperty, Base::Ref<UIElement>{}, AffectsArrange)
        .Property(Popup::PopupAnimationProperty, PopupAnimation::None, AffectsRender)
        .Property(Popup::AllowsTransparencyProperty, false, AffectsRender)
        .Factory();
}

} // namespace Primitives

} // namespace Aero
