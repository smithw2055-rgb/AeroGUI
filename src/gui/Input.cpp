#include <Aero/Input.hpp>

#include <Aero/Base/Utf8.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Media/Transforms.hpp>

#include <cmath>
#include "gui/InputInternal.hpp"
#include "gui/ElementInternal.hpp"

namespace Aero::Input {

using namespace Aero::Meta;
using namespace Aero::Threading;
namespace {

bool Contains(Size size, Point point) noexcept {
    return point.x >= 0.0 && point.y >= 0.0 &&
        point.x < size.width && point.y < size.height;
}

bool IsVisualDescendantOrSelf(
    const Visual& root, const Visual& target) noexcept {
    const Visual* current = &target;
    while (current != &root) {
        current = current->GetVisualParent();
        if (current == nullptr) return false;
    }
    return true;
}

bool ParentToLocal(
    UIElement& element,
    Point parentPosition,
    Point& localPosition) noexcept {
    const Rect slot = element.GetLayoutSlot();
    Point translated{
        parentPosition.x - slot.x,
        parentPosition.y - slot.y};
    FrameworkElement* framework =
        element.AsFrameworkElement();
    if (framework == nullptr) {
        localPosition = translated;
        return true;
    }
    Base::Transform2D inverse;
    if (!Media::InvertTransform(
            framework->GetLocalVisualTransform(),
            inverse)) {
        return false;
    }
    localPosition =
        Media::TransformPoint(inverse, translated);
    return IsFinite(localPosition);
}

} // namespace

} // namespace Aero::Input

namespace Aero::Internal {

using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero::Input;

Base::Result<void> ElementPrivate::SetMouseOver(Aero::UIElement& element, bool value) noexcept {
    element.SetMouseOverState(value);
    return {};
}

Base::Result<void> ElementPrivate::SetPressed(Aero::UIElement& element, bool value) noexcept {
    element.SetPressedState(value);
    return {};
}

Base::Result<void> ElementPrivate::SetKeyboardFocused(Aero::UIElement& element, bool value) noexcept {
    element.SetKeyboardFocusedState(value);
    return {};
}

Base::Result<void> ElementPrivate::SetKeyboardFocusWithin(Aero::UIElement& element, bool value) noexcept {
    element.SetKeyboardFocusWithinState(value);
    return {};
}

Base::Result<void> HitTestState::SetOverlays(
    Base::Span<UIElement* const> overlays,
    Base::Span<const Point> origins) noexcept {
    if (overlays.Size() != origins.Size()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Input overlay elements and origins must have equal lengths");
    }
    Base::Vector<OverlayRecord> next;
    Base::Result<void> reserved =
        next.Reserve(overlays.Size());
    if (!reserved) return reserved.GetStatus();
    for (std::uint32_t index = 0U;
         index < overlays.Size();
         ++index) {
        UIElement* overlay = overlays[index];
        if (overlay == nullptr) continue;
        if (!IsFinite(origins[index])) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Input overlay origin must be finite");
        }
        bool duplicate = false;
        for (const OverlayRecord& current : next) {
            duplicate =
                duplicate ||
                current.element == overlay;
        }
        if (duplicate) continue;
        Base::Result<void> appended =
            next.PushBack(
                {overlay, origins[index]});
        if (!appended) return appended.GetStatus();
    }
    overlays_ = std::move(next);
    return {};
}

Base::Result<HitTestResult> HitTestState::HitTest(
    Visual& root, Point position) const noexcept {
    if (!IsFinite(position)) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Hit-test position must be finite");
    }
    UIElement* rootElement = AsUIElement(root);
    if (rootElement == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Hit-test root is not a registered UIElement");
    }
    for (std::uint32_t index = overlays_.Size();
         index > 0U;
         --index) {
        const OverlayRecord& record =
            overlays_[index - 1U];
        UIElement* overlay = record.element;
        if (overlay == nullptr ||
            !IsVisualDescendantOrSelf(root, *overlay) ||
            !overlay->GetIsArrangeValid() ||
            !overlay->GetIsVisible()) {
            continue;
        }
        Point local{
            position.x - record.origin.x,
            position.y - record.origin.y};
        FrameworkElement* overlayFramework =
            overlay->AsFrameworkElement();
        if (overlayFramework != nullptr) {
            Base::Transform2D inverse;
            if (!Media::InvertTransform(
                    overlayFramework->
                        GetLocalVisualTransform(),
                    inverse)) {
                continue;
            }
            local = Media::TransformPoint(
                inverse,
                local);
        }
        Base::Result<HitTestResult> hit =
            HitTestElement(*overlay, local);
        if (!hit) return hit.GetStatus();
        if (hit.Value().HasTarget()) return hit;
    }
    Point rootLocal;
    if (!ParentToLocal(
            *rootElement,
            position,
            rootLocal)) {
        return HitTestResult{};
    }
    return HitTestElement(
        *rootElement,
        rootLocal);
}

