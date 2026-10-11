#include "gui/ViewFrame.hpp"
#include "render/RenderTree.hpp"
#include <Aero/Documents.hpp>
#include <Aero/Controls/Menus.hpp>

#include <cstdint>
#include <utility>

namespace Aero {

using namespace ::Aero;

OverlayHost::OverlayHost(ViewFrame& owner) noexcept
    : view(&owner),
      renderOverlays(owner.allocator),
      inputOverlays(owner.allocator),
      overlayTransforms(owner.allocator) {}

void OverlayHost::Bind() noexcept {
    // Services are read on demand from the owning ViewFrame / ElementTree hub.
}

Base::IAllocator* OverlayHost::Allocator() const noexcept {
    return view != nullptr ? view->allocator : nullptr;
}

::Aero::Meta::Registry* OverlayHost::Metadata() const noexcept {
    return view != nullptr ? view->metadata : nullptr;
}

Aero::InputRouter* OverlayHost::Input() const noexcept {
    return view != nullptr ? view->Input() : nullptr;
}

::Aero::Render::RenderTree* OverlayHost::RenderTree() const noexcept {
    return view != nullptr ? view->RenderTree() : nullptr;
}

Base::Result<void> OverlayHost::SynchronizeOverlays() noexcept {
        renderOverlays.Clear();
        inputOverlays.Clear();
        overlayTransforms.Clear();
        Aero::Media::Visual* rootVisual =
            view->RootVisual();
        if (rootVisual == nullptr ||
            RenderTree() == nullptr) {
            if (Input() != nullptr) Input()->ClearOverlays();
            return {};
        }
        Base::Vector<Aero::Media::Visual*> stack(
            Allocator());
        stack.PushBack(rootVisual);
        if (activeContextMenu && activeContextMenu->GetIsOpen()) {
            stack.PushBack(activeContextMenu.Get());
        }
        while (!stack.Empty()) {
            Aero::Media::Visual* node =
                stack.Back();
            stack.PopBack();
            if (node == nullptr) continue;
            const Meta::TypeId type =
                node->RuntimeType();
            bool open = false;
            if (Metadata()->Types().IsDerivedFrom(
                    type,
                    Controls::Primitives::Popup::
                        StaticTypeId())) {
                open =
                    static_cast<Controls::Primitives::Popup*>(
                        node)->GetIsOpen();
            } else if (
                Metadata()->Types().IsDerivedFrom(
                    type,
                    Controls::ToolTip::StaticTypeId())) {
                open = static_cast<Controls::ToolTip*>(node)->GetIsOpen();
            } else if (
                Metadata()->Types().IsDerivedFrom(
                    type,
                    Controls::ContextMenu::
                        StaticTypeId())) {
                open =
                    static_cast<
                        Controls::ContextMenu*>(
                        node)->GetIsOpen();
            }
            if (open) {
                Aero::Media::Visual* ancestor =
                    node->GetVisualParent();
                while (ancestor != nullptr) {
                    Aero::UIElement*
                        element =
                            ::Aero::TryCast<::Aero::UIElement>(ancestor);
                    if (element != nullptr &&
                        element->GetVisibility() !=
                            Aero::Visibility::Visible) {
                        open = false;
                        break;
                    }
                    ancestor =
                        ancestor->GetVisualParent();
                }
            }
            if (open) {
                Aero::FrameworkElement*
                    framework =
                        ::Aero::TryCast<::Aero::FrameworkElement>(node);
                Aero::UIElement* inputElement =
                    ::Aero::TryCast<::Aero::UIElement>(node);
                if (framework != nullptr &&
                    inputElement != nullptr) {
                    auto makeTranslate = [](double dx, double dy) noexcept -> Base::Transform2D {
                        Base::Transform2D t{};
                        t.dx = dx;
                        t.dy = dy;
                        return t;
                    };
                    auto rootTransform = [&makeTranslate](
                        Aero::UIElement&
                            element) noexcept -> Base::Transform2D {
                        Base::ProjectiveTransform2D
                            result = Base::IdentityProjective();
                        Aero::Media::Visual*
                            current = &element;
                        while (current != nullptr) {
                            Aero::UIElement*
                                currentElement =
                                    ::Aero::TryCast<::Aero::UIElement>(
                                        current);
                            if (currentElement !=
                                nullptr) {
                                Aero::FrameworkElement*
                                    currentFramework =
                                        ::Aero::TryCast<::Aero::FrameworkElement>(
                                            currentElement);
                                if (currentFramework !=
                                    nullptr) {
                                    const Base::ProjectiveTransform2D localT =
                                        currentFramework->
                                            GetLocalVisualTransform();
                                    if (Base::IsFiniteTransform(localT)) {
                                        result =
                                            Base::Compose(
                                                result, localT);
                                    }
                                }
                                const Aero::Rect slot =
                                        currentElement->
                                            GetLayoutSlot();
                                result =
                                    Base::Compose(
                                        result,
                                        Base::ToProjective(
                                            makeTranslate(
                                                slot.x, slot.y)));
                            }
                            current =
                                current->
                                    GetVisualParent();
                        }
                        Base::Transform2D affine{};
                        if (!Base::TryToTransform2D(result, affine)) {
                            return {};
                        }
                        return affine;
                    };
                    Base::Transform2D transform =
                        rootTransform(*inputElement);
                    if (Metadata()->Types().IsDerivedFrom(
                            type, Controls::ContextMenu::StaticTypeId())) {
                        if (contextMenuHasPosition) {
                            transform = makeTranslate(
                                contextMenuPosition.x,
                                contextMenuPosition.y);
                        } else {
                            Base::Ref<Aero::UIElement> target =
                                static_cast<Controls::ContextMenu*>(node)->GetPlacementTarget();
                            if (target &&
                                target->
                                    GetIsArrangeValid()) {
                                transform =
                                    rootTransform(*target);
                                transform =
                                    Aero::Media::ComposeTransforms(
                                        makeTranslate(
                                            0.0,
                                            target->
                                                GetRenderSize().
                                                    height),
                                        transform);
                            }
                        }
                    }
                    renderOverlays.PushBack(
                            framework);
                    overlayTransforms.PushBack(
                            transform);
                    inputOverlays.PushBack(
                            inputElement);
                }
            }
            if (Metadata()->Types().IsDerivedFrom(
                    type,
                    Documents::AdornerLayer::StaticTypeId())) {
                auto* layer = static_cast<Documents::AdornerLayer*>(node);
                auto makeTranslate = [](double dx, double dy) noexcept -> Base::Transform2D {
                    Base::Transform2D t{};
                    t.dx = dx;
                    t.dy = dy;
                    return t;
                };
                auto rootTransform = [&makeTranslate](
                    Aero::UIElement& element) noexcept -> Base::Transform2D {
                    Base::ProjectiveTransform2D result = Base::IdentityProjective();
                    Aero::Media::Visual* current = &element;
                    while (current != nullptr) {
                        Aero::UIElement* currentElement =
                            ::Aero::TryCast<::Aero::UIElement>(current);
                        if (currentElement != nullptr) {
                            Aero::FrameworkElement* currentFramework =
                                ::Aero::TryCast<::Aero::FrameworkElement>(
                                    currentElement);
                            if (currentFramework != nullptr) {
                                const Base::ProjectiveTransform2D localT =
                                    currentFramework->GetLocalVisualTransform();
                                if (Base::IsFiniteTransform(localT)) {
                                    result = Base::Compose(result, localT);
                                }
                            }
                            const Aero::Rect slot = currentElement->GetLayoutSlot();
                            result = Base::Compose(
                                result,
                                Base::ToProjective(makeTranslate(slot.x, slot.y)));
                        }
                        current = current->GetVisualParent();
                    }
                    Base::Transform2D affine{};
                    if (!Base::TryToTransform2D(result, affine)) {
                        return {};
                    }
                    return affine;
                };
                for (const Base::Ref<Documents::Adorner>& adorner :
                     layer->GetAdorners()) {
                    if (!adorner) continue;
                    Aero::UIElement* target = adorner->GetAdornedElement();
                    if (target == nullptr) {
                        target = adorner.Get();
                    }
                    Base::Transform2D transform = rootTransform(*target);
                    renderOverlays.PushBack(adorner.Get());
                    overlayTransforms.PushBack(transform);
                    inputOverlays.PushBack(adorner.Get());
                }
            }
            const auto children =
                    (*node).RenderChildren();
            for (std::uint32_t index =
                     children.Size();
                 index > 0U;
                 --index) {
                stack.PushBack(
                        children[index - 1U]);
            }
        }
        Base::Result<void> render =
            RenderTree()->SetOverlays(
                renderOverlays.AsSpan(),
                overlayTransforms.AsSpan());
        if (!render) return render.GetStatus();
        return Input() != nullptr
            ? Input()->SetOverlays(
                  inputOverlays.AsSpan(),
                  overlayTransforms.AsSpan())
            : Base::Result<void>();
    }

void OverlayHost::ClearOverlays() noexcept {
        if (Input() != nullptr) Input()->ClearOverlays();
        renderOverlays.Clear();
        inputOverlays.Clear();
        overlayTransforms.Clear();
        if (RenderTree() != nullptr) {
            static_cast<void>(
                RenderTree()->SetOverlays(
                    renderOverlays.AsSpan(),
                    overlayTransforms.AsSpan()));
        }
    }

void OverlayHost::CloseAllOverlays() noexcept {
        for (Aero::UIElement* overlay :
             inputOverlays) {
            if (overlay == nullptr) continue;
            const Meta::TypeId type =
                overlay->RuntimeType();
            if (Metadata()->Types().IsDerivedFrom(
                    type,
                    Controls::Primitives::Popup::
                        StaticTypeId())) {
                auto* popup =
                    static_cast<Controls::Primitives::Popup*>(
                        overlay);
                static_cast<void>(
                    popup->SetIsOpen(false));
                static_cast<void>(
                    popup->SetPlacementTarget({}));
            } else if (
                Metadata()->Types().IsDerivedFrom(
                    type,
                    Controls::ContextMenu::
                        StaticTypeId())) {
                auto* menu =
                    static_cast<
                        Controls::ContextMenu*>(
                        overlay);
                static_cast<void>(
                    menu->SetIsOpen(false));
                static_cast<void>(
                    menu->SetPlacementTarget({}));
            }
        }
        if (activeContextMenu) {
            activeContextMenu->SetIsOpen(false);
            static_cast<void>(activeContextMenu->SetPlacementTarget({}));
            activeContextMenu.Reset();
            contextMenuTarget.Reset();
            contextMenuHasPosition = false;
        }
    }

bool OverlayHost::IsVisualDescendantOrSelf(
        const Aero::Media::Visual& root,
        const Aero::Media::Visual& target)
        noexcept {
        const Aero::Media::Visual* current =
            &target;
        while (current != &root) {
            current = current->GetVisualParent();
            if (current == nullptr) return false;
        }
        return true;
    }

Base::Result<void> OverlayHost::RestoreOverlayFocus()
        noexcept {
        if (!overlayFocusReturn ||
            Input() == nullptr) {
            overlayFocusReturn.Reset();
            return {};
        }
        Base::Ref<Aero::UIElement>
            target =
                std::move(overlayFocusReturn);
        Base::Result<bool> restored =
            Input()->SetFocus(target.Get());
        if (!restored &&
            restored.GetStatus().code !=
                Base::ErrorCode::NotFound &&
            restored.GetStatus().code !=
                Base::ErrorCode::InvalidState) {
            return restored.GetStatus();
        }
        return {};
    }

Base::Result<void> OverlayHost::DismissOverlaysForPointer(
        const Input::PointerInput& pointer,
        Aero::UIElement* target)
        noexcept {
        if (pointer.action !=
                Input::PointerAction::Down) {
            return {};
        }
        static_cast<void>(SynchronizeOverlays());
        if (inputOverlays.Empty() && !activeContextMenu) {
            return {};
        }
        bool closedFocusedOverlay = false;
        if (activeContextMenu && (target == nullptr || !IsVisualDescendantOrSelf(*activeContextMenu, *target))) {
            activeContextMenu->SetIsOpen(false);
            static_cast<void>(activeContextMenu->SetPlacementTarget({}));
            activeContextMenu.Reset();
            contextMenuTarget.Reset();
            contextMenuHasPosition = false;
            closedFocusedOverlay = true;
        }
        for (std::uint32_t index =
                 inputOverlays.Size();
             index > 0U;
             --index) {
            Aero::UIElement* overlay =
                inputOverlays[index - 1U];
            if (overlay == nullptr) continue;
            if (target != nullptr &&
                IsVisualDescendantOrSelf(
                    *overlay, *target)) {
                return {};
            }
            const Meta::TypeId type =
                overlay->RuntimeType();
            if (Metadata()->Types().IsDerivedFrom(
                    type,
                    Controls::Primitives::Popup::
                        StaticTypeId())) {
                auto* popup =
                    static_cast<Controls::Primitives::Popup*>(
                        overlay);
                if (target != nullptr) {
                    UIElement* placement = popup->GetPlacementTarget().Get();
                    if (placement == nullptr) {
                        DependencyObject* templated = popup->GetTemplatedParent();
                        if (templated != nullptr &&
                            Metadata()->Types().IsDerivedFrom(
                                templated->RuntimeType(),
                                UIElement::StaticTypeId())) {
                            placement = static_cast<UIElement*>(templated);
                        } else if (popup->GetVisualParent() != nullptr) {
                            placement = ::Aero::TryCast<::Aero::UIElement>(
                                popup->GetVisualParent());
                        }
                    }
                    if (placement != nullptr &&
                        IsVisualDescendantOrSelf(*placement, *target)) {
                        return {};
                    }
                }
                DependencyObject* templated = popup->GetTemplatedParent();
                bool isMenuPopup = (templated != nullptr &&
                    Metadata()->Types().IsDerivedFrom(
                        templated->RuntimeType(),
                        Controls::MenuItem::StaticTypeId()));
                UIElement* placement = popup->GetPlacementTarget().Get();
                if (!isMenuPopup && placement != nullptr &&
                    Metadata()->Types().IsDerivedFrom(
                        placement->RuntimeType(),
                        Controls::MenuItem::StaticTypeId())) {
                    isMenuPopup = true;
                }
                if (!popup->GetStaysOpen() || isMenuPopup) {
                    popup->SetIsOpen(false);
                    if (templated != nullptr &&
                        Metadata()->Types().IsDerivedFrom(
                            templated->RuntimeType(),
                            Controls::MenuItem::StaticTypeId())) {
                        Controls::MenuItem::DismissMenuTree(
                            *static_cast<Controls::MenuItem*>(templated));
                    }
                    if (placement != nullptr &&
                        Metadata()->Types().IsDerivedFrom(
                            placement->RuntimeType(),
                            Controls::MenuItem::StaticTypeId())) {
                        Controls::MenuItem::DismissMenuTree(
                            *static_cast<Controls::MenuItem*>(placement));
                    }
                }
            } else if (
                Metadata()->Types().IsDerivedFrom(
                    type,
                    Controls::ContextMenu::
                        StaticTypeId())) {
                static_cast<Controls::ContextMenu*>(
                    overlay)->SetIsOpen(false);
                closedFocusedOverlay = true;
                static_cast<void>(
                    static_cast<
                        Controls::ContextMenu*>(
                        overlay)->
                        SetPlacementTarget({}));
                if (activeContextMenu.Get() == overlay) {
                    activeContextMenu.Reset();
                    contextMenuTarget.Reset();
                    contextMenuHasPosition = false;
                }
            }
        }
        Base::Result<void> synced = SynchronizeOverlays();
        if (!synced) return synced.GetStatus();
        return closedFocusedOverlay
            ? RestoreOverlayFocus()
            : Base::Result<void>();
    }

Base::Result<bool> OverlayHost::DismissTopOverlayForEscape()
        noexcept {
        Base::Result<void> synced = SynchronizeOverlays();
        if (!synced) return Base::Result<bool>(synced.GetStatus());
        if (activeContextMenu) {
            activeContextMenu->SetIsOpen(false);
            static_cast<void>(activeContextMenu->SetPlacementTarget({}));
            activeContextMenu.Reset();
            contextMenuTarget.Reset();
            contextMenuHasPosition = false;
            synced = SynchronizeOverlays();
            if (!synced) return Base::Result<bool>(synced.GetStatus());
            Base::Result<void> restored = RestoreOverlayFocus();
            return restored ? Base::Result<bool>(true) : Base::Result<bool>(restored.GetStatus());
        }
        for (std::uint32_t index =
                 inputOverlays.Size();
             index > 0U;
             --index) {
            Aero::UIElement* overlay =
                inputOverlays[index - 1U];
            if (overlay == nullptr) continue;
            const Meta::TypeId type =
                overlay->RuntimeType();
            if (Metadata()->Types().IsDerivedFrom(
                    type,
                    Controls::Primitives::Popup::
                        StaticTypeId())) {
                auto* popup =
                    static_cast<Controls::Primitives::Popup*>(
                        overlay);
                // Keep PlacementTarget so the ComboBox can reopen in place.
                popup->SetIsOpen(false);
                DependencyObject* templated = popup->GetTemplatedParent();
                if (templated != nullptr &&
                    Metadata()->Types().IsDerivedFrom(
                        templated->RuntimeType(),
                        Controls::MenuItem::StaticTypeId())) {
                    static_cast<Controls::MenuItem*>(templated)->SetIsSubmenuOpen(false);
                }
                UIElement* placement = popup->GetPlacementTarget().Get();
                if (placement != nullptr &&
                    Metadata()->Types().IsDerivedFrom(
                        placement->RuntimeType(),
                        Controls::MenuItem::StaticTypeId())) {
                    static_cast<Controls::MenuItem*>(placement)->SetIsSubmenuOpen(false);
                }
                synced = SynchronizeOverlays();
                if (!synced) {
                    return Base::Result<bool>(synced.GetStatus());
                }
                Base::Result<void> restored =
                    RestoreOverlayFocus();
                return restored
                    ? Base::Result<bool>(true)
                    : Base::Result<bool>(
                          restored.GetStatus());
            }
            if (Metadata()->Types().IsDerivedFrom(
                    type,
                    Controls::ContextMenu::
                        StaticTypeId())) {
                static_cast<Controls::ContextMenu*>(
                    overlay)->SetIsOpen(false);
                static_cast<void>(
                    static_cast<
                        Controls::ContextMenu*>(
                        overlay)->
                        SetPlacementTarget({}));
                synced = SynchronizeOverlays();
                if (!synced) {
                    return Base::Result<bool>(synced.GetStatus());
                }
                Base::Result<void> restored =
                    RestoreOverlayFocus();
                return restored
                    ? Base::Result<bool>(true)
                    : Base::Result<bool>(
                          restored.GetStatus());
            }
        }
        return false;
    }

Base::Result<void> OverlayHost::OpenContextMenuForPointer(
        const Input::PointerInput& pointer,
        Aero::UIElement* hitTarget)
        noexcept {
        if (pointer.action !=
                Input::PointerAction::Down ||
            pointer.changedButton !=
                Input::MouseButton::Right) {
            return {};
        }
        Aero::Media::Visual* current =
            hitTarget;
        while (current != nullptr) {
            Aero::UIElement* element =
                ::Aero::TryCast<::Aero::UIElement>(current);
            if (element != nullptr) {
                Base::Ref<Controls::ContextMenu>
                    menu =
                        Controls::
                            ContextMenuService::
                                GetContextMenu(
                                    *element);
                if (menu) {
                    if (this->Input() != nullptr &&
                        !overlayFocusReturn) {
                        Aero::UIElement*
                            focused =
                                this->Input()->GetFocusedElement();
                        if (focused != nullptr) {
                            overlayFocusReturn =
                                Base::Ref<
                                    Aero::UIElement>::
                                    TryFromBorrowed(
                                        *focused);
                        }
                    }
                    Base::Ref<
                        Aero::UIElement>
                        target =
                            Base::Ref<
                                Aero::UIElement>::
                                TryFromBorrowed(
                                    *element);
                    if (target) {
                        menu->SetPlacementTarget(std::move(target));
                    }

                    Aero::Media::Visual* rootVisual = view != nullptr ? view->RootVisual() : nullptr;
                    if (rootVisual != nullptr && menu->GetVisualParent() == nullptr) {
                        ElementTree* tree = ElementTree::Of(*rootVisual);
                        if (tree != nullptr) {
                            if (contextMenuAttachment.IsAttached()) {
                                (void)tree->DetachElement(contextMenuAttachment);
                            }
                            auto attachRes = tree->AttachElement(*rootVisual, *rootVisual, *menu);
                            if (attachRes) {
                                contextMenuAttachment = attachRes.Value();
                            }
                        }
                    }

                    activeContextMenu = menu;
                    contextMenuTarget = Base::Ref<Aero::UIElement>::TryFromBorrowed(*element);
                    contextMenuPosition = pointer.position;
                    contextMenuHasPosition = true;
                    menu->SetIsOpen(true);

                    menu->Measure(Size{10000.0, 10000.0});
                    menu->Arrange(Rect{0.0, 0.0, menu->GetDesiredSize().width, menu->GetDesiredSize().height});

                    static_cast<void>(SynchronizeOverlays());

                    if (this->Input() != nullptr) {
                        static_cast<void>(this->Input()->SetFocus(menu.Get()));
                    }
                    return {};
                }
            }
            current = current->GetVisualParent();
        }
        return {};
    }

Base::Result<void> OverlayHost::UpdateToolTipForPointer(
        const Input::PointerInput& pointer,
        Aero::UIElement* hitTarget)
        noexcept {
        if (pointer.action ==
                Input::PointerAction::Down) {
            if (activeToolTip) {
                activeToolTip->SetIsOpen(false);
                static_cast<void>(
                    activeToolTip->
                        SetPlacementTarget({}));
            }
            pendingToolTip.Reset();
            activeToolTip.Reset();
            toolTipTarget.Reset();
            toolTipElapsed = 0U;
            toolTipVisibleElapsed = 0U;
            return {};
        }
        if (pointer.action !=
            Input::PointerAction::Move) {
            return {};
        }
        Base::Ref<Controls::ToolTip> next;
        Base::Ref<Aero::UIElement>
            nextTarget;
        Aero::Media::Visual* current =
            hitTarget;
        while (current != nullptr) {
            Aero::UIElement* element =
                ::Aero::TryCast<::Aero::UIElement>(current);
            if (element != nullptr) {
                next =
                    Controls::ToolTipService::
                        GetToolTip(*element);
                if (next) {
                    nextTarget =
                        Base::Ref<
                            Aero::UIElement>::
                            TryFromBorrowed(
                                *element);
                    break;
                }
            }
            current = current->GetVisualParent();
        }
        if (next.Get() == pendingToolTip.Get() &&
            nextTarget.Get() == toolTipTarget.Get()) {
            return {};
        }
        if (activeToolTip) {
            activeToolTip->SetIsOpen(false);
            static_cast<void>(
                activeToolTip->
                    SetPlacementTarget({}));
        }
        pendingToolTip = std::move(next);
        activeToolTip.Reset();
        toolTipTarget = std::move(nextTarget);
        toolTipElapsed = 0U;
        toolTipVisibleElapsed = 0U;
        if (pendingToolTip && toolTipTarget) {
            pendingToolTip->SetPlacementTarget(toolTipTarget);
        }
        return {};
    }

Base::Result<std::uint32_t>
 OverlayHost::AdvanceToolTipTime(
        std::uint32_t elapsedMilliseconds)
        noexcept {
        if (!pendingToolTip ||
            !toolTipTarget) {
            return 0U;
        }
        if (!activeToolTip) {
            const std::uint32_t delay =
                Controls::ToolTipService::
                    GetInitialShowDelay(
                        *toolTipTarget);
            toolTipElapsed =
                elapsedMilliseconds >
                        UINT32_MAX -
                            toolTipElapsed
                    ? UINT32_MAX
                    : toolTipElapsed +
                        elapsedMilliseconds;
            if (toolTipElapsed < delay) {
                return 0U;
            }
            pendingToolTip->SetIsOpen(true);
            activeToolTip = pendingToolTip;
            toolTipVisibleElapsed = 0U;
            return 1U;
        }
        toolTipVisibleElapsed =
            elapsedMilliseconds >
                    UINT32_MAX -
                        toolTipVisibleElapsed
                ? UINT32_MAX
                : toolTipVisibleElapsed +
                    elapsedMilliseconds;
        const std::uint32_t duration =
            Controls::ToolTipService::
                GetShowDuration(*toolTipTarget);
        if (toolTipVisibleElapsed < duration) {
            return 0U;
        }
        activeToolTip->SetIsOpen(false);
        static_cast<void>(
            activeToolTip->SetPlacementTarget({}));
        pendingToolTip.Reset();
        activeToolTip.Reset();
        toolTipTarget.Reset();
        overlayFocusReturn.Reset();
        toolTipElapsed = 0U;
        toolTipVisibleElapsed = 0U;
        return 1U;
    }

} // namespace Aero
