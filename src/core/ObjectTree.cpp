#include <Aero/Core/ObjectTree.hpp>

#include <Aero/Base/Assert.hpp>

#include <utility>

namespace Aero::Core {
namespace {

Base::Status InvalidState(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

Base::Status InvalidArgument(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidArgument, message);
}

Base::Status NotFound(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::NotFound, message);
}

} // namespace

TreeNode::TreeNode(
    Dispatcher& dispatcher,
    DependencyPropertyRegistry& registry,
    TypeId runtimeType,
    Base::IAllocator* allocator) noexcept
    : DependencyObject(dispatcher, registry, runtimeType, allocator),
      logicalChildren_(allocator),
      visualChildren_(allocator),
      handlers_(allocator) {}

TreeNode::~TreeNode() {
    AERO_ASSERT(tree_ == nullptr);
    AERO_ASSERT(logicalParent_ == nullptr);
    AERO_ASSERT(visualParent_ == nullptr);
    AERO_ASSERT(logicalChildren_.Empty());
    AERO_ASSERT(visualChildren_.Empty());
    CleanupHandlers();
}

Base::Result<RoutedEventHandlerToken> TreeNode::AddHandler(
    RoutedEventHandle event,
    RoutedEventHandler handler,
    void* context,
    RoutedEventCleanup cleanup,
    bool handledEventsToo) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) {
        return access.GetStatus();
    }
    if (!event.IsValid() || handler == nullptr) {
        return InvalidArgument("Routed event handler requires a valid event and callback");
    }
    if (nextHandlerToken_ == 0U || nextHandlerSequence_ == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Routed event handler token space exhausted");
    }

    HandlerRecord record;
    record.token.value = nextHandlerToken_;
    record.event = event;
    record.handler = handler;
    record.cleanup = cleanup;
    record.context = context;
    record.sequence = nextHandlerSequence_;
    record.handledEventsToo = handledEventsToo;
    Base::Result<void> appended = handlers_.TryPushBack(record);
    if (!appended) {
        return appended.GetStatus();
    }
    ++nextHandlerToken_;
    ++nextHandlerSequence_;
    return record.token;
}

Base::Result<bool> TreeNode::RemoveHandler(
    RoutedEventHandlerToken token) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) {
        return access.GetStatus();
    }
    if (!token.IsValid()) {
        return InvalidArgument("Routed event handler token is invalid");
    }
    for (std::uint32_t index = 0U; index < handlers_.Size(); ++index) {
        if (handlers_[index].token.value == token.value) {
            HandlerRecord removed = handlers_[index];
            for (std::uint32_t current = index + 1U;
                 current < handlers_.Size(); ++current) {
                handlers_[current - 1U] = handlers_[current];
            }
            handlers_.PopBack();
            if (removed.cleanup != nullptr) {
                removed.cleanup(removed.context);
            }
            return true;
        }
    }
    return false;
}

void TreeNode::CleanupHandlers() noexcept {
    for (HandlerRecord& record : handlers_) {
        if (record.cleanup != nullptr) {
            record.cleanup(record.context);
        }
    }
    handlers_.Clear();
}

ObjectTree::ObjectTree(
    Dispatcher& dispatcher,
    EffectiveValueEngine& values,
    Base::IAllocator* allocator) noexcept
    : dispatcher_(&dispatcher),
      values_(&values),
      allocator_(allocator != nullptr ? allocator : &dispatcher.Allocator()),
      lifecycleQueue_(allocator_),
      handles_(allocator_) {}

ObjectTree::~ObjectTree() noexcept {
    if (lifecycleHook_.IsValid() && dispatcher_->CheckAccess()) {
        (void)dispatcher_->RemoveFrameHook(lifecycleHook_);
    }
    if (root_ != nullptr && dispatcher_->CheckAccess()) {
        (void)SetRoot(nullptr);
    }
}

