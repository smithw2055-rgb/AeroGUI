#include "gui/ViewState.hpp"
#include "gui/internal/AeroGuiInternal.hpp"

#include <cstdint>
#include <utility>

namespace Aero {

using namespace ::Aero;

OverlayHost::OverlayHost(ViewState& owner) noexcept
    : view(&owner),
      allocator(owner.allocator),
      renderOverlays(owner.allocator),
      inputOverlays(owner.allocator),
      overlayTransforms(owner.allocator) {}

void OverlayHost::Bind() noexcept {
    allocator = view->allocator;
    metadata = view->metadata;
    input = view->input;
    renderer = view->renderer;
}

Base::Result<void> OverlayHost::SynchronizeOverlays() noexcept {
        renderOverlays.Clear();
        inputOverlays.Clear();
        overlayTransforms.Clear();
        Aero::Media::Visual* rootVisual =
            view->RootVisual();
        if (rootVisual == nullptr ||
            renderer == nullptr) {
            if (input != nullptr) input->ClearOverlays();
            return {};
        }
        Base::Vector<Aero::Media::Visual*> stack(
            allocator);
        Base::Result<void> appended =
            stack.PushBack(rootVisual);
        if (!appended) return appended.GetStatus();
        while (!stack.Empty()) {
            Aero::Media::Visual* node =
                stack.Back();
            stack.PopBack();
            if (node == nullptr) continue;
            const Meta::TypeId type =
                node->RuntimeType();
            bool open = false;
            if (metadata->Types().IsDerivedFrom(
                    type,
                    Controls::Primitives::Popup::
                        StaticTypeId())) {
                open =
                    static_cast<Controls::Primitives::Popup*>(
                        node)->GetIsOpen();
            } else if (
                metadata->Types().IsDerivedFrom(
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
                            ancestor->AsUIElement();
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
                        node->AsFrameworkElement();
                Aero::UIElement* inputElement =
                    node->AsUIElement();
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
                        Base::Transform2D
                            result{};
                        Aero::Media::Visual*
                            current = &element;
                        while (current != nullptr) {
                            Aero::UIElement*
                                currentElement =
                                    current->
                                        AsUIElement();
                            if (currentElement !=
                                nullptr) {
                                Aero::FrameworkElement*
                                    currentFramework =
                                        currentElement->
                                            AsFrameworkElement();
                                if (currentFramework !=
                                    nullptr) {
                                    const Base::Transform2D localT =
                                        currentFramework->
                                            GetLocalVisualTransform();
                                    if (Base::IsFiniteTransform(localT)) {
                                        result =
                                            Aero::Media::ComposeTransforms(
                                                result, localT);
                                    }
                                }
                                const Aero::Rect slot =
                                        currentElement->
                                            GetLayoutSlot();
                                result =
                                    Aero::Media::ComposeTransforms(
                                        result,
                                        makeTranslate(
                                            slot.x, slot.y));
                            }
                            current =
                                current->
                                    GetVisualParent();
                        }
                        return result;
                    };
                    Base::Transform2D transform =
                        rootTransform(*inputElement);
                    if (metadata->Types().
                            IsDerivedFrom(
                                type,
                                Controls::
                                    ContextMenu::
                                        StaticTypeId())) {
                        Base::Ref<
                            Aero::UIElement>
                            target =
                                static_cast<
                                    Controls::
                                    ContextMenu*>(
                                    node)->
                                    GetPlacementTarget();
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
                    appended =
                        renderOverlays.PushBack(
                            framework);
                    if (!appended) {
                        return appended.GetStatus();
                    }
                    appended =
                        overlayTransforms.PushBack(
                            transform);
                    if (!appended) {
                        return appended.GetStatus();
                    }
                    appended =
                        inputOverlays.PushBack(
                            inputElement);
                    if (!appended) {
                        return appended.GetStatus();
                    }
                }
            }
            const auto children =
                    AeroGuiInternal::RenderChildren(*node);
            for (std::uint32_t index =
                     children.Size();
                 index > 0U;
                 --index) {
                appended =
                    stack.PushBack(
                        children[index - 1U]);
                if (!appended) {
                    return appended.GetStatus();
                }
            }
        }
        Base::Result<void> render =
            renderer->SetOverlays(
                renderOverlays.AsSpan(),
                overlayTransforms.AsSpan());
        if (!render) return render.GetStatus();
        return input != nullptr
            ? input->SetOverlays(
                  inputOverlays.AsSpan(),
                  overlayTransforms.AsSpan())
            : Base::Result<void>();
    }

void OverlayHost::ClearOverlays() noexcept {
        if (input != nullptr) input->ClearOverlays();
        renderOverlays.Clear();
        inputOverlays.Clear();
        overlayTransforms.Clear();
        if (renderer != nullptr) {
            static_cast<void>(
                renderer->SetOverlays(
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
            if (metadata->Types().IsDerivedFrom(
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
                metadata->Types().IsDerivedFrom(
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
            input == nullptr) {
            overlayFocusReturn.Reset();
            return {};
        }
        Base::Ref<Aero::UIElement>
            target =
                std::move(overlayFocusReturn);
        Base::Result<bool> restored =
            input->SetFocus(target.Get());
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
        if (inputOverlays.Empty()) {
            return {};
        }
        bool closedFocusedOverlay = false;
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
            if (metadata->Types().IsDerivedFrom(
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
                            metadata->Types().IsDerivedFrom(
                                templated->RuntimeType(),
                                UIElement::StaticTypeId())) {
                            placement = static_cast<UIElement*>(templated);
                        } else if (popup->GetVisualParent() != nullptr) {
                            placement = popup->GetVisualParent()->AsUIElement();
                        }
                    }
                    if (placement != nullptr &&
                        IsVisualDescendantOrSelf(*placement, *target)) {
                        return {};
                    }
                }
                if (!popup->GetStaysOpen()) {
                    popup->SetIsOpen(false);
                }
            } else if (
                metadata->Types().IsDerivedFrom(
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
            }
        }
        return closedFocusedOverlay
            ? RestoreOverlayFocus()
            : Base::Result<void>();
    }

Base::Result<bool> OverlayHost::DismissTopOverlayForEscape()
        noexcept {
        for (std::uint32_t index =
                 inputOverlays.Size();
             index > 0U;
             --index) {
            Aero::UIElement* overlay =
                inputOverlays[index - 1U];
            if (overlay == nullptr) continue;
            const Meta::TypeId type =
                overlay->RuntimeType();
            if (metadata->Types().IsDerivedFrom(
                    type,
                    Controls::Primitives::Popup::
                        StaticTypeId())) {
                static_cast<Controls::Primitives::Popup*>(
                    overlay)->SetIsOpen(false);
                static_cast<void>(
                    static_cast<Controls::Primitives::Popup*>(
                        overlay)->
                        SetPlacementTarget({}));
                Base::Result<void> restored =
                    RestoreOverlayFocus();
                return restored
                    ? Base::Result<bool>(true)
                    : Base::Result<bool>(
                          restored.GetStatus());
            }
            if (metadata->Types().IsDerivedFrom(
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
                current->AsUIElement();
            if (element != nullptr) {
                Base::Ref<Controls::ContextMenu>
                    menu =
                        Controls::
                            ContextMenuService::
                                GetContextMenu(
                                    *element);
                if (menu) {
                    if (this->input != nullptr &&
                        !overlayFocusReturn) {
                        Aero::UIElement*
                            focused =
                                this->input->GetFocusedElement();
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
                    menu->SetIsOpen(true);
                    if (this->input != nullptr) {
                        Base::Result<bool> focused =
                            this->input->SetFocus(
                                menu.Get());
                        if (!focused) {
                            static_cast<void>(
                                menu->
                                    SetIsOpen(
                                        false));
                            return focused.GetStatus();
                        }
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
                current->AsUIElement();
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