Base::Result<HitTestResult> HitTestState::RootToLocal(
    Visual& root, Visual& target, Point position) const noexcept {
    if (!IsFinite(position)) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Pointer position must be finite");
    }
    UIElement* rootElement = AsUIElement(root);
    if (rootElement == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Hit-test root is not a registered UIElement");
    }
    UIElement* targetElement = AsUIElement(target);
    if (targetElement == nullptr || !targetElement->GetIsArrangeValid()) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Pointer capture target must be an arranged UIElement");
    }

    Base::Vector<UIElement*> path;
    Visual* current = &target;
    while (current != &root) {
        UIElement* currentElement = AsUIElement(*current);
        if (currentElement == nullptr || !currentElement->GetIsArrangeValid()) {
            return Base::Status::Failure(Base::ErrorCode::InvalidState,
                "Pointer capture route contains an unarranged element");
        }
        Visual* parent = current->GetVisualParent();
        if (parent == nullptr) {
            return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
                "Pointer capture target is not below the input root");
        }
        Base::Result<void> appended =
            path.PushBack(currentElement);
        if (!appended) {
            return appended.GetStatus();
        }
        current = parent;
    }
    Point local;
    if (!ParentToLocal(
            *rootElement,
            position,
            local)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Pointer input root contains a non-invertible transform");
    }
    for (std::uint32_t index = path.Size();
         index > 0U;
         --index) {
        Point next;
        if (!ParentToLocal(
                *path[index - 1U],
                local,
                next)) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Pointer capture route contains a non-invertible transform");
        }
        local = next;
    }
    return HitTestResult{targetElement, local};
}

bool HitTestState::IsOverlay(
    const UIElement& element) const noexcept {
    for (const OverlayRecord& overlay :
         overlays_) {
        if (overlay.element == &element) {
            return true;
        }
    }
    return false;
}

Base::Result<HitTestResult> HitTestState::HitTestElement(
    UIElement& element, Point position) const noexcept {
    if (!element.GetIsVisible() ||
        !element.GetIsHitTestVisible()) {
        return HitTestResult{};
    }
    const bool contains =
        Contains(element.GetRenderSize(), position);
    if (!contains && element.GetClipToBounds()) {
        return HitTestResult{};
    }

    const Base::Span<Visual* const> children = Aero::Internal::ElementPrivate::VisualChildren(element);
    for (std::uint32_t index = children.Size(); index > 0U; --index) {
        Visual* childNode = children[index - 1U];
        if (childNode == nullptr) continue;
        UIElement* child = AsUIElement(*childNode);
        if (child == nullptr) continue;
        if (IsOverlay(*child)) continue;
        // Hidden/template branches may be present in the visual tree before
        // they receive a layout slot. They are not hittable and must not
        // poison hit testing for an otherwise arranged root.
        if (!child->GetIsArrangeValid()) continue;
        Point childPosition;
        if (!ParentToLocal(
                *child,
                position,
                childPosition)) {
            continue;
        }
        Base::Result<HitTestResult> nested =
            HitTestElement(
                *child,
                childPosition);
        if (!nested) return nested.GetStatus();
        if (nested.Value().HasTarget()) return nested;
    }
    return contains
        ? HitTestResult{&element, position}
        : HitTestResult{};
}

PointerStateMachine::PointerStateMachine(HitTestState& hitTests,
    EventRouter& events) noexcept
    : hitTests_(&hitTests), events_(&events),
      captures_(&Base::GetDefaultAllocator()),
      states_(&Base::GetDefaultAllocator()) {}

std::uint32_t PointerStateMachine::FindCapture(
    std::uint32_t pointerId) const noexcept {
    for (std::uint32_t index = 0U; index < captures_.Size(); ++index) {
        if (captures_[index].pointerId == pointerId) return index;
    }
    return UINT32_MAX;
}

void PointerStateMachine::RemoveCaptureAt(std::uint32_t index) noexcept {
    AERO_ASSERT(index < captures_.Size());
    for (std::uint32_t next = index + 1U; next < captures_.Size(); ++next) {
        captures_[next - 1U] = captures_[next];
    }
    captures_.PopBack();
}

std::uint32_t PointerStateMachine::FindState(
    std::uint32_t pointerId) const noexcept {
    for (std::uint32_t index = 0U; index < states_.Size(); ++index) {
        if (states_[index].pointerId == pointerId) return index;
    }
    return UINT32_MAX;
}

bool PointerStateMachine::HasHover(
    VisualHandle target, std::uint32_t ignoredIndex) const noexcept {
    if (!target.IsValid()) return false;
    ElementTree* tree = root_ != nullptr
        ? Aero::Internal::ElementPrivate::Tree(*root_)
        : nullptr;
    Visual* targetVisual =
        tree != nullptr
        ? tree->ResolveHandle(target)
        : nullptr;
    if (targetVisual == nullptr) return false;
    for (std::uint32_t index = 0U; index < states_.Size(); ++index) {
        if (index == ignoredIndex) continue;
        Visual* current =
            tree->ResolveHandle(states_[index].hover);
        while (current != nullptr) {
            if (current == targetVisual) return true;
            current = current->GetVisualParent() != nullptr
                ? current->GetVisualParent()
                : current->GetLogicalParent();
        }
    }
    return false;
}

