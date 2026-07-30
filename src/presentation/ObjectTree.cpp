#include <Aero/Presentation/ObjectTree.hpp>
#include <Aero/Core/Metadata/MetadataBehaviorRegistrationStore.hpp>
#include <Aero/Presentation/Layout.hpp>
#include <Aero/Presentation/Rendering.hpp>

#include "../core/metadata/RoutedEventCatalog.hpp"

#include <Aero/Base/Assert.hpp>

#include <utility>
#include "RuntimeManagers.hpp"

namespace Aero::Presentation {

using namespace Aero::Core;
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

RoutedEventCatalog& EventCatalog(void* state) noexcept {
    AERO_ASSERT(state != nullptr);
    return *static_cast<RoutedEventCatalog*>(state);
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
    if (lifetime_) lifetime_->Invalidate();
}

Base::Result<Base::Ref<Detail::VisualLifetime>>
Visual::AcquireLifetime() noexcept {
    if (!lifetime_) {
        Base::Result<Base::Ref<Detail::VisualLifetime>> created =
            Base::MakeRef<Detail::VisualLifetime>(*this);
        if (!created) return created.GetStatus();
        lifetime_ = std::move(created).Value();
    }
    return lifetime_;
}

Base::Result<Detail::VisualLease> Detail::VisualLease::Acquire(
    Visual& node) noexcept {
    VisualLease lease;
    lease.strong = Base::Ref<Visual>::TryFromBorrowed(node);
    if (lease.strong) return lease;

    Base::Result<Base::Ref<VisualLifetime>> lifetime =
        node.AcquireLifetime();
    if (!lifetime) return lifetime.GetStatus();
    lease.lifetime = std::move(lifetime).Value();
    return lease;
}

ObjectTree::ObjectTree(
    Dispatcher& dispatcher,
    EffectiveValueEngine& values) noexcept
    : dispatcher_(&dispatcher),
      values_(&values),
      lifecycleQueue_(),
      handles_(),
      dataContextChangedHandler_(
          this, &ObjectTree::OnDataContextChanged) {}

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

Base::Result<void> ObjectTree::CollectLogicalSubtree(
    Visual& node,
    Base::Vector<Visual*>& nodes) noexcept {
    Base::Result<void> appended = nodes.TryPushBack(&node);
    if (!appended) return appended.GetStatus();
    for (Visual* child : node.logicalChildren_) {
        if (child == nullptr) continue;
        Base::Result<void> collected =
            CollectLogicalSubtree(*child, nodes);
        if (!collected) return collected.GetStatus();
    }
    return {};
}

Base::Result<void> ObjectTree::RegisterHandleSubtree(Visual& node) noexcept {
    Base::Vector<Visual*> nodes;
    Base::Result<void> collected = CollectLogicalSubtree(node, nodes);
    if (!collected) return collected.GetStatus();

    std::uint32_t required = 0U;
    for (Visual* current : nodes) {
        if (current->handle_.IsValid()) {
            if (current->handle_.index >= handles_.Size()) {
                return InvalidState("ObjectTree node has an invalid pre-existing handle");
            }
            const HandleEntry& entry = handles_[current->handle_.index];
            if (entry.node != current ||
                entry.generation != current->handle_.generation) {
                return InvalidState("ObjectTree node has a stale pre-existing handle");
            }
        } else {
            ++required;
        }
    }

    Base::Result<void> reserved =
        handles_.TryReserve(handles_.Size() + required);
    if (!reserved) return reserved.GetStatus();

    Base::Vector<Visual*> added;
    reserved = added.TryReserve(required);
    if (!reserved) return reserved.GetStatus();

    for (Visual* current : nodes) {
        if (current->handle_.IsValid()) continue;

        HandleEntry entry;
        entry.node = current;
        Base::Result<void> appended = handles_.TryPushBack(entry);
        AERO_ASSERT(appended);
        (void)appended;
        current->handle_ = {handles_.Size() - 1U, entry.generation};

        Base::Result<void> tracked = TrackInheritedValues(*current);
        if (!tracked) {
            current->handle_ = {};
            handles_.PopBack();
            while (!added.Empty()) {
                Visual* rollback = added.Back();
                UntrackInheritedValues(*rollback);
                rollback->handle_ = {};
                handles_.PopBack();
                added.PopBack();
            }
            return tracked.GetStatus();
        }
        Base::Result<void> remembered = added.TryPushBack(current);
        AERO_ASSERT(remembered);
        (void)remembered;
    }
    return {};
}

void ObjectTree::InvalidateHandleSubtree(Visual& node) noexcept {
    for (Visual* child : node.logicalChildren_) {
        if (child != nullptr) InvalidateHandleSubtree(*child);
    }
    if (node.handle_.IsValid() && node.handle_.index < handles_.Size()) {
        UntrackInheritedValues(node);
        HandleEntry& entry = handles_[node.handle_.index];
        if (entry.node == &node) {
            entry.node = nullptr;
            ++entry.generation;
            if (entry.generation == 0U) ++entry.generation;
        }
    }
    node.handle_ = {};
}

Base::Result<void> ObjectTree::TrackInheritedValues(
    Visual& node) noexcept {
    FrameworkElement* element = node.AsFrameworkElement();
    if (element == nullptr ||
        element->PropertyRegistry().Find(
            FrameworkElement::DataContextProperty) == nullptr) {
        return {};
    }
    Base::Result<void> subscribed =
        element->TryAddValueChangedHandler(
            FrameworkElement::DataContextProperty,
            dataContextChangedHandler_);
    if (!subscribed) return subscribed.GetStatus();
    Base::Result<void> invalidated = values_->Invalidate(
        *element, FrameworkElement::DataContextProperty);
    if (!invalidated) {
        (void)element->RemoveValueChangedHandler(
            FrameworkElement::DataContextProperty,
            dataContextChangedHandler_);
        return invalidated.GetStatus();
    }
    return {};
}

void ObjectTree::UntrackInheritedValues(Visual& node) noexcept {
    FrameworkElement* element = node.AsFrameworkElement();
    if (element == nullptr ||
        element->PropertyRegistry().Find(
            FrameworkElement::DataContextProperty) == nullptr) {
        return;
    }
    (void)element->RemoveValueChangedHandler(
        FrameworkElement::DataContextProperty,
        dataContextChangedHandler_);
}

void ObjectTree::OnDataContextChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs& args) noexcept {
    if (args.property == FrameworkElement::DataContextProperty) {
        (void)values_->Invalidate(object, args.property);
    }
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

Base::Result<void> ObjectTree::StageLifecycleSubtree(
    Visual& node,
    bool loaded,
    Base::Vector<LifecycleRecord>& staged) noexcept {
    if (node.loaded_ != loaded) {
        Base::Result<Detail::VisualLease> lease =
            Detail::VisualLease::Acquire(node);
        if (!lease) return lease.GetStatus();

        LifecycleRecord record;
        record.node = std::move(lease).Value();
        record.loaded = loaded;
        Base::Result<void> appended =
            staged.TryPushBack(std::move(record));
        if (!appended) return appended.GetStatus();
    }
    for (Visual* child : node.logicalChildren_) {
        if (child == nullptr) continue;
        Base::Result<void> childResult =
            StageLifecycleSubtree(*child, loaded, staged);
        if (!childResult) return childResult.GetStatus();
    }
    return {};
}

void ObjectTree::PublishLifecycle(
    Base::Vector<LifecycleRecord>& staged) noexcept {
    for (LifecycleRecord& record : staged) {
        record.sequence = nextLifecycleSequence_++;
        record.treeVersion = version_;
        Base::Result<void> appended =
            lifecycleQueue_.TryPushBack(std::move(record));
        AERO_ASSERT(appended);
        (void)appended;
    }
    staged.Clear();
}

void ObjectTree::ApplyLoadedSubtree(Visual& node, bool loaded) noexcept {
    node.loaded_ = loaded;
    for (Visual* child : node.logicalChildren_) {
        if (child != nullptr) ApplyLoadedSubtree(*child, loaded);
    }
}

void ObjectTree::SetTreeSubtree(
    Visual& node, ObjectTree* tree) noexcept {
    node.tree_ = tree;
    for (Visual* child : node.logicalChildren_) {
        if (child != nullptr) SetTreeSubtree(*child, tree);
    }
}

Base::Result<void> ObjectTree::SetRoot(Visual* root) noexcept {
    if (root == root_) return {};
    if (root == nullptr && root_ == nullptr) return {};

    Visual& verificationNode = root != nullptr ? *root : *root_;
    Base::Result<void> verified = VerifyMutation(verificationNode, root_);
    if (!verified) return verified.GetStatus();

    if (root != nullptr &&
        (root->logicalParent_ != nullptr ||
         root->visualParent_ != nullptr ||
         root->tree_ != nullptr)) {
        return InvalidState("ObjectTree root must be fully detached");
    }

    Base::Vector<LifecycleRecord> staged;
    if (root_ != nullptr) {
        Base::Result<void> prepared =
            StageLifecycleSubtree(*root_, false, staged);
        if (!prepared) return prepared.GetStatus();
    }
    if (root != nullptr) {
        Base::Result<void> prepared =
            StageLifecycleSubtree(*root, true, staged);
        if (!prepared) return prepared.GetStatus();
    }
    Base::Result<void> queueReserved = lifecycleQueue_.TryReserve(
        lifecycleQueue_.Size() + staged.Size());
    if (!queueReserved) return queueReserved.GetStatus();

    if (root != nullptr) {
        Base::Result<void> registered = RegisterHandleSubtree(*root);
        if (!registered) return registered.GetStatus();
        Base::Result<void> inherited =
            values_->SetInheritanceParent(*root, nullptr);
        if (!inherited) {
            InvalidateHandleSubtree(*root);
            return inherited.GetStatus();
        }
    }

    mutating_ = true;
    Visual* oldRoot = root_;
    ++version_;
    if (oldRoot != nullptr) {
        ApplyLoadedSubtree(*oldRoot, false);
        InvalidateHandleSubtree(*oldRoot);
        SetTreeSubtree(*oldRoot, nullptr);
    }

    root_ = root;
    if (root_ != nullptr) {
        SetTreeSubtree(*root_, this);
        ApplyLoadedSubtree(*root_, true);
    }
    PublishLifecycle(staged);
    mutating_ = false;
    return {};
}

Base::Result<void> ObjectTree::AttachLogical(
    Visual& parent,
    Visual& child) noexcept {
    Base::Result<void> verified = VerifyMutation(parent, &child);
    if (!verified) return verified.GetStatus();
    if (&parent == &child || IsLogicalAncestor(child, parent)) {
        return Base::Status::Failure(
            Base::ErrorCode::CycleDetected,
            "Logical tree attachment would create a cycle");
    }
    if (child.logicalParent_ != nullptr || child.tree_ != nullptr ||
        parent.tree_ != this) {
        return InvalidState(
            "Logical child must be detached and parent must belong to this tree");
    }

    Base::Result<void> childReserved = parent.logicalChildren_.TryReserve(
        parent.logicalChildren_.Size() + 1U);
    if (!childReserved) return childReserved.GetStatus();

    Base::Vector<LifecycleRecord> staged;
    if (parent.loaded_) {
        Base::Result<void> prepared =
            StageLifecycleSubtree(child, true, staged);
        if (!prepared) return prepared.GetStatus();
    }
    Base::Result<void> queueReserved = lifecycleQueue_.TryReserve(
        lifecycleQueue_.Size() + staged.Size());
    if (!queueReserved) return queueReserved.GetStatus();

    Base::Result<void> registered = RegisterHandleSubtree(child);
    if (!registered) return registered.GetStatus();
    Base::Result<void> inherited =
        values_->SetInheritanceParent(child, &parent);
    if (!inherited) {
        InvalidateHandleSubtree(child);
        return inherited.GetStatus();
    }

    mutating_ = true;
    Base::Result<void> appended =
        parent.logicalChildren_.TryPushBack(&child);
    AERO_ASSERT(appended);
    (void)appended;
    child.logicalParent_ = &parent;
    SetTreeSubtree(child, this);
    ++version_;
    if (parent.loaded_) ApplyLoadedSubtree(child, true);
    PublishLifecycle(staged);
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
    if (!verified) return verified.GetStatus();
    if (child.logicalParent_ != &parent || child.tree_ != this) {
        return NotFound("Logical parent-child relationship was not found");
    }

    Base::Vector<LifecycleRecord> staged;
    if (child.loaded_) {
        Base::Result<void> prepared =
            StageLifecycleSubtree(child, false, staged);
        if (!prepared) return prepared.GetStatus();
    }
    Base::Result<void> queueReserved = lifecycleQueue_.TryReserve(
        lifecycleQueue_.Size() + staged.Size());
    if (!queueReserved) return queueReserved.GetStatus();

    Base::Result<void> inherited =
        values_->SetInheritanceParent(child, nullptr);
    if (!inherited) return inherited.GetStatus();

    mutating_ = true;
    if (child.loaded_) ApplyLoadedSubtree(child, false);
    RemoveChild(parent.logicalChildren_, child);
    child.logicalParent_ = nullptr;
    SetTreeSubtree(child, nullptr);
    InvalidateHandleSubtree(child);
    ++version_;
    PublishLifecycle(staged);
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
    if (!access) return access.GetStatus();

    Base::Vector<LifecycleRecord> snapshot;
    Base::Result<void> assigned = snapshot.TryAssign(
        Base::Span<const LifecycleRecord>(
            lifecycleQueue_.Data(), lifecycleQueue_.Size()));
    if (!assigned) return assigned.GetStatus();
    lifecycleQueue_.Clear();

    std::uint32_t count = 0U;
    for (const LifecycleRecord& record : snapshot) {
        Visual* node = record.node.Resolve();
        if (node == nullptr) continue;
        if (lifecycleHandler_ != nullptr) {
            const ObjectTreeLifecycleEvent event{
                node, record.loaded, record.treeVersion};
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

} // namespace Aero::Presentation

namespace Aero::Detail {

using namespace Aero::Core;
using namespace Aero::Presentation;

RoutedEventManager::RoutedEventManager(
    void* eventState) noexcept
    : eventState_(eventState),
      classHandlers_() {}

RoutedEventManager::~RoutedEventManager() noexcept {
    CleanupClassHandlers();
}

Base::Result<void> RoutedEventManager::ValidateClassHandler(
    RoutedEventHandle event,
    TypeId classType,
    TypeId eventArgsType) const noexcept {
    RoutedEventCatalog& catalog =
        EventCatalog(eventState_);
    if (!catalog.IsFrozen()) {
        return InvalidState(
            "RoutedEventCatalog must be frozen before handlers");
    }
    const RoutedEventCatalog::Definition* definition =
        catalog.Find(event);
    if (definition == nullptr ||
        catalog.Types().FindType(classType) == nullptr ||
        definition->eventArgsType != eventArgsType) {
        return InvalidArgument(
            "Class handler registration is invalid");
    }
    return {};
}

Base::Result<void> RoutedEventManager::BuildRoute(
    Visual& source,
    RoutingStrategy strategy,
    Base::Vector<Presentation::Detail::VisualLease>& route) noexcept {
    if (strategy == RoutingStrategy::Direct) {
        Base::Result<Presentation::Detail::VisualLease> lease =
            Presentation::Detail::VisualLease::Acquire(source);
        if (!lease) return lease.GetStatus();
        return route.TryPushBack(std::move(lease).Value());
    }

    Visual* current = &source;
    while (current != nullptr) {
        Base::Result<Presentation::Detail::VisualLease> lease =
            Presentation::Detail::VisualLease::Acquire(*current);
        if (!lease) return lease.GetStatus();
        Base::Result<void> appended =
            route.TryPushBack(std::move(lease).Value());
        if (!appended) return appended.GetStatus();
        current = current->VisualParent() != nullptr
            ? current->VisualParent()
            : current->LogicalParent();
    }
    if (strategy == RoutingStrategy::Tunnel) {
        for (std::uint32_t left = 0U, right = route.Size() - 1U;
             left < right; ++left, --right) {
            Presentation::Detail::VisualLease temporary = std::move(route[left]);
            route[left] = std::move(route[right]);
            route[right] = std::move(temporary);
        }
    }
    return {};
}

void RoutedEventManager::InvokeNode(
    Visual& node,
    RoutedEventArgs& args) noexcept {
    UIElement* element = node.AsUIElement();
    if (element == nullptr) return;
    for (const ClassHandlerRecord& record : classHandlers_) {
        if (record.event == args.routedEvent &&
            EventCatalog(eventState_).Types().IsDerivedFrom(
                element->RuntimeType(), record.classType) &&
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

Base::Result<void> RoutedEventManager::RaiseEvent(
    UIElement& source,
    RoutedEventHandle event,
    RoutedEventArgs* suppliedArgs) noexcept {
    Base::Result<void> access = source.VerifyAccess();
    if (!access) {
        return access;
    }
    RoutedEventCatalog& catalog =
        EventCatalog(eventState_);
    if (!catalog.IsFrozen()) {
        return InvalidState(
            "RoutedEventCatalog must be frozen before dispatch");
    }
    if (raiseDepth_ == 64U) {
        return InvalidState(
            "Routed event nesting limit was exceeded");
    }
    const RoutedEventCatalog::Definition* definition =
        catalog.Find(event);
    if (definition == nullptr) {
        return NotFound("Routed event was not found");
    }

    Base::Vector<Presentation::Detail::VisualLease> route;
    Base::Result<void> built =
        BuildRoute(source, definition->strategy, route);
    if (!built) {
        return built;
    }

    RoutedEventArgs localArgs;
    RoutedEventArgs& args = suppliedArgs != nullptr ? *suppliedArgs : localArgs;
    if (args.eventArgsType != definition->eventArgsType) {
        return InvalidArgument("Routed event arguments do not match the registered type");
    }
    args.routedEvent = event;
    args.source = &source;
    if (args.originalSource == nullptr) {
        args.originalSource = &source;
    }

    ++raiseDepth_;
    for (const Presentation::Detail::VisualLease& lease : route) {
        Visual* node = lease.Resolve();
        if (node != nullptr) InvokeNode(*node, args);
    }
    --raiseDepth_;
    return {};
}

void RoutedEventManager::CleanupClassHandlers() noexcept {
    classHandlers_.Clear();
}

} // namespace Aero::Detail