Base::Result<void> ObjectTree::Initialize() noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) {
        return access;
    }
    if (lifecycleHook_.IsValid()) {
        return {};
    }
    Base::Result<DispatcherFrameHookHandle> hook = dispatcher_->RegisterFrameHook(
        DispatcherFramePhase::Lifecycle,
        &ObjectTree::LifecycleHook,
        this);
    if (!hook) {
        return hook.GetStatus();
    }
    lifecycleHook_ = hook.Value();
    return {};
}

Base::Result<TreeNodeHandle> ObjectTree::GetHandle(
    const TreeNode& node) const noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    if (node.tree_ != this || !node.handle_.IsValid() ||
        node.handle_.index >= handles_.Size()) {
        return NotFound("Tree node does not have an active ObjectTree handle");
    }
    const HandleEntry& entry = handles_[node.handle_.index];
    if (entry.node != &node || entry.generation != node.handle_.generation) {
        return NotFound("Tree node handle is stale");
    }
    return node.handle_;
}

TreeNode* ObjectTree::ResolveHandle(TreeNodeHandle handle) const noexcept {
    if (!handle.IsValid() || handle.index >= handles_.Size()) return nullptr;
    const HandleEntry& entry = handles_[handle.index];
    return entry.generation == handle.generation ? entry.node : nullptr;
}

Base::Result<void> ObjectTree::RegisterHandleSubtree(TreeNode& node) noexcept {
    if (!node.handle_.IsValid()) {
        HandleEntry entry;
        entry.node = &node;
        Base::Result<void> appended = handles_.TryPushBack(entry);
        if (!appended) return appended.GetStatus();
        node.handle_ = {handles_.Size() - 1U, entry.generation};
    }
    for (TreeNode* child : node.logicalChildren_) {
        if (child != nullptr) {
            Base::Result<void> registered = RegisterHandleSubtree(*child);
            if (!registered) return registered;
        }
    }
    return {};
}

void ObjectTree::InvalidateHandleSubtree(TreeNode& node) noexcept {
    for (TreeNode* child : node.logicalChildren_) {
        if (child != nullptr) InvalidateHandleSubtree(*child);
    }
    if (node.handle_.IsValid() && node.handle_.index < handles_.Size()) {
        HandleEntry& entry = handles_[node.handle_.index];
        if (entry.node == &node) {
            entry.node = nullptr;
            ++entry.generation;
            if (entry.generation == 0U) ++entry.generation;
        }
    }
    node.handle_ = {};
}

Base::Result<void> ObjectTree::VerifyMutation(
    const TreeNode& first,
    const TreeNode* second) const noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) {
        return access;
    }
    if (!lifecycleHook_.IsValid()) {
        return InvalidState("ObjectTree must be initialized before mutation");
    }
    if (mutating_) {
        return InvalidState("Nested object-tree mutation is not allowed");
    }
    if (&first.GetDispatcher() != dispatcher_ ||
        (second != nullptr && &second->GetDispatcher() != dispatcher_)) {
        return Base::Status::Failure(
            Base::ErrorCode::WrongThread,
            "Tree nodes must belong to the ObjectTree Dispatcher");
    }
    return {};
}

bool ObjectTree::IsLogicalAncestor(
    const TreeNode& possibleAncestor,
    const TreeNode& node) const noexcept {
    const TreeNode* current = node.logicalParent_;
    while (current != nullptr) {
        if (current == &possibleAncestor) {
            return true;
        }
        current = current->logicalParent_;
    }
    return false;
}

bool ObjectTree::IsVisualAncestor(
    const TreeNode& possibleAncestor,
    const TreeNode& node) const noexcept {
    const TreeNode* current = node.visualParent_;
    while (current != nullptr) {
        if (current == &possibleAncestor) {
            return true;
        }
        current = current->visualParent_;
    }
    return false;
}

