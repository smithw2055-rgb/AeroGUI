#include "gui/GuiPrivate.hpp"
#include "gui/GuiPrivate.hpp"
#include "gui/GuiPrivate.hpp"
#include <Aero/Layout.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/ContentElement.hpp>

#include "gui/GuiPrivate.hpp"

#include <Aero/Base/Assert.hpp>

#include <utility>
#include "gui/GuiPrivate.hpp"
#include "render/RenderTree.hpp"

namespace Aero {

using namespace Aero::Meta;
using namespace Aero::Threading;
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

RoutedEventTable& Events(void* state) noexcept {
    AERO_ASSERT(state != nullptr);
    return *static_cast<RoutedEventTable*>(state);
}

} // namespace

Visual::Visual(TypeId runtimeType) noexcept
    : DependencyObject(runtimeType),
      logicalChildren_(),
      visualChildren_() {}

Visual* VisualTreeHelper::GetParent(const Visual& visual) noexcept {
    return visual.visualParent_;
}

std::uint32_t VisualTreeHelper::GetChildrenCount(const Visual& visual) noexcept {
    return visual.GetVisualChildrenCountCore();
}

Visual* VisualTreeHelper::GetChild(const Visual& visual, std::uint32_t index) noexcept {
    return visual.GetVisualChildCore(index);
}

DependencyObject* LogicalTreeHelper::GetParent(
    const DependencyObject& object) noexcept {
    const TypeRegistry& types = object.PropertyRegistry().Types();
    if (types.IsDerivedFrom(
            object.RuntimeType(), FrameworkContentElement::StaticTypeId())) {
        return static_cast<const FrameworkContentElement&>(object).GetParent();
    }
    if (types.IsDerivedFrom(
            object.RuntimeType(), FrameworkElement::StaticTypeId())) {
        return static_cast<const FrameworkElement&>(object).GetParent();
    }
    if (types.IsDerivedFrom(object.RuntimeType(), Visual::StaticTypeId())) {
        return static_cast<const Visual&>(object).GetLogicalParent();
    }
    return nullptr;
}

std::uint32_t LogicalTreeHelper::GetChildrenCount(
    const DependencyObject& object) noexcept {
    const TypeRegistry& types = object.PropertyRegistry().Types();
    if (types.IsDerivedFrom(
            object.RuntimeType(), FrameworkContentElement::StaticTypeId())) {
        return Aero::GuiPrivate::Detail::ElementPrivate::LogicalChildrenCount(
            static_cast<const FrameworkContentElement&>(object));
    }
    if (types.IsDerivedFrom(
            object.RuntimeType(), FrameworkElement::StaticTypeId())) {
        return static_cast<const FrameworkElement&>(object)
            .GetLogicalChildrenCountCore();
    }
    if (types.IsDerivedFrom(object.RuntimeType(), Visual::StaticTypeId())) {
        return static_cast<const Visual&>(object).logicalChildren_.Size();
    }
    return 0U;
}

DependencyObject* LogicalTreeHelper::GetChild(
    const DependencyObject& object,
    std::uint32_t index) noexcept {
    const TypeRegistry& types = object.PropertyRegistry().Types();
    if (types.IsDerivedFrom(
            object.RuntimeType(), FrameworkContentElement::StaticTypeId())) {
        return Aero::GuiPrivate::Detail::ElementPrivate::LogicalChild(
            static_cast<const FrameworkContentElement&>(object), index);
    }
    if (types.IsDerivedFrom(
            object.RuntimeType(), FrameworkElement::StaticTypeId())) {
        return static_cast<const FrameworkElement&>(object)
            .GetLogicalChildCore(index);
    }
    if (types.IsDerivedFrom(object.RuntimeType(), Visual::StaticTypeId())) {
        const auto& visual = static_cast<const Visual&>(object);
        return index < visual.logicalChildren_.Size()
            ? visual.logicalChildren_[index] : nullptr;
    }
    return nullptr;
}

Visual* LogicalTreeHelper::GetParent(const Visual& visual) noexcept {
    return visual.logicalParent_;
}

std::uint32_t LogicalTreeHelper::GetChildrenCount(const Visual& visual) noexcept {
    return visual.logicalChildren_.Size();
}

Visual* LogicalTreeHelper::GetChild(const Visual& visual, std::uint32_t index) noexcept {
    return index < visual.logicalChildren_.Size() ? visual.logicalChildren_[index] : nullptr;
}

