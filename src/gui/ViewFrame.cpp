#include "gui/ViewState.hpp"
#include "gui/internal/AeroGuiInternal.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

namespace Aero {

using namespace ::Aero;
namespace MediaAnimation = ::Aero::Media::Animation;

void ViewState::ReportFrameFailure(
        Base::Status& slot,
        Base::Status status,
        std::uint16_t diagnosticNumber) noexcept {
        const bool repeated = slot.code == status.code &&
            slot.message == status.message;
        slot = status;
        if (repeated || status.IsOk() || options.diagnostics == nullptr) {
            return;
        }
        Base::Result<Diagnostics::Diagnostic> diagnostic =
            Diagnostics::Diagnostic::Create(
                Diagnostics::MakeDiagnosticCode(
                    Diagnostics::DiagnosticDomain::Render,
                    diagnosticNumber),
                Diagnostics::DiagnosticSeverity::Error,
                Base::StringView(
                    status.message,
                    static_cast<std::uint32_t>(
                        std::strlen(status.message))));
        if (!diagnostic) return;
        static_cast<void>(options.diagnostics->Report(
            std::move(diagnostic).Value()));
    }

void ViewState::ReportUpdateFailure(Base::Status status) noexcept {
        ReportFrameFailure(updateStatus, status, 101U);
    }

void ViewState::ReportRendererFailure(Base::Status status) noexcept {
        ReportFrameFailure(rendererStatus, status, 102U);
    }

void ViewState::ClearUpdateFailure() noexcept { updateStatus = {}; }

void ViewState::ClearRendererFailure() noexcept { rendererStatus = {}; }

bool ViewState::HasAttachedRoot() const noexcept {
        return rootAttachment.IsAttached();
    }

Base::Result<void> ViewState::AttachVisualGraph(
        ::Aero::Media::Visual& rootVisual,
        UIElement& rootLayout,
        FrameworkElement* rootRender,
        Base::Span<Aero::Markup::VisualEdge> edges,
        Size availableSize) noexcept {
        if (tree == nullptr || layout == nullptr || HasAttachedRoot() ||
            !IsValidLayoutSize(availableSize)) {
            return ViewInvalidState(
                "Gui root cannot be attached in its current state");
        }
        tree->AttachPresentation(layout, renderer);
        Base::Result<Aero::RootAttachment> rootAttached =
            tree->AttachRoot(rootVisual, availableSize);
        if (!rootAttached) return rootAttached.GetStatus();
        rootAttachment = std::move(rootAttached).Value();
        attachedRootVisual = &rootVisual;
        attachedRootLayout = &rootLayout;
        attachedRootRender = rootRender;

        std::uint32_t attached = 0U;
        while (attached < edges.Size()) {
            bool progressed = false;
            for (Aero::Markup::VisualEdge& edge : edges) {
                if (edge.state.logicalAttached || edge.parent == nullptr ||
                    edge.child == nullptr ||
                    edge.parent->GetTree() != tree) {
                    continue;
                }
                Base::Result<Aero::ElementAttachment> edgeAttached =
                    tree->AttachElement(*edge.parent, *edge.child);
                if (!edgeAttached) {
                    static_cast<void>(DetachVisualGraph(edges));
                    return edgeAttached.GetStatus();
                }
                edge.state = std::move(edgeAttached).Value();
                ++attached;
                progressed = true;
            }
            if (!progressed) break;
        }
        return {};
    }

Base::Result<void> ViewState::CompleteVisualEdges(
        Base::Span<Aero::Markup::VisualEdge> edges) noexcept {
        if (!HasAttachedRoot() || tree == nullptr) {
            return ViewInvalidState(
                "Deferred visual edges require an attached root");
        }
        std::uint32_t attached = 0U;
        for (const Aero::Markup::VisualEdge& edge : edges) {
            if (edge.state.logicalAttached) ++attached;
        }
        while (attached < edges.Size()) {
            bool progressed = false;
            for (Aero::Markup::VisualEdge& edge : edges) {
                if (edge.state.logicalAttached ||
                    (edge.child != nullptr &&
                     edge.child->GetTree() == tree) ||
                    edge.parent == nullptr || edge.child == nullptr ||
                    edge.parent->GetTree() != tree) {
                    continue;
                }
                Base::Result<Aero::ElementAttachment> edgeAttached =
                    tree->AttachElement(*edge.parent, *edge.child);
                if (!edgeAttached) return edgeAttached.GetStatus();
                edge.state = std::move(edgeAttached).Value();
                ++attached;
                progressed = true;
            }
            if (!progressed) break;
        }
        return {};
    }

Base::Result<void> ViewState::ResizeVisualRoot(Size availableSize) noexcept {
        if (!HasAttachedRoot() || attachedRootLayout == nullptr ||
            layout == nullptr) {
            return AeroNotInitialized(
                "View resize requires an attached layout root");
        }
        if (!IsValidLayoutSize(availableSize)) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "View dimensions are invalid");
        }
        Base::Result<void> resized =
            layout->SetRoot(attachedRootLayout, availableSize);
        if (!resized) return resized.GetStatus();
        if (renderer != nullptr &&
            attachedRootVisual != nullptr) {
            return renderer->Invalidate(
                *attachedRootVisual,
                Aero::Render::RenderInvalidation::State);
        }
        return {};
    }

