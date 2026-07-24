#include <Aero/Presentation/Input.hpp>

#include <Aero/Presentation/Commands.hpp>

#include <cmath>

namespace Aero::Presentation {

using namespace Aero::Core;
namespace {

bool Contains(Size size, Point point) noexcept {
    return point.x >= 0.0 && point.y >= 0.0 &&
        point.x < size.width && point.y < size.height;
}

bool IsVisualDescendantOrSelf(
    const Visual& root, const Visual& target) noexcept {
    const Visual* current = &target;
    while (current != &root) {
        current = current->VisualParent();
        if (current == nullptr) return false;
    }
    return true;
}

} // namespace

Base::Result<HitTestResult> HitTestManager::HitTest(
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
    return HitTestElement(*rootElement, position);
}

Base::Result<HitTestResult> HitTestManager::RootToLocal(
    Visual& root, Visual& target, Point position) const noexcept {
    if (!IsFinite(position)) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Pointer position must be finite");
    }
    if (AsUIElement(root) == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Hit-test root is not a registered UIElement");
    }
    UIElement* targetElement = AsUIElement(target);
    if (targetElement == nullptr || !targetElement->IsArrangeValid()) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Pointer capture target must be an arranged UIElement");
    }

    Point local = position;
    Visual* current = &target;
    while (current != &root) {
        UIElement* currentElement = AsUIElement(*current);
        if (currentElement == nullptr || !currentElement->IsArrangeValid()) {
            return Base::Status::Failure(Base::ErrorCode::InvalidState,
                "Pointer capture route contains an unarranged element");
        }
        Visual* parent = current->VisualParent();
        if (parent == nullptr) {
            return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
                "Pointer capture target is not below the input root");
        }
        const Rect slot = currentElement->LayoutSlot();
        local.x -= slot.x;
        local.y -= slot.y;
        current = parent;
    }
    return HitTestResult{targetElement, local};
}

Base::Result<HitTestResult> HitTestManager::HitTestElement(
    UIElement& element, Point position) const noexcept {
    if (!element.IsArrangeValid()) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Hit-test requires an arranged visual tree");
    }
    if (!element.IsHitTestVisible() ||
        !Contains(element.RenderSize(), position)) return HitTestResult{};

    const Base::Span<Visual* const> children = element.VisualChildren();
    for (std::uint32_t index = children.Size(); index > 0U; --index) {
        Visual* childNode = children[index - 1U];
        if (childNode == nullptr) continue;
        UIElement* child = AsUIElement(*childNode);
        if (child == nullptr) continue;
        const Rect slot = child->LayoutSlot();
        Base::Result<HitTestResult> nested = HitTestElement(*child,
            {position.x - slot.x, position.y - slot.y});
        if (!nested) return nested.GetStatus();
        if (nested.Value().HasTarget()) return nested;
    }
    return HitTestResult{&element, position};
}

PointerInputManager::PointerInputManager(HitTestManager& hitTests,
    RoutedEventManager& events, Visual& root) noexcept
    : hitTests_(&hitTests), events_(&events), root_(&root),
      captures_(&Base::GetDefaultAllocator()),
      states_(&Base::GetDefaultAllocator()) {}

std::uint32_t PointerInputManager::FindCapture(
    std::uint32_t pointerId) const noexcept {
    for (std::uint32_t index = 0U; index < captures_.Size(); ++index) {
        if (captures_[index].pointerId == pointerId) return index;
    }
    return UINT32_MAX;
}

void PointerInputManager::RemoveCaptureAt(std::uint32_t index) noexcept {
    AERO_ASSERT(index < captures_.Size());
    for (std::uint32_t next = index + 1U; next < captures_.Size(); ++next) {
        captures_[next - 1U] = captures_[next];
    }
    captures_.PopBack();
}

std::uint32_t PointerInputManager::FindState(
    std::uint32_t pointerId) const noexcept {
    for (std::uint32_t index = 0U; index < states_.Size(); ++index) {
        if (states_[index].pointerId == pointerId) return index;
    }
    return UINT32_MAX;
}