Visual::~Visual() {
    AERO_ASSERT(tree_ == nullptr);
    AERO_ASSERT(logicalParent_ == nullptr);
    AERO_ASSERT(visualParent_ == nullptr);
    AERO_ASSERT(logicalChildren_.Empty());
    AERO_ASSERT(visualChildren_.Empty());
    AERO_ASSERT(renderRuntime_ == nullptr);
    AERO_ASSERT(!renderAttached_);
    AERO_ASSERT(!renderQueued_);
    AERO_ASSERT(!rendering_);
    AERO_ASSERT(renderNodeId_ == Base::InvalidRenderNodeId);
    if (lifetime_) static_cast<Aero::GuiPrivate::Detail::VisualLifetime*>(lifetime_.Get())->Invalidate();
}

Base::Result<Base::Ref<Base::Object>>
Visual::AcquireLifetime() noexcept {
    if (!lifetime_) {
        Base::Result<Base::Ref<Aero::GuiPrivate::Detail::VisualLifetime>> created =
            Base::MakeRef<Aero::GuiPrivate::Detail::VisualLifetime>(*this);
        if (!created) return created.GetStatus();
        lifetime_ = std::move(created).Value();
    }
    return lifetime_;
}

Base::Result<Aero::GuiPrivate::Detail::VisualLease> Aero::GuiPrivate::Detail::VisualLease::Acquire(
    Visual& node) noexcept {
    VisualLease lease;
    lease.strong = Base::Ref<Visual>::TryFromBorrowed(node);
    if (lease.strong) return lease;

    Base::Result<Base::Ref<Base::Object>> lifetime =
        ElementPrivate::AcquireLifetime(node);
    if (!lifetime) return lifetime.GetStatus();
    lease.lifetime = Base::Ref<VisualLifetime>::FromBorrowed(
        *static_cast<VisualLifetime*>(lifetime.Value().Get()));
    return lease;
}

ElementTree::ElementTree(
    Dispatcher& dispatcher,
    EffectiveValueEngine& values) noexcept
    : dispatcher_(&dispatcher),
      values_(&values),
      lifecycleQueue_(),
      handles_(),
      dataContextChangedHandler_(
          this, &ElementTree::OnDataContextChanged) {}

ElementTree::~ElementTree() noexcept {
    if (lifecycleHook_.IsValid() && dispatcher_->CheckAccess()) {
        (void)dispatcher_->RemoveFrameHook(lifecycleHook_);
    }
    if (root_ != nullptr && dispatcher_->CheckAccess()) {
        (void)SetRoot(nullptr);
    }
}

Base::Result<void> ElementTree::Initialize() noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) {
        return access;
    }
    if (lifecycleHook_.IsValid()) {
        return {};
    }
    Base::Result<DispatcherFrameHookHandle> hook = dispatcher_->RegisterFrameHook(
        DispatcherFramePhase::Lifecycle,
        &ElementTree::LifecycleHook,
        this);
    if (!hook) {
        return hook.GetStatus();
    }
    lifecycleHook_ = hook.Value();
    return {};
}

Base::Result<VisualHandle> ElementTree::GetHandle(
    const Visual& node) const noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    const VisualHandle handle{node.handleIndex_, node.handleGeneration_};
    if (node.tree_ != this || !handle.IsValid() ||
        handle.index >= handles_.Size()) {
        return NotFound("Tree node does not have an active ElementTree handle");
    }
    const HandleEntry& entry = handles_[handle.index];
    if (entry.node != &node || entry.generation != handle.generation) {
        return NotFound("Tree node handle is stale");
    }
    return handle;
}

Visual* ElementTree::ResolveHandle(VisualHandle handle) const noexcept {
    if (!handle.IsValid() || handle.index >= handles_.Size()) return nullptr;
    const HandleEntry& entry = handles_[handle.index];
    return entry.generation == handle.generation ? entry.node : nullptr;
}

Base::Result<void> ElementTree::CollectLogicalSubtree(
    Visual& node,
    Base::Vector<Visual*>& nodes) noexcept {
    Base::Result<void> appended = nodes.PushBack(&node);
    if (!appended) return appended.GetStatus();
    for (Visual* child : node.logicalChildren_) {
        if (child == nullptr) continue;
        Base::Result<void> collected =
            CollectLogicalSubtree(*child, nodes);
        if (!collected) return collected.GetStatus();
    }
    return {};
}