Base::Result<void> ViewState::DetachVisualGraph(
        Base::Span<Aero::Markup::VisualEdge> edges) noexcept {
        if (!HasAttachedRoot() && attachedRootVisual == nullptr) return {};
        if (tree == nullptr) {
            return ViewInvalidState(
                "Gui context is unavailable during root detach");
        }

        const auto reconcileAttachment =
            [this](Aero::Markup::VisualEdge& edge) noexcept {
                auto& state = edge.state;
                if (state.child == nullptr) {
                    state.logicalAttached = false;
                    state.visualAttached = false;
                    state.layoutAttached = false;
                    state.renderAttached = false;
                    return;
                }
                state.logicalAttached =
                    state.logicalParent != nullptr &&
                    state.child->GetLogicalParent() == state.logicalParent;
                state.visualAttached =
                    state.visualParent != nullptr &&
                    state.child->GetVisualParent() == state.visualParent;

                Aero::UIElement* childElement =
                    state.child->AsUIElement();
                Aero::UIElement* parentElement =
                    state.visualParent != nullptr
                    ? state.visualParent->AsUIElement()
                    : nullptr;
                if (childElement != nullptr &&
                    childElement->GetIsLayoutAttached() &&
                    AeroGuiInternal::LayoutEngineOf(*childElement) == nullptr) {
                    // The logical subtree has already left its ElementTree, so
                    // no LayoutEngine remains to consume this stale edge bit.
                    AeroGuiInternal::Layout(*childElement).layoutAttached = false;
                    AeroGuiInternal::Layout(*childElement).measureQueued = false;
                    AeroGuiInternal::Layout(*childElement).arrangeQueued = false;
                }
                state.layoutAttached =
                    layout != nullptr && childElement != nullptr &&
                    parentElement != nullptr &&
                    childElement->GetIsLayoutAttached() &&
                    AeroGuiInternal::LayoutEngineOf(*childElement) == layout &&
                    childElement->LayoutParent() == parentElement;

                if (AeroGuiInternal::RenderAttached(
                        *state.child) &&
                    AeroGuiInternal::RenderRuntime(
                        *state.child) == nullptr) {
                    AeroGuiInternal::RenderAttached(
                        *state.child) = false;
                    AeroGuiInternal::RenderQueued(
                        *state.child) = false;
                    AeroGuiInternal::Rendering(
                        *state.child) = false;
                    AeroGuiInternal::NodeId(
                        *state.child) = Base::InvalidRenderNodeId;
                    AeroGuiInternal::RenderValid(
                        *state.child) = false;
                }
                state.renderAttached =
                    renderer != nullptr &&
                    AeroGuiInternal::RenderAttached(
                        *state.child) &&
                    AeroGuiInternal::RenderRuntime(
                        *state.child) == renderer &&
                    AeroGuiInternal::RenderParent(
                        *state.child) == state.visualParent;
            };

        std::uint32_t remaining = 0U;
        for (Aero::Markup::VisualEdge& edge : edges) {
            reconcileAttachment(edge);
            if (edge.state.IsAttached()) ++remaining;
        }
        while (remaining > 0U) {
            bool progressed = false;
            for (Aero::Markup::VisualEdge& edge : edges) {
                reconcileAttachment(edge);
                if (!edge.state.IsAttached()) continue;
                bool hasAttachedChild = false;
                for (const Aero::Markup::VisualEdge& candidate : edges) {
                    if (candidate.state.IsAttached() &&
                        candidate.parent == edge.child) {
                        hasAttachedChild = true;
                        break;
                    }
                }
                if (hasAttachedChild) continue;
                Base::Result<void> detached =
                    tree->DetachElement(edge.state);
                if (!detached) return detached.GetStatus();
                --remaining;
                progressed = true;
            }
            if (!progressed) {
                return ViewInvalidState(
                    "Visual edges cannot be detached leaf-first");
            }
        }

        Base::Result<void> rootDetached = tree->DetachRoot(rootAttachment);
        if (!rootDetached) return rootDetached.GetStatus();
        rootAttachment = {};
        attachedRootVisual = nullptr;
        attachedRootLayout = nullptr;
        attachedRootRender = nullptr;
        return {};
    }

Aero::ResourceEnvironment ViewState::ResourceEnvironment() const noexcept {
        return {
            &applicationResources,
            &themeResources,
            &systemResources};
    }

Base::Result<Aero::ResourceDictionary*>
 ViewState::ResolveResourceLayer(
        ResourceLayer layer) noexcept {
        switch (layer) {
        case ResourceLayer::Application:
            return &applicationResources;
        case ResourceLayer::Theme:
            return &themeResources;
        case ResourceLayer::System:
            return &systemResources;
        }
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "View resource layer is invalid");
    }

Base::Result<void> ViewState::RebuildDynamicResourceEnvironment() noexcept {
        dynamicResourceEnvironment.Clear();
        Base::Result<void> rebuilt =
            dynamicResourceEnvironment.AddMerged(
                systemResources);
        if (rebuilt) {
            rebuilt =
                dynamicResourceEnvironment.AddMerged(
                    themeResources);
        }
        if (rebuilt) {
            rebuilt =
                dynamicResourceEnvironment.AddMerged(
                    applicationResources);
        }
        return rebuilt;
    }

void ViewState::DetachUi() noexcept {
        DetachUi(
            RootVisual(),
            {loadedDocument.visualContent.nodes.Data(),
             loadedDocument.visualContent.nodes.Size()});
    }

Aero::Media::Visual* ViewState::RootVisual() noexcept {
        if (!root) return nullptr;
        if (!metadata->Types().IsDerivedFrom(
                root->RuntimeType(),
                Aero::Media::Visual::StaticTypeId())) {
            return nullptr;
        }
        return static_cast<Aero::Media::Visual*>(root.Get());
    }