Base::Result<void> ObjectTree::QueueLifecycleSubtree(
    TreeNode& node,
    bool loaded) noexcept {
    LifecycleRecord record;
    record.node = &node;
    record.loaded = loaded;
    record.sequence = nextLifecycleSequence_++;
    record.treeVersion = version_;
    Base::Result<void> appended = lifecycleQueue_.TryPushBack(record);
    if (!appended) {
        return appended;
    }
    for (TreeNode* child : node.logicalChildren_) {
        Base::Result<void> childResult = QueueLifecycleSubtree(*child, loaded);
        if (!childResult) {
            return childResult;
        }
    }
    return {};
}

Base::Result<void> ObjectTree::SetLoadedSubtree(
    TreeNode& node,
    bool loaded) noexcept {
    if (node.loaded_ == loaded) {
        return {};
    }

    LifecycleRecord record;
    record.node = &node;
    record.loaded = loaded;
    record.sequence = nextLifecycleSequence_++;
    record.treeVersion = version_;
    Base::Result<void> appended = lifecycleQueue_.TryPushBack(record);
    if (!appended) {
        return appended;
    }

    node.loaded_ = loaded;
    for (TreeNode* child : node.logicalChildren_) {
        Base::Result<void> childResult = SetLoadedSubtree(*child, loaded);
        if (!childResult) {
            return childResult;
        }
    }
    return {};
}

Base::Result<void> ObjectTree::SetRoot(TreeNode* root) noexcept {
    if (root == root_) {
        return {};
    }
    if (root == nullptr && root_ == nullptr) {
        return {};
    }

    TreeNode& verificationNode = root != nullptr ? *root : *root_;
    Base::Result<void> verified = VerifyMutation(verificationNode, root_);
    if (!verified) {
        return verified;
    }
    if (root != nullptr && (root->logicalParent_ != nullptr ||
        (root->tree_ != nullptr && root->tree_ != this))) {
        return InvalidState("ObjectTree root must be detached");
    }

    mutating_ = true;
    TreeNode* oldRoot = root_;
    if (oldRoot != nullptr) {
        Base::Result<void> unloaded = SetLoadedSubtree(*oldRoot, false);
        if (!unloaded) {
            mutating_ = false;
            return unloaded;
        }
        InvalidateHandleSubtree(*oldRoot);
        oldRoot->tree_ = nullptr;
        (void)values_->SetInheritanceParent(*oldRoot, nullptr);
    }
    root_ = root;
    ++version_;
    if (root_ != nullptr) {
        Base::Result<void> registered = RegisterHandleSubtree(*root_);
        if (!registered) {
            root_ = oldRoot;
            if (oldRoot != nullptr) oldRoot->tree_ = this;
            mutating_ = false;
            return registered;
        }
        root_->tree_ = this;
        Base::Result<void> loaded = SetLoadedSubtree(*root_, true);
        if (!loaded) {
            root_->tree_ = nullptr;
            root_ = oldRoot;
            if (oldRoot != nullptr) {
                oldRoot->tree_ = this;
                (void)SetLoadedSubtree(*oldRoot, true);
            }
            mutating_ = false;
            return loaded;
        }
    }
    mutating_ = false;
    return {};
}

Base::Result<void> ObjectTree::AttachLogical(
    TreeNode& parent,
    TreeNode& child) noexcept {
    Base::Result<void> verified = VerifyMutation(parent, &child);
    if (!verified) {
        return verified;
    }
    if (&parent == &child || IsLogicalAncestor(child, parent)) {
        return Base::Status::Failure(
            Base::ErrorCode::CycleDetected,
            "Logical tree attachment would create a cycle");
    }
    if (child.logicalParent_ != nullptr ||
        (child.tree_ != nullptr && child.tree_ != this) ||
        (parent.tree_ != nullptr && parent.tree_ != this)) {
        return InvalidState("Logical child or parent belongs to another tree");
    }
    if (parent.tree_ == nullptr && &parent != root_) {
        return InvalidState("Logical parent must already belong to this tree");
    }

    Base::Result<void> reserve = parent.logicalChildren_.TryReserve(
        parent.logicalChildren_.Size() + 1U);
    if (!reserve) {
        return reserve;
    }
    Base::Result<void> registered = RegisterHandleSubtree(child);
    if (!registered) return registered;
    mutating_ = true;
    Base::Result<void> inherited = values_->SetInheritanceParent(child, &parent);
    if (!inherited) {
        mutating_ = false;
        return inherited;
    }
    Base::Result<void> appended = parent.logicalChildren_.TryPushBack(&child);
    AERO_ASSERT(appended);
    child.logicalParent_ = &parent;
    child.tree_ = this;
    ++version_;
    if (parent.loaded_) {
        Base::Result<void> loaded = SetLoadedSubtree(child, true);
        if (!loaded) {
            child.logicalParent_ = nullptr;
            child.tree_ = nullptr;
            InvalidateHandleSubtree(child);
            parent.logicalChildren_.PopBack();
            (void)values_->SetInheritanceParent(child, nullptr);
            mutating_ = false;
            return loaded;
        }
    }
    mutating_ = false;
    return {};
}