bool PointerStateMachine::HasPressed(
    VisualHandle target, std::uint32_t ignoredIndex) const noexcept {
    if (!target.IsValid()) return false;
    for (std::uint32_t index = 0U; index < states_.Size(); ++index) {
        if (index == ignoredIndex) continue;
        if (states_[index].pressed.index == target.index &&
            states_[index].pressed.generation == target.generation) {
            return true;
        }
    }
    return false;
}

Base::Result<void> PointerStateMachine::UpdateHover(
    std::uint32_t pointerId, UIElement* target) noexcept {
    ElementTree* tree = Aero::Internal::ElementPrivate::Tree(*root_);
    if (tree == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Pointer state requires an ElementTree");
    }
    VisualHandle next;
    if (target != nullptr && target->GetIsEnabled()) {
        Base::Result<VisualHandle> handle = tree->GetHandle(*target);
        if (!handle) return handle.GetStatus();
        next = handle.Value();
    }
    std::uint32_t index = FindState(pointerId);
    if (index == UINT32_MAX) {
        Base::Result<void> appended =
            states_.PushBack({pointerId, {}, {}});
        if (!appended) return appended.GetStatus();
        index = states_.Size() - 1U;
    }
    const VisualHandle previous = states_[index].hover;
    const bool sameTarget =
        previous.index == next.index &&
        previous.generation == next.generation;
    if (sameTarget) {
        Visual* current =
            next.IsValid()
            ? tree->ResolveHandle(next)
            : nullptr;
        bool stateIsCurrent = true;
        while (current != nullptr) {
            UIElement* element = current->AsUIElement();
            if (element != nullptr &&
                !element->GetIsMouseOver()) {
                stateIsCurrent = false;
                break;
            }
            current = current->GetVisualParent() != nullptr
                ? current->GetVisualParent()
                : current->GetLogicalParent();
        }
        if (stateIsCurrent) return {};
    }

    Visual* previousVisual =
        previous.IsValid()
        ? tree->ResolveHandle(previous)
        : nullptr;
    Visual* nextVisual =
        next.IsValid()
        ? tree->ResolveHandle(next)
        : nullptr;
    const auto isAncestorOrSelf = [](
        Visual* ancestor,
        Visual* descendant) noexcept {
        Visual* current = descendant;
        while (current != nullptr) {
            if (current == ancestor) return true;
            current = current->GetVisualParent() != nullptr
                ? current->GetVisualParent()
                : current->GetLogicalParent();
        }
        return false;
    };

    Visual* current = nextVisual;
    while (current != nullptr) {
        UIElement* element = current->AsUIElement();
        if (element != nullptr &&
            (!element->GetIsMouseOver() ||
             sameTarget ||
             !isAncestorOrSelf(current, previousVisual))) {
            Base::Result<VisualHandle> handle =
                tree->GetHandle(*current);
            if (!handle) return handle.GetStatus();
            if (!HasHover(handle.Value(), index) ||
                !element->GetIsMouseOver()) {
                Base::Result<void> set =
                    ElementPrivate::SetMouseOver(*element, true);
                if (!set) return set.GetStatus();
                if (!stateChanged_.Empty()) {
                    stateChanged_.Invoke(*element);
                }
            }
        }
        current = current->GetVisualParent() != nullptr
            ? current->GetVisualParent()
            : current->GetLogicalParent();
    }

    current = previousVisual;
    while (current != nullptr) {
        UIElement* element = current->AsUIElement();
        if (element != nullptr &&
            !sameTarget &&
            !isAncestorOrSelf(current, nextVisual)) {
            Base::Result<VisualHandle> handle =
                tree->GetHandle(*current);
            if (!handle) return handle.GetStatus();
            if (!HasHover(handle.Value(), index)) {
                Base::Result<void> cleared =
                    ElementPrivate::SetMouseOver(*element, false);
                if (!cleared) {
                    return cleared.GetStatus();
                }
                if (!stateChanged_.Empty()) {
                    stateChanged_.Invoke(*element);
                }
            }
        }
        current = current->GetVisualParent() != nullptr
            ? current->GetVisualParent()
            : current->GetLogicalParent();
    }
    states_[index].hover = next;
    return {};
}

