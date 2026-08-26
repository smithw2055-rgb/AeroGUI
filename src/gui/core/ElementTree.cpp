#include "gui/meta/MetadataState.hpp"
#include "gui/core/State.hpp" 
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include <Aero/Layout.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/FrameworkContentElement.hpp>
#include <Aero/LogicalTreeHelper.hpp>
#include <Aero/VisualTreeHelper.hpp>
#include <Aero/Controls/ContentPresenter.hpp>
#include <Aero/Controls/Decorator.hpp>
#include <Aero/Controls/Panel.hpp>
#include <Aero/Controls/ContentControl.hpp>


#include "render/RenderTree.hpp"
#include "gui/internal/AeroGuiInternal.hpp"
#include "gui/markup/MarkupState.hpp"

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

Base::Result<void> EnsureVisualChildStorage(
    ::Aero::Media::Visual& parent,
    ::Aero::Media::Visual& child) noexcept {
    UIElement* childElement = ::Aero::TryCast<::Aero::UIElement>(&(child));
    if (childElement == nullptr) return {};
    const TypeRegistry& types = parent.PropertyRegistry().Types();
    if (types.IsDerivedFrom(
            parent.RuntimeType(), Controls::Panel::StaticTypeId())) {
        auto& panel = static_cast<Controls::Panel&>(parent);
        const std::uint32_t count = AeroGuiInternal::PanelChildCount(panel);
        for (std::uint32_t index = 0U; index < count; ++index) {
            if (AeroGuiInternal::PanelChildAt(panel, index).Get() == childElement) {
                return {};
            }
        }
        Base::Ref<Base::Object> borrowed =
            Base::Ref<Base::Object>::FromBorrowed(*childElement);
        return AeroGuiInternal::PanelAddChild(panel, borrowed, *childElement);
    }
    if (types.IsDerivedFrom(
            parent.RuntimeType(), Controls::ContentPresenter::StaticTypeId())) {
        auto& presenter = static_cast<Controls::ContentPresenter&>(parent);
        if (presenter.GetContent() == nullptr) {
            presenter.SetContent(childElement);
        }
        return {};
    }
    if (types.IsDerivedFrom(
            parent.RuntimeType(), Controls::Decorator::StaticTypeId())) {
        auto& decorator = static_cast<Controls::Decorator&>(parent);
        if (decorator.GetChild() == nullptr) {
            decorator.SetChild(childElement);
        }
        return {};
    }
    if (types.IsDerivedFrom(
            parent.RuntimeType(), Controls::BulletDecorator::StaticTypeId())) {
        auto& bullet = static_cast<Controls::BulletDecorator&>(parent);
        if (bullet.GetChild() == childElement ||
            bullet.GetBullet() == childElement) {
            return {};
        }
        Base::Ref<UIElement> borrowed =
            Base::Ref<UIElement>::FromBorrowed(*childElement);
        if (bullet.GetChild() == nullptr) {
            bullet.SetChild(std::move(borrowed));
            return {};
        }
        if (bullet.GetBullet() == nullptr) {
            bullet.SetBullet(std::move(borrowed));
        }
        return {};
    }
    return {};
}

} // namespace

Media::Visual::Visual(TypeId runtimeType) noexcept
    : DependencyObject(runtimeType) {}

void Media::Visual::AddVisualChild(Visual* child) noexcept {
    if (child == nullptr || child == this) return;
    Visual* const oldParent = child->visualParent_;
    if (oldParent == this) return;
    if (oldParent != nullptr) return;
    child->visualParent_ = this;
    child->OnVisualParentChanged(oldParent);
    OnVisualChildrenChanged(child, nullptr);
}

void Media::Visual::RemoveVisualChild(Visual* child) noexcept {
    if (child == nullptr) return;
    Visual* const oldParent = child->visualParent_;
    if (oldParent == this) {
        child->visualParent_ = nullptr;
        child->OnVisualParentChanged(oldParent);
    }
    OnVisualChildrenChanged(nullptr, child);
}