void ObjectTree::RemoveChild(
    Base::Vector<TreeNode*>& children,
    TreeNode& child) noexcept {
    for (std::uint32_t index = 0U; index < children.Size(); ++index) {
        if (children[index] == &child) {
            for (std::uint32_t current = index + 1U;
                 current < children.Size(); ++current) {
                children[current - 1U] = children[current];
            }
            children.PopBack();
            return;
        }
    }
}

Base::Result<void> ObjectTree::DetachLogical(
    TreeNode& parent,
    TreeNode& child) noexcept {
    Base::Result<void> verified = VerifyMutation(parent, &child);
    if (!verified) {
        return verified;
    }
    if (child.logicalParent_ != &parent || child.tree_ != this) {
        return NotFound("Logical parent-child relationship was not found");
    }

    mutating_ = true;
    if (child.loaded_) {
        Base::Result<void> unloaded = SetLoadedSubtree(child, false);
        if (!unloaded) {
            mutating_ = false;
            return unloaded;
        }
    }
    RemoveChild(parent.logicalChildren_, child);
    child.logicalParent_ = nullptr;
    child.tree_ = nullptr;
    InvalidateHandleSubtree(child);
    (void)values_->SetInheritanceParent(child, nullptr);
    ++version_;
    mutating_ = false;
    return {};
}

Base::Result<void> ObjectTree::AttachVisual(
    TreeNode& parent,
    TreeNode& child) noexcept {
    Base::Result<void> verified = VerifyMutation(parent, &child);
    if (!verified) {
        return verified;
    }
    if (&parent == &child || IsVisualAncestor(child, parent)) {
        return Base::Status::Failure(
            Base::ErrorCode::CycleDetected,
            "Visual tree attachment would create a cycle");
    }
    if (child.visualParent_ != nullptr || parent.tree_ != this ||
        child.tree_ != this) {
        return InvalidState("Visual nodes must be logical members of this tree");
    }
    Base::Result<void> appended = parent.visualChildren_.TryPushBack(&child);
    if (!appended) {
        return appended;
    }
    child.visualParent_ = &parent;
    ++version_;
    return {};
}

Base::Result<void> ObjectTree::DetachVisual(
    TreeNode& parent,
    TreeNode& child) noexcept {
    Base::Result<void> verified = VerifyMutation(parent, &child);
    if (!verified) {
        return verified;
    }
    if (child.visualParent_ != &parent) {
        return NotFound("Visual parent-child relationship was not found");
    }
    RemoveChild(parent.visualChildren_, child);
    child.visualParent_ = nullptr;
    ++version_;
    return {};
}

