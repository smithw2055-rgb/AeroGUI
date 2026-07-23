#include <Aero/Core/ObjectTree.hpp>
#include <Aero/Core/Layout.hpp>

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

Detail::RoutedHandlerStorage::RoutedHandlerStorage(
    const RoutedHandlerStorage& other) noexcept
    : operations_(other.operations_), argsType_(other.argsType_) {
    if (operations_ != nullptr) operations_->copy(storage_, other.storage_);
}

Detail::RoutedHandlerStorage::RoutedHandlerStorage(
    RoutedHandlerStorage&& other) noexcept
    : RoutedHandlerStorage(static_cast<const RoutedHandlerStorage&>(other)) {
    other.Reset();
}

Detail::RoutedHandlerStorage& Detail::RoutedHandlerStorage::operator=(
    const RoutedHandlerStorage& other) noexcept {
    if (this != &other) {
        Reset();
        operations_ = other.operations_;
        argsType_ = other.argsType_;
        if (operations_ != nullptr) operations_->copy(storage_, other.storage_);
    }
    return *this;
}

Detail::RoutedHandlerStorage& Detail::RoutedHandlerStorage::operator=(
    RoutedHandlerStorage&& other) noexcept {
    if (this != &other) {
        *this = static_cast<const RoutedHandlerStorage&>(other);
        other.Reset();
    }
    return *this;
}

Detail::RoutedHandlerStorage::~RoutedHandlerStorage() noexcept { Reset(); }

void Detail::RoutedHandlerStorage::Reset() noexcept {
    if (operations_ != nullptr) operations_->destroy(storage_);
    operations_ = nullptr;
    argsType_ = InvalidTypeId;
}

bool Detail::RoutedHandlerStorage::Equals(
    const RoutedHandlerStorage& other) const noexcept {
    return operations_ == other.operations_ && argsType_ == other.argsType_ &&
        (operations_ == nullptr || operations_->equals(storage_, other.storage_));
}

void Detail::RoutedHandlerStorage::Invoke(
    Base::Object* sender,
    const RoutedEventArgs& args) const noexcept {
    AERO_ASSERT(operations_ != nullptr && args.eventArgsType == argsType_);
    operations_->invoke(storage_, sender, args);
}

Visual::Visual(TypeId runtimeType) noexcept
    : DependencyObject(runtimeType),
      logicalChildren_(),
      visualChildren_() {}

Visual::~Visual() {
    AERO_ASSERT(tree_ == nullptr);
    AERO_ASSERT(logicalParent_ == nullptr);
    AERO_ASSERT(visualParent_ == nullptr);
    AERO_ASSERT(logicalChildren_.Empty());
    AERO_ASSERT(visualChildren_.Empty());
}

ObjectTree::ObjectTree(
    Dispatcher& dispatcher,
    EffectiveValueEngine& values) noexcept
    : dispatcher_(&dispatcher),
      values_(&values),
      lifecycleQueue_(),
      handles_() {}

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