Base::Result<void> ElementTree::RegisterHandleSubtree(Visual& node) noexcept {
    Base::Vector<Visual*> nodes;
    Base::Result<void> collected = CollectLogicalSubtree(node, nodes);
    if (!collected) return collected.GetStatus();

    std::uint32_t required = 0U;
    for (Visual* current : nodes) {
        const VisualHandle currentHandle{current->handleIndex_, current->handleGeneration_};
        if (currentHandle.IsValid()) {
            if (currentHandle.index >= handles_.Size()) {
                return InvalidState("ElementTree node has an invalid pre-existing handle");
            }
            const HandleEntry& entry = handles_[currentHandle.index];
            if (entry.node != current ||
                entry.generation != currentHandle.generation) {
                return InvalidState("ElementTree node has a stale pre-existing handle");
            }
        } else {
            ++required;
        }
    }

    Base::Result<void> reserved =
        handles_.Reserve(handles_.Size() + required);
    if (!reserved) return reserved.GetStatus();

    Base::Vector<Visual*> added;
    reserved = added.Reserve(required);
    if (!reserved) return reserved.GetStatus();

    for (Visual* current : nodes) {
        if (VisualHandle{current->handleIndex_, current->handleGeneration_}.IsValid()) continue;

        HandleEntry entry;
        entry.node = current;
        Base::Result<void> appended = handles_.PushBack(entry);
        AERO_ASSERT(appended);
        (void)appended;
        current->handleIndex_ = handles_.Size() - 1U;
        current->handleGeneration_ = entry.generation;

        Base::Result<void> tracked = TrackInheritedValues(*current);
        if (!tracked) {
            current->handleIndex_ = UINT32_MAX;
            current->handleGeneration_ = 0U;
            handles_.PopBack();
            while (!added.Empty()) {
                Visual* rollback = added.Back();
                UntrackInheritedValues(*rollback);
                rollback->handleIndex_ = UINT32_MAX;
                rollback->handleGeneration_ = 0U;
                handles_.PopBack();
                added.PopBack();
            }
            return tracked.GetStatus();
        }
        Base::Result<void> remembered = added.PushBack(current);
        AERO_ASSERT(remembered);
        (void)remembered;
    }
    return {};
}

void ElementTree::InvalidateHandleSubtree(Visual& node) noexcept {
    for (Visual* child : node.logicalChildren_) {
        if (child != nullptr) InvalidateHandleSubtree(*child);
    }
    const VisualHandle handle{node.handleIndex_, node.handleGeneration_};
    if (handle.IsValid() && handle.index < handles_.Size()) {
        UntrackInheritedValues(node);
        HandleEntry& entry = handles_[handle.index];
        if (entry.node == &node) {
            entry.node = nullptr;
            ++entry.generation;
            if (entry.generation == 0U) ++entry.generation;
        }
    }
    node.handleIndex_ = UINT32_MAX;
    node.handleGeneration_ = 0U;
}

Base::Result<void> ElementTree::TrackInheritedValues(
    Visual& node) noexcept {
    FrameworkElement* element = node.AsFrameworkElement();
    if (element == nullptr ||
        element->PropertyRegistry().Find(
            FrameworkElement::DataContextProperty) == nullptr) {
        return {};
    }
    Base::Result<void> subscribed =
        element->AddValueChangedHandlerChecked(
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

void ElementTree::UntrackInheritedValues(Visual& node) noexcept {
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

void ElementTree::OnDataContextChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs& args) noexcept {
    if (args.GetProperty() == FrameworkElement::DataContextProperty) {
        (void)values_->Invalidate(object, args.GetProperty());
    }
}

Base::Result<void> ElementTree::VerifyMutation(
    const Visual& first,
    const Visual* second) const noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) {
        return access;
    }
    if (!lifecycleHook_.IsValid()) {
        return InvalidState("ElementTree must be initialized before mutation");
    }
    if (mutating_) {
        return InvalidState("Nested object-tree mutation is not allowed");
    }
    if (&first.GetDispatcher() != dispatcher_ ||
        (second != nullptr && &second->GetDispatcher() != dispatcher_)) {
        return Base::Status::Failure(
            Base::ErrorCode::WrongThread,
            "Tree nodes must belong to the ElementTree Dispatcher");
    }
    return {};
}