Base::Result<void> ObjectTree::DetachNode(TreeNode& node) noexcept {
    Base::Result<void> verified = VerifyMutation(node);
    if (!verified) {
        return verified;
    }
    while (!node.visualChildren_.Empty()) {
        Base::Result<void> detached = DetachVisual(
            node, *node.visualChildren_[node.visualChildren_.Size() - 1U]);
        if (!detached) {
            return detached;
        }
    }
    if (node.visualParent_ != nullptr) {
        Base::Result<void> detached = DetachVisual(*node.visualParent_, node);
        if (!detached) {
            return detached;
        }
    }
    while (!node.logicalChildren_.Empty()) {
        Base::Result<void> detached = DetachLogical(
            node, *node.logicalChildren_[node.logicalChildren_.Size() - 1U]);
        if (!detached) {
            return detached;
        }
    }
    if (node.logicalParent_ != nullptr) {
        return DetachLogical(*node.logicalParent_, node);
    }
    if (root_ == &node) {
        return SetRoot(nullptr);
    }
    return values_->DetachObject(node);
}

Base::Result<std::uint32_t> ObjectTree::FlushLifecycle() noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) {
        return access.GetStatus();
    }
    Base::Vector<LifecycleRecord> snapshot(allocator_);
    Base::Result<void> assigned = snapshot.TryAssign(
        Base::Span<const LifecycleRecord>(
            lifecycleQueue_.Data(), lifecycleQueue_.Size()));
    if (!assigned) {
        return assigned.GetStatus();
    }
    lifecycleQueue_.Clear();
    std::uint32_t count = 0U;
    for (const LifecycleRecord& record : snapshot) {
        if (record.node == nullptr) {
            continue;
        }
        if (lifecycleHandler_ != nullptr) {
            const TreeLifecycleEvent event{
                record.node,
                record.loaded,
                record.treeVersion};
            lifecycleHandler_(event, lifecycleContext_);
        }
        ++count;
    }
    return count;
}

void ObjectTree::LifecycleHook(void* context) noexcept {
    ObjectTree* tree = static_cast<ObjectTree*>(context);
    (void)tree->FlushLifecycle();
}

RoutedEventRegistry::RoutedEventRegistry(
    TypeRegistry& types,
    Base::IAllocator* allocator) noexcept
    : types_(&types),
      allocator_(allocator != nullptr ? allocator : &types.Allocator()),
      events_(allocator_),
      classHandlers_(allocator_) {}

RoutedEventRegistry::~RoutedEventRegistry() noexcept {
    CleanupClassHandlers();
}

Base::Result<RoutedEventHandle> RoutedEventRegistry::TryRegister(
    const RoutedEventRegistration& registration) noexcept {
    if (frozen_ || types_->IsFrozen()) {
        return InvalidState("Routed events must be registered before TypeRegistry freeze");
    }
    if (registration.name.Empty() || registration.ownerType == InvalidTypeId ||
        registration.eventArgsType == InvalidTypeId ||
        types_->FindType(registration.ownerType) == nullptr ||
        types_->FindType(registration.eventArgsType) == nullptr) {
        return InvalidArgument("Routed event registration is incomplete");
    }
    Base::Result<MemberId> member = types_->TryRegisterEvent(
        registration.ownerType,
        {registration.name, registration.eventArgsType, EventFlags::Routed});
    if (!member) {
        return member.GetStatus();
    }

    EventRecord record(allocator_);
    record.handle.value = member.Value();
    record.ownerType = registration.ownerType;
    record.argsType = registration.eventArgsType;
    record.strategy = registration.strategy;
    Base::Result<void> nameResult = record.name.TryAssign(registration.name);
    if (!nameResult) {
        return nameResult.GetStatus();
    }
    Base::Result<void> appended = events_.TryPushBack(std::move(record));
    if (!appended) {
        return appended.GetStatus();
    }
    return events_[events_.Size() - 1U].handle;
}

Base::Result<void> RoutedEventRegistry::Freeze() noexcept {
    if (frozen_) {
        return {};
    }
    if (!types_->IsFrozen()) {
        return InvalidState("TypeRegistry must be frozen before routed events");
    }
    frozen_ = true;
    return {};
}

const RoutedEventRegistry::EventRecord* RoutedEventRegistry::Find(
    RoutedEventHandle event) const noexcept {
    for (const EventRecord& record : events_) {
        if (record.handle == event) {
            return &record;
        }
    }
    return nullptr;
}

