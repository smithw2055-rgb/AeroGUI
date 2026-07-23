#include <Aero/Core/Input.hpp>

#include <cmath>

namespace Aero::Core {
namespace {

bool Contains(Size size, Point point) noexcept {
    return point.x >= 0.0 && point.y >= 0.0 &&
        point.x < size.width && point.y < size.height;
}

bool IsVisualDescendantOrSelf(
    const TreeNode& root, const TreeNode& target) noexcept {
    const TreeNode* current = &target;
    while (current != &root) {
        current = current->VisualParent();
        if (current == nullptr) return false;
    }
    return true;
}

} // namespace

HitTestManager::HitTestManager(TypeRegistry& types, Base::IAllocator* allocator) noexcept
    : types_(&types),
      allocator_(allocator != nullptr ? allocator : &Base::GetDefaultAllocator()),
      typesByRuntimeType_(allocator_) {}

Base::Result<void> HitTestManager::TryRegisterType(
    const HitTestTypeRegistration& registration) noexcept {
    if (registration.type == InvalidTypeId || registration.cast == nullptr ||
        types_->FindType(registration.type) == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Hit-test type registration is invalid");
    }
    for (const HitTestTypeRegistration& current : typesByRuntimeType_) {
        if (current.type == registration.type) {
            return Base::Status::Failure(Base::ErrorCode::AlreadyExists,
                "Hit-test type is already registered");
        }
    }
    return typesByRuntimeType_.TryPushBack(registration);
}

const HitTestTypeRegistration* HitTestManager::FindRegistration(TypeId type) const noexcept {
    TypeId current = type;
    while (current != InvalidTypeId) {
        for (const HitTestTypeRegistration& registration : typesByRuntimeType_) {
            if (registration.type == current) return &registration;
        }
        const TypeInfo* info = types_->FindType(current);
        if (info == nullptr) break;
        current = info->BaseType();
    }
    return nullptr;
}

LayoutElement* HitTestManager::AsLayoutElement(TreeNode& node) const noexcept {
    const HitTestTypeRegistration* registration = FindRegistration(node.RuntimeType());
    if (registration == nullptr || registration->cast == nullptr) return nullptr;
    LayoutElement* element = registration->cast(node, registration->context);
    return element != nullptr && element->RuntimeType() == node.RuntimeType() ? element : nullptr;
}

Base::Result<HitTestResult> HitTestManager::HitTest(
    TreeNode& root, Point position) const noexcept {
    if (!IsFinite(position)) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Hit-test position must be finite");
    }
    LayoutElement* rootElement = AsLayoutElement(root);
    if (rootElement == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Hit-test root is not a registered LayoutElement");
    }
    return HitTestElement(*rootElement, position);
}

Base::Result<HitTestResult> HitTestManager::RootToLocal(
    TreeNode& root, TreeNode& target, Point position) const noexcept {
    if (!IsFinite(position)) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Pointer position must be finite");
    }
    if (AsLayoutElement(root) == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Hit-test root is not a registered LayoutElement");
    }
    LayoutElement* targetElement = AsLayoutElement(target);
    if (targetElement == nullptr || !targetElement->IsArrangeValid()) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Pointer capture target must be an arranged LayoutElement");
    }

    Point local = position;
    TreeNode* current = &target;
    while (current != &root) {
        LayoutElement* currentElement = AsLayoutElement(*current);
        if (currentElement == nullptr || !currentElement->IsArrangeValid()) {
            return Base::Status::Failure(Base::ErrorCode::InvalidState,
                "Pointer capture route contains an unarranged element");
        }
        TreeNode* parent = current->VisualParent();
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
    LayoutElement& element, Point position) const noexcept {
    if (!element.IsArrangeValid()) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Hit-test requires an arranged visual tree");
    }
    if (!element.IsHitTestVisible() ||
        !Contains(element.RenderSize(), position)) return HitTestResult{};

    const Base::Span<TreeNode* const> children = element.VisualChildren();
    for (std::uint32_t index = children.Size(); index > 0U; --index) {
        TreeNode* childNode = children[index - 1U];
        if (childNode == nullptr) continue;
        LayoutElement* child = AsLayoutElement(*childNode);
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
    RoutedEventRegistry& events, TreeNode& root) noexcept
    : hitTests_(&hitTests), events_(&events), root_(&root),
      captures_(&Base::GetDefaultAllocator()) {}

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