bool PointerInputManager::HasHover(
    VisualHandle target, std::uint32_t ignoredIndex) const noexcept {
    if (!target.IsValid()) return false;
    for (std::uint32_t index = 0U; index < states_.Size(); ++index) {
        if (index == ignoredIndex) continue;
        if (states_[index].hover.index == target.index &&
            states_[index].hover.generation == target.generation) {
            return true;
        }
    }
    return false;
}

bool PointerInputManager::HasPressed(
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

Base::Result<void> PointerInputManager::UpdateHover(
    std::uint32_t pointerId, UIElement* target) noexcept {
    ObjectTree* tree = root_->OwningTree();
    if (tree == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Pointer state requires an ObjectTree");
    }
    VisualHandle next;
    if (target != nullptr && target->IsEnabled()) {
        Base::Result<VisualHandle> handle = tree->GetHandle(*target);
        if (!handle) return handle.GetStatus();
        next = handle.Value();
    }
    std::uint32_t index = FindState(pointerId);
    if (index == UINT32_MAX) {
        Base::Result<void> appended =
            states_.TryPushBack({pointerId, {}, {}});
        if (!appended) return appended.GetStatus();
        index = states_.Size() - 1U;
    }
    const VisualHandle previous = states_[index].hover;
    if (previous.index == next.index &&
        previous.generation == next.generation) return {};

    UIElement* nextElement = nullptr;
    if (next.IsValid() && !HasHover(next, index)) {
        Visual* visual = tree->ResolveHandle(next);
        nextElement = visual != nullptr ? visual->AsUIElement() : nullptr;
        if (nextElement != nullptr) {
            Base::Result<void> set = nextElement->SetMouseOverState(true);
            if (!set) return set.GetStatus();
        }
    }
    if (previous.IsValid() && !HasHover(previous, index)) {
        Visual* visual = tree->ResolveHandle(previous);
        UIElement* previousElement =
            visual != nullptr ? visual->AsUIElement() : nullptr;
        if (previousElement != nullptr) {
            Base::Result<void> cleared =
                previousElement->SetMouseOverState(false);
            if (!cleared) {
                if (nextElement != nullptr) {
                    static_cast<void>(
                        nextElement->SetMouseOverState(false));
                }
                return cleared.GetStatus();
            }
        }
    }
    states_[index].hover = next;
    return {};
}

Base::Result<void> PointerInputManager::UpdatePressed(
    std::uint32_t pointerId, UIElement* target) noexcept {
    ObjectTree* tree = root_->OwningTree();
    if (tree == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Pointer state requires an ObjectTree");
    }
    VisualHandle next;
    if (target != nullptr && target->IsEnabled()) {
        Base::Result<VisualHandle> handle = tree->GetHandle(*target);
        if (!handle) return handle.GetStatus();
        next = handle.Value();
    }
    std::uint32_t index = FindState(pointerId);
    if (index == UINT32_MAX) {
        Base::Result<void> appended =
            states_.TryPushBack({pointerId, {}, {}});
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
            Base::Result<void> set = nextElement->SetPressedState(true);
            if (!set) return set.GetStatus();
        }
    }
    if (previous.IsValid() && !HasPressed(previous, index)) {
        Visual* visual = tree->ResolveHandle(previous);
        UIElement* previousElement =
            visual != nullptr ? visual->AsUIElement() : nullptr;
        if (previousElement != nullptr) {
            Base::Result<void> cleared =
                previousElement->SetPressedState(false);
            if (!cleared) {
                if (nextElement != nullptr) {
                    static_cast<void>(
                        nextElement->SetPressedState(false));
                }
                return cleared.GetStatus();
            }
        }
    }
    states_[index].pressed = next;
    return {};
}

UIElement* PointerInputManager::CapturedNode(std::uint32_t pointerId) noexcept {
    const std::uint32_t index = FindCapture(pointerId);
    if (index == UINT32_MAX) return nullptr;
    ObjectTree* tree = root_->OwningTree();
    Visual* target = tree != nullptr
        ? tree->ResolveHandle(captures_[index].target) : nullptr;
    UIElement* element = target != nullptr ? target->AsUIElement() : nullptr;
    if (element == nullptr || !element->IsLoaded() ||
        !IsVisualDescendantOrSelf(*root_, *element)) {
        RemoveCaptureAt(index);
        static_cast<void>(UpdatePressed(pointerId, nullptr));
        return nullptr;
    }
    return element;
}