Base::Result<void> PointerStateMachine::UpdatePressed(
    std::uint32_t pointerId, UIElement* target) noexcept {
    ElementTree* tree = Aero::Internal::ElementPrivate::Tree(*root_);
    if (tree == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Pointer state requires an ElementTree");
    }
    VisualHandle next;
    if (target != nullptr && target->GetIsEnabled()) {
        Base::Result<VisualHandle> handle = tree->GetHandle(*target);
        if (!handle) return handle.GetStatus();
        next = handle.Value();
    }
    std::uint32_t index = FindState(pointerId);
    if (index == UINT32_MAX) {
        Base::Result<void> appended =
            states_.PushBack({pointerId, {}, {}});
        if (!appended) return appended.GetStatus();
        index = states_.Size() - 1U;
    }
    const VisualHandle previous = states_[index].pressed;
    if (previous.index == next.index &&
        previous.generation == next.generation) return {};

    UIElement* nextElement = nullptr;
    if (next.IsValid() && !HasPressed(next, index)) {
        Visual* visual = tree->ResolveHandle(next);
        nextElement = visual != nullptr ? visual->AsUIElement() : nullptr;
        if (nextElement != nullptr) {
            Base::Result<void> set = ElementPrivate::SetPressed(*nextElement, true);
            if (!set) return set.GetStatus();
            if (!stateChanged_.Empty()) {
                stateChanged_.Invoke(*nextElement);
            }
        }
    }
    if (previous.IsValid() && !HasPressed(previous, index)) {
        Visual* visual = tree->ResolveHandle(previous);
        UIElement* previousElement =
            visual != nullptr ? visual->AsUIElement() : nullptr;
        if (previousElement != nullptr) {
            Base::Result<void> cleared =
                ElementPrivate::SetPressed(*previousElement, false);
            if (!cleared) {
                if (nextElement != nullptr) {
                    static_cast<void>(
                        ElementPrivate::SetPressed(*nextElement, false));
                }
                return cleared.GetStatus();
            }
            if (!stateChanged_.Empty()) {
                stateChanged_.Invoke(*previousElement);
            }
        }
    }
    states_[index].pressed = next;
    return {};
}

UIElement* PointerStateMachine::CapturedNode(std::uint32_t pointerId) noexcept {
    const std::uint32_t index = FindCapture(pointerId);
    if (index == UINT32_MAX) return nullptr;
    ElementTree* tree = Aero::Internal::ElementPrivate::Tree(*root_);
    Visual* target = tree != nullptr
        ? tree->ResolveHandle(captures_[index].target) : nullptr;
    UIElement* element = target != nullptr ? target->AsUIElement() : nullptr;
    if (element == nullptr || !element->GetIsLoaded() ||
        !IsVisualDescendantOrSelf(*root_, *element)) {
        UIElement* lost = element;
        RemoveCaptureAt(index);
        static_cast<void>(UpdatePressed(pointerId, nullptr));
        if (!captureChanged_.Empty()) {
            captureChanged_.Invoke(pointerId, lost, false);
        }
        return nullptr;
    }
    return element;
}

Base::Result<void> PointerStateMachine::CapturePointer(
    std::uint32_t pointerId, UIElement& target) noexcept {
    Base::Result<void> access = root_->VerifyAccess();
    if (!access) return access.GetStatus();
    ElementTree* tree = Aero::Internal::ElementPrivate::Tree(*root_);
    if (tree == nullptr || Aero::Internal::ElementPrivate::Tree(target) != tree || !target.GetIsLoaded()) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Pointer capture target must be loaded in the input tree");
    }
    Base::Result<VisualHandle> handle = tree->GetHandle(target);
    if (!handle) return handle.GetStatus();
    Base::Result<HitTestResult> local = hitTests_->RootToLocal(
        *root_, target, {});
    if (!local) return local.GetStatus();
    const std::uint32_t index = FindCapture(pointerId);
    if (index != UINT32_MAX) {
        Visual* previousVisual =
            tree->ResolveHandle(captures_[index].target);
        UIElement* previous = previousVisual != nullptr
            ? previousVisual->AsUIElement() : nullptr;
        captures_[index].target = handle.Value();
        if (!captureChanged_.Empty() && previous != &target) {
            captureChanged_.Invoke(pointerId, previous, false);
            captureChanged_.Invoke(pointerId, &target, true);
        }
        return {};
    }
    Base::Result<void> appended =
        captures_.PushBack({pointerId, handle.Value()});
    if (!appended) return appended.GetStatus();
    if (!captureChanged_.Empty()) {
        captureChanged_.Invoke(pointerId, &target, true);
    }
    return {};
}

Base::Result<bool> PointerStateMachine::ReleasePointer(
    std::uint32_t pointerId) noexcept {
    Base::Result<void> access = root_->VerifyAccess();
    if (!access) return access.GetStatus();
    const std::uint32_t index = FindCapture(pointerId);
    if (index == UINT32_MAX) return false;
    Base::Result<void> state = UpdatePressed(pointerId, nullptr);
    if (!state) return state.GetStatus();
    Visual* visual = Aero::Internal::ElementPrivate::Tree(*root_)->ResolveHandle(
        captures_[index].target);
    UIElement* target =
        visual != nullptr ? visual->AsUIElement() : nullptr;
    RemoveCaptureAt(index);
    if (!captureChanged_.Empty()) {
        captureChanged_.Invoke(pointerId, target, false);
    }
    return true;
}