::Aero::Media::Visual* Media::VisualTreeHelper::GetParent(const ::Aero::Media::Visual& visual) noexcept {
    return visual.visualParent_;
}

std::uint32_t Media::VisualTreeHelper::GetChildrenCount(const ::Aero::Media::Visual& visual) noexcept {
    return visual.GetVisualChildrenCount();
}

::Aero::Media::Visual* Media::VisualTreeHelper::GetChild(const ::Aero::Media::Visual& visual, std::uint32_t index) noexcept {
    return visual.GetVisualChild(index);
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
    if (types.IsDerivedFrom(object.RuntimeType(), Media::Visual::StaticTypeId())) {
        return static_cast<const ::Aero::Media::Visual&>(object).GetLogicalParent();
    }
    return nullptr;
}

std::uint32_t LogicalTreeHelper::GetChildrenCount(
    const DependencyObject& object) noexcept {
    const TypeRegistry& types = object.PropertyRegistry().Types();
    if (types.IsDerivedFrom(
            object.RuntimeType(), FrameworkContentElement::StaticTypeId())) {
        return AeroGuiInternal::LogicalChildrenCount(
            static_cast<const FrameworkContentElement&>(object));
    }
    if (types.IsDerivedFrom(
            object.RuntimeType(), FrameworkElement::StaticTypeId())) {
        return static_cast<const FrameworkElement&>(object)
            .GetLogicalChildrenCount();
    }
    return 0U;
}

DependencyObject* LogicalTreeHelper::GetChild(
    const DependencyObject& object,
    std::uint32_t index) noexcept {
    const TypeRegistry& types = object.PropertyRegistry().Types();
    if (types.IsDerivedFrom(
            object.RuntimeType(), FrameworkContentElement::StaticTypeId())) {
        return AeroGuiInternal::LogicalChild(
            static_cast<const FrameworkContentElement&>(object), index);
    }
    if (types.IsDerivedFrom(
            object.RuntimeType(), FrameworkElement::StaticTypeId())) {
        return static_cast<const FrameworkElement&>(object)
            .GetLogicalChild(index);
    }
    return nullptr;
}

Media::Visual::~Visual() {
    AERO_ASSERT(tree_ == nullptr);
    AERO_ASSERT(logicalParent_ == nullptr);
    AERO_ASSERT(visualParent_ == nullptr);
    AERO_ASSERT(!renderAttached_);
    AERO_ASSERT(!renderQueued_);
    AERO_ASSERT(!rendering_);
    AERO_ASSERT(renderNodeId_ == Base::InvalidRenderNodeId);
    if (lifetime_) static_cast<Aero::VisualLifetime*>(lifetime_.Get())->Invalidate();
}