Base::Result<void> PointerInputManager::CapturePointer(
    std::uint32_t pointerId, UIElement& target) noexcept {
    Base::Result<void> access = root_->VerifyAccess();
    if (!access) return access.GetStatus();
    ObjectTree* tree = root_->OwningTree();
    if (tree == nullptr || target.OwningTree() != tree || !target.IsLoaded()) {
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
        captures_[index].target = handle.Value();
        return {};
    }
    return captures_.TryPushBack({pointerId, handle.Value()});
}

Base::Result<bool> PointerInputManager::ReleasePointer(
    std::uint32_t pointerId) noexcept {
    Base::Result<void> access = root_->VerifyAccess();
    if (!access) return access.GetStatus();
    const std::uint32_t index = FindCapture(pointerId);
    if (index == UINT32_MAX) return false;
    Base::Result<void> state = UpdatePressed(pointerId, nullptr);
    if (!state) return state.GetStatus();
    RemoveCaptureAt(index);
    return true;
}

Base::Result<PointerDispatchResult> PointerInputManager::Dispatch(
    const PointerInput& input) noexcept {
    Base::Result<void> access = root_->VerifyAccess();
    if (!access) return access.GetStatus();
    if (!IsFinite(input.position)) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Pointer position must be finite");
    }
    RoutedEventHandle event;
    switch (input.action) {
    case PointerAction::Move: event = UIElement::MouseMoveEvent; break;
    case PointerAction::Down: event = UIElement::MouseDownEvent; break;
    case PointerAction::Up: event = UIElement::MouseUpEvent; break;
    }
    UIElement* captured = CapturedNode(input.pointerId);
    Base::Result<HitTestResult> hit = captured != nullptr
        ? hitTests_->RootToLocal(*root_, *captured, input.position)
        : hitTests_->HitTest(*root_, input.position);
    if (!hit) return hit.GetStatus();
    PointerDispatchResult result;
    result.hit = hit.Value();
    UIElement* stateTarget = result.hit.HasTarget() &&
        result.hit.target->IsEnabled() ? result.hit.target : nullptr;
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
        args.pointerId = input.pointerId;
        args.position = result.hit.position;
        raised = events_->RaiseEvent(*result.hit.target, event, &args);
    } else {
        MouseButtonEventArgs args;
        args.pointerId = input.pointerId;
        args.position = result.hit.position;
        args.changedButton = input.changedButton;
        args.buttonState = input.action == PointerAction::Down
            ? MouseButtonState::Pressed : MouseButtonState::Released;
        raised = events_->RaiseEvent(*result.hit.target, event, &args);
    }
    if (!raised) return raised.GetStatus();
    result.routed = true;
    if (input.action == PointerAction::Up) {
        const std::uint32_t index = FindCapture(input.pointerId);
        if (index != UINT32_MAX) RemoveCaptureAt(index);
    }
    return result;
}

FocusManager::FocusManager(
    ObjectTree& tree, RoutedEventManager& events) noexcept
    : tree_(&tree), events_(&events),
      scopeFocus_(&Base::GetDefaultAllocator()) {}

UIElement* FocusManager::FocusedNode() noexcept {
    Visual* visual = tree_->ResolveHandle(focused_);
    UIElement* node = visual != nullptr ? visual->AsUIElement() : nullptr;
    if (node == nullptr) focused_ = {};
    return node;
}

UIElement* FocusManager::FindNavigationScope(UIElement* node) noexcept {
    Visual* current = node != nullptr
        ? (node->LogicalParent() != nullptr
            ? node->LogicalParent() : node->VisualParent())
        : nullptr;
    while (current != nullptr) {
        UIElement* element = current->AsUIElement();
        if (element != nullptr && element->IsFocusScope()) return element;
        current = current->LogicalParent() != nullptr
            ? current->LogicalParent() : current->VisualParent();
    }
    Visual* root = tree_->Root();
    return root != nullptr ? root->AsUIElement() : nullptr;
}