Base::Result<PointerDispatchResult> PointerStateMachine::Dispatch(
    const PointerInput& input) noexcept {
    Base::Result<void> access = root_->VerifyAccess();
    if (!access) return access.GetStatus();
    if (!IsFinite(input.position)) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Pointer position must be finite");
    }
    if (input.action == PointerAction::Wheel &&
        (!std::isfinite(input.wheelDeltaX) ||
            !std::isfinite(input.wheelDeltaY))) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Pointer wheel delta must be finite");
    }
    RoutedEventHandle previewEvent;
    RoutedEventHandle event;
    switch (input.action) {
    case PointerAction::Move:
        previewEvent = UIElement::PreviewMouseMoveEvent;
        event = UIElement::MouseMoveEvent;
        break;
    case PointerAction::Down:
        previewEvent = UIElement::PreviewMouseDownEvent;
        event = UIElement::MouseDownEvent;
        break;
    case PointerAction::Up:
        previewEvent = UIElement::PreviewMouseUpEvent;
        event = UIElement::MouseUpEvent;
        break;
    case PointerAction::Wheel:
        previewEvent = UIElement::PreviewMouseWheelEvent;
        event = UIElement::MouseWheelEvent;
        break;
    }
    UIElement* captured = CapturedNode(input.pointerId);
    Base::Result<HitTestResult> hit = captured != nullptr
        ? hitTests_->RootToLocal(*root_, *captured, input.position)
        : hitTests_->HitTest(*root_, input.position);
    if (!hit) return hit.GetStatus();
    PointerDispatchResult result;
    result.hit = hit.Value();
    UIElement* stateTarget = result.hit.HasTarget() &&
        result.hit.target->GetIsEnabled() ? result.hit.target : nullptr;
    if (captured != nullptr && stateTarget != nullptr &&
        !Contains(stateTarget->GetRenderSize(), result.hit.position)) {
        stateTarget = nullptr;
    }
    Base::Result<void> hover =
        UpdateHover(input.pointerId, stateTarget);
    if (!hover) return hover.GetStatus();
    if (input.action == PointerAction::Down) {
        Base::Result<void> pressed =
            UpdatePressed(input.pointerId, stateTarget);
        if (!pressed) return pressed.GetStatus();
    } else if (input.action == PointerAction::Up) {
        Base::Result<void> released =
            UpdatePressed(input.pointerId, nullptr);
        if (!released) return released.GetStatus();
    }
    if (!result.hit.HasTarget()) return result;
    Base::Result<void> raised;
    if (input.action == PointerAction::Move) {
        MouseEventArgs args;
        args.SetPointerId(input.pointerId);
        args.SetPosition(result.hit.position);
        raised = events_->RaiseEvent(*result.hit.target, previewEvent, &args);
        if (raised) raised = events_->RaiseEvent(*result.hit.target, event, &args);
    } else if (input.action == PointerAction::Wheel) {
        MouseWheelEventArgs args;
        args.SetPointerId(input.pointerId);
        args.SetPosition(result.hit.position);
        args.SetDeltaX(input.wheelDeltaX);
        args.SetDeltaY(input.wheelDeltaY);
        raised = events_->RaiseEvent(*result.hit.target, previewEvent, &args);
        if (raised) raised = events_->RaiseEvent(*result.hit.target, event, &args);
    } else {
        MouseButtonEventArgs args;
        args.SetPointerId(input.pointerId);
        args.SetPosition(result.hit.position);
        args.SetChangedButton(input.changedButton);
        args.SetButtonState(input.action == PointerAction::Down
            ? MouseButtonState::Pressed : MouseButtonState::Released);
        raised = events_->RaiseEvent(*result.hit.target, previewEvent, &args);
        if (raised) raised = events_->RaiseEvent(*result.hit.target, event, &args);
    }
    if (!raised) return raised.GetStatus();
    result.routed = true;
    if (input.action == PointerAction::Up) {
        const std::uint32_t index = FindCapture(input.pointerId);
        if (index != UINT32_MAX) {
            Visual* visual = Aero::Internal::ElementPrivate::Tree(*root_)->ResolveHandle(
                captures_[index].target);
            UIElement* target =
                visual != nullptr ? visual->AsUIElement() : nullptr;
            RemoveCaptureAt(index);
            if (!captureChanged_.Empty()) {
                captureChanged_.Invoke(
                    input.pointerId, target, false);
            }
        }
    }
    return result;
}

FocusState::FocusState(
    ElementTree& tree, EventRouter& events) noexcept
    : tree_(&tree), events_(&events),
      scopeFocus_(&Base::GetDefaultAllocator()) {}

UIElement* FocusState::FocusedNode() noexcept {
    Visual* visual = tree_->ResolveHandle(focused_);
    UIElement* node = visual != nullptr ? visual->AsUIElement() : nullptr;
    if (node == nullptr) focused_ = {};
    return node;
}

UIElement* FocusState::FindNavigationScope(UIElement* node) noexcept {
    Visual* current = node != nullptr
        ? (node->GetLogicalParent() != nullptr
            ? node->GetLogicalParent() : node->GetVisualParent())
        : nullptr;
    while (current != nullptr) {
        UIElement* element = current->AsUIElement();
        if (element != nullptr && element->GetIsFocusScope()) return element;
        current = current->GetLogicalParent() != nullptr
            ? current->GetLogicalParent() : current->GetVisualParent();
    }
    Visual* root = tree_->Root();
    return root != nullptr ? root->AsUIElement() : nullptr;
}