bool ElementTree::IsLogicalAncestor(
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

bool ElementTree::IsVisualAncestor(
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

Base::Result<void> ElementTree::StageLifecycleSubtree(
    Visual& node,
    bool loaded,
    Base::Vector<LifecycleRecord>& staged) noexcept {
    if (node.loaded_ != loaded) {
        Base::Result<Aero::GuiPrivate::Detail::VisualLease> lease =
            Aero::GuiPrivate::Detail::VisualLease::Acquire(node);
        if (!lease) return lease.GetStatus();

        LifecycleRecord record;
        record.node = std::move(lease).Value();
        record.loaded = loaded;
        Base::Result<void> appended =
            staged.PushBack(std::move(record));
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

void ElementTree::PublishLifecycle(
    Base::Vector<LifecycleRecord>& staged) noexcept {
    for (LifecycleRecord& record : staged) {
        record.sequence = nextLifecycleSequence_++;
        record.treeVersion = version_;
        Base::Result<void> appended =
            lifecycleQueue_.PushBack(std::move(record));
        AERO_ASSERT(appended);
        (void)appended;
    }
    staged.Clear();
}

void ElementTree::ApplyLoadedSubtree(Visual& node, bool loaded) noexcept {
    node.loaded_ = loaded;
    for (Visual* child : node.logicalChildren_) {
        if (child != nullptr) ApplyLoadedSubtree(*child, loaded);
    }
}

void ElementTree::SetTreeSubtree(
    Visual& node, ElementTree* tree) noexcept {
    node.tree_ = tree;
    for (Visual* child : node.logicalChildren_) {
        if (child != nullptr) SetTreeSubtree(*child, tree);
    }
}

Base::Result<void> ElementTree::SetRoot(Visual* root) noexcept {
    if (root == root_) return {};
    if (root == nullptr && root_ == nullptr) return {};

    Visual& verificationNode = root != nullptr ? *root : *root_;
    Base::Result<void> verified = VerifyMutation(verificationNode, root_);
    if (!verified) return verified.GetStatus();

    if (root != nullptr &&
        (root->logicalParent_ != nullptr ||
         root->visualParent_ != nullptr ||
         root->tree_ != nullptr)) {
        return InvalidState("ElementTree root must be fully detached");
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
    Base::Result<void> queueReserved = lifecycleQueue_.Reserve(
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

Base::Result<void> ElementTree::AttachLogical(
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

    Base::Result<void> childReserved = parent.logicalChildren_.Reserve(
        parent.logicalChildren_.Size() + 1U);
    if (!childReserved) return childReserved.GetStatus();

    Base::Vector<LifecycleRecord> staged;
    if (parent.loaded_) {
        Base::Result<void> prepared =
            StageLifecycleSubtree(child, true, staged);
        if (!prepared) return prepared.GetStatus();
    }
    Base::Result<void> queueReserved = lifecycleQueue_.Reserve(
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
        parent.logicalChildren_.PushBack(&child);
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

void ElementTree::RemoveChild(
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

Base::Result<void> ElementTree::DetachLogical(
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
    Base::Result<void> queueReserved = lifecycleQueue_.Reserve(
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

Base::Result<void> ElementTree::AttachVisual(
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
    Base::Result<void> appended = parent.visualChildren_.PushBack(&child);
    if (!appended) {
        return appended;
    }
    child.visualParent_ = &parent;
    ++version_;
    return {};
}

Base::Result<void> ElementTree::DetachVisual(
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

Base::Result<void> ElementTree::DetachNode(Visual& node) noexcept {
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

Base::Result<std::uint32_t> ElementTree::FlushLifecycle() noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();

    Base::Vector<LifecycleRecord> snapshot;
    Base::Result<void> assigned = snapshot.Assign(
        Base::Span<const LifecycleRecord>(
            lifecycleQueue_.Data(), lifecycleQueue_.Size()));
    if (!assigned) return assigned.GetStatus();
    lifecycleQueue_.Clear();

    std::uint32_t count = 0U;
    for (const LifecycleRecord& record : snapshot) {
        Visual* node = record.node.Resolve();
        if (node == nullptr) continue;
        if (lifecycleHandler_ != nullptr) {
            const ElementTreeLifecycleEvent event{
                node, record.loaded, record.treeVersion};
            lifecycleHandler_(event, lifecycleContext_);
        }
        ++count;
    }
    return count;
}

void ElementTree::LifecycleHook(void* context) noexcept {
    ElementTree* tree = static_cast<ElementTree*>(context);
    (void)tree->FlushLifecycle();
}


Base::Result<void> ElementTree::AttachLayout(
    Visual& parent, Visual& child, bool& attached) noexcept {
    UIElement* parentElement = parent.AsUIElement();
    UIElement* childElement = child.AsUIElement();
    if (layout_ == nullptr || parentElement == nullptr || childElement == nullptr) {
        return {};
    }
    Base::Result<void> result = layout_->Attach(*parentElement, *childElement);
    if (!result) return result.GetStatus();
    attached = true;
    return {};
}

Base::Result<void> ElementTree::AttachRender(
    Visual& parent, Visual& child, bool& attached) noexcept {
    if (renderer_ == nullptr) {
        return {};
    }
    Base::Result<void> result = renderer_->Attach(parent, child);
    if (!result) return result.GetStatus();
    attached = true;
    return {};
}

Base::Result<void> ElementTree::DetachLayout(
    Visual& parent, Visual& child, bool& attached) noexcept {
    if (!attached) return {};
    UIElement* parentElement = parent.AsUIElement();
    UIElement* childElement = child.AsUIElement();
    if (layout_ == nullptr || parentElement == nullptr || childElement == nullptr) {
        return InvalidState("Attached layout edge has no layout service");
    }
    Base::Result<void> result = layout_->Detach(*parentElement, *childElement);
    if (!result) return result.GetStatus();
    attached = false;
    return {};
}

Base::Result<void> ElementTree::DetachRender(
    Visual& parent, Visual& child, bool& attached) noexcept {
    if (!attached) return {};
    if (renderer_ == nullptr) {
        return InvalidState("Attached render edge has no render tree");
    }
    Base::Result<void> result = renderer_->Detach(parent, child);
    if (!result) return result.GetStatus();
    attached = false;
    return {};
}

Base::Result<Aero::GuiPrivate::Detail::ElementAttachment> ElementTree::AttachElement(
    Visual& logicalParent, Visual& visualParent, Visual& child) noexcept {
    if (&logicalParent == &child || &visualParent == &child) {
        return InvalidState("Element cannot be attached to itself");
    }

    Aero::GuiPrivate::Detail::ElementAttachment state;
    state.logicalParent = &logicalParent;
    state.visualParent = &visualParent;
    state.child = &child;

    Base::Result<void> logical = AttachLogical(logicalParent, child);
    if (!logical) return logical.GetStatus();
    state.logicalAttached = true;

    Base::Result<void> visual = AttachVisual(visualParent, child);
    if (!visual) {
        (void)DetachLogical(logicalParent, child);
        return visual.GetStatus();
    }
    state.visualAttached = true;

    Base::Result<void> layout = AttachLayout(visualParent, child, state.layoutAttached);
    if (!layout) {
        (void)DetachVisual(visualParent, child);
        (void)DetachLogical(logicalParent, child);
        return layout.GetStatus();
    }

    Base::Result<void> render = AttachRender(visualParent, child, state.renderAttached);
    if (!render) {
        (void)DetachLayout(visualParent, child, state.layoutAttached);
        (void)DetachVisual(visualParent, child);
        (void)DetachLogical(logicalParent, child);
        return render.GetStatus();
    }

    Base::Result<VisualHandle> handle = GetHandle(child);
    if (!handle) {
        (void)DetachElement(state);
        return handle.GetStatus();
    }
    state.childHandle = handle.Value();
    return state;
}

Base::Result<void> ElementTree::DetachElement(
    Aero::GuiPrivate::Detail::ElementAttachment& state) noexcept {
    if (!state.IsAttached()) return {};
    if (state.logicalParent == nullptr || state.visualParent == nullptr ||
        state.child == nullptr) {
        return InvalidState("Element attachment is incomplete");
    }

    Visual& logicalParent = *state.logicalParent;
    Visual& visualParent = *state.visualParent;
    Visual& child = *state.child;

    if (state.renderAttached) {
        Base::Result<void> result = DetachRender(visualParent, child, state.renderAttached);
        if (!result) return result.GetStatus();
    }
    if (state.layoutAttached) {
        Base::Result<void> result = DetachLayout(visualParent, child, state.layoutAttached);
        if (!result) {
            (void)AttachRender(visualParent, child, state.renderAttached);
            return result.GetStatus();
        }
    }
    if (state.visualAttached) {
        Base::Result<void> result = DetachVisual(visualParent, child);
        if (!result) {
            (void)AttachLayout(visualParent, child, state.layoutAttached);
            (void)AttachRender(visualParent, child, state.renderAttached);
            return result.GetStatus();
        }
        state.visualAttached = false;
    }
    if (state.logicalAttached) {
        Base::Result<void> result = DetachLogical(logicalParent, child);
        if (!result) {
            Base::Result<void> restored = AttachVisual(visualParent, child);
            if (restored) state.visualAttached = true;
            (void)AttachLayout(visualParent, child, state.layoutAttached);
            (void)AttachRender(visualParent, child, state.renderAttached);
            return result.GetStatus();
        }
        state.logicalAttached = false;
    }
    state.childHandle = {};
    return {};
}

Base::Result<void> ElementTree::DetachVisual(
    Aero::GuiPrivate::Detail::ElementAttachment& state) noexcept {
    Aero::GuiPrivate::Detail::VisualAttachment visualState;
    visualState.visualParent = state.visualParent;
    visualState.child = state.child;
    visualState.visualAttached = state.visualAttached;
    visualState.layoutAttached = state.layoutAttached;
    visualState.renderAttached = state.renderAttached;
    Base::Result<void> detached = DetachVisual(visualState);
    state.visualAttached = visualState.visualAttached;
    state.layoutAttached = visualState.layoutAttached;
    state.renderAttached = visualState.renderAttached;
    return detached;
}

Base::Result<void> ElementTree::AttachVisual(
    Aero::GuiPrivate::Detail::ElementAttachment& state, Visual& newVisualParent) noexcept {
    if (state.child == nullptr || state.visualAttached ||
        state.layoutAttached || state.renderAttached) {
        return InvalidState("Element attachment is not ready for a visual parent");
    }
    Base::Result<Aero::GuiPrivate::Detail::VisualAttachment> attached =
        AttachVisualChild(newVisualParent, *state.child);
    if (!attached) return attached.GetStatus();
    Aero::GuiPrivate::Detail::VisualAttachment visualState = std::move(attached).Value();
    state.visualParent = visualState.visualParent;
    state.visualAttached = visualState.visualAttached;
    state.layoutAttached = visualState.layoutAttached;
    state.renderAttached = visualState.renderAttached;
    return {};
}

Base::Result<Aero::GuiPrivate::Detail::VisualAttachment> ElementTree::AttachVisualChild(
    Visual& visualParent, Visual& child) noexcept {
    Aero::GuiPrivate::Detail::VisualAttachment state;
    state.visualParent = &visualParent;
    state.child = &child;

    Base::Result<void> visual = AttachVisual(visualParent, child);
    if (!visual) return visual.GetStatus();
    state.visualAttached = true;

    Base::Result<void> layout = AttachLayout(visualParent, child, state.layoutAttached);
    if (!layout) {
        (void)DetachVisual(visualParent, child);
        return layout.GetStatus();
    }
    Base::Result<void> render = AttachRender(visualParent, child, state.renderAttached);
    if (!render) {
        (void)DetachLayout(visualParent, child, state.layoutAttached);
        (void)DetachVisual(visualParent, child);
        return render.GetStatus();
    }
    if (state.renderAttached && renderer_ != nullptr) {
        auto attachDescendants = [&](auto&& self, Visual& parent) noexcept
            -> Base::Result<void> {
            for (Visual* descendant :
                 Aero::GuiPrivate::Detail::ElementPrivate::RenderChildren(parent)) {
                if (descendant == nullptr) continue;
                Base::Result<void> attached = renderer_->Attach(parent, *descendant);
                if (!attached) return attached.GetStatus();
                Base::Result<void> nested = self(self, *descendant);
                if (!nested) return nested.GetStatus();
            }
            return {};
        };
        Base::Result<void> descendants =
            attachDescendants(attachDescendants, child);
        if (!descendants) {
            (void)DetachRender(visualParent, child, state.renderAttached);
            (void)DetachLayout(visualParent, child, state.layoutAttached);
            (void)DetachVisual(visualParent, child);
            state.visualAttached = false;
            return descendants.GetStatus();
        }
    }
    return state;
}

Base::Result<void> ElementTree::DetachVisual(
    Aero::GuiPrivate::Detail::VisualAttachment& state) noexcept {
    if (!state.IsAttached()) return {};
    if (state.visualParent == nullptr || state.child == nullptr) {
        return InvalidState("Visual attachment is incomplete");
    }
    Visual& parent = *state.visualParent;
    Visual& child = *state.child;

    if (state.renderAttached) {
        Base::Result<void> result = DetachRender(parent, child, state.renderAttached);
        if (!result) return result.GetStatus();
    }
    if (state.layoutAttached) {
        Base::Result<void> result = DetachLayout(parent, child, state.layoutAttached);
        if (!result) {
            (void)AttachRender(parent, child, state.renderAttached);
            return result.GetStatus();
        }
    }
    if (state.visualAttached) {
        Base::Result<void> result = DetachVisual(parent, child);
        if (!result) {
            (void)AttachLayout(parent, child, state.layoutAttached);
            (void)AttachRender(parent, child, state.renderAttached);
            return result.GetStatus();
        }
        state.visualAttached = false;
    }
    return {};
}

Base::Result<Aero::GuiPrivate::Detail::VisualAttachment> ElementTree::ReparentVisual(
    Aero::GuiPrivate::Detail::VisualAttachment& current, Visual& newVisualParent) noexcept {
    if (current.child == nullptr || current.visualParent == nullptr) {
        return InvalidState("Visual reparent state is incomplete");
    }
    Visual* oldParent = current.visualParent;
    Visual* child = current.child;
    Aero::GuiPrivate::Detail::VisualAttachment oldState = current;

    Base::Result<void> detached = DetachVisual(current);
    if (!detached) return detached.GetStatus();

    Base::Result<Aero::GuiPrivate::Detail::VisualAttachment> attached =
        AttachVisualChild(newVisualParent, *child);
    if (attached) return attached;

    Base::Result<Aero::GuiPrivate::Detail::VisualAttachment> restored =
        AttachVisualChild(*oldParent, *child);
    if (restored) current = std::move(restored).Value();
    else current = oldState;
    return attached.GetStatus();
}

Base::Result<Aero::GuiPrivate::Detail::RootAttachment> ElementTree::AttachRoot(
    Visual& root, Size availableSize) noexcept {
    Aero::GuiPrivate::Detail::RootAttachment state;
    state.root = &root;
    state.availableSize = availableSize;

    Base::Result<void> context = SetRoot(&root);
    if (!context) return context.GetStatus();
    state.contextAttached = true;

    UIElement* layoutRoot = root.AsUIElement();
    if (layoutRoot != nullptr) {
        if (layout_ == nullptr) {
            (void)SetRoot(nullptr);
            return InvalidState("UI root requires a layout service");
        }
        Base::Result<void> layout = layout_->SetRoot(layoutRoot, availableSize);
        if (!layout) {
            (void)SetRoot(nullptr);
            return layout.GetStatus();
        }
        state.layoutAttached = true;
    }

    if (renderer_ != nullptr) {
        Base::Result<void> render = renderer_->SetRoot(&root);
        if (!render) {
            if (state.layoutAttached) (void)layout_->SetRoot(nullptr, {});
            (void)SetRoot(nullptr);
            return render.GetStatus();
        }
        state.renderAttached = true;
    }
    return state;
}

Base::Result<void> ElementTree::DetachRoot(
    Aero::GuiPrivate::Detail::RootAttachment& state) noexcept {
    if (!state.IsAttached()) return {};
    if (state.root == nullptr) return InvalidState("Root attachment is incomplete");

    if (state.renderAttached) {
        Base::Result<void> result = renderer_->SetRoot(nullptr);
        if (!result) return result.GetStatus();
        state.renderAttached = false;
    }
    if (state.layoutAttached) {
        Base::Result<void> result = layout_->SetRoot(nullptr, {});
        if (!result) {
            if (renderer_ != nullptr) {
                Base::Result<void> restored =
                    renderer_->SetRoot(state.root);
                if (restored) state.renderAttached = true;
            }
            return result.GetStatus();
        }
        state.layoutAttached = false;
    }
    if (state.contextAttached) {
        Base::Result<void> result = SetRoot(nullptr);
        if (!result) {
            if (state.root->AsUIElement() != nullptr && layout_ != nullptr) {
                Base::Result<void> restored =
                    layout_->SetRoot(state.root->AsUIElement(), state.availableSize);
                if (restored) state.layoutAttached = true;
            }
            if (renderer_ != nullptr) {
                Base::Result<void> restored =
                    renderer_->SetRoot(state.root);
                if (restored) state.renderAttached = true;
            }
            return result.GetStatus();
        }
        state.contextAttached = false;
    }
    return {};
}

} // namespace Aero

namespace Aero {

void Visual::Impl::InvokeHandlers(
    UIElement& element,
    RoutedEventHandle event,
    RoutedEventArgs& args) noexcept {
    element.InvokeHandlers(event, args);
}

} // namespace Aero

namespace Aero::GuiPrivate::Detail {

using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero;

EventRouter::EventRouter(
    void* eventState) noexcept
    : eventState_(eventState),
      classHandlers_() {}

EventRouter::~EventRouter() noexcept {
    CleanupClassHandlers();
}

Base::Result<void> EventRouter::ValidateClassHandler(
    RoutedEventHandle event,
    TypeId classType,
    TypeId eventArgsType) const noexcept {
    RoutedEventTable& catalog =
        Events(eventState_);
    if (!catalog.IsFrozen()) {
        return InvalidState(
            "RoutedEventTable must be frozen before handlers");
    }
    const RoutedEventTable::Definition* definition =
        catalog.Find(event);
    if (definition == nullptr ||
        catalog.Types().FindType(classType) == nullptr ||
        definition->eventArgsType != eventArgsType) {
        return InvalidArgument(
            "Class handler registration is invalid");
    }
    return {};
}

void EventRouter::InvokeNode(
    DependencyObject& node,
    RoutedEventArgs& args) noexcept {
    RoutedEventTable& catalog = Events(eventState_);
    for (const ClassHandlerRecord& record : classHandlers_) {
        if (record.event == args.GetRoutedEvent() &&
            catalog.Types().IsDerivedFrom(
                node.RuntimeType(), record.classType) &&
            (!args.GetHandled() || record.handledEventsToo)) {
            record.handler.Invoke(&node, args);
        }
    }

    if (catalog.Types().IsDerivedFrom(
            node.RuntimeType(), UIElement::StaticTypeId())) {
        ElementPrivate::InvokeHandlers(
            static_cast<UIElement&>(node), args.GetRoutedEvent(), args);
        return;
    }
    if (catalog.Types().IsDerivedFrom(
            node.RuntimeType(), ContentElement::StaticTypeId())) {
        ::Aero::Visual::Impl::InvokeContentHandlers(
            static_cast<ContentElement&>(node),
            args.GetRoutedEvent(), args);
    }
}

Base::Result<void> EventRouter::RaiseEvent(
    DependencyObject& source,
    RoutedEventHandle event,
    RoutedEventArgs* suppliedArgs) noexcept {
    Base::Result<void> access = source.VerifyAccess();
    if (!access) return access;

    RoutedEventTable& catalog = Events(eventState_);
    if (!catalog.IsFrozen()) {
        return InvalidState(
            "RoutedEventTable must be frozen before dispatch");
    }
    if (raiseDepth_ == 64U) {
        return InvalidState(
            "Routed event nesting limit was exceeded");
    }
    const RoutedEventTable::Definition* definition = catalog.Find(event);
    if (definition == nullptr) {
        return NotFound("Routed event was not found");
    }

    EventRoute route;
    Base::Result<void> built = route.Build(source, definition->strategy);
    if (!built) return built.GetStatus();

    RoutedEventArgs localArgs;
    RoutedEventArgs& args = suppliedArgs != nullptr ? *suppliedArgs : localArgs;
    if (args.GetEventArgsType() != definition->eventArgsType) {
        return InvalidArgument(
            "Routed event arguments do not match the registered type");
    }
    args.SetRoutedEvent(event);
    args.SetSource(&source);
    if (args.GetOriginalSource() == nullptr) args.SetOriginalSource(&source);

    ++raiseDepth_;
    for (const EventRouteNode& lease : route.Nodes()) {
        DependencyObject* node = lease.Resolve();
        if (node != nullptr) InvokeNode(*node, args);
    }
    --raiseDepth_;
    return {};
}

void EventRouter::CleanupClassHandlers() noexcept {
    classHandlers_.Clear();
}


} // namespace Aero::GuiPrivate::Detail

namespace Aero {

Base::Object* FrameworkElement::FindName(
    Base::StringView name) noexcept {
    return FindNameObject(name, Meta::InvalidTypeId);
}

Base::Object* FrameworkElement::FindNameObject(
    Base::StringView name,
    Meta::TypeId expectedType) noexcept {
    return Aero::GuiPrivate::Detail::ElementPrivate::FindName(
        *this, name, expectedType);
}

} // namespace Aero