Base::Result<void> FocusManager::RememberFocus(
    UIElement& node) noexcept {
    Visual* current = &node;
    Visual* root = tree_->Root();
    while (current != nullptr) {
        UIElement* element = current->AsUIElement();
        const bool isScope = current == root ||
            (element != nullptr && element->IsFocusScope());
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
                Base::Result<void> appended = scopeFocus_.TryPushBack(
                    {scope.Value(), node.Handle()});
                if (!appended) return appended.GetStatus();
            } else {
                scopeFocus_[recordIndex].focused = node.Handle();
            }
        }
        if (current == root) break;
        current = current->LogicalParent() != nullptr
            ? current->LogicalParent() : current->VisualParent();
    }
    return {};
}

UIElement* FocusManager::FocusedElement(UIElement& scope) noexcept {
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
        if (element == nullptr || !element->IsLoaded()) {
            record.focused = {};
            return nullptr;
        }
        return element;
    }
    return nullptr;
}

Base::Result<void> FocusManager::CollectCandidates(
    Visual& parent,
    Base::Vector<FocusCandidate>& candidates,
    std::uint32_t& order) noexcept {
    for (Visual* child : parent.VisualChildren()) {
        if (child == nullptr) continue;
        UIElement* element = child->AsUIElement();
        const std::uint32_t candidateOrder = order++;
        if (element != nullptr && element->IsLoaded() &&
            element->IsEnabled() && element->IsTabStop()) {
            Base::Result<void> appended = candidates.TryPushBack(
                {element, element->TabIndex(), candidateOrder});
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

Base::Result<bool> FocusManager::SetFocus(UIElement* node) noexcept {
    if (node == nullptr) return ClearFocus();
    Base::Result<VisualHandle> next = tree_->GetHandle(*node);
    if (!next) return next.GetStatus();
    if (!node->IsLoaded() || !node->IsEnabled()) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Keyboard focus target must be loaded and enabled");
    }
    std::uint32_t ancestorCount = 1U;
    Visual* ancestor = node;
    while (ancestor != tree_->Root()) {
        ancestor = ancestor->LogicalParent() != nullptr
            ? ancestor->LogicalParent() : ancestor->VisualParent();
        if (ancestor == nullptr) break;
        ++ancestorCount;
    }
    Base::Result<void> reserved = scopeFocus_.TryReserve(
        scopeFocus_.Size() + ancestorCount);
    if (!reserved) return reserved.GetStatus();
    UIElement* previous = FocusedNode();
    if (previous == node) return false;
    if (previous != nullptr) {
        Base::Result<void> state =
            previous->SetKeyboardFocusedState(false);
        if (!state) return state.GetStatus();
        KeyboardFocusChangedEventArgs args;
        args.oldFocus = previous;
        args.newFocus = node;
        Base::Result<void> lost = events_->RaiseEvent(
            *previous, UIElement::LostKeyboardFocusEvent, &args);
        if (!lost) {
            static_cast<void>(
                previous->SetKeyboardFocusedState(true));
            return lost.GetStatus();
        }
    }
    Base::Result<void> state = node->SetKeyboardFocusedState(true);
    if (!state) {
        if (previous != nullptr) {
            static_cast<void>(
                previous->SetKeyboardFocusedState(true));
        }
        return state.GetStatus();
    }
    KeyboardFocusChangedEventArgs args;
    args.oldFocus = previous;
    args.newFocus = node;
    Base::Result<void> gained = events_->RaiseEvent(
        *node, UIElement::GotKeyboardFocusEvent, &args);
    if (!gained) {
        static_cast<void>(node->SetKeyboardFocusedState(false));
        if (previous != nullptr) {
            static_cast<void>(
                previous->SetKeyboardFocusedState(true));
        }
        return gained.GetStatus();
    }
    focused_ = next.Value();
    Base::Result<void> remembered = RememberFocus(*node);
    if (!remembered) return remembered.GetStatus();
    return true;
}

Base::Result<bool> FocusManager::ClearFocus() noexcept {
    UIElement* previous = FocusedNode();
    if (previous == nullptr) return false;
    Base::Result<void> access = previous->VerifyAccess();
    if (!access) return access.GetStatus();
    Base::Result<void> state =
        previous->SetKeyboardFocusedState(false);
    if (!state) return state.GetStatus();
    KeyboardFocusChangedEventArgs args;
    args.oldFocus = previous;
    Base::Result<void> lost = events_->RaiseEvent(
        *previous, UIElement::LostKeyboardFocusEvent, &args);
    if (!lost) {
        static_cast<void>(
            previous->SetKeyboardFocusedState(true));
        return lost.GetStatus();
    }
    focused_ = {};
    return true;
}

Base::Result<bool> FocusManager::MoveFocus(
    FocusNavigationDirection direction,
    bool wrap) noexcept {
    UIElement* current = FocusedNode();
    UIElement* scope = FindNavigationScope(current);
    if (scope == nullptr || !scope->IsLoaded()) {
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

KeyboardInputManager::KeyboardInputManager(FocusManager& focus,
    RoutedEventManager& events, ObjectTree& tree) noexcept
    : KeyboardInputManager(focus, events, tree, nullptr) {}

KeyboardInputManager::KeyboardInputManager(FocusManager& focus,
    RoutedEventManager& events, ObjectTree& tree,
    CommandManager* commands) noexcept
    : focus_(&focus), events_(&events), tree_(&tree),
      commands_(commands) {}

Base::Result<KeyboardDispatchResult> KeyboardInputManager::Dispatch(
    const KeyboardInput& input) noexcept {
    Visual* root = tree_->Root();
    if (root == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Keyboard input requires an ObjectTree root");
    }
    Base::Result<void> access = root->VerifyAccess();
    if (!access) return access.GetStatus();
    if (input.key == 0U) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Keyboard key identifier must be non-zero");
    }
    RoutedEventHandle event;
    switch (input.action) {
    case KeyboardAction::Down: event = UIElement::KeyDownEvent; break;
    case KeyboardAction::Up: event = UIElement::KeyUpEvent; break;
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
    if (!result.target->IsEnabled()) {
        Base::Result<bool> cleared = focus_->ClearFocus();
        if (!cleared) return cleared.GetStatus();
        result.target = nullptr;
        return result;
    }
    if (!result.target->IsLoaded() || result.target->OwningTree() != tree_) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Keyboard focus target is not loaded in the input tree");
    }
    KeyEventArgs args;
    args.action = input.action;
    args.key = input.key;
    args.modifiers = input.modifiers;
    args.isRepeat = input.isRepeat;
    Base::Result<void> raised = events_->RaiseEvent(*result.target, event, &args);
    if (!raised) return raised.GetStatus();
    result.routed = true;
    if (commands_ != nullptr &&
        input.action == KeyboardAction::Down &&
        !args.handled) {
        Base::Result<bool> command =
            commands_->ProcessInput(*result.target, input);
        if (!command) return command.GetStatus();
        result.commandExecuted = command.Value();
    }
    if (input.action == KeyboardAction::Down &&
        input.key == KeyboardKeyTab &&
        !args.handled && !result.commandExecuted) {
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

TextInputManager::TextInputManager(FocusManager& focus,
    RoutedEventManager& events, ObjectTree& tree) noexcept
    : focus_(&focus), events_(&events), tree_(&tree) {}

Base::Result<TextInputDispatchResult> TextInputManager::Dispatch(
    const TextInput& input) noexcept {
    Visual* root = tree_->Root();
    if (root == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Text input requires an ObjectTree root");
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
    if (!result.target->IsLoaded() || result.target->OwningTree() != tree_) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Text input focus target is not loaded in the input tree");
    }
    TextCompositionEventArgs args;
    args.text = input.text;
    Base::Result<void> raised = events_->RaiseEvent(
        *result.target, UIElement::TextInputEvent, &args);
    if (!raised) return raised.GetStatus();
    result.routed = true;
    return result;
}

} // namespace Aero::Presentation