Base::Result<void> FocusState::RememberFocus(
    UIElement& node) noexcept {
    Visual* current = &node;
    Visual* root = tree_->Root();
    while (current != nullptr) {
        UIElement* element = current->AsUIElement();
        const bool isScope = current == root ||
            (element != nullptr && element->GetIsFocusScope());
        if (isScope) {
            Base::Result<VisualHandle> scope =
                tree_->GetHandle(*current);
            if (!scope) return scope.GetStatus();
            std::uint32_t recordIndex = UINT32_MAX;
            for (std::uint32_t index = 0U;
                index < scopeFocus_.Size(); ++index) {
                if (scopeFocus_[index].scope.index ==
                        scope.Value().index &&
                    scopeFocus_[index].scope.generation ==
                        scope.Value().generation) {
                    recordIndex = index;
                    break;
                }
            }
            if (recordIndex == UINT32_MAX) {
                Base::Result<void> appended = scopeFocus_.PushBack(
                    {scope.Value(), Aero::Internal::ElementPrivate::Handle(node)});
                if (!appended) return appended.GetStatus();
            } else {
                scopeFocus_[recordIndex].focused = Aero::Internal::ElementPrivate::Handle(node);
            }
        }
        if (current == root) break;
        current = current->GetLogicalParent() != nullptr
            ? current->GetLogicalParent() : current->GetVisualParent();
    }
    return {};
}

UIElement* FocusState::FocusedElement(UIElement& scope) noexcept {
    Base::Result<VisualHandle> handle = tree_->GetHandle(scope);
    if (!handle) return nullptr;
    for (std::uint32_t index = 0U;
        index < scopeFocus_.Size(); ++index) {
        ScopeFocus& record = scopeFocus_[index];
        if (record.scope.index != handle.Value().index ||
            record.scope.generation != handle.Value().generation) {
            continue;
        }
        Visual* visual = tree_->ResolveHandle(record.focused);
        UIElement* element =
            visual != nullptr ? visual->AsUIElement() : nullptr;
        if (element == nullptr || !element->GetIsLoaded()) {
            record.focused = {};
            return nullptr;
        }
        return element;
    }
    return nullptr;
}

Base::Result<void> FocusState::CollectCandidates(
    Visual& parent,
    Base::Vector<FocusCandidate>& candidates,
    std::uint32_t& order) noexcept {
    for (Visual* child : Aero::Internal::ElementPrivate::VisualChildren(parent)) {
        if (child == nullptr) continue;
        UIElement* element = child->AsUIElement();
        const std::uint32_t candidateOrder = order++;
        if (element != nullptr && element->GetIsLoaded() &&
            element->GetIsEnabled() &&
            element->GetFocusable() &&
            element->GetIsTabStop()) {
            Base::Result<void> appended = candidates.PushBack(
                {element,
                 element->GetValueOr(
                     KeyboardNavigation::TabIndexProperty,
                     element->GetTabIndex()),
                 candidateOrder});
            if (!appended) return appended.GetStatus();
            std::uint32_t index = candidates.Size() - 1U;
            while (index > 0U) {
                FocusCandidate& previous = candidates[index - 1U];
                FocusCandidate& current = candidates[index];
                if (previous.tabIndex < current.tabIndex ||
                    (previous.tabIndex == current.tabIndex &&
                        previous.order <= current.order)) {
                    break;
                }
                FocusCandidate temporary = previous;
                previous = current;
                current = temporary;
                --index;
            }
        }
        Base::Result<void> nested =
            CollectCandidates(*child, candidates, order);
        if (!nested) return nested.GetStatus();
    }
    return {};
}

