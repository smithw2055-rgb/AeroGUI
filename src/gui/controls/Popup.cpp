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
    MeasureChild(*popupChild, Size{Unconstrained, Unconstrained});
    popupDesiredSize_ =
        popupChild->GetDesiredSize();
    // Popup content participates in rendering and input, but never consumes
    // space in its placement target's layout.
    return Size{};
}

Rect PlacePopupContent(FrameworkElement& host, Size contentSize, Size finalSize, UIElement* placementTarget,
    Primitives::PlacementMode placementMode, double horizontalOffset, double verticalOffset,
    bool matchPlacementTargetWidth) noexcept {
    if (placementTarget == nullptr) {
        DependencyObject* templatedParent =
            host.GetTemplatedParent();
        if (templatedParent != nullptr &&
            DependencyObjectAccess::PropertyRegistry((host)).Types().IsDerivedFrom(
                templatedParent->RuntimeType(),
                UIElement::StaticTypeId())) {
            placementTarget =
                static_cast<UIElement*>(
                    templatedParent);
        } else if (host.GetVisualParent() != nullptr) {
            placementTarget =
                ::Aero::TryCast<::Aero::UIElement>(host.GetVisualParent());
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
            host, nullptr, &popupScaleX, &popupScaleY);
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
    if (matchPlacementTargetWidth) {
        contentSize.width =
            std::max(
                contentSize.width,
                targetSize.width);
    }
    const double targetWidth = targetSize.width;
    const double targetHeight = targetSize.height;
    double x = targetOriginLocal.x + horizontalOffset;
    double y = targetOriginLocal.y + verticalOffset;
    const PlacementMode placement = placementMode;
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
                verticalOffset * targetScaleY +
                popupHeightAbs;
            if (bottomAbsolute > rootHeight &&
                spaceAbove > spaceBelow) {
                y = targetOriginLocal.y -
                    contentSize.height -
                    verticalOffset;
            }
        } else if (placement == PlacementMode::Top) {
            const double topAbsolute =
                targetTopAbs -
                verticalOffset * targetScaleY -
                popupHeightAbs;
            if (topAbsolute < 0.0 &&
                spaceBelow > spaceAbove) {
                y = targetOriginLocal.y +
                    targetHeight +
                    verticalOffset;
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

    return {x, y, contentSize.width, contentSize.height};
}

Size Popup::ArrangeOverride(
    Size finalSize) noexcept {
    UIElement* popupChild =
        GetTemplateRoot() != nullptr
            ? GetTemplateRoot()
            : GetContentElement();
    if (popupChild == nullptr) return finalSize;
    if (!GetIsOpen()) {
        ArrangeChild(*popupChild, {});
        return finalSize;
    }

    const Rect slot = PlacePopupContent(*this, popupDesiredSize_, finalSize, GetPlacementTarget().Get(),
        GetPlacement(), GetHorizontalOffset(), GetVerticalOffset(), GetMatchPlacementTargetWidth());
    ArrangeChild(*popupChild, slot);
    return finalSize;
}

Size ToolTip::ArrangeOverride(Size finalSize) noexcept {
    UIElement* child = GetTemplateRoot() != nullptr ? GetTemplateRoot() : GetContentElement();
    if (child == nullptr) return finalSize;
    if (!GetIsOpen()) {
        ArrangeChild(*child, {});
        return finalSize;
    }
    const Rect slot = PlacePopupContent(*this, child->GetDesiredSize(), finalSize, GetPlacementTarget().Get(),
        GetPlacement(), GetHorizontalOffset(), GetVerticalOffset(), false);
    ArrangeChild(*child, slot);
    return finalSize;
}

namespace Primitives {

void SetPopupChild(Base::Object& owner, const Base::Ref<Base::Object>& child, void*) noexcept {
    if (!child) return;
    static_cast<Popup&>(owner).SetContent(child);
}

void ClearPopupChild(Base::Object& owner, void*) noexcept {
    static_cast<Popup&>(owner).SetChild(nullptr);
}

AERO_DESCRIBE(Popup) {
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
        .Content<UIElement>("Child", ContentKind::Single, &SetPopupChild, &ClearPopupChild, ContentFlags::Visual)
        .Factory();
}

} // namespace Primitives

} // namespace Aero::Controls