Base::Result<void> ViewState::SynchronizeOverlays() noexcept {
        renderOverlays.Clear();
        inputOverlays.Clear();
        overlayTransforms.Clear();
        Aero::Media::Visual* rootVisual =
            RootVisual();
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
            const Base::Span<
                Aero::Media::Visual* const>
                children =
                    node->GetVisualChildren();
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

void ViewState::ClearOverlays() noexcept {
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

void ViewState::CloseAllOverlays() noexcept {
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

bool ViewState::IsVisualDescendantOrSelf(
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

Base::Result<void> ViewState::RestoreOverlayFocus()
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

Base::Result<void> ViewState::DismissOverlaysForPointer(
        const Input::PointerInput& input,
        Aero::UIElement* target)
        noexcept {
        if (input.action !=
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

Base::Result<bool> ViewState::DismissTopOverlayForEscape()
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

Base::Result<void> ViewState::OpenContextMenuForPointer(
        const Input::PointerInput& input,
        Aero::UIElement* hitTarget)
        noexcept {
        if (input.action !=
                Input::PointerAction::Down ||
            input.changedButton !=
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

Base::Result<void> ViewState::UpdateToolTipForPointer(
        const Input::PointerInput& input,
        Aero::UIElement* hitTarget)
        noexcept {
        if (input.action ==
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
        if (input.action !=
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
 ViewState::AdvanceToolTipTime(
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

Base::Result<Aero::Media::Visual*> ViewState::ResolveVisual(
        Base::Object& object, Meta::TypeId type) noexcept {
        if (object.RuntimeType() != type ||
            !metadata->Types().IsDerivedFrom(
                type, Aero::Media::Visual::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "View root is not a registered Visual");
        }
        return static_cast<Aero::Media::Visual*>(&object);
    }

Base::Result<Aero::UIElement*> ViewState::ResolveUIElement(
        Base::Object& object, Meta::TypeId type) noexcept {
        Base::Result<Aero::Media::Visual*> visual =
            ResolveVisual(object, type);
        if (!visual) return visual.GetStatus();
        Aero::UIElement* element =
            visual.Value()->AsUIElement();
        if (element == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "View root is not a UIElement");
        }
        return element;
    }

Aero::FrameworkElement* ViewState::ResolveFrameworkElement(
        Base::Object& object, Meta::TypeId type) noexcept {
        Base::Result<Aero::Media::Visual*> visual =
            ResolveVisual(object, type);
        return visual ? visual.Value()->AsFrameworkElement() : nullptr;
    }

Base::Result<void> ViewState::ApplyUi(Aero::Media::Visual& root) noexcept {
        if (metadata == nullptr || values == nullptr || bindings == nullptr ||
            events == nullptr || input == nullptr || styles == nullptr ||
            templates == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "View UI state is unavailable");
        }

        const Aero::ResourceEnvironment resources = ResourceEnvironment();
        Base::Vector<Aero::Media::Visual*> stack(allocator);
        Base::Result<void> pushed = stack.PushBack(&root);
        if (!pushed) return pushed.GetStatus();
        while (!stack.Empty()) {
            Aero::Media::Visual* node = stack.Back();
            stack.PopBack();
            if (node == nullptr) continue;

            Base::Result<std::uint32_t> activated =
                bindings->ActivateDeferred(
                    *static_cast<::Aero::DependencyObject*>(node));
            if (!activated) return activated.GetStatus();

            Aero::FrameworkElement* element = node->AsFrameworkElement();
            if (element != nullptr) {
                Base::Result<const Aero::Style*> resolved =
                    ResolveUiValue<Aero::Style>(
                        *element, Aero::FrameworkElement::StyleProperty,
                        resources,
                        "FrameworkElement Style value is not a Style");
                if (!resolved) return resolved.GetStatus();
                const Aero::Style* style = resolved.Value();
                if (style != nullptr) {
                    if (!style->GetIsSealed()) {
                        return Base::Status::Failure(
                            Base::ErrorCode::InvalidState,
                            "Implicit Style is not sealed");
                    }
                    if (styles->AppliedStyle(*element) != style) {
                        ClearStyleDataTriggersFor(*element);
                        Base::Result<void> applied = styles->Apply(*element, *style);
                        if (!applied) return applied.GetStatus();
                    }
                    Base::Result<std::uint32_t> dataTriggers =
                        StartStyleDataTriggers(*element, *style);
                    if (!dataTriggers) return dataTriggers.GetStatus();
                }
            }

            Base::Result<std::uint32_t> styleValues = values->Flush();
            if (!styleValues) return styleValues.GetStatus();

            if (metadata->Types().IsDerivedFrom(
                    node->RuntimeType(), Controls::Control::StaticTypeId())) {
                auto& control = *static_cast<Controls::Control*>(node);
                AeroGuiInternal::AttachTemplateEngine(
                    control, templates);
                Base::Result<const Controls::ControlTemplate*> resolved =
                    ResolveUiValue<Controls::ControlTemplate>(
                        control, Controls::Control::TemplateProperty, resources,
                        "Control Template value is not a ControlTemplate");
                if (!resolved) return resolved.GetStatus();
                const Controls::ControlTemplate* controlTemplate =
                    resolved.Value();
                if (controlTemplate != nullptr) {
                    const ::Aero::Controls::TemplateHandle existing =
                        templates->AppliedHandle(control);
                    if (!existing.IsValid() ||
                        templates->AppliedTemplate(existing) != controlTemplate) {
                        Base::Result<::Aero::Controls::TemplateHandle> applied =
                            templates->Apply(control, *controlTemplate);
                        if (!applied) return applied.GetStatus();
                        // TemplateEngine installs the handle while its
                        // transaction is active. Invoke the control callback
                        // only after Apply has returned so PART_* lookups and
                        // ItemsHost realization cannot re-enter that
                        // transaction.
                        AeroGuiInternal::
                            InvokeTemplateApplied(control);
                    }
                }
            }

            for (Aero::Media::Visual* child :
                 node->GetVisualChildren()) {
                pushed = stack.PushBack(child);
                if (!pushed) return pushed.GetStatus();
            }
        }
        Base::Result<std::uint32_t> appliedValues = values->Flush();
        return appliedValues ? Base::Result<void>()
                             : Base::Result<void>(appliedValues.GetStatus());
    }

void ViewState::DetachUi(
        Aero::Media::Visual* root,
        Base::Span<Aero::Media::Visual* const> declarationNodes) noexcept {
        if (values == nullptr) return;

        Base::Vector<Aero::Media::Visual*> reachable(allocator);
        if (root != nullptr) {
            (void)reachable.PushBack(root);
            for (std::uint32_t index = 0U; index < reachable.Size(); ++index) {
                Aero::Media::Visual* node = reachable[index];
                if (node == nullptr) continue;
                for (Aero::Media::Visual* child :
                     node->GetVisualChildren()) {
                    if (child != nullptr) (void)reachable.PushBack(child);
                }
            }
        }

        for (Aero::Media::Visual* node : reachable) {
            if (node == nullptr) continue;
            if (bindings != nullptr) (void)bindings->DetachObject(*node);
            Aero::FrameworkElement* element = node->AsFrameworkElement();
            if (element != nullptr && styles != nullptr) {
                ClearStyleDataTriggersFor(*element);
                (void)styles->DetachObject(*element);
            }
        }
        for (std::uint32_t index = reachable.Size(); index > 0U; --index) {
            Aero::Media::Visual* node = reachable[index - 1U];
            if (node == nullptr || metadata == nullptr ||
                !metadata->Types().IsDerivedFrom(
                    node->RuntimeType(), Controls::Control::StaticTypeId())) {
                continue;
            }
            auto& control = *static_cast<Controls::Control*>(node);
            if (visualStates != nullptr) {
                (void)::Aero::Controls::TemplatePrivate::Clear(
                    *visualStates, control);
            }
            if (templates != nullptr) {
                (void)templates->Clear(control);
            }
        }
        for (Aero::Media::Visual* node : declarationNodes) {
            if (node != nullptr) (void)values->DetachObject(*node);
        }
    }

Base::Result<void> ViewState::CreateUiEngines() noexcept {
        Base::Result<void> status = AllocateObject(*allocator, Base::MemoryTag::Ui, templates, *tree, *values,
            ::Aero::MetadataPrivate::
                DependencyProperties(*metadata),
            layout, renderer, metadata, bindings,
            &dynamicResourceEnvironment);
        if (!status) return status.GetStatus();
        Base::Result<VisualStateManager*> createdStates =
            ::Aero::Controls::TemplatePrivate::Create(
                *values,
                *templates,
                *animations,
                ::Aero::MetadataPrivate::
                    DependencyProperties(*metadata));
        if (!createdStates) return createdStates.GetStatus();
        visualStates = createdStates.Value();
        status = AllocateObject(*allocator, Base::MemoryTag::Ui, styles, *values,
            ::Aero::MetadataPrivate::
                DependencyProperties(*metadata));
        if (!status) return status.GetStatus();
        styles->SetTriggerActionHandler(
            &ViewState::ExecuteStyleTriggerActions, this);
        if (tree != nullptr) {
            tree->SetLayout(layout);
            tree->SetBindings(bindings);
            tree->SetStyles(styles);
            tree->SetEvents(events);
            tree->SetInput(input);
            tree->SetAnimations(animations);
            tree->SetVisualStates(visualStates);
            tree->SetTemplates(templates);
            tree->SetTextLayout(text != nullptr ? text->Layout() : nullptr);
            tree->SetMeshResources(GetMeshResources());
            if (controlBehaviors != nullptr) {
                tree->SetControlBehaviors(controlBehaviors);
            }
            tree->SetNameScope(this, &ViewState::FindNameForElement);
        }
        return {};
    }

Base::Result<void> ViewState::GeneratedItemSubtreeChanged(
        Aero::Media::Visual& root,
        Controls::ItemSubtreeChange change,
        void* context) noexcept {
        auto* runtime = static_cast<ViewState*>(context);
        if (runtime == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Generated item subtree runtime context is null");
        }
        if (change ==
            Controls::ItemSubtreeChange::Unmounting) {
            Base::Result<Aero::VisualHandle>
                rootHandle =
                    runtime->tree->GetHandle(root);
            if (rootHandle) {
                for (std::uint32_t index = 0U;
                     index <
                         runtime->
                             pendingGeneratedVisuals.
                                 Size();) {
                    if (runtime->
                            pendingGeneratedVisuals[
                                index].index !=
                            rootHandle.Value().index ||
                        runtime->
                            pendingGeneratedVisuals[
                                index].generation !=
                            rootHandle.Value().generation) {
                        ++index;
                        continue;
                    }
                    for (std::uint32_t move =
                             index + 1U;
                         move <
                             runtime->
                                 pendingGeneratedVisuals.
                                     Size();
                         ++move) {
                        runtime->
                            pendingGeneratedVisuals[
                                move - 1U] =
                            runtime->
                                pendingGeneratedVisuals[
                                    move];
                    }
                    runtime->
                        pendingGeneratedVisuals.
                            PopBack();
                    return {};
                }
            }
            runtime->DetachUi(
                &root, {});
            return {};
        }
        if (runtime->deferGeneratedActivation ||
            (runtime->bindings != nullptr &&
             runtime->bindings->IsFlushing())) {
            Base::Result<Aero::VisualHandle>
                handle =
                    runtime->tree->GetHandle(root);
            if (!handle) return handle.GetStatus();
            return runtime->
                pendingGeneratedVisuals.
                    PushBack(handle.Value());
        }
        Base::Result<void> applied =
            runtime->ApplyUi(root);
        if (!applied) {
            runtime->DetachUi(
                &root, {});
            return applied.GetStatus();
        }
        Base::Result<void> attached =
            runtime->VisitAndAttach(root);
        if (!attached) {
            runtime->DetachUi(
                &root, {});
            return attached.GetStatus();
        }
        Base::Result<std::uint32_t> rebound =
            runtime->bindings->Flush();
        if (!rebound) {
            runtime->DetachUi(&root, {});
            return rebound.GetStatus();
        }
        Base::Result<std::uint32_t> started =
            runtime->StartLoadedAnimations(&root);
        if (!started) {
            runtime->DetachUi(
                &root, {});
            return started.GetStatus();
        }
        return {};
    }

Base::Result<void>
 ViewState::FlushGeneratedVisuals() noexcept {
        constexpr std::uint32_t MaximumWaves = 16U;
        for (std::uint32_t wave = 0U;
             wave < MaximumWaves;
             ++wave) {
            if (pendingGeneratedVisuals.Empty()) {
                return {};
            }
            Base::Vector<Aero::VisualHandle>
                pending =
                    std::move(
                        pendingGeneratedVisuals);
            pendingGeneratedVisuals.Clear();
            for (const Aero::VisualHandle handle :
                 pending) {
                Aero::Media::Visual* subtreeRoot =
                    tree->ResolveHandle(handle);
                if (subtreeRoot == nullptr) continue;
                Base::Result<void> applied =
                    ApplyUi(
                        *subtreeRoot);
                if (!applied) return applied.GetStatus();
                Base::Result<void> attached =
                    VisitAndAttach(
                        *subtreeRoot);
                if (!attached) return attached.GetStatus();
                Base::Result<std::uint32_t> reboundBeforeTriggers =
                    bindings->Flush();
                if (!reboundBeforeTriggers) {
                    return reboundBeforeTriggers.GetStatus();
                }
                Base::Result<std::uint32_t> started =
                    StartLoadedAnimations(subtreeRoot);
                if (!started) return started.GetStatus();
            }
            Base::Result<std::uint32_t> rebound =
                bindings->Flush();
            if (!rebound) return rebound.GetStatus();
        }
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Generated item visual activation exceeded the bounded activation waves");
    }

Base::Result<void> ViewState::AttachItemGenerator(
        Controls::ItemsControl& itemsControl) noexcept {
        if (AeroGuiInternal::
                HasAttachedGenerator(itemsControl)) {
            return {};
        }
        Controls::Panel* host = itemsControl.GetItemsHost();
        fprintf(stderr, "[ATTACHGEN] type=%u hasItemsHost=%d\n",
            static_cast<unsigned>(itemsControl.RuntimeType()),
            host != nullptr);
        if (host == nullptr) return {};

        Base::Result<Controls::ItemContainerGenerator*> created =
            AeroGuiInternal::CreateItemContainerGenerator(
                *tree,
                *layout,
                *values,
                styles,
                renderer,
                templates,
                &ViewState::GeneratedItemSubtreeChanged,
                this);
        if (!created) return created.GetStatus();
        Controls::ItemContainerGenerator* generator = created.Value();
        Base::Result<void> attached;
        if (metadata->Types().IsDerivedFrom(
                host->RuntimeType(),
                Controls::VirtualizingStackPanel::StaticTypeId())) {
            attached = generator->AttachVirtualized(
                itemsControl,
                *static_cast<Controls::VirtualizingStackPanel*>(host));
        } else {
            attached = generator->Attach(itemsControl, *host);
        }
        if (!attached) {
            delete generator;
            return attached.GetStatus();
        }

        Base::Result<void> generatedUiApplied = ApplyUi(*host);
        if (!generatedUiApplied) {
            DetachUi(host, {});
            static_cast<void>(generator->Detach());
            delete generator;
            return generatedUiApplied.GetStatus();
        }
        Base::Result<void> tracked = itemGenerators.PushBack(generator);
        if (!tracked) {
            static_cast<void>(generator->Detach());
            delete generator;
            return tracked.GetStatus();
        }
        fprintf(stderr, "[ATTACHGEN] attached ok type=%u itemCount=%u\n",
            static_cast<unsigned>(itemsControl.RuntimeType()),
            itemsControl.GetCount());
        return {};
    }

Base::Result<void> ViewState::AttachPendingItemGenerators(
        Aero::Media::Visual& rootVisual) noexcept {
        Base::Vector<Aero::Media::Visual*> stack(allocator);
        Base::Result<void> pushed = stack.PushBack(&rootVisual);
        if (!pushed) return pushed.GetStatus();
        while (!stack.Empty()) {
            Aero::Media::Visual* node = stack.Back();
            stack.PopBack();
            if (node == nullptr) continue;
            if (metadata->Types().IsDerivedFrom(
                    node->RuntimeType(),
                    Controls::ItemsControl::StaticTypeId())) {
                Base::Result<void> attached = AttachItemGenerator(
                    *static_cast<Controls::ItemsControl*>(node));
                if (!attached) return attached.GetStatus();
            }
            for (Aero::Media::Visual* child :
                 node->GetVisualChildren()) {
                pushed = stack.PushBack(child);
                if (!pushed) return pushed.GetStatus();
            }
        }
        return {};
    }

void ViewState::DestroyUiEngines() noexcept {
        if (tree != nullptr) {
            tree->SetLayout(nullptr);
            tree->SetBindings(nullptr);
            tree->SetStyles(nullptr);
            tree->SetEvents(nullptr);
            tree->SetInput(nullptr);
            tree->SetAnimations(nullptr);
            tree->SetVisualStates(nullptr);
            tree->SetTemplates(nullptr);
            tree->SetTextLayout(nullptr);
            tree->SetMeshResources(nullptr);
            tree->SetControlBehaviors(nullptr);
            tree->SetNameScope(nullptr, nullptr);
        }
        FreeObject(*allocator, Base::MemoryTag::Ui, styles);
        delete visualStates;
        visualStates = nullptr;
        FreeObject(*allocator, Base::MemoryTag::Ui, templates);
    }

Base::Result<void> ViewState::VisitAndAttach(
        Aero::Media::Visual& rootVisual) noexcept {
        Base::Vector<Aero::Media::Visual*> stack(allocator);
        Base::Result<void> pushed =
            stack.PushBack(&rootVisual);
        if (!pushed) return pushed.GetStatus();
        while (!stack.Empty()) {
            Aero::Media::Visual* node = stack.Back();
            stack.PopBack();
            if (node == nullptr) continue;
            const Meta::TypeId type = node->RuntimeType();
            if (controlBehaviors != nullptr) {
                Base::Result<void> attached = controlBehaviors->Attach(
                    *node, options.textInputMethodHost);
                if (!attached) return attached.GetStatus();
            }
            AttachTextLayout(
                *node,
                text != nullptr
                    ? text->Layout()
                    : nullptr);
            AttachPathResources(*node, GetMeshResources());
            if (metadata->Types().IsDerivedFrom(
                    type,
                    Controls::ItemsControl::StaticTypeId())) {
                Base::Result<void> attached = AttachItemGenerator(
                    *static_cast<Controls::ItemsControl*>(node));
                if (!attached) return attached.GetStatus();
            }
            const Base::Span<Aero::Media::Visual* const>
                children = node->GetVisualChildren();
            for (std::uint32_t index = 0U;
                 index < children.Size(); ++index) {
                pushed = stack.PushBack(children[index]);
                if (!pushed) return pushed.GetStatus();
            }
        }
        return {};
    }

void ViewState::ClearTextInputHosts(
        Aero::Media::Visual* node) noexcept {
        if (node == nullptr) return;
        if (metadata->Types().IsDerivedFrom(
                node->RuntimeType(),
                Controls::TextBox::StaticTypeId())) {
            static_cast<void>(
                static_cast<Controls::TextBox*>(node)->
                    SetInputMethodHost(nullptr));
        }
        if (metadata->Types().IsDerivedFrom(
                node->RuntimeType(),
                Controls::PasswordBox::
                    StaticTypeId())) {
            static_cast<void>(
                static_cast<
                    Controls::PasswordBox*>(node)->
                    SetInputMethodHost(nullptr));
        }
        for (Aero::Media::Visual* child :
             node->GetVisualChildren()) {
            ClearTextInputHosts(child);
        }
    }

bool ViewState::IsInVisualSubtree(
        Aero::Media::Visual* node,
        const Aero::Media::Visual& fragmentRoot) const noexcept {
        while (node != nullptr) {
            if (node == &fragmentRoot) return true;
            node = node->GetLogicalParent() != nullptr
                ? node->GetLogicalParent()
                : node->GetVisualParent();
        }
        return false;
    }

Base::Result<std::uint32_t> ViewState::ExecuteFrame(
    View& view) noexcept
{
    ViewState* state_ = this;
    if (!state_->initialized) {
        return ViewNotInitialized(
            "View must be initialized before running frames");
    }
    if (!state_->animationEventStatus.IsOk()) {
        return state_->animationEventStatus;
    }
    if (state_->styles != nullptr &&
        !state_->styles->LastActionStatus().IsOk()) {
        return state_->styles->LastActionStatus();
    }
    bool deviceGenerationChanged = false;
    GuiState& guiState = static_cast<GuiState&>(*state_->gui);
    const bool fontProviderChanged =
        guiState.fontChangeGeneration != state_->seenFontProviderChange;
    if (fontProviderChanged) {
        state_->seenFontProviderChange = guiState.fontChangeGeneration;
    }
    if (state_->images != nullptr &&
        guiState.textureChangeGeneration !=
            state_->seenTextureProviderChange) {
        if (guiState.textureChangesLost) {
            state_->images->Invalidate({}, state_->GetImageResources());
        } else {
            for (const XamlProviderChangeRecord& change :
                 guiState.textureChanges) {
                if (change.generation <=
                        state_->seenTextureProviderChange) {
                    continue;
                }
                state_->images->Invalidate(
                    change.uri,
                    state_->GetImageResources());
            }
        }
        state_->seenTextureProviderChange =
            guiState.textureChangeGeneration;
    }
    if (state_->device) {
        if (state_->device->State() != RenderDeviceState::Ready) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Device is not ready");
        }
        const std::uint64_t generation =
            state_->device->Generation();
        if (generation !=
            state_->deviceGeneration) {
            deviceGenerationChanged = true;
            Aero::Media::Visual* rootVisual =
                state_->RootVisual();
            if (rootVisual != nullptr) {
                Base::Result<void> invalidated =
                    state_->renderer->Invalidate(
                        *rootVisual,
                        Aero::Render::RenderInvalidation::All);
                if (!invalidated) {
                    return invalidated.GetStatus();
                }
            }
            state_->VisitPaths(
                rootVisual,
                state_->GetMeshResources(),
                true);
            if (state_->tree != nullptr) {
                state_->tree->SetMeshResources(state_->GetMeshResources());
            }
            state_->deviceGeneration = generation;
        }
    }
    if (state_->text != nullptr) {
        Base::Result<bool> synchronized =
            state_->text->SynchronizeBackend(
                *state_->device,
                state_->publicRenderer.Resources().text,
                deviceGenerationChanged || fontProviderChanged);
        if (!synchronized) {
            return synchronized.GetStatus();
        }
        if (synchronized.Value()) {
            state_->VisitTextElements(
                state_->RootVisual(),
                state_->text->Layout(),
                true);
        }
    }
    if (state_->images != nullptr) {
        Base::Result<bool> synchronized =
            state_->images->Synchronize(
                state_->RootVisual(),
                state_->loadedDocument.canonicalUri,
                state_->xamlRuntime->Providers(),
                guiState.textureProvider.Get(),
                state_->GetImageResources(),
                deviceGenerationChanged);
        if (!synchronized) {
            return synchronized.GetStatus();
        }
        if (synchronized.Value()) {
            Aero::Media::Visual* rootVisual =
                state_->RootVisual();
            if (rootVisual != nullptr) {
                Base::Result<void> invalidated =
                    state_->renderer->Invalidate(
                        *rootVisual,
                        Aero::Render::RenderInvalidation::All);
                if (!invalidated) {
                    return invalidated.GetStatus();
                }
            }
        }
    }
    const ::Aero::Threading::DispatcherFramePhase phases[] = {
        ::Aero::Threading::DispatcherFramePhase::BeginFrame,
        ::Aero::Threading::DispatcherFramePhase::Input,
        ::Aero::Threading::DispatcherFramePhase::PropertyChanges,
        ::Aero::Threading::DispatcherFramePhase::DataBind,
        ::Aero::Threading::DispatcherFramePhase::Animation,
        ::Aero::Threading::DispatcherFramePhase::Lifecycle,
        ::Aero::Threading::DispatcherFramePhase::Layout,
        ::Aero::Threading::DispatcherFramePhase::RenderCommit,
        ::Aero::Threading::DispatcherFramePhase::EndFrame};
    ViewFrameResult result;
    for (::Aero::Threading::DispatcherFramePhase phase : phases) {
        if (phase ==
                ::Aero::Threading::DispatcherFramePhase::Layout &&
            state_->HasAttachedRoot()) {
            Base::Result<void> completed =
                state_->CompleteVisualEdges({
                    state_->loadedDocument.visualContent.mountEdges.Data(),
                    state_->loadedDocument.visualContent.mountEdges.Size()});
            if (!completed) return completed.GetStatus();
        }
        if (phase ==
            ::Aero::Threading::DispatcherFramePhase::
                RenderCommit) {
            ::Aero::Media::CompositionTarget::RaiseRendering(view);
            Base::Result<void> overlays =
                state_->SynchronizeOverlays();
            if (!overlays) {
                return overlays.GetStatus();
            }
        }
        Base::Result<std::uint32_t> ran =
            state_->dispatcher->RunFramePhase(phase);
        if (!ran) return ran.GetStatus();
        if (phase ==
                ::Aero::Threading::DispatcherFramePhase::Lifecycle) {
            Base::Result<std::uint32_t> focused =
                state_->ProcessPendingFocus();
            if (!focused) return focused.GetStatus();
            if (result.callbackCount >
                UINT32_MAX - focused.Value()) {
                return Base::Status::Failure(
                    Base::ErrorCode::OutOfRange,
                    "View callback count overflow");
            }
            result.callbackCount += focused.Value();
        }
        if (phase ==
                ::Aero::Threading::DispatcherFramePhase::DataBind) {
            Base::Result<void> generatedVisualsFlushed =
                state_->FlushGeneratedVisuals();
            if (!generatedVisualsFlushed) {
                return generatedVisualsFlushed.GetStatus();
            }
        }
        if (phase == ::Aero::Threading::DispatcherFramePhase::Layout &&
            !state_->layout->LastFlushStatus().IsOk()) {
            return state_->layout->LastFlushStatus();
        }
        if (phase ==
                ::Aero::Threading::DispatcherFramePhase::Layout &&
            state_->layout->Diagnostics().arrangedCount != 0U) {
            for (auto& behavior : state_->attachedBehaviorInstances) {
                if (behavior.instance) {
                    behavior.instance->NotifyLayoutUpdated();
                }
            }
            Aero::Media::Visual* rootVisual =
                state_->RootVisual();
            if (rootVisual != nullptr &&
                state_->renderer != nullptr) {
                Base::Result<void> invalidated =
                    state_->renderer->Invalidate(
                        *rootVisual,
                        Aero::Render::RenderInvalidation::All);
                if (!invalidated) {
                    return invalidated.GetStatus();
                }
            }
        }
        if (phase == ::Aero::Threading::DispatcherFramePhase::Animation &&
            state_->animations != nullptr) {
            const Base::Status animationStatus =
                state_->animations->LastTickStatus();
            if (!animationStatus.IsOk()) {
                return animationStatus;
            }
            const auto animationDiagnostics =
                state_->animations->Diagnostics();
            if (animationDiagnostics.appliedValueCount != 0U) {
                Aero::Media::Visual* rootVisual = state_->RootVisual();
                if (rootVisual != nullptr &&
                    state_->renderer != nullptr) {
                    Base::Result<void> invalidated =
                        state_->renderer->Invalidate(
                            *rootVisual,
                            Aero::Render::RenderInvalidation::All);
                    if (!invalidated) return invalidated.GetStatus();
                }
            }
            Base::Result<std::uint32_t> completed =
                state_->ProcessStoryboardCompletions();
            if (!completed) {
                return completed.GetStatus();
            }
            if (result.callbackCount >
                UINT32_MAX - completed.Value()) {
                return Base::Status::Failure(
                    Base::ErrorCode::OutOfRange,
                    "Frame callback count overflow");
            }
            result.callbackCount += completed.Value();
        }
        if (phase == ::Aero::Threading::DispatcherFramePhase::Lifecycle &&
            state_->animations != nullptr) {
            Base::Result<std::uint32_t> initialValues =
                state_->animations->ApplyPendingInitialValues();
            if (!initialValues) return initialValues.GetStatus();
            if (result.callbackCount >
                UINT32_MAX - initialValues.Value()) {
                return Base::Status::Failure(
                    Base::ErrorCode::OutOfRange,
                    "Initial animation callback count overflow");
            }
            result.callbackCount += initialValues.Value();
        }
        if (result.callbackCount >
            UINT32_MAX - ran.Value()) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Frame callback count overflow");
        }
        result.callbackCount += ran.Value();
        if (phase ==
            ::Aero::Threading::DispatcherFramePhase::RenderCommit) {
            const Base::Status committed =
                state_->renderer->LastCommitStatus();
            if (!committed.IsOk()) return committed;
            if (state_->animations != nullptr) {
                state_->animations->CommitPendingInitialValues();
            }
        }
    }
    if (state_->text != nullptr) {
        Base::Result<std::uint32_t> collected =
            state_->text->CollectGarbage();
        if (!collected) return collected.GetStatus();
    }
    result.frameNumber = ++state_->frameNumber;
    const Aero::LayoutDiagnostics layout =
        state_->layout->Diagnostics();
    result.layout.passVersion = layout.passVersion;
    result.layout.measuredCount = layout.measuredCount;
    result.layout.arrangedCount = layout.arrangedCount;
    result.layout.pendingMeasureCount =
        layout.pendingMeasureCount;
    result.layout.pendingArrangeCount =
        layout.pendingArrangeCount;
    const ::Aero::Render::RenderDiagnostics render =
        state_->renderer->Diagnostics();
    result.render.snapshotVersion = render.commitVersion;
    result.render.nodeCount = render.nodeCount;
    result.render.commandCount = render.commandCount;
    result.render.glyphCommandCount =
        render.glyphCommandCount;
    result.render.dirtyCount = render.dirtyCount;
    result.render.snapshotHash = render.frameHash;
    if (state_->device) {
        const Diagnostics::RenderFrameStatistics deviceStatistics =
            Diagnostics::GetLastRenderFrameStatistics(*state_->device);
        result.render.drawPacketCount =
            deviceStatistics.drawPacketCount;
        result.render.batchCount =
            deviceStatistics.batchCount;
        result.render.drawCallCount =
            deviceStatistics.drawCallCount;
        result.render.mergedPacketCount =
            deviceStatistics.mergedPacketCount;
        result.render.barrierCount =
            deviceStatistics.barrierCount;
        result.render.instanceCount =
            deviceStatistics.instanceCount;
        result.render.stateBindingCount =
            deviceStatistics.stateBindingCount;
        result.render.batchingEnabled =
            deviceStatistics.batchingEnabled;
    }
    return result.callbackCount;
}

namespace {

[[maybe_unused]] Base::Result<std::uint32_t> AdvanceViewAnimations(
    ViewState& state,
    std::uint32_t elapsedMilliseconds) noexcept {
    ViewState* state_ = &state;
    if (!state_->mounted || state_->animations == nullptr) {
        return ViewNotInitialized(
            "Animation timing requires a mounted View");
    }
    Base::Result<std::uint32_t> advanced =
        state_->animations->AdvanceBy(
        static_cast<Aero::Media::Animation::AnimationTime>(
            elapsedMilliseconds) * 1000U);
    if (!advanced) return advanced.GetStatus();
    Base::Result<std::uint32_t> completed =
        state_->ProcessStoryboardCompletions();
    if (!completed) return completed.GetStatus();
    if (advanced.Value() >
        UINT32_MAX - completed.Value()) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Animation action count overflow");
    }
    return advanced.Value() + completed.Value();
}

} // namespace

Base::Result<std::uint32_t> AdvanceViewClocks(
    ViewState& state,
    std::uint32_t elapsedMilliseconds) noexcept {
    ViewState* state_ = &state;
    if (!state_->mounted || state_->animations == nullptr) {
        return ViewNotInitialized(
            "View timing requires a mounted animation manager");
    }
    std::uint32_t actionCount = 0U;
    if (state_->controlBehaviors != nullptr) {
        Base::Result<std::uint32_t> controls =
            state_->controlBehaviors->AdvanceTime(
                elapsedMilliseconds);
        if (!controls) return controls.GetStatus();
        actionCount = controls.Value();
    }
    Base::Result<std::uint32_t> toolTips =
        state_->AdvanceToolTipTime(
            elapsedMilliseconds);
    if (!toolTips) return toolTips.GetStatus();
    if (actionCount > UINT32_MAX - toolTips.Value()) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Control timing action count overflow");
    }
    actionCount += toolTips.Value();
    if (state_->options.automaticAnimationClock) {
        return actionCount;
    }
    Base::Result<std::uint32_t> animations =
        state_->animations->AdvanceBy(
            static_cast<Aero::Media::Animation::AnimationTime>(
                elapsedMilliseconds) * 1000U);
    if (!animations) return animations.GetStatus();
    Base::Result<std::uint32_t> completed =
        state_->ProcessStoryboardCompletions();
    if (!completed) return completed.GetStatus();
    if (actionCount > UINT32_MAX - animations.Value()) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "View timing action count overflow");
    }
    actionCount += animations.Value();
    if (actionCount > UINT32_MAX - completed.Value()) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Storyboard completed action count overflow");
    }
    return actionCount + completed.Value();
}


} // namespace Aero