Base::Result<bool> FocusState::SetFocus(UIElement* node) noexcept {
    if (node == nullptr) return ClearFocus();
    Base::Result<VisualHandle> next = tree_->GetHandle(*node);
    if (!next) return next.GetStatus();
    if (!node->GetIsLoaded() || !node->GetIsEnabled()) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Keyboard focus target must be loaded and enabled");
    }
    std::uint32_t ancestorCount = 1U;
    Visual* ancestor = node;
    while (ancestor != tree_->Root()) {
        ancestor = ancestor->GetLogicalParent() != nullptr
            ? ancestor->GetLogicalParent() : ancestor->GetVisualParent();
        if (ancestor == nullptr) break;
        ++ancestorCount;
    }
    Base::Result<void> reserved = scopeFocus_.Reserve(
        scopeFocus_.Size() + ancestorCount);
    if (!reserved) return reserved.GetStatus();
    auto setFocusWithin = [](UIElement& element, bool value)
        -> Base::Result<void> {
        Visual* current = &element;
        while (current != nullptr) {
            if (UIElement* ancestor = current->AsUIElement()) {
                Base::Result<void> updated =
                    ElementPrivate::SetKeyboardFocusWithin(*ancestor, value);
                if (!updated) return updated.GetStatus();
            }
            current = current->GetLogicalParent() != nullptr
                ? current->GetLogicalParent() : current->GetVisualParent();
        }
        return {};
    };
    UIElement* previous = FocusedNode();
    if (previous == node) return false;
    if (previous != nullptr) {
        Base::Result<void> state =
            ElementPrivate::SetKeyboardFocused(*previous, false);
        if (!state) return state.GetStatus();
        state = setFocusWithin(*previous, false);
        if (!state) {
            static_cast<void>(ElementPrivate::SetKeyboardFocused(*previous, true));
            return state.GetStatus();
        }
        KeyboardFocusChangedEventArgs args;
        args.SetOldFocus(previous);
        args.SetNewFocus(node);
        Base::Result<void> lost = events_->RaiseEvent(
            *previous, UIElement::LostKeyboardFocusEvent, &args);
        if (!lost) {
            static_cast<void>(
                ElementPrivate::SetKeyboardFocused(*previous, true));
            static_cast<void>(setFocusWithin(*previous, true));
            return lost.GetStatus();
        }
    }
    Base::Result<void> state = ElementPrivate::SetKeyboardFocused(*node, true);
    if (!state) {
        if (previous != nullptr) {
            static_cast<void>(
                ElementPrivate::SetKeyboardFocused(*previous, true));
            static_cast<void>(setFocusWithin(*previous, true));
        }
        return state.GetStatus();
    }
    state = setFocusWithin(*node, true);
    if (!state) {
        static_cast<void>(ElementPrivate::SetKeyboardFocused(*node, false));
        if (previous != nullptr) {
            static_cast<void>(ElementPrivate::SetKeyboardFocused(*previous, true));
            static_cast<void>(setFocusWithin(*previous, true));
        }
        return state.GetStatus();
    }
    KeyboardFocusChangedEventArgs args;
    args.SetOldFocus(previous);
    args.SetNewFocus(node);
    Base::Result<void> gained = events_->RaiseEvent(
        *node, UIElement::GotKeyboardFocusEvent, &args);
    if (!gained) {
        static_cast<void>(ElementPrivate::SetKeyboardFocused(*node, false));
        static_cast<void>(setFocusWithin(*node, false));
        if (previous != nullptr) {
            static_cast<void>(
                ElementPrivate::SetKeyboardFocused(*previous, true));
            static_cast<void>(setFocusWithin(*previous, true));
        }
        return gained.GetStatus();
    }
    focused_ = next.Value();
    Base::Result<void> remembered = RememberFocus(*node);
    if (!remembered) return remembered.GetStatus();
    return true;
}

Base::Result<bool> FocusState::ClearFocus() noexcept {
    UIElement* previous = FocusedNode();
    if (previous == nullptr) return false;
    Base::Result<void> access = previous->VerifyAccess();
    if (!access) return access.GetStatus();
    Base::Result<void> state =
        ElementPrivate::SetKeyboardFocused(*previous, false);
    if (!state) return state.GetStatus();
    auto setFocusWithin = [](UIElement& element, bool value)
        -> Base::Result<void> {
        Visual* current = &element;
        while (current != nullptr) {
            if (UIElement* ancestor = current->AsUIElement()) {
                Base::Result<void> updated =
                    ElementPrivate::SetKeyboardFocusWithin(*ancestor, value);
                if (!updated) return updated.GetStatus();
            }
            current = current->GetLogicalParent() != nullptr
                ? current->GetLogicalParent() : current->GetVisualParent();
        }
        return {};
    };
    state = setFocusWithin(*previous, false);
    if (!state) {
        static_cast<void>(ElementPrivate::SetKeyboardFocused(*previous, true));
        return state.GetStatus();
    }
    KeyboardFocusChangedEventArgs args;
    args.SetOldFocus(previous);
    Base::Result<void> lost = events_->RaiseEvent(
        *previous, UIElement::LostKeyboardFocusEvent, &args);
    if (!lost) {
        static_cast<void>(
            ElementPrivate::SetKeyboardFocused(*previous, true));
        static_cast<void>(setFocusWithin(*previous, true));
        return lost.GetStatus();
    }
    focused_ = {};
    return true;
}

Base::Result<bool> FocusState::MoveFocus(
    FocusNavigationDirection direction,
    bool wrap) noexcept {
    UIElement* current = FocusedNode();
    UIElement* scope = FindNavigationScope(current);
    if (scope == nullptr || !scope->GetIsLoaded()) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Focus navigation requires a loaded UIElement root");
    }
    Base::Vector<FocusCandidate> candidates(
        &Base::GetDefaultAllocator());
    std::uint32_t order = 0U;
    Base::Result<void> collected =
        CollectCandidates(*scope, candidates, order);
    if (!collected) return collected.GetStatus();
    if (candidates.Empty()) return false;

    std::uint32_t currentIndex = UINT32_MAX;
    for (std::uint32_t index = 0U;
        index < candidates.Size(); ++index) {
        if (candidates[index].element == current) {
            currentIndex = index;
            break;
        }
    }

    std::uint32_t nextIndex = 0U;
    if (direction == FocusNavigationDirection::Next) {
        if (currentIndex == UINT32_MAX) {
            nextIndex = 0U;
        } else if (currentIndex + 1U < candidates.Size()) {
            nextIndex = currentIndex + 1U;
        } else if (wrap) {
            nextIndex = 0U;
        } else {
            return false;
        }
    } else {
        if (currentIndex == UINT32_MAX) {
            nextIndex = candidates.Size() - 1U;
        } else if (currentIndex > 0U) {
            nextIndex = currentIndex - 1U;
        } else if (wrap) {
            nextIndex = candidates.Size() - 1U;
        } else {
            return false;
        }
    }
    return SetFocus(candidates[nextIndex].element);
}