Base::Result<VisualHandle> ObjectTree::GetHandle(
    const Visual& node) const noexcept {
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

Visual* ObjectTree::ResolveHandle(VisualHandle handle) const noexcept {
    if (!handle.IsValid() || handle.index >= handles_.Size()) return nullptr;
    const HandleEntry& entry = handles_[handle.index];
    return entry.generation == handle.generation ? entry.node : nullptr;
}

Base::Result<void> ObjectTree::RegisterHandleSubtree(Visual& node) noexcept {
    if (!node.handle_.IsValid()) {
        HandleEntry entry;
        entry.node = &node;
        Base::Result<void> appended = handles_.TryPushBack(entry);
        if (!appended) return appended.GetStatus();
        node.handle_ = {handles_.Size() - 1U, entry.generation};
    }
    for (Visual* child : node.logicalChildren_) {
        if (child != nullptr) {
            Base::Result<void> registered = RegisterHandleSubtree(*child);
            if (!registered) return registered;
        }
    }
    return {};
}

void ObjectTree::InvalidateHandleSubtree(Visual& node) noexcept {
    for (Visual* child : node.logicalChildren_) {
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
    const Visual& first,
    const Visual* second) const noexcept {
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
    const Visual& possibleAncestor,
    const Visual& node) const noexcept {
    const Visual* current = node.logicalParent_;
    while (current != nullptr) {
        if (current == &possibleAncestor) {
            return true;
        }
        current = current->logicalParent_;
    }
    return false;
}

bool ObjectTree::IsVisualAncestor(
    const Visual& possibleAncestor,
    const Visual& node) const noexcept {
    const Visual* current = node.visualParent_;
    while (current != nullptr) {
        if (current == &possibleAncestor) {
            return true;
        }
        current = current->visualParent_;
    }
    return false;
}

Base::Result<void> ObjectTree::QueueLifecycleSubtree(
    Visual& node,
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
    for (Visual* child : node.logicalChildren_) {
        Base::Result<void> childResult = QueueLifecycleSubtree(*child, loaded);
        if (!childResult) {
            return childResult;
        }
    }
    return {};
}

Base::Result<void> ObjectTree::SetLoadedSubtree(
    Visual& node,
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
    for (Visual* child : node.logicalChildren_) {
        Base::Result<void> childResult = SetLoadedSubtree(*child, loaded);
        if (!childResult) {
            return childResult;
        }
    }
    return {};
}

Base::Result<void> ObjectTree::SetRoot(Visual* root) noexcept {
    if (root == root_) {
        return {};
    }
    if (root == nullptr && root_ == nullptr) {
        return {};
    }

    Visual& verificationNode = root != nullptr ? *root : *root_;
    Base::Result<void> verified = VerifyMutation(verificationNode, root_);
    if (!verified) {
        return verified;
    }
    if (root != nullptr && (root->logicalParent_ != nullptr ||
        (root->tree_ != nullptr && root->tree_ != this))) {
        return InvalidState("ObjectTree root must be detached");
    }

    mutating_ = true;
    Visual* oldRoot = root_;
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
    Visual& parent,
    Visual& child) noexcept {
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
    if (!appended) {
        (void)values_->SetInheritanceParent(child, nullptr);
        InvalidateHandleSubtree(child);
        mutating_ = false;
        return appended.GetStatus();
    }
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
    Base::Vector<Visual*>& children,
    Visual& child) noexcept {
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
    Visual& parent,
    Visual& child) noexcept {
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
    Visual& parent,
    Visual& child) noexcept {
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
    Visual& parent,
    Visual& child) noexcept {
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

Base::Result<void> ObjectTree::DetachNode(Visual& node) noexcept {
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
    Base::Vector<LifecycleRecord> snapshot;
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
            const ObjectTreeLifecycleEvent event{
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

RoutedEventRegistry::RoutedEventRegistry(TypeRegistry& types) noexcept
    : types_(&types),
      events_(),
      classHandlers_() {}

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

    EventRecord record;
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

Base::Result<void> RoutedEventRegistry::BuildRoute(
    Visual& source,
    RoutingStrategy strategy,
    Base::Vector<Visual*>& route) noexcept {
    if (strategy == RoutingStrategy::Direct) {
        return route.TryPushBack(&source);
    }
    Visual* current = &source;
    while (current != nullptr) {
        Base::Result<void> appended = route.TryPushBack(current);
        if (!appended) {
            return appended;
        }
        current = current->LogicalParent() != nullptr
            ? current->LogicalParent()
            : current->VisualParent();
    }
    if (strategy == RoutingStrategy::Tunnel) {
        for (std::uint32_t left = 0U, right = route.Size() - 1U;
             left < right; ++left, --right) {
            Visual* temporary = route[left];
            route[left] = route[right];
            route[right] = temporary;
        }
    }
    return {};
}

void RoutedEventRegistry::InvokeNode(
    Visual& node,
    RoutedEventArgs& args) noexcept {
    UIElement* element = node.AsUIElement();
    if (element == nullptr) return;
    for (const ClassHandlerRecord& record : classHandlers_) {
        if (record.event == args.routedEvent &&
            types_->IsDerivedFrom(element->RuntimeType(), record.classType) &&
            (!args.handled || record.handledEventsToo)) {
            record.handler.Invoke(element, args);
        }
    }

    const std::uint32_t count = element->handlers_.Size();
    for (std::uint32_t index = 0U; index < count; ++index) {
        const UIElement::HandlerRecord record = element->handlers_[index];
        if (record.event == args.routedEvent &&
            (!args.handled || record.handledEventsToo)) {
            record.handler.Invoke(element, args);
        }
    }
}

Base::Result<void> RoutedEventRegistry::RaiseEvent(
    UIElement& source,
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

    Base::Vector<Visual*> route;
    Base::Result<void> built = BuildRoute(source, record->strategy, route);
    if (!built) {
        return built;
    }

    RoutedEventArgs localArgs;
    RoutedEventArgs& args = suppliedArgs != nullptr ? *suppliedArgs : localArgs;
    if (args.eventArgsType != record->argsType) {
        return InvalidArgument("Routed event arguments do not match the registered type");
    }
    args.routedEvent = event;
    args.source = &source;
    if (args.originalSource == nullptr) {
        args.originalSource = &source;
    }

    raising_ = true;
    for (Visual* node : route) {
        InvokeNode(*node, args);
    }
    raising_ = false;
    return {};
}

void RoutedEventRegistry::CleanupClassHandlers() noexcept {
    classHandlers_.Clear();
}

} // namespace Aero::Core