Base::Result<void> RoutedEventRegistry::RegisterClassHandler(
    RoutedEventHandle event,
    TypeId classType,
    RoutedEventHandler handler,
    void* context,
    RoutedEventCleanup cleanup,
    bool handledEventsToo) noexcept {
    if (!frozen_) {
        return InvalidState("RoutedEventRegistry must be frozen before handlers");
    }
    if (raising_) {
        return InvalidState("Cannot mutate class handlers during routed event dispatch");
    }
    if (Find(event) == nullptr || types_->FindType(classType) == nullptr ||
        handler == nullptr) {
        return InvalidArgument("Class handler registration is invalid");
    }
    ClassHandlerRecord record;
    record.event = event;
    record.classType = classType;
    record.handler = handler;
    record.context = context;
    record.cleanup = cleanup;
    record.handledEventsToo = handledEventsToo;
    record.sequence = nextClassSequence_++;
    return classHandlers_.TryPushBack(record);
}

Base::Result<void> RoutedEventRegistry::BuildRoute(
    TreeNode& source,
    RoutingStrategy strategy,
    Base::Vector<TreeNode*>& route) noexcept {
    if (strategy == RoutingStrategy::Direct) {
        return route.TryPushBack(&source);
    }
    TreeNode* current = &source;
    while (current != nullptr) {
        Base::Result<void> appended = route.TryPushBack(current);
        if (!appended) {
            return appended;
        }
        current = current->logicalParent_ != nullptr
            ? current->logicalParent_
            : current->visualParent_;
    }
    if (strategy == RoutingStrategy::Tunnel) {
        for (std::uint32_t left = 0U, right = route.Size() - 1U;
             left < right; ++left, --right) {
            TreeNode* temporary = route[left];
            route[left] = route[right];
            route[right] = temporary;
        }
    }
    return {};
}

void RoutedEventRegistry::InvokeNode(
    TreeNode& node,
    RoutedEventArgs& args) noexcept {
    for (const ClassHandlerRecord& record : classHandlers_) {
        if (record.event == args.event &&
            types_->IsDerivedFrom(node.RuntimeType(), record.classType) &&
            (!args.handled || record.handledEventsToo)) {
            record.handler(node, args, record.context);
        }
    }

    const std::uint32_t count = node.handlers_.Size();
    for (std::uint32_t index = 0U; index < count; ++index) {
        const TreeNode::HandlerRecord record = node.handlers_[index];
        if (record.event == args.event &&
            (!args.handled || record.handledEventsToo)) {
            record.handler(node, args, record.context);
        }
    }
}

Base::Result<void> RoutedEventRegistry::RaiseEvent(
    TreeNode& source,
    RoutedEventHandle event,
    RoutedEventArgs* suppliedArgs) noexcept {
    Base::Result<void> access = source.VerifyAccess();
    if (!access) {
        return access;
    }
    if (!frozen_) {
        return InvalidState("RoutedEventRegistry must be frozen before dispatch");
    }
    if (raising_) {
        return InvalidState("Nested routed event dispatch is deferred to a later slice");
    }
    const EventRecord* record = Find(event);
    if (record == nullptr) {
        return NotFound("Routed event was not found");
    }

    Base::Vector<TreeNode*> route(allocator_);
    Base::Result<void> built = BuildRoute(source, record->strategy, route);
    if (!built) {
        return built;
    }

    RoutedEventArgs localArgs;
    RoutedEventArgs& args = suppliedArgs != nullptr ? *suppliedArgs : localArgs;
    args.event = event;
    args.source = &source;
    if (args.originalSource == nullptr) {
        args.originalSource = &source;
    }

    raising_ = true;
    for (TreeNode* node : route) {
        InvokeNode(*node, args);
    }
    raising_ = false;
    return {};
}

void RoutedEventRegistry::CleanupClassHandlers() noexcept {
    for (ClassHandlerRecord& record : classHandlers_) {
        if (record.cleanup != nullptr) {
            record.cleanup(record.context);
        }
    }
    classHandlers_.Clear();
}

} // namespace Aero::Core