Base::Result<Aero::VisualLease> Aero::VisualLease::Acquire(
    ::Aero::Media::Visual& node) noexcept {
    VisualLease lease;
    lease.strong = Base::Ref<::Aero::Media::Visual>::TryFromBorrowed(node);
    if (lease.strong) return lease;

    Base::Result<Base::Ref<Base::Object>> lifetime =
        AeroGuiInternal::AcquireLifetime(node);
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
    const ::Aero::Media::Visual& node) const noexcept {
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

::Aero::Media::Visual* ElementTree::ResolveHandle(VisualHandle handle) const noexcept {
    if (!handle.IsValid() || handle.index >= handles_.Size()) return nullptr;
    const HandleEntry& entry = handles_[handle.index];
    return entry.generation == handle.generation ? entry.node : nullptr;
}

Base::Result<void> ElementTree::CollectLogicalSubtree(
    ::Aero::Media::Visual& node,
    Base::Vector<::Aero::Media::Visual*>& nodes) noexcept {
    Base::Result<void> appended = nodes.PushBack(&node);
    if (!appended) return appended.GetStatus();
    const std::uint32_t childCount = LogicalTreeHelper::GetChildrenCount(node);
    for (std::uint32_t index = 0U; index < childCount; ++index) {
        ::Aero::Media::Visual* child = ::Aero::TryCast<::Aero::Media::Visual>(
            LogicalTreeHelper::GetChild(node, index));
        if (child == nullptr) continue;
        Base::Result<void> collected =
            CollectLogicalSubtree(*child, nodes);
        if (!collected) return collected.GetStatus();
    }
    return {};
}

Base::Result<void> ElementTree::RegisterHandleSubtree(::Aero::Media::Visual& node) noexcept {
    Base::Vector<::Aero::Media::Visual*> nodes;
    Base::Result<void> collected = CollectLogicalSubtree(node, nodes);
    if (!collected) return collected.GetStatus();

    std::uint32_t required = 0U;
    for (::Aero::Media::Visual* current : nodes) {
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

    Base::Vector<::Aero::Media::Visual*> added;
    reserved = added.Reserve(required);
    if (!reserved) return reserved.GetStatus();

    for (::Aero::Media::Visual* current : nodes) {
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
                ::Aero::Media::Visual* rollback = added.Back();
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

void ElementTree::InvalidateHandleSubtree(::Aero::Media::Visual& node) noexcept {
    const std::uint32_t childCount = LogicalTreeHelper::GetChildrenCount(node);
    for (std::uint32_t index = 0U; index < childCount; ++index) {
        ::Aero::Media::Visual* child = ::Aero::TryCast<::Aero::Media::Visual>(
            LogicalTreeHelper::GetChild(node, index));
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
    ::Aero::Media::Visual& node) noexcept {
    FrameworkElement* element = ::Aero::TryCast<::Aero::FrameworkElement>(&(node));
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

void ElementTree::UntrackInheritedValues(::Aero::Media::Visual& node) noexcept {
    FrameworkElement* element = ::Aero::TryCast<::Aero::FrameworkElement>(&(node));
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
    const ::Aero::Media::Visual& first,
    const ::Aero::Media::Visual* second) const noexcept {
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
    const ::Aero::Media::Visual& possibleAncestor,
    const ::Aero::Media::Visual& node) const noexcept {
    const ::Aero::DependencyObject* current = node.logicalParent_;
    while (current != nullptr) {
        if (current == &possibleAncestor) {
            return true;
        }
        current = LogicalTreeHelper::GetParent(*current);
    }
    return false;
}

bool ElementTree::IsVisualAncestor(
    const ::Aero::Media::Visual& possibleAncestor,
    const ::Aero::Media::Visual& node) const noexcept {
    const ::Aero::Media::Visual* current = node.visualParent_;
    while (current != nullptr) {
        if (current == &possibleAncestor) {
            return true;
        }
        current = current->visualParent_;
    }
    return false;
}

Base::Result<void> ElementTree::StageLifecycleSubtree(
    ::Aero::Media::Visual& node,
    bool loaded,
    Base::Vector<LifecycleRecord>& staged) noexcept {
    if (node.loaded_ != loaded) {
        Base::Result<Aero::VisualLease> lease =
            Aero::VisualLease::Acquire(node);
        if (!lease) return lease.GetStatus();

        LifecycleRecord record;
        record.node = std::move(lease).Value();
        record.loaded = loaded;
        Base::Result<void> appended =
            staged.PushBack(std::move(record));
        if (!appended) return appended.GetStatus();
    }
    const std::uint32_t childCount = LogicalTreeHelper::GetChildrenCount(node);
    for (std::uint32_t index = 0U; index < childCount; ++index) {
        ::Aero::Media::Visual* child = ::Aero::TryCast<::Aero::Media::Visual>(
            LogicalTreeHelper::GetChild(node, index));
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

void ElementTree::ApplyLoadedSubtree(::Aero::Media::Visual& node, bool loaded) noexcept {
    node.loaded_ = loaded;
    const std::uint32_t childCount = LogicalTreeHelper::GetChildrenCount(node);
    for (std::uint32_t index = 0U; index < childCount; ++index) {
        ::Aero::Media::Visual* child = ::Aero::TryCast<::Aero::Media::Visual>(
            LogicalTreeHelper::GetChild(node, index));
        if (child != nullptr) ApplyLoadedSubtree(*child, loaded);
    }
}

void ElementTree::SetTreeSubtree(
    ::Aero::Media::Visual& node, ElementTree* tree) noexcept {
    node.tree_ = tree;
    const std::uint32_t childCount = LogicalTreeHelper::GetChildrenCount(node);
    for (std::uint32_t index = 0U; index < childCount; ++index) {
        ::Aero::Media::Visual* child = ::Aero::TryCast<::Aero::Media::Visual>(
            LogicalTreeHelper::GetChild(node, index));
        if (child != nullptr) SetTreeSubtree(*child, tree);
    }
}

Base::Result<void> ElementTree::SetRoot(::Aero::Media::Visual* root) noexcept {
    if (root == root_) return {};
    if (root == nullptr && root_ == nullptr) return {};

    ::Aero::Media::Visual& verificationNode = root != nullptr ? *root : *root_;
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
    ::Aero::Media::Visual* oldRoot = root_;
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
    ::Aero::Media::Visual& parent,
    ::Aero::Media::Visual& child) noexcept {
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
    child.logicalParent_ = &parent;
    SetTreeSubtree(child, this);
    ++version_;
    if (parent.loaded_) ApplyLoadedSubtree(child, true);
    PublishLifecycle(staged);
    mutating_ = false;
    return {};
}

Base::Result<void> ElementTree::DetachLogical(
    ::Aero::Media::Visual& parent,
    ::Aero::Media::Visual& child) noexcept {
    // A complete logical subtree may already have been removed from this
    // ElementTree while its internal parent/child ownership is still being
    // dismantled leaf-first. Finish that detached-subtree relationship without
    // re-entering inheritance or lifecycle services owned by the old tree.
    if (child.logicalParent_ == &parent &&
        child.tree_ == nullptr && parent.tree_ == nullptr) {
        child.logicalParent_ = nullptr;
        return {};
    }
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
    child.logicalParent_ = nullptr;
    SetTreeSubtree(child, nullptr);
    InvalidateHandleSubtree(child);
    ++version_;
    PublishLifecycle(staged);
    mutating_ = false;
    return {};
}

Base::Result<void> ElementTree::AttachVisual(
    ::Aero::Media::Visual& parent,
    ::Aero::Media::Visual& child) noexcept {
    Base::Result<void> verified = VerifyMutation(parent, &child);
    if (!verified) {
        return verified;
    }
    if (&parent == &child || IsVisualAncestor(child, parent)) {
        return Base::Status::Failure(
            Base::ErrorCode::CycleDetected,
            "Visual tree attachment would create a cycle");
    }
    if (child.visualParent_ == &parent) {
        ++version_;
        return {};
    }
    if (child.visualParent_ != nullptr || parent.tree_ != this ||
        child.tree_ != this) {
        return InvalidState("Visual nodes must be logical members of this tree");
    }
    Base::Result<void> stored = EnsureVisualChildStorage(parent, child);
    if (!stored) return stored.GetStatus();
    if (child.visualParent_ != &parent) {
        parent.AddVisualChild(&child);
    }
    if (parent.PropertyRegistry().Types().IsDerivedFrom(
            parent.RuntimeType(), Controls::Control::StaticTypeId()) &&
        ::Aero::TryCast<::Aero::UIElement>(&(child)) != nullptr) {
        auto& control = static_cast<Controls::Control&>(parent);
        const bool contentVisual =
            parent.PropertyRegistry().Types().IsDerivedFrom(
                parent.RuntimeType(),
                Controls::ContentControl::StaticTypeId()) &&
            AeroGuiInternal::ContentControlContent(
                static_cast<Controls::ContentControl&>(parent)) ==
                ::Aero::TryCast<::Aero::UIElement>(&(child));
        if (AeroGuiInternal::TemplateRoot(control) == nullptr && !contentVisual) {
            (void)AeroGuiInternal::SetTemplateRoot(control, ::Aero::TryCast<::Aero::UIElement>(&(child)));
        }
    }
    ++version_;
    return {};
}

Base::Result<void> ElementTree::DetachVisual(
    ::Aero::Media::Visual& parent,
    ::Aero::Media::Visual& child) noexcept {
    if (child.visualParent_ == &parent &&
        child.tree_ == nullptr && parent.tree_ == nullptr) {
        parent.RemoveVisualChild(&child);
        return {};
    }
    Base::Result<void> verified = VerifyMutation(parent, &child);
    if (!verified) {
        return verified;
    }
    if (child.visualParent_ != &parent) {
        return NotFound("Visual parent-child relationship was not found");
    }
    parent.RemoveVisualChild(&child);
    ++version_;
    return {};
}

Base::Result<void> ElementTree::DetachNode(::Aero::Media::Visual& node) noexcept {
    Base::Result<void> verified = VerifyMutation(node);
    if (!verified) {
        return verified;
    }
    Base::Vector<::Aero::Media::Visual*> visualChildren;
    const std::uint32_t visualCount =
        Media::VisualTreeHelper::GetChildrenCount(node);
    Base::Result<void> reserved = visualChildren.Reserve(visualCount);
    if (!reserved) return reserved.GetStatus();
    for (std::uint32_t index = 0U; index < visualCount; ++index) {
        ::Aero::Media::Visual* child =
            Media::VisualTreeHelper::GetChild(node, index);
        if (child != nullptr) {
            Base::Result<void> appended = visualChildren.PushBack(child);
            if (!appended) return appended.GetStatus();
        }
    }
    for (std::uint32_t index = visualChildren.Size(); index > 0U; --index) {
        Base::Result<void> detached = DetachVisual(
            node, *visualChildren[index - 1U]);
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
    Base::Vector<::Aero::Media::Visual*> logicalChildren;
    const std::uint32_t logicalCount = LogicalTreeHelper::GetChildrenCount(node);
    reserved = logicalChildren.Reserve(logicalCount);
    if (!reserved) return reserved.GetStatus();
    for (std::uint32_t index = 0U; index < logicalCount; ++index) {
        ::Aero::Media::Visual* child = ::Aero::TryCast<::Aero::Media::Visual>(
            LogicalTreeHelper::GetChild(node, index));
        if (child != nullptr) {
            Base::Result<void> appended = logicalChildren.PushBack(child);
            if (!appended) return appended.GetStatus();
        }
    }
    for (std::uint32_t index = logicalChildren.Size(); index > 0U; --index) {
        ::Aero::Media::Visual* child = logicalChildren[index - 1U];
        if (child->GetLogicalParent() != &node) continue;
        Base::Result<void> detached = DetachLogical(node, *child);
        if (!detached) {
            return detached;
        }
    }
    if (node.logicalParent_ != nullptr) {
        ::Aero::Media::Visual* logicalParent =
            ::Aero::TryCast<::Aero::Media::Visual>(node.logicalParent_);
        if (logicalParent != nullptr) {
            return DetachLogical(*logicalParent, node);
        }
        node.logicalParent_ = nullptr;
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
        ::Aero::Media::Visual* node = record.node.Resolve();
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
    ::Aero::Media::Visual& parent, ::Aero::Media::Visual& child, bool& attached) noexcept {
    UIElement* parentElement = ::Aero::TryCast<::Aero::UIElement>(&(parent));
    UIElement* childElement = ::Aero::TryCast<::Aero::UIElement>(&(child));
    if (layout_ == nullptr || parentElement == nullptr || childElement == nullptr) {
        return {};
    }
    Base::Result<void> result = layout_->Attach(*parentElement, *childElement);
    if (!result) return result.GetStatus();
    attached = true;
    return {};
}

Base::Result<void> ElementTree::AttachRender(
    ::Aero::Media::Visual& parent, ::Aero::Media::Visual& child, bool& attached) noexcept {
    if (renderer_ == nullptr) {
        return {};
    }
    Base::Result<void> result = renderer_->Attach(parent, child);
    if (!result) return result.GetStatus();
    attached = true;
    return {};
}

Base::Result<void> ElementTree::DetachLayout(
    ::Aero::Media::Visual& parent, ::Aero::Media::Visual& child, bool& attached) noexcept {
    if (!attached) return {};
    UIElement* parentElement = ::Aero::TryCast<::Aero::UIElement>(&(parent));
    UIElement* childElement = ::Aero::TryCast<::Aero::UIElement>(&(child));
    if (layout_ == nullptr || parentElement == nullptr || childElement == nullptr) {
        return InvalidState("Attached layout edge has no layout service");
    }
    Base::Result<void> result = layout_->Detach(*parentElement, *childElement);
    if (!result) return result.GetStatus();
    attached = false;
    return {};
}

Base::Result<void> ElementTree::DetachRender(
    ::Aero::Media::Visual& parent, ::Aero::Media::Visual& child, bool& attached) noexcept {
    if (!attached) return {};
    if (renderer_ == nullptr) {
        return InvalidState("Attached render edge has no render tree");
    }
    Base::Result<void> result = renderer_->Detach(parent, child);
    if (!result) return result.GetStatus();
    attached = false;
    return {};
}

Base::Result<Aero::ElementAttachment> ElementTree::AttachElement(
    ::Aero::Media::Visual& logicalParent, ::Aero::Media::Visual& visualParent, ::Aero::Media::Visual& child) noexcept {
    if (&logicalParent == &child || &visualParent == &child) {
        return InvalidState("Element cannot be attached to itself");
    }

    Aero::ElementAttachment state;
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
    Aero::ElementAttachment& state) noexcept {
    if (!state.IsAttached()) return {};
    if (state.logicalParent == nullptr || state.visualParent == nullptr ||
        state.child == nullptr) {
        return InvalidState("Element attachment is incomplete");
    }

    ::Aero::Media::Visual& logicalParent = *state.logicalParent;
    ::Aero::Media::Visual& visualParent = *state.visualParent;
    ::Aero::Media::Visual& child = *state.child;

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
    Aero::ElementAttachment& state) noexcept {
    Aero::VisualAttachment visualState;
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
    Aero::ElementAttachment& state, ::Aero::Media::Visual& newVisualParent) noexcept {
    if (state.child == nullptr || state.visualAttached ||
        state.layoutAttached || state.renderAttached) {
        return InvalidState("Element attachment is not ready for a visual parent");
    }
    Base::Result<Aero::VisualAttachment> attached =
        AttachVisualChild(newVisualParent, *state.child);
    if (!attached) return attached.GetStatus();
    Aero::VisualAttachment visualState = std::move(attached).Value();
    state.visualParent = visualState.visualParent;
    state.visualAttached = visualState.visualAttached;
    state.layoutAttached = visualState.layoutAttached;
    state.renderAttached = visualState.renderAttached;
    return {};
}

Base::Result<Aero::VisualAttachment> ElementTree::AttachVisualChild(
    ::Aero::Media::Visual& visualParent, ::Aero::Media::Visual& child) noexcept {
    Aero::VisualAttachment state;
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
        auto attachDescendants = [&](auto&& self, ::Aero::Media::Visual& parent) noexcept
            -> Base::Result<void> {
            for (::Aero::Media::Visual* descendant :
                 AeroGuiInternal::RenderChildren(parent)) {
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
    Aero::VisualAttachment& state) noexcept {
    if (!state.IsAttached()) return {};
    if (state.visualParent == nullptr || state.child == nullptr) {
        return InvalidState("Visual attachment is incomplete");
    }
    ::Aero::Media::Visual& parent = *state.visualParent;
    ::Aero::Media::Visual& child = *state.child;

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

Base::Result<Aero::VisualAttachment> ElementTree::ReparentVisual(
    Aero::VisualAttachment& current, ::Aero::Media::Visual& newVisualParent) noexcept {
    if (current.child == nullptr || current.visualParent == nullptr) {
        return InvalidState("Visual reparent state is incomplete");
    }
    ::Aero::Media::Visual* oldParent = current.visualParent;
    ::Aero::Media::Visual* child = current.child;
    Aero::VisualAttachment oldState = current;

    Base::Result<void> detached = DetachVisual(current);
    if (!detached) return detached.GetStatus();

    Base::Result<Aero::VisualAttachment> attached =
        AttachVisualChild(newVisualParent, *child);
    if (attached) return attached;

    Base::Result<Aero::VisualAttachment> restored =
        AttachVisualChild(*oldParent, *child);
    if (restored) current = std::move(restored).Value();
    else current = oldState;
    return attached.GetStatus();
}

Base::Result<Aero::RootAttachment> ElementTree::AttachRoot(
    ::Aero::Media::Visual& root, Size availableSize) noexcept {
    Aero::RootAttachment state;
    state.root = &root;
    state.availableSize = availableSize;

    Base::Result<void> context = SetRoot(&root);
    if (!context) return context.GetStatus();
    state.contextAttached = true;

    UIElement* layoutRoot = ::Aero::TryCast<::Aero::UIElement>(&(root));
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
    Aero::RootAttachment& state) noexcept {
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
            if (::Aero::TryCast<::Aero::UIElement>(state.root) != nullptr && layout_ != nullptr) {
                Base::Result<void> restored =
                    layout_->SetRoot(::Aero::TryCast<::Aero::UIElement>(state.root), state.availableSize);
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

Base::Result<void> ElementTree::AttachVisualGraph(
    ::Aero::Media::Visual& visualRoot,
    Base::Span<Markup::VisualEdge> edges,
    Size availableSize,
    RootAttachment& outAttachment) noexcept {
    if (layout_ == nullptr || outAttachment.IsAttached() ||
        !IsValidLayoutSize(availableSize)) {
        return InvalidState(
            "Gui root cannot be attached in its current state");
    }
    AttachPresentation(layout_, renderer_);
    Base::Result<Aero::RootAttachment> rootAttached =
        AttachRoot(visualRoot, availableSize);
    if (!rootAttached) return rootAttached.GetStatus();
    outAttachment = std::move(rootAttached).Value();

    std::uint32_t attached = 0U;
    while (attached < edges.Size()) {
        bool progressed = false;
        for (Markup::VisualEdge& edge : edges) {
            if (edge.state.logicalAttached || edge.parent == nullptr ||
                edge.child == nullptr ||
                edge.parent->GetTree() != this) {
                continue;
            }
            Base::Result<Aero::ElementAttachment> edgeAttached =
                AttachElement(*edge.parent, *edge.child);
            if (!edgeAttached) {
                static_cast<void>(DetachVisualGraph(outAttachment, edges));
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

Base::Result<void> ElementTree::CompleteVisualEdges(
    Base::Span<Markup::VisualEdge> edges) noexcept {
    if (root_ == nullptr) {
        return InvalidState(
            "Deferred visual edges require an attached root");
    }
    std::uint32_t attached = 0U;
    for (const Markup::VisualEdge& edge : edges) {
        if (edge.state.logicalAttached) ++attached;
    }
    while (attached < edges.Size()) {
        bool progressed = false;
        for (Markup::VisualEdge& edge : edges) {
            if (edge.state.logicalAttached ||
                (edge.child != nullptr &&
                 edge.child->GetTree() == this) ||
                edge.parent == nullptr || edge.child == nullptr ||
                edge.parent->GetTree() != this) {
                continue;
            }
            Base::Result<Aero::ElementAttachment> edgeAttached =
                AttachElement(*edge.parent, *edge.child);
            if (!edgeAttached) return edgeAttached.GetStatus();
            edge.state = std::move(edgeAttached).Value();
            ++attached;
            progressed = true;
        }
        if (!progressed) break;
    }
    return {};
}

Base::Result<void> ElementTree::ResizeRoot(
    UIElement& layoutRoot,
    Size availableSize,
    ::Aero::Media::Visual* renderRoot) noexcept {
    if (layout_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "View resize requires an attached layout root");
    }
    if (!IsValidLayoutSize(availableSize)) {
        return InvalidArgument("View dimensions are invalid");
    }
    Base::Result<void> resized = layout_->SetRoot(&layoutRoot, availableSize);
    if (!resized) return resized.GetStatus();
    if (renderer_ != nullptr && renderRoot != nullptr) {
        return renderer_->Invalidate(
            *renderRoot, Aero::Render::RenderInvalidation::State);
    }
    return {};
}

Base::Result<void> ElementTree::DetachVisualGraph(
    RootAttachment& attachment,
    Base::Span<Markup::VisualEdge> edges) noexcept {
    if (!attachment.IsAttached() && root_ == nullptr) return {};

    const auto reconcileAttachment =
        [this](Markup::VisualEdge& edge) noexcept {
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

            Aero::UIElement* childElement = ::Aero::TryCast<::Aero::UIElement>(state.child);
            Aero::UIElement* parentElement =
                state.visualParent != nullptr
                ? ::Aero::TryCast<::Aero::UIElement>(state.visualParent)
                : nullptr;
            if (childElement != nullptr &&
                childElement->GetIsLayoutAttached() &&
                AeroGuiInternal::LayoutEngineOf(*childElement) == nullptr) {
                AeroGuiInternal::Layout(*childElement).layoutAttached = false;
                AeroGuiInternal::Layout(*childElement).measureQueued = false;
                AeroGuiInternal::Layout(*childElement).arrangeQueued = false;
            }
            state.layoutAttached =
                layout_ != nullptr && childElement != nullptr &&
                parentElement != nullptr &&
                childElement->GetIsLayoutAttached() &&
                AeroGuiInternal::LayoutEngineOf(*childElement) == layout_ &&
                childElement->LayoutParent() == parentElement;

            if (AeroGuiInternal::RenderAttached(*state.child) &&
                AeroGuiInternal::RenderRuntime(*state.child) == nullptr) {
                AeroGuiInternal::RenderAttached(*state.child) = false;
                AeroGuiInternal::RenderQueued(*state.child) = false;
                AeroGuiInternal::Rendering(*state.child) = false;
                AeroGuiInternal::NodeId(*state.child) = Base::InvalidRenderNodeId;
                AeroGuiInternal::RenderValid(*state.child) = false;
            }
            state.renderAttached =
                renderer_ != nullptr &&
                AeroGuiInternal::RenderAttached(*state.child) &&
                AeroGuiInternal::RenderRuntime(*state.child) == renderer_ &&
                AeroGuiInternal::RenderParent(*state.child) == state.visualParent;
        };

    std::uint32_t remaining = 0U;
    for (Markup::VisualEdge& edge : edges) {
        reconcileAttachment(edge);
        if (edge.state.IsAttached()) ++remaining;
    }
    while (remaining > 0U) {
        bool progressed = false;
        for (Markup::VisualEdge& edge : edges) {
            reconcileAttachment(edge);
            if (!edge.state.IsAttached()) continue;
            bool hasAttachedChild = false;
            for (const Markup::VisualEdge& candidate : edges) {
                if (candidate.state.IsAttached() &&
                    candidate.parent == edge.child) {
                    hasAttachedChild = true;
                    break;
                }
            }
            if (hasAttachedChild) continue;
            Base::Result<void> detached = DetachElement(edge.state);
            if (!detached) return detached.GetStatus();
            --remaining;
            progressed = true;
        }
        if (!progressed) {
            return InvalidState(
                "Visual edges cannot be detached leaf-first");
        }
    }

    Base::Result<void> rootDetached = DetachRoot(attachment);
    if (!rootDetached) return rootDetached.GetStatus();
    attachment = {};
    return {};
}


} // namespace Aero

namespace Aero {

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
        AeroGuiInternal::InvokeHandlers(
            static_cast<UIElement&>(node), args.GetRoutedEvent(), args);
        return;
    }
    if (catalog.Types().IsDerivedFrom(
            node.RuntimeType(), ContentElement::StaticTypeId())) {
        AeroGuiInternal::InvokeContentHandlers(
            static_cast<ContentElement&>(node), args.GetRoutedEvent(), args);
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

} // namespace Aero