TreeNode* PointerInputManager::CapturedNode(std::uint32_t pointerId) noexcept {
    const std::uint32_t index = FindCapture(pointerId);
    if (index == UINT32_MAX) return nullptr;
    ObjectTree* tree = root_->OwningTree();
    TreeNode* target = tree != nullptr ? tree->ResolveHandle(captures_[index].target) : nullptr;
    if (target == nullptr || !target->IsLoaded() ||
        !IsVisualDescendantOrSelf(*root_, *target)) {
        RemoveCaptureAt(index);
        return nullptr;
    }
    return target;
}

Base::Result<void> PointerInputManager::CapturePointer(
    std::uint32_t pointerId, TreeNode& target) noexcept {
    Base::Result<void> access = root_->VerifyAccess();
    if (!access) return access.GetStatus();
    ObjectTree* tree = root_->OwningTree();
    if (tree == nullptr || target.OwningTree() != tree || !target.IsLoaded()) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Pointer capture target must be loaded in the input tree");
    }
    Base::Result<TreeNodeHandle> handle = tree->GetHandle(target);
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
    case PointerAction::Move: event = TreeNode::MouseMoveEvent; break;
    case PointerAction::Down: event = TreeNode::MouseDownEvent; break;
    case PointerAction::Up: event = TreeNode::MouseUpEvent; break;
    }
    TreeNode* captured = CapturedNode(input.pointerId);
    Base::Result<HitTestResult> hit = captured != nullptr
        ? hitTests_->RootToLocal(*root_, *captured, input.position)
        : hitTests_->HitTest(*root_, input.position);
    if (!hit) return hit.GetStatus();
    PointerDispatchResult result;
    result.hit = hit.Value();
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
    ObjectTree& tree, RoutedEventRegistry& events) noexcept
    : tree_(&tree), events_(&events) {}

TreeNode* FocusManager::FocusedNode() noexcept {
    TreeNode* node = tree_->ResolveHandle(focused_);
    if (node == nullptr) focused_ = {};
    return node;
}

Base::Result<bool> FocusManager::SetFocus(TreeNode* node) noexcept {
    if (node == nullptr) return ClearFocus();
    Base::Result<TreeNodeHandle> next = tree_->GetHandle(*node);
    if (!next) return next.GetStatus();
    if (!node->IsLoaded()) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Keyboard focus target must be loaded");
    }
    TreeNode* previous = FocusedNode();
    if (previous == node) return false;
    if (previous != nullptr) {
        KeyboardFocusChangedEventArgs args;
        args.oldFocus = previous;
        args.newFocus = node;
        Base::Result<void> lost = events_->RaiseEvent(
            *previous, TreeNode::LostKeyboardFocusEvent, &args);
        if (!lost) return lost.GetStatus();
    }
    KeyboardFocusChangedEventArgs args;
    args.oldFocus = previous;
    args.newFocus = node;
    Base::Result<void> gained = events_->RaiseEvent(
        *node, TreeNode::GotKeyboardFocusEvent, &args);
    if (!gained) return gained.GetStatus();
    focused_ = next.Value();
    return true;
}

Base::Result<bool> FocusManager::ClearFocus() noexcept {
    TreeNode* previous = FocusedNode();
    if (previous == nullptr) return false;
    Base::Result<void> access = previous->VerifyAccess();
    if (!access) return access.GetStatus();
    KeyboardFocusChangedEventArgs args;
    args.oldFocus = previous;
    Base::Result<void> lost = events_->RaiseEvent(
        *previous, TreeNode::LostKeyboardFocusEvent, &args);
    if (!lost) return lost.GetStatus();
    focused_ = {};
    return true;
}

KeyboardInputManager::KeyboardInputManager(FocusManager& focus,
    RoutedEventRegistry& events, ObjectTree& tree) noexcept
    : focus_(&focus), events_(&events), tree_(&tree) {}

Base::Result<KeyboardDispatchResult> KeyboardInputManager::Dispatch(
    const KeyboardInput& input) noexcept {
    TreeNode* root = tree_->Root();
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
    case KeyboardAction::Down: event = TreeNode::KeyDownEvent; break;
    case KeyboardAction::Up: event = TreeNode::KeyUpEvent; break;
    }
    KeyboardDispatchResult result;
    result.target = focus_->FocusedNode();
    if (result.target == nullptr) return result;
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
    return result;
}

TextInputManager::TextInputManager(FocusManager& focus,
    RoutedEventRegistry& events, ObjectTree& tree) noexcept
    : focus_(&focus), events_(&events), tree_(&tree) {}

Base::Result<TextInputDispatchResult> TextInputManager::Dispatch(
    const TextInput& input) noexcept {
    TreeNode* root = tree_->Root();
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
        *result.target, TreeNode::TextInputEvent, &args);
    if (!raised) return raised.GetStatus();
    result.routed = true;
    return result;
}

} // namespace Aero::Core