KeyboardState::KeyboardState(FocusState& focus,
    EventRouter& events, ElementTree& tree) noexcept
    : KeyboardState(focus, events, tree, nullptr) {}

KeyboardState::KeyboardState(FocusState& focus,
    EventRouter& events, ElementTree& tree,
    CommandState* commands) noexcept
    : focus_(&focus), events_(&events), tree_(&tree),
      commands_(commands) {}

Base::Result<KeyboardDispatchResult> KeyboardState::Dispatch(
    const KeyboardInput& input) noexcept {
    Visual* root = tree_->Root();
    if (root == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Keyboard input requires an ElementTree root");
    }
    Base::Result<void> access = root->VerifyAccess();
    if (!access) return access.GetStatus();
    if (input.key == 0U) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Keyboard key identifier must be non-zero");
    }
    RoutedEventHandle previewEvent;
    RoutedEventHandle event;
    switch (input.action) {
    case KeyboardAction::Down:
        previewEvent = UIElement::PreviewKeyDownEvent;
        event = UIElement::KeyDownEvent;
        break;
    case KeyboardAction::Up:
        previewEvent = UIElement::PreviewKeyUpEvent;
        event = UIElement::KeyUpEvent;
        break;
    }
    KeyboardDispatchResult result;
    result.target = focus_->FocusedNode();
    if (result.target == nullptr) {
        if (input.action == KeyboardAction::Down &&
            input.key == KeyboardKeyTab) {
            Base::Result<bool> moved = focus_->MoveFocus(
                HasKeyboardModifier(input.modifiers,
                    KeyboardModifiers::Shift)
                    ? FocusNavigationDirection::Previous
                    : FocusNavigationDirection::Next);
            if (!moved) return moved.GetStatus();
            result.focusMoved = moved.Value();
            result.target = focus_->FocusedNode();
        }
        return result;
    }
    if (!result.target->GetIsEnabled()) {
        Base::Result<bool> cleared = focus_->ClearFocus();
        if (!cleared) return cleared.GetStatus();
        result.target = nullptr;
        return result;
    }
    if (!result.target->GetIsLoaded() || Aero::Internal::ElementPrivate::Tree(*result.target) != tree_) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Keyboard focus target is not loaded in the input tree");
    }
    KeyEventArgs args;
    args.SetAction(input.action);
    args.SetKey(input.key);
    args.SetModifiers(input.modifiers);
    args.SetIsRepeat(input.isRepeat);
    Base::Result<void> raised = events_->RaiseEvent(*result.target, previewEvent, &args);
    if (raised) raised = events_->RaiseEvent(*result.target, event, &args);
    if (!raised) return raised.GetStatus();
    result.routed = true;
    if (commands_ != nullptr &&
        input.action == KeyboardAction::Down &&
        !args.GetHandled()) {
        Base::Result<bool> command =
            commands_->ProcessInput(*result.target, input);
        if (!command) return command.GetStatus();
        result.commandExecuted = command.Value();
    }
    if (input.action == KeyboardAction::Down &&
        input.key == KeyboardKeyTab &&
        !args.GetHandled() && !result.commandExecuted) {
        Base::Result<bool> moved = focus_->MoveFocus(
            HasKeyboardModifier(input.modifiers,
                KeyboardModifiers::Shift)
                ? FocusNavigationDirection::Previous
                : FocusNavigationDirection::Next);
        if (!moved) return moved.GetStatus();
        result.focusMoved = moved.Value();
    }
    return result;
}

TextInputState::TextInputState(FocusState& focus,
    EventRouter& events, ElementTree& tree) noexcept
    : focus_(&focus), events_(&events), tree_(&tree) {}

Base::Result<TextInputDispatchResult> TextInputState::Dispatch(
    const TextInput& input) noexcept {
    Visual* root = tree_->Root();
    if (root == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Text input requires an ElementTree root");
    }
    Base::Result<void> access = root->VerifyAccess();
    if (!access) return access.GetStatus();
    if (input.text.Empty() || !Base::ValidateUtf8(input.text).valid) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Text input must be non-empty, valid UTF-8");
    }
    TextInputDispatchResult result;
    result.target = focus_->FocusedNode();
    if (result.target == nullptr) return result;
    if (!result.target->GetIsLoaded() || Aero::Internal::ElementPrivate::Tree(*result.target) != tree_) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Text input focus target is not loaded in the input tree");
    }
    TextCompositionEventArgs args;
    args.SetText(input.text);
    Base::Result<void> raised = events_->RaiseEvent(
        *result.target, UIElement::PreviewTextInputEvent, &args);
    if (raised) {
        raised = events_->RaiseEvent(*result.target, UIElement::TextInputEvent, &args);
    }
    if (!raised) return raised.GetStatus();
    result.routed = true;
    return result;
}

} // namespace Aero::Internal
