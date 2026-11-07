#include "gui/core/TypeRegistryCore.hpp"
#include "gui/styles/StyleEngine.hpp"
#include "gui/data/BindingEngine.hpp"
#include "gui/markup/XamlSchema.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/PropertyStore.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include <Aero/Layout.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/FrameworkContentElement.hpp>
#include <Aero/LogicalTreeHelper.hpp>
#include <Aero/VisualTreeHelper.hpp>

#include "render/RenderTree.hpp"
#include "gui/ViewFrame.hpp"
#include "gui/controls/TextBlockLayout.hpp"
#include <Aero/TryCast.hpp>
#include <Aero/Controls/Buttons.hpp>
#include <Aero/Controls/Panels.hpp>
#include <Aero/Controls/Control.hpp>
#include <Aero/Controls/Decorators.hpp>
#include <Aero/Controls/Ranges.hpp>
#include <Aero/Controls/Popup.hpp>
#include <Aero/Collections.hpp>
#include <Aero/Freezable.hpp>
#include "gui/core/DependencyObjectAccess.hpp"
#include <atomic>
#include <mutex>

namespace Aero {

BindingEngine* ElementTree::BindingsOf(const DependencyObject& object) noexcept {
    const DependencyObject* current = &object;
    for (std::uint32_t depth = 0U; depth < 32U && current != nullptr; ++depth) {
        if (const Media::Visual* visual = TryCast<const Media::Visual>(current)) {
            if (ElementTree* tree = Of(*visual)) {
                return tree->Bindings();
            }
            if (const DependencyObject* logical = visual->GetLogicalParent()) {
                current = logical;
                continue;
            }
            if (const FrameworkElement* fe = TryCast<const FrameworkElement>(visual)) {
                if (const DependencyObject* templated = fe->GetTemplatedParent()) {
                    current = templated;
                    continue;
                }
            }
            current = visual->GetVisualParent();
            continue;
        }
        if (const Freezable* freezable = TryCast<const Freezable>(current)) {
            current = freezable->Parent();
            continue;
        }
        if (const DependencyObject* inherited = current->inheritanceParent_) {
            current = inherited;
            continue;
        }
        current = LogicalTreeHelper::GetParent(*current);
    }
    return nullptr;
}

Aero::LayoutEngine* ElementTree::Layout() const noexcept {
    return viewState_ != nullptr ? viewState_->Layout() : nullptr;
}
::Aero::Render::RenderTree* ElementTree::RenderTree() const noexcept {
    return viewState_ != nullptr ? viewState_->RenderTree() : nullptr;
}
Aero::BindingEngine* ElementTree::Bindings() const noexcept {
    return viewState_ != nullptr ? viewState_->Bindings() : nullptr;
}
Aero::StyleEngine* ElementTree::Styles() const noexcept {
    return viewState_ != nullptr ? viewState_->Styles() : nullptr;
}
Aero::EventRouter* ElementTree::Events() const noexcept {
    return viewState_ != nullptr ? viewState_->Events() : nullptr;
}
Aero::InputManager* ElementTree::Input() const noexcept {
    return viewState_ != nullptr ? viewState_->Input() : nullptr;
}
Aero::AnimationEngine* ElementTree::Animations() const noexcept {
    return viewState_ != nullptr ? viewState_->Animations() : nullptr;
}
VisualStateManager* ElementTree::VisualStates() const noexcept {
    return viewState_ != nullptr ? viewState_->VisualStates() : nullptr;
}
Controls::TemplateEngine* ElementTree::Templates() const noexcept {
    return viewState_ != nullptr ? viewState_->Templates() : nullptr;
}
Controls::TextBlockLayout* ElementTree::TextLayout() const noexcept {
    return viewState_ != nullptr && viewState_->Text() != nullptr ? viewState_->Text()->Layout() : nullptr;
}
Aero::Input::IClipboard* ElementTree::Clipboard() const noexcept {
    return viewState_ != nullptr ? viewState_->options.clipboard : nullptr;
}
Render::MeshResources* ElementTree::MeshResources() const noexcept {
    return viewState_ != nullptr ? viewState_->GetMeshResources() : nullptr;
}

using namespace Aero::Meta;
using namespace Aero::Threading;
namespace {

Base::Status TreeInvalidState(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

Base::Status TreeInvalidArgument(const char* message) noexcept {
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

Media::Visual::Visual(TypeId runtimeType) noexcept
    : DependencyObject(runtimeType) {
    AddCoreTypeFlag(Base::kCoreTypeVisual);
}

void Media::Visual::UpdateSubtreeDepth(std::uint16_t depth) noexcept {
    treeDepth_ = depth;
    const std::uint16_t nextDepth = depth < 0xFFFEU
        ? static_cast<std::uint16_t>(depth + 1U)
        : depth;
    for (Visual* cur = firstVisualChild_; cur != nullptr; cur = cur->nextVisualSibling_) {
        cur->UpdateSubtreeDepth(nextDepth);
    }
}

void Media::Visual::AddVisualChild(Visual* child) noexcept {
    if (child == nullptr || child == this) return;
    Visual* const oldParent = child->visualParent_;
    if (oldParent == this) return;
    if (oldParent != nullptr) return;
    child->visualParent_ = this;
    child->prevVisualSibling_ = nullptr;
    child->nextVisualSibling_ = firstVisualChild_;
    if (firstVisualChild_ != nullptr) {
        firstVisualChild_->prevVisualSibling_ = child;
    }
    firstVisualChild_ = child;
    child->UpdateSubtreeDepth(treeDepth_ < 0xFFFEU
        ? static_cast<std::uint16_t>(treeDepth_ + 1U)
        : treeDepth_);
    child->OnVisualParentChanged(oldParent);
    OnVisualChildrenChanged(child, nullptr);
    if (tree_ != nullptr && child->tree_ != tree_) {
        static_cast<void>(tree_->AttachVisualChild(*this, *child));
    }
}

void Media::Visual::RemoveVisualChild(Visual* child) noexcept {
    if (child == nullptr) return;
    Visual* const oldParent = child->visualParent_;
    if (oldParent == this) {
        if (child->prevVisualSibling_ != nullptr) {
            child->prevVisualSibling_->nextVisualSibling_ = child->nextVisualSibling_;
        } else if (firstVisualChild_ == child) {
            firstVisualChild_ = child->nextVisualSibling_;
        }
        if (child->nextVisualSibling_ != nullptr) {
            child->nextVisualSibling_->prevVisualSibling_ = child->prevVisualSibling_;
        }
        child->prevVisualSibling_ = nullptr;
        child->nextVisualSibling_ = nullptr;
        child->visualParent_ = nullptr;
        child->UpdateSubtreeDepth(0U);
        if (tree_ != nullptr && child->tree_ == tree_ && child->logicalParent_ == nullptr) {
            ElementTree::CleanVisualChildStorage(*this, *child);
            if (child->LoadedFlag()) {
                Base::Vector<ElementTree::LifecycleRecord> staged;
                if (tree_->StageLifecycleSubtree(*child, false, staged)) {
                    tree_->ApplyLoadedSubtree(*child, false);
                    tree_->InvalidateHandleSubtree(*child);
                    tree_->SetTreeSubtree(*child, nullptr);
                    tree_->PublishLifecycle(staged);
                }
            } else {
                tree_->InvalidateHandleSubtree(*child);
                tree_->SetTreeSubtree(*child, nullptr);
            }
        }
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

Media::PointHitTestResult Media::VisualTreeHelper::HitTest(
    const ::Aero::Media::Visual& reference,
    Base::Point point) noexcept {
    const UIElement* ui = ::Aero::TryCast<UIElement>(&reference);
    if (ui != nullptr) {
        if (!ui->GetIsVisible() || !ui->GetIsHitTestVisible()) {
            return Media::PointHitTestResult{};
        }
        const Size renderSize = ui->GetRenderSize();
        if (point.x < 0.0 || point.y < 0.0 || point.x >= renderSize.width || point.y >= renderSize.height) {
            if (ui->GetClipToBounds()) return Media::PointHitTestResult{};
        }
    }
    const std::uint32_t count = reference.GetVisualChildrenCount();
    for (std::uint32_t i = count; i > 0U; --i) {
        Visual* child = reference.GetVisualChild(i - 1U);
        if (child == nullptr) continue;
        const UIElement* childUi = ::Aero::TryCast<UIElement>(child);
        if (childUi != nullptr && (!childUi->GetIsVisible() || !childUi->GetIsHitTestVisible())) {
            continue;
        }
        Base::ProjectiveTransform2D transform;
        if (reference.TryTransformToVisual(*child, transform)) {
            Base::Point childPoint;
            if (Base::TryTransformPoint(transform, point, childPoint)) {
                Media::PointHitTestResult hit = HitTest(*child, childPoint);
                if (hit.HasHit()) return hit;
            }
        }
    }
    if (ui != nullptr) {
        const Size renderSize = ui->GetRenderSize();
        if (point.x < 0.0 || point.y < 0.0 || point.x >= renderSize.width || point.y >= renderSize.height) {
            return Media::PointHitTestResult{};
        }
    }
    return reference.HitTestCore(point);
}

DependencyObject* LogicalTreeHelper::GetParent(
    const DependencyObject& object) noexcept {
    if (const auto* visual = ::Aero::TryCast<Media::Visual>(&object)) {
        return visual->GetLogicalParent();
    }
    if (const auto* ce = ::Aero::TryCast<ContentElement>(&object)) {
        return ce->GetParent();
    }
    return nullptr;
}

std::uint32_t LogicalTreeHelper::GetChildrenCount(
    const DependencyObject& object) noexcept {
    if (const auto* fe = ::Aero::TryCast<FrameworkElement>(&object)) {
        return fe->GetLogicalChildrenCount();
    }
    if (::Aero::TryCast<ContentElement>(&object) != nullptr) {
        if (const auto* fce = ::Aero::TryCast<FrameworkContentElement>(&object)) {
            return fce->LogicalChildCount();
        }
    }
    return 0U;
}

DependencyObject* LogicalTreeHelper::GetChild(
    const DependencyObject& object,
    std::uint32_t index) noexcept {
    if (const auto* fe = ::Aero::TryCast<FrameworkElement>(&object)) {
        return fe->GetLogicalChild(index);
    }
    if (::Aero::TryCast<ContentElement>(&object) != nullptr) {
        if (const auto* fce = ::Aero::TryCast<FrameworkContentElement>(&object)) {
            return fce->LogicalChildAt(index);
        }
    }
    return nullptr;
}

namespace {

std::mutex g_activeTreesMutex;
ElementTree* g_activeTreesHead = nullptr;
std::atomic<std::uint32_t> g_activeTreeCount{0U};

} // namespace

void ElementTree::DetachPropertyDependencyGraph(
    DependencyObject& node,
    AnimationEngine* animations,
    Base::HashSet<DependencyObject*>& visited) noexcept {
    node.DetachPropertyDependencyObjects(Bindings(), values_, animations, visited);
}

bool ElementTree::HasActiveVisualHoldingObject(
    const DependencyObject& target,
    const Base::HashSet<DependencyObject*>& detachedSet) const noexcept {
    if (activeHandleCount_ == 0U) {
        return false;
    }
    const auto holdsTarget = [&target](const Meta::PropertyValue& val) noexcept {
        if (val.Kind() == Base::ValueKind::Object && !val.IsNullObject()) {
            const Base::Ref<Base::Object>& ref = val.AsObject();
            if (!ref) return false;
            if (ref.Get() == &target) return true;
            Collections::IItemsSource* itemsSource =
                ::Aero::TryCastToInterface<Collections::IItemsSource>(ref.Get());
            if (itemsSource == nullptr) {
                itemsSource = Collections::CollectionAsItemsSource(ref.Get());
            }
            if (itemsSource != nullptr) {
                const std::uint32_t count = itemsSource->GetCount();
                for (std::uint32_t i = 0U; i < count; ++i) {
                    Base::Ref<Base::Object> item = itemsSource->GetItem(i);
                    if (item.Get() == &target) {
                        return true;
                    }
                }
            }
        }
        return false;
    };
    for (const HandleEntry& entry : handles_) {
        const ::Aero::Media::Visual* node = entry.node;
        if (node == nullptr || node->tree_ != this) continue;
        if (detachedSet.Contains(const_cast<::Aero::Media::Visual*>(node))) continue;
        const PropertyStore* store = node->Store();
        if (store == nullptr) continue;
        for (const auto& rec : store->entries) {
            const StoredValueEntry& stored = rec.Value();
            if (!stored.SourceInfo().isInherited && holdsTarget(stored.effectiveValue)) {
                return true;
            }
            if (stored.rare != nullptr) {
                if (holdsTarget(stored.rare->localBackup) ||
                    holdsTarget(stored.rare->currentValue) ||
                    holdsTarget(stored.rare->animationValue)) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool ElementTree::HasActiveVisualOrBindingOnDispatcher(
    const ::Aero::Threading::Dispatcher& dispatcher,
    const DependencyObject& target,
    bool checkVisualProperties,
    const Base::HashSet<DependencyObject*>& detachedSet,
    const Aero::BindingEngine* alreadyChecked) noexcept {
    if (!checkVisualProperties && alreadyChecked != nullptr &&
        g_activeTreeCount.load(std::memory_order_acquire) <= 1U) {
        return false;
    }
    Base::BasicVector<ElementTree*, 4U> dispatcherTrees;
    {
        std::lock_guard<std::mutex> lock(g_activeTreesMutex);
        for (ElementTree* tree = g_activeTreesHead; tree != nullptr; tree = tree->nextActiveTree_) {
            if (tree->dispatcher_ == &dispatcher) {
                (void)dispatcherTrees.PushBack(tree);
            }
        }
    }
    for (const ElementTree* tree : dispatcherTrees) {
        if (const auto* treeBindings = tree->Bindings();
            treeBindings != nullptr &&
            treeBindings != alreadyChecked &&
            treeBindings->HasActiveBindingsOutside(target, detachedSet)) {
            return true;
        }
        if (checkVisualProperties &&
            tree->HasActiveVisualHoldingObject(target, detachedSet)) {
            return true;
        }
    }
    return false;
}

bool ElementTree::HasAnyBindingOnDispatcher(
    const ::Aero::Threading::Dispatcher& dispatcher,
    const DependencyObject& target,
    const Aero::BindingEngine* alreadyChecked) noexcept {
    if (alreadyChecked != nullptr &&
        g_activeTreeCount.load(std::memory_order_acquire) <= 1U) {
        return false;
    }
    Base::BasicVector<ElementTree*, 4U> dispatcherTrees;
    {
        std::lock_guard<std::mutex> lock(g_activeTreesMutex);
        for (ElementTree* tree = g_activeTreesHead; tree != nullptr; tree = tree->nextActiveTree_) {
            if (tree->dispatcher_ == &dispatcher) {
                (void)dispatcherTrees.PushBack(tree);
            }
        }
    }
    for (const ElementTree* tree : dispatcherTrees) {
        if (const auto* treeBindings = tree->Bindings();
            treeBindings != nullptr &&
            treeBindings != alreadyChecked &&
            treeBindings->HasAnyObjectReference(target)) {
            return true;
        }
    }
    return false;
}

void ElementTree::DetachObjectFromAllBindingsOnDispatcher(
    const ::Aero::Threading::Dispatcher& dispatcher,
    DependencyObject& target,
    const Aero::BindingEngine* alreadyDetached) noexcept {
    if (alreadyDetached != nullptr &&
        g_activeTreeCount.load(std::memory_order_acquire) <= 1U) {
        return;
    }
    Base::BasicVector<ElementTree*, 4U> dispatcherTrees;
    {
        std::lock_guard<std::mutex> lock(g_activeTreesMutex);
        for (ElementTree* tree = g_activeTreesHead; tree != nullptr; tree = tree->nextActiveTree_) {
            if (tree->dispatcher_ == &dispatcher) {
                (void)dispatcherTrees.PushBack(tree);
            }
        }
    }
    for (ElementTree* tree : dispatcherTrees) {
        if (auto* treeBindings = tree->Bindings();
            treeBindings != nullptr && treeBindings != alreadyDetached) {
            (void)treeBindings->DetachObject(target);
        }
    }
}

void ElementTree::InvalidateNodeHandle(::Aero::Media::Visual& node) noexcept {
    Base::HashSet<DependencyObject*> detachedProps;
    (void)detachedProps.Insert(&node);
    InvalidateNodeHandle(node, detachedProps);
}

void ElementTree::InvalidateNodeHandle(
    ::Aero::Media::Visual& node,
    Base::HashSet<DependencyObject*>& detachedProps) noexcept {
    const VisualHandle handle{node.handleIndex_, node.handleGeneration_};
    if (handle.IsValid() && handle.index < handles_.Size()) {
        HandleEntry& entry = handles_[handle.index];
        if (entry.node == &node) {
            entry.node = nullptr;
            ++entry.generation;
            if (entry.generation == 0U) ++entry.generation;
            if (activeHandleCount_ > 0U) --activeHandleCount_;
        }
    }
    if (activeRepeatButton_ == static_cast<void*>(&node)) {
        activeRepeatButton_ = nullptr;
        repeatElapsed_ = 0U;
        nextRepeat_ = 0U;
    }
    if (viewState_ != nullptr) {
        viewState_->UntrackVisualNode(node);
    }
    auto* curBindings = Bindings();
    if (curBindings != nullptr) {
        static_cast<void>(curBindings->DetachObject(node));
    }
    if (curBindings != nullptr || values_ != nullptr) {
        node.DetachPropertyDependencyObjects(
            curBindings, values_, Animations(), detachedProps);
    }
    if (auto* curStyles = Styles(); curStyles != nullptr) {
        static_cast<void>(curStyles->DetachObject(node));
    }
    if (values_ != nullptr) {
        static_cast<void>(values_->DetachObject(node));
    }
    node.handleIndex_ = UINT32_MAX;
    node.handleGeneration_ = 0U;
    if (node.tree_ == this) {
        node.tree_ = nullptr;
    }
}

void ElementTree::InvalidateNodeHandleFromDestructor(::Aero::Media::Visual& node) noexcept {
    const VisualHandle handle{node.handleIndex_, node.handleGeneration_};
    if (handle.IsValid() && handle.index < handles_.Size()) {
        HandleEntry& entry = handles_[handle.index];
        if (entry.node == &node) {
            entry.node = nullptr;
            ++entry.generation;
            if (entry.generation == 0U) ++entry.generation;
            if (activeHandleCount_ > 0U) --activeHandleCount_;
        }
    }
    if (activeRepeatButton_ == static_cast<void*>(&node)) {
        activeRepeatButton_ = nullptr;
        repeatElapsed_ = 0U;
        nextRepeat_ = 0U;
    }
    if (viewState_ != nullptr) {
        viewState_->UntrackVisualNode(node);
    }
    node.handleIndex_ = UINT32_MAX;
    node.handleGeneration_ = 0U;
    if (node.tree_ == this) {
        node.tree_ = nullptr;
    }
}

Media::Visual::~Visual() {
    // A discarded document may still own a parented visual subtree after the
    // ElementTree unmount walk. Tear the node out of tree/layout/render state
    // instead of asserting: WPF allows GC of a still-parented visual.
    if (tree_ != nullptr) {
        tree_->InvalidateNodeHandleFromDestructor(*this);
    }
    while (firstVisualChild_ != nullptr) {
        RemoveVisualChild(firstVisualChild_);
    }
    if (visualParent_ != nullptr) {
        visualParent_->RemoveVisualChild(this);
    }
    tree_ = nullptr;
    visualParent_ = nullptr;
    logicalParent_ = nullptr;
    visualFlags_ = static_cast<std::uint8_t>(
        visualFlags_ & static_cast<std::uint8_t>(
            ~(kFlagLoaded | kFlagRenderAttached | kFlagRenderQueued |
                kFlagRendering)));
    renderNodeId_ = Base::InvalidRenderNodeId;
    ExpireWeakReferences();
}


Base::Result<Aero::VisualLease> Aero::VisualLease::Acquire(
    ::Aero::Media::Visual& node) noexcept {
    VisualLease lease;
    lease.strong = Base::Ref<::Aero::Media::Visual>::TryFromBorrowed(node);
    if (lease.strong) return lease;

    if (!lease.weak.TryAttach(node)) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "VisualLease weak control block allocation failed");
    }
    return lease;
}

ElementTree::ElementTree(
    Dispatcher& dispatcher,
    EffectiveValueEngine& values) noexcept
    : dispatcher_(&dispatcher),
      values_(&values),
      lifecycleQueue_(),
      handles_() {
    std::lock_guard<std::mutex> lock(g_activeTreesMutex);
    nextActiveTree_ = g_activeTreesHead;
    if (g_activeTreesHead != nullptr) {
        g_activeTreesHead->prevActiveTree_ = this;
    }
    g_activeTreesHead = this;
    g_activeTreeCount.fetch_add(1U, std::memory_order_release);
}

ElementTree::~ElementTree() noexcept {
    {
        std::lock_guard<std::mutex> lock(g_activeTreesMutex);
        if (prevActiveTree_ != nullptr) {
            prevActiveTree_->nextActiveTree_ = nextActiveTree_;
        } else if (g_activeTreesHead == this) {
            g_activeTreesHead = nextActiveTree_;
        }
        if (nextActiveTree_ != nullptr) {
            nextActiveTree_->prevActiveTree_ = prevActiveTree_;
        }
        prevActiveTree_ = nullptr;
        nextActiveTree_ = nullptr;
        g_activeTreeCount.fetch_sub(1U, std::memory_order_release);
    }
    // P3.2: no frame-hook registration; nothing to unregister.
    if (root_ != nullptr && dispatcher_->CheckAccess()) {
        (void)SetRoot(nullptr);
    }
    lifecycleQueue_.Clear();
    handles_.Clear();
    activeHandleCount_ = 0U;
}

Base::Result<void> ElementTree::Initialize() noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) {
        return access;
    }
    if (initialized_) {
        return {};
    }
    // P3.2: ViewFrame drives LifecycleHook() directly; no Lifecycle hook.
    initialized_ = true;
    return {};
}

Base::Result<VisualHandle> ElementTree::GetHandle(
    const ::Aero::Media::Visual& node) const noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    VisualHandle handle{node.handleIndex_, node.handleGeneration_};
    if (node.tree_ == this && !handle.IsValid()) {
        static_cast<void>(const_cast<ElementTree*>(this)->RegisterHandleSubtree(
            const_cast<::Aero::Media::Visual&>(node)));
        handle = VisualHandle{node.handleIndex_, node.handleGeneration_};
    }
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

namespace {

// ContentControl / Window logical children are Content, while the applied
// ControlTemplate lives only on the visual tree. Unmount, Loaded, and handle
// walks must visit both, or template visuals keep tree_/layout/Loaded state
// and abort in ~UIElement / ~Visual.
template<class Fn>
void ForEachElementTreeChild(
    ::Aero::Media::Visual& node, Fn&& fn) noexcept {
    const std::uint32_t visualCount =
        ::Aero::Media::VisualTreeHelper::GetChildrenCount(node);
    for (std::uint32_t index = 0U; index < visualCount; ++index) {
        ::Aero::Media::Visual* child =
            ::Aero::Media::VisualTreeHelper::GetChild(node, index);
        if (child != nullptr) {
            fn(*child);
        }
    }
    const std::uint32_t logicalCount =
        LogicalTreeHelper::GetChildrenCount(node);
    for (std::uint32_t index = 0U; index < logicalCount; ++index) {
        ::Aero::Media::Visual* child = ::Aero::TryCast<::Aero::Media::Visual>(
            LogicalTreeHelper::GetChild(node, index));
        if (child != nullptr && child->GetVisualParent() != &node) {
            fn(*child);
        }
    }
}

/* ClearDetachedPresentation moved to ElementTree:: */

} // namespace

Base::Result<void> ElementTree::CollectLogicalSubtree(
    ::Aero::Media::Visual& node,
    Base::Vector<::Aero::Media::Visual*>& nodes) noexcept {
    for (::Aero::Media::Visual* existing : nodes) {
        if (existing == &node) {
            return {};
        }
    }
    nodes.PushBack(&node);
    Base::Result<void> childStatus{};
    ForEachElementTreeChild(node, [&](::Aero::Media::Visual& child) noexcept {
        if (!childStatus) {
            return;
        }
        childStatus = CollectLogicalSubtree(child, nodes);
    });
    return childStatus;
}

Base::Result<void> ElementTree::RegisterHandleSubtree(::Aero::Media::Visual& node) noexcept {
    Base::Vector<::Aero::Media::Visual*> nodes;
    // P1 perf: attach walks allocate once instead of per-PushBack growth.
    nodes.Reserve(32U);
    Base::Result<void> collected = CollectLogicalSubtree(node, nodes);
    if (!collected) return collected.GetStatus();

    std::uint32_t required = 0U;
    for (::Aero::Media::Visual* current : nodes) {
        const VisualHandle currentHandle{current->handleIndex_, current->handleGeneration_};
        if (currentHandle.IsValid()) {
            if (currentHandle.index >= handles_.Size()) {
                return TreeInvalidState("ElementTree node has an invalid pre-existing handle");
            }
            const HandleEntry& entry = handles_[currentHandle.index];
            if (entry.node != current ||
                entry.generation != currentHandle.generation) {
                return TreeInvalidState("ElementTree node has a stale pre-existing handle");
            }
        } else {
            ++required;
        }
    }

    handles_.Reserve(handles_.Size() + required);

    for (::Aero::Media::Visual* current : nodes) {
        if (VisualHandle{current->handleIndex_, current->handleGeneration_}.IsValid()) continue;

        HandleEntry entry;
        entry.node = current;
        handles_.PushBack(entry);
        ++activeHandleCount_;
        current->handleIndex_ = handles_.Size() - 1U;
        current->handleGeneration_ = entry.generation;
    }
    return {};
}

void ElementTree::InvalidateHandleSubtree(::Aero::Media::Visual& node) noexcept {
    Base::HashSet<DependencyObject*> detachedProps;
    const auto collectSubtree = [&](const auto& self, ::Aero::Media::Visual& cur) noexcept -> void {
        if (!detachedProps.Insert(&cur).inserted) {
            return;
        }
        const VisualHandle handle{cur.handleIndex_, cur.handleGeneration_};
        if (handle.IsValid() && handle.index < handles_.Size()) {
            HandleEntry& entry = handles_[handle.index];
            if (entry.node == &cur) {
                entry.node = nullptr;
                ++entry.generation;
                if (entry.generation == 0U) ++entry.generation;
                if (activeHandleCount_ > 0U) --activeHandleCount_;
            }
        }
        ForEachElementTreeChild(cur, [&](::Aero::Media::Visual& child) noexcept {
            self(self, child);
        });
    };
    collectSubtree(collectSubtree, node);

    const auto invalidateSubtree = [&](const auto& self, ::Aero::Media::Visual& cur) noexcept -> void {
        ForEachElementTreeChild(cur, [&](::Aero::Media::Visual& child) noexcept {
            self(self, child);
        });
        InvalidateNodeHandle(cur, detachedProps);
    };
    invalidateSubtree(invalidateSubtree, node);
}


Base::Result<void> ElementTree::VerifyMutation(
    const ::Aero::Media::Visual& first,
    const ::Aero::Media::Visual* second) const noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) {
        return access;
    }
    if (!initialized_) {
        return TreeInvalidState("ElementTree must be initialized before mutation");
    }
    if (mutating_) {
        return TreeInvalidState("Nested object-tree mutation is not allowed");
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
    for (const LifecycleRecord& record : staged) {
        if (record.node.Resolve() == &node) {
            return {};
        }
    }
    if (node.LoadedFlag() != loaded) {
        Base::Result<Aero::VisualLease> lease =
            Aero::VisualLease::Acquire(node);
        if (!lease) return lease.GetStatus();

        LifecycleRecord record;
        record.node = std::move(lease).Value();
        record.loaded = loaded;
        staged.PushBack(std::move(record));
    }
    Base::Result<void> childStatus{};
    ForEachElementTreeChild(node, [&](::Aero::Media::Visual& child) noexcept {
        if (!childStatus) {
            return;
        }
        childStatus = StageLifecycleSubtree(child, loaded, staged);
    });
    return childStatus;
}

void ElementTree::PublishLifecycle(
    Base::Vector<LifecycleRecord>& staged) noexcept {
    for (LifecycleRecord& record : staged) {
        record.sequence = nextLifecycleSequence_++;
        record.treeVersion = version_;
        lifecycleQueue_.PushBack(std::move(record));
    }
    staged.Clear();
}

void ElementTree::ApplyLoadedSubtree(::Aero::Media::Visual& node, bool loaded) noexcept {
    if (node.LoadedFlag() == loaded) {
        return;
    }
    node.SetLoadedFlag(loaded);
    ForEachElementTreeChild(node, [this, loaded](::Aero::Media::Visual& child) noexcept {
        ApplyLoadedSubtree(child, loaded);
    });
}

void ElementTree::ClearDetachedPresentation(::Aero::Media::Visual& node) noexcept {
    UIElement* element = ::Aero::TryCast<UIElement>(&node);
    if (element != nullptr) {
        element->Layout().SetFlag(UIElement::LayoutFlags::LayoutAttached, false);
        element->Layout().SetFlag(UIElement::LayoutFlags::MeasureQueued, false);
        element->Layout().SetFlag(UIElement::LayoutFlags::ArrangeQueued, false);
    }
    (node).RenderAttached() = false;
    (node).RenderQueued() = false;
    (node).Rendering() = false;
    (node).NodeId() = Base::InvalidRenderNodeId;
}

void ElementTree::SetTreeSubtree(
    ::Aero::Media::Visual& node, ElementTree* tree) noexcept {
    if (node.tree_ == tree) {
        return;
    }
    node.tree_ = tree;
    if (tree == nullptr) {
        ClearDetachedPresentation(node);
    } else {
        if (auto* curBindings = Bindings(); curBindings != nullptr) {
            (void)curBindings->ActivateDeferredWhenReady(node);
        }
        if (viewState_ != nullptr) {
            if (auto* ic = ::Aero::TryCast<Controls::ItemsControl>(&node)) {
                viewState_->TrackItemsControl(*ic);
            }
        }
    }
    ForEachElementTreeChild(node, [this, tree, &node](::Aero::Media::Visual& child) noexcept {
        if (tree != nullptr) {
            DependencyObject* inheritParent =
                child.GetLogicalParent() != nullptr ? child.GetLogicalParent() : &node;
            (void)values_->SetInheritanceParent(child, inheritParent);
            if (child.GetVisualParent() == &node) {
                bool attachedLayout = false;
                (void)AttachLayout(node, child, attachedLayout);
                bool attachedRender = false;
                (void)AttachRender(node, child, attachedRender);
            }
        }
        SetTreeSubtree(child, tree);
    });
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
        return TreeInvalidState("ElementTree root must be fully detached");
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
    lifecycleQueue_.Reserve(
        lifecycleQueue_.Size() + staged.Size());

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
    if (parent.tree_ != this) {
        return TreeInvalidState(
            "Logical child must be detached and parent must belong to this tree");
    }
    if (child.tree_ == this) {
        if (child.logicalParent_ != nullptr &&
            child.logicalParent_ != &parent) {
            return TreeInvalidState(
                "Logical child must be detached and parent must belong to this tree");
        }
        Base::Vector<LifecycleRecord> staged;
        if (parent.LoadedFlag() && !child.LoadedFlag()) {
            Base::Result<void> prepared =
                StageLifecycleSubtree(child, true, staged);
            if (!prepared) return prepared.GetStatus();
            lifecycleQueue_.Reserve(
                lifecycleQueue_.Size() + staged.Size());
        }
        child.logicalParent_ = &parent;
        Base::Result<void> registered = RegisterHandleSubtree(child);
        if (!registered) return registered.GetStatus();
        Base::Result<void> inherited =
            values_->SetInheritanceParent(child, &parent);
        if (!inherited) {
            return inherited.GetStatus();
        }
        if (parent.LoadedFlag() && !child.LoadedFlag()) {
            ApplyLoadedSubtree(child, true);
            PublishLifecycle(staged);
        }
        return {};
    }
    if (child.logicalParent_ != nullptr || child.tree_ != nullptr) {
        return TreeInvalidState(
            "Logical child must be detached and parent must belong to this tree");
    }

    Base::Vector<LifecycleRecord> staged;
    if (parent.LoadedFlag()) {
        Base::Result<void> prepared =
            StageLifecycleSubtree(child, true, staged);
        if (!prepared) return prepared.GetStatus();
    }
    lifecycleQueue_.Reserve(
        lifecycleQueue_.Size() + staged.Size());

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
    Markup::NotifyDynamicResourceScopeChanged(child);
    SetTreeSubtree(child, this);
    ++version_;
    if (parent.LoadedFlag()) ApplyLoadedSubtree(child, true);
    PublishLifecycle(staged);
    mutating_ = false;
    return {};
}

Base::Result<void> ElementTree::DetachLogical(
    ::Aero::Media::Visual& parent,
    ::Aero::Media::Visual& child) noexcept {
    if (child.logicalParent_ == &parent && child.tree_ == nullptr) {
        child.logicalParent_ = nullptr;
        return {};
    }
    if (child.logicalParent_ == nullptr) {
        return {};
    }
    Base::Result<void> verified = VerifyMutation(parent, &child);
    if (!verified) return verified.GetStatus();
    if (child.logicalParent_ != &parent || child.tree_ != this) {
        return NotFound("Logical parent-child relationship was not found");
    }

    Base::Vector<LifecycleRecord> staged;
    if (child.LoadedFlag()) {
        Base::Result<void> prepared =
            StageLifecycleSubtree(child, false, staged);
        if (!prepared) return prepared.GetStatus();
    }
    lifecycleQueue_.Reserve(
        lifecycleQueue_.Size() + staged.Size());

    Base::Result<void> inherited =
        values_->SetInheritanceParent(child, nullptr);
    if (!inherited) {
        return inherited.GetStatus();
    }
    child.logicalParent_ = nullptr;

    mutating_ = true;
    if (child.LoadedFlag()) ApplyLoadedSubtree(child, false);
    InvalidateHandleSubtree(child);
    SetTreeSubtree(child, nullptr);
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
        if (parent.tree_ == this && child.tree_ != this) {
            static_cast<void>(RegisterHandleSubtree(child));
            SetTreeSubtree(child, this);
        }
        if (child.tree_ == this && values_ != nullptr) {
            (void)values_->SetInheritanceParent(
                child,
                child.logicalParent_ != nullptr ? child.logicalParent_ : &parent);
        }
        ElementTree::AttachVisualControlTemplateRoot(parent, child);
        ++version_;
        return {};
    }
    if ((parent.tree_ != nullptr && parent.tree_ != this) ||
        (child.tree_ != nullptr && child.tree_ != this)) {
        return TreeInvalidState("Visual nodes must be logical members of this tree");
    }
    if (child.visualParent_ != nullptr && child.visualParent_ != &parent) {
        child.visualParent_->RemoveVisualChild(&child);
    }
    ElementTree::EnsureVisualChildStorage(parent, child);
    if (child.visualParent_ != &parent) {
        parent.AddVisualChild(&child);
    }
    if (parent.tree_ == this && child.tree_ != this) {
        static_cast<void>(RegisterHandleSubtree(child));
        SetTreeSubtree(child, this);
    }
    if (child.tree_ == this && values_ != nullptr) {
        (void)values_->SetInheritanceParent(
            child,
            child.logicalParent_ != nullptr ? child.logicalParent_ : &parent);
    }
    ElementTree::AttachVisualControlTemplateRoot(parent, child);
    ++version_;
    return {};
}

Base::Result<void> ElementTree::DetachVisual(
    ::Aero::Media::Visual& parent,
    ::Aero::Media::Visual& child) noexcept {
    if (child.visualParent_ == nullptr || child.visualParent_ != &parent) {
        return {};
    }
    if (child.tree_ == nullptr && parent.tree_ == nullptr) {
        parent.RemoveVisualChild(&child);
        ElementTree::CleanVisualChildStorage(parent, child);
        return {};
    }
    Base::Result<void> verified = VerifyMutation(parent, &child);
    if (!verified) {
        return verified;
    }
    if ((child.tree_ != nullptr && child.tree_ != this) ||
        (parent.tree_ != nullptr && parent.tree_ != this)) {
        return NotFound("Visual parent-child relationship was not found");
    }
    if (child.tree_ == this && values_ != nullptr) {
        (void)values_->SetInheritanceParent(child, child.logicalParent_);
    }
    parent.RemoveVisualChild(&child);
    ElementTree::CleanVisualChildStorage(parent, child);
    if (child.LoadedFlag() && child.logicalParent_ == nullptr) {
        Base::Vector<LifecycleRecord> staged;
        if (StageLifecycleSubtree(child, false, staged)) {
            ApplyLoadedSubtree(child, false);
            InvalidateHandleSubtree(child);
            SetTreeSubtree(child, nullptr);
            PublishLifecycle(staged);
        }
    }
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
    visualChildren.Reserve(visualCount);
    for (std::uint32_t index = 0U; index < visualCount; ++index) {
        ::Aero::Media::Visual* child =
            Media::VisualTreeHelper::GetChild(node, index);
        if (child != nullptr) {
            visualChildren.PushBack(child);
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
    logicalChildren.Reserve(logicalCount);
    for (std::uint32_t index = 0U; index < logicalCount; ++index) {
        ::Aero::Media::Visual* child = ::Aero::TryCast<::Aero::Media::Visual>(
            LogicalTreeHelper::GetChild(node, index));
        if (child != nullptr) {
            logicalChildren.PushBack(child);
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

std::uint32_t ElementTree::FlushLifecycle() noexcept {
    if (lifecycleQueue_.Empty() || !dispatcher_->CheckAccess()) return 0U;

    std::uint32_t count = 0U;
    {
        Base::Vector<LifecycleRecord> snapshot = std::move(lifecycleQueue_);
        lifecycleQueue_.Clear();

        for (std::uint32_t i = 0U; i < snapshot.Size(); ++i) {
            const LifecycleRecord& record = snapshot[i];
            ::Aero::Media::Visual* node = record.node.Resolve();
            if (node == nullptr) continue;
            if (lifecycleHandler_ != nullptr) {
                const ElementTreeLifecycleEvent event{
                    node, record.loaded, record.treeVersion};
                lifecycleHandler_(event, lifecycleContext_);
            }
            if (auto* curEvents = Events(); curEvents != nullptr) {
                if (auto* fe = ::Aero::TryCast<FrameworkElement>(node)) {
                    RoutedEventArgs args;
                    if (record.loaded) {
                        static_cast<void>(curEvents->RaiseEvent(
                            *fe, FrameworkElement::LoadedEvent.Handle(), &args));
                    } else {
                        static_cast<void>(curEvents->RaiseEvent(
                            *fe, FrameworkElement::UnloadedEvent.Handle(), &args));
                    }
                }
            }
            ++count;
        }
    }
    return count;
}

void ElementTree::LifecycleHook(void* context) noexcept {
    ElementTree* tree = static_cast<ElementTree*>(context);
    tree->FlushLifecycle();
}

std::uint32_t ElementTree::AdvanceRepeatButtonTime(
    std::uint32_t elapsedMilliseconds) noexcept {
    auto* button = static_cast<Controls::Primitives::RepeatButton*>(activeRepeatButton_);
    return button != nullptr
        ? button->AdvanceTime(elapsedMilliseconds, repeatElapsed_, nextRepeat_)
        : 0U;
}


Base::Result<void> ElementTree::AttachLayout(
    ::Aero::Media::Visual& parent, ::Aero::Media::Visual& child, bool& attached) noexcept {
    UIElement* parentElement = ::Aero::TryCast<::Aero::UIElement>(&(parent));
    UIElement* childElement = ::Aero::TryCast<::Aero::UIElement>(&(child));
    auto* curLayout = Layout();
    if (curLayout == nullptr || parentElement == nullptr || childElement == nullptr) {
        return {};
    }
    Base::Result<void> result = curLayout->Attach(*parentElement, *childElement);
    if (!result) return result.GetStatus();
    attached = true;
    return {};
}

Base::Result<void> ElementTree::AttachRender(
    ::Aero::Media::Visual& parent, ::Aero::Media::Visual& child, bool& attached) noexcept {
    auto* curRenderTree = RenderTree();
    if (curRenderTree == nullptr ||
        ElementTree::RenderTreeOf(parent) != static_cast<void*>(curRenderTree)) {
        return {};
    }
    Base::Result<void> result = curRenderTree->Attach(parent, child);
    if (!result) return result.GetStatus();
    attached = true;
    return {};
}

Base::Result<void> ElementTree::DetachLayout(
    ::Aero::Media::Visual& parent, ::Aero::Media::Visual& child, bool& attached) noexcept {
    if (!attached) return {};
    UIElement* parentElement = ::Aero::TryCast<::Aero::UIElement>(&(parent));
    UIElement* childElement = ::Aero::TryCast<::Aero::UIElement>(&(child));
    auto* curLayout = Layout();
    if (curLayout == nullptr || parentElement == nullptr || childElement == nullptr) {
        return TreeInvalidState("Attached layout edge has no layout service");
    }
    Base::Result<void> result = curLayout->Detach(*parentElement, *childElement);
    if (!result) return result.GetStatus();
    attached = false;
    return {};
}

Base::Result<void> ElementTree::DetachRender(
    ::Aero::Media::Visual& parent, ::Aero::Media::Visual& child, bool& attached) noexcept {
    if (!attached) return {};
    auto* curRenderTree = RenderTree();
    if (curRenderTree == nullptr) {
        return TreeInvalidState("Attached render edge has no render tree");
    }
    Base::Result<void> result = curRenderTree->Detach(parent, child);
    if (!result) return result.GetStatus();
    attached = false;
    return {};
}

Base::Result<Aero::ElementAttachment> ElementTree::AttachElement(
    ::Aero::Media::Visual& logicalParent, ::Aero::Media::Visual& visualParent, ::Aero::Media::Visual& child) noexcept {
    if (&logicalParent == &child || &visualParent == &child) {
        return TreeInvalidState("Element cannot be attached to itself");
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
    if (auto* curRenderTree = RenderTree(); state.renderAttached && curRenderTree != nullptr) {
        auto attachDescendants = [&](auto&& self, ::Aero::Media::Visual& parent) noexcept
            -> Base::Result<void> {
            for (::Aero::Media::Visual* descendant :
                 (parent).RenderChildren()) {
                if (descendant == nullptr) continue;
                if (!(*descendant).RenderAttached()) {
                    Base::Result<void> attached = curRenderTree->Attach(parent, *descendant);
                    if (!attached) return attached.GetStatus();
                }
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
            (void)DetachLogical(logicalParent, child);
            state.visualAttached = false;
            return descendants.GetStatus();
        }
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
        return TreeInvalidState("Element attachment is incomplete");
    }

    Base::Ref<::Aero::Media::Visual> childKeepAlive =
        Base::Ref<::Aero::Media::Visual>::FromBorrowed(*state.child);
    Base::Ref<::Aero::Media::Visual> parentKeepAlive =
        Base::Ref<::Aero::Media::Visual>::FromBorrowed(*state.visualParent);

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
        return TreeInvalidState("Element attachment is not ready for a visual parent");
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
        // Keep the visual parent. Rolling back returned UserControl content
        // to Window, so ColorSelector ArrangeOverride never hosted LayoutRoot.
        return layout.GetStatus();
    }
    Base::Result<void> render = AttachRender(visualParent, child, state.renderAttached);
    if (!render) {
        return render.GetStatus();
    }
    if (auto* curRenderTree = RenderTree(); state.renderAttached && curRenderTree != nullptr) {
        auto attachDescendants = [&](auto&& self, ::Aero::Media::Visual& parent) noexcept
            -> Base::Result<void> {
            for (::Aero::Media::Visual* descendant :
                 (parent).RenderChildren()) {
                if (descendant == nullptr) continue;
                Base::Result<void> attached = curRenderTree->Attach(parent, *descendant);
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
        return TreeInvalidState("Visual attachment is incomplete");
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
        return TreeInvalidState("Visual reparent state is incomplete");
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
    auto* curLayout = Layout();
    if (layoutRoot != nullptr) {
        if (curLayout == nullptr) {
            (void)SetRoot(nullptr);
            return TreeInvalidState("UI root requires a layout service");
        }
        Base::Result<void> layout = curLayout->SetRoot(layoutRoot, availableSize);
        if (!layout) {
            (void)SetRoot(nullptr);
            return layout.GetStatus();
        }
        state.layoutAttached = true;
    }

    auto* curRenderTree = RenderTree();
    if (curRenderTree != nullptr) {
        Base::Result<void> render = curRenderTree->SetRoot(&root);
        if (!render) {
            if (state.layoutAttached && curLayout != nullptr) (void)curLayout->SetRoot(nullptr, {});
            (void)SetRoot(nullptr);
            return render.GetStatus();
        }
        state.renderAttached = true;
    }
    if (state.renderAttached) {
        auto syncExistingVisualDescendants = [&](auto&& self, ::Aero::Media::Visual& parent) noexcept -> void {
            const std::uint32_t count = ::Aero::Media::VisualTreeHelper::GetChildrenCount(parent);
            for (std::uint32_t i = 0U; i < count; ++i) {
                ::Aero::Media::Visual* child = ::Aero::Media::VisualTreeHelper::GetChild(parent, i);
                if (child == nullptr) continue;
                if (child->GetVisualParent() == &parent) {
                    UIElement* childElement = ::Aero::TryCast<UIElement>(child);
                    if (curLayout != nullptr && childElement != nullptr && !childElement->GetIsLayoutAttached()) {
                        bool attachedLayout = false;
                        (void)AttachLayout(parent, *child, attachedLayout);
                    }
                    if (curRenderTree != nullptr && !(*child).RenderAttached()) {
                        bool attachedRender = false;
                        (void)AttachRender(parent, *child, attachedRender);
                    }
                }
                self(self, *child);
            }
        };
        syncExistingVisualDescendants(syncExistingVisualDescendants, root);
    }
    return state;
}

Base::Result<void> ElementTree::DetachRoot(
    Aero::RootAttachment& state) noexcept {
    if (!state.IsAttached()) return {};
    if (state.root == nullptr) return TreeInvalidState("Root attachment is incomplete");

    auto* curRenderTree = RenderTree();
    auto* curLayout = Layout();
    if (state.renderAttached) {
        if (curRenderTree != nullptr) {
            Base::Result<void> result = curRenderTree->SetRoot(nullptr);
            if (!result) return result.GetStatus();
        }
        state.renderAttached = false;
    }
    if (state.layoutAttached) {
        if (curLayout != nullptr) {
            Base::Result<void> result = curLayout->SetRoot(nullptr, {});
            if (!result) {
                if (curRenderTree != nullptr) {
                    Base::Result<void> restored =
                        curRenderTree->SetRoot(state.root);
                    if (restored) state.renderAttached = true;
                }
                return result.GetStatus();
            }
        }
        state.layoutAttached = false;
    }
    if (state.contextAttached) {
        Base::Result<void> result = SetRoot(nullptr);
        if (!result) {
            if (::Aero::TryCast<::Aero::UIElement>(state.root) != nullptr && curLayout != nullptr) {
                Base::Result<void> restored =
                    curLayout->SetRoot(::Aero::TryCast<::Aero::UIElement>(state.root), state.availableSize);
                if (restored) state.layoutAttached = true;
            }
            if (curRenderTree != nullptr) {
                Base::Result<void> restored =
                    curRenderTree->SetRoot(state.root);
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
    if (Layout() == nullptr || outAttachment.IsAttached() ||
        !IsValidLayoutSize(availableSize)) {
        return TreeInvalidState(
            "Gui root cannot be attached in its current state");
    }
    Base::Result<Aero::RootAttachment> rootAttached =
        AttachRoot(visualRoot, availableSize);
    if (!rootAttached) return rootAttached.GetStatus();
    outAttachment = std::move(rootAttached).Value();


    Base::Result<void> completed = CompleteVisualEdges(edges);
    if (!completed) {
        static_cast<void>(DetachVisualGraph(outAttachment, edges));
        return completed.GetStatus();
    }
    return {};
}

Base::Result<void> ElementTree::CompleteVisualEdges(
    Base::Span<Markup::VisualEdge> edges) noexcept {
    if (root_ == nullptr) {
        return TreeInvalidState(
            "Deferred visual edges require an attached root");
    }
    std::uint32_t attached = 0U;
    for (const Markup::VisualEdge& edge : edges) {
        if (edge.state.logicalAttached ||
            edge.state.layoutAttached ||
            edge.parent == nullptr ||
            edge.child == nullptr) {
            ++attached;
        }
    }
    while (attached < edges.Size()) {
        bool progressed = false;
        for (Markup::VisualEdge& edge : edges) {
            if (edge.state.logicalAttached ||
                edge.state.layoutAttached ||
                edge.parent == nullptr || edge.child == nullptr ||
                VisualTree(edge.parent) != this) {
                continue;
            }
            ::Aero::Media::Visual* visualParent = edge.parent;
            if (edge.child->GetVisualParent() != nullptr &&
                VisualTree(edge.child->GetVisualParent()) == this) {
                visualParent = edge.child->GetVisualParent();
            }
            auto* curRenderTree = RenderTree();
            if (curRenderTree != nullptr &&
                ElementTree::RenderTreeOf(*visualParent) !=
                    static_cast<void*>(curRenderTree) &&
                visualParent != root_) {
                continue;
            }
            Base::Result<Aero::ElementAttachment> edgeAttached =
                AttachElement(*edge.parent, *visualParent, *edge.child);
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
    auto* curLayout = Layout();
    if (curLayout == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "View resize requires an attached layout root");
    }
    if (!IsValidLayoutSize(availableSize)) {
        return TreeInvalidArgument("View dimensions are invalid");
    }
    Base::Result<void> resized = curLayout->SetRoot(&layoutRoot, availableSize);
    if (!resized) return resized.GetStatus();
    auto* curRenderTree = RenderTree();
    if (curRenderTree != nullptr && renderRoot != nullptr) {
        curRenderTree->Invalidate(
            *renderRoot, Aero::Render::RenderInvalidation::State);
    }
    return {};
}

Base::Result<void> ElementTree::DetachVisualEdges(
    Base::Span<Markup::VisualEdge> edges) noexcept {
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
                ElementTree::LayoutOf(*childElement) == nullptr) {
                childElement->Layout().SetFlag(UIElement::LayoutFlags::LayoutAttached, false);
                childElement->Layout().SetFlag(UIElement::LayoutFlags::MeasureQueued, false);
                childElement->Layout().SetFlag(UIElement::LayoutFlags::ArrangeQueued, false);
            }
            auto* curLayout = Layout();
            state.layoutAttached =
                curLayout != nullptr && childElement != nullptr &&
                parentElement != nullptr &&
                childElement->GetIsLayoutAttached() &&
                ElementTree::LayoutOf(*childElement) == curLayout &&
                childElement->LayoutParent() == parentElement;

            if ((*state.child).RenderAttached() &&
                ElementTree::RenderTreeOf(*state.child) == nullptr) {
                (*state.child).RenderAttached() = false;
                (*state.child).RenderQueued() = false;
                (*state.child).Rendering() = false;
                (*state.child).NodeId() = Base::InvalidRenderNodeId;
                (*state.child).RenderValid() = false;
            }
            auto* curRenderTree = RenderTree();
            state.renderAttached =
                curRenderTree != nullptr &&
                (*state.child).RenderAttached() &&
                ElementTree::RenderTreeOf(*state.child) == curRenderTree &&
                (*state.child).GetVisualParent() == state.visualParent;
        };

    // Fast path: reverse topological detachment O(E).
    // Edges are collected in preorder during XAML instantiation;
    // traversing in reverse detaches child leaf nodes before parents.
    for (std::uint32_t i = edges.Size(); i > 0U; --i) {
        Markup::VisualEdge& edge = edges[i - 1U];
        reconcileAttachment(edge);
        if (!edge.state.IsAttached()) continue;
        Base::Result<void> detached = DetachElement(edge.state);
        if (!detached && detached.GetStatus().code != Base::ErrorCode::NotFound) {
            return detached.GetStatus();
        }
        edge.state = {};
    }

    std::uint32_t remaining = 0U;
    for (Markup::VisualEdge& edge : edges) {
        reconcileAttachment(edge);
        if (edge.state.IsAttached()) ++remaining;
    }
    if (remaining > 0U) {
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
                if (!detached && detached.GetStatus().code != Base::ErrorCode::NotFound) {
                    return detached.GetStatus();
                }
                edge.state = {};
                --remaining;
                progressed = true;
            }
            if (!progressed) {
                break;
            }
        }
    }
    return {};
}

Base::Result<void> ElementTree::DetachVisualGraph(
    RootAttachment& attachment,
    Base::Span<Markup::VisualEdge> edges) noexcept {
    if (!attachment.IsAttached() && root_ == nullptr) return {};
    Base::Result<void> detachedEdges = DetachVisualEdges(edges);
    if (!detachedEdges) return detachedEdges.GetStatus();
    Base::Result<void> rootDetached = DetachRoot(attachment);
    if (!rootDetached) return rootDetached.GetStatus();
    attachment = {};
    return {};
}


void ElementTree::EnsureVisualChildStorage(
    Media::Visual& parent,
    Media::Visual& child) noexcept {
    UIElement* childElement = ::Aero::TryCast<::Aero::UIElement>(&child);
    if (childElement == nullptr) return;
    const Meta::TypeRegistry& types = DependencyObjectAccess::PropertyRegistry((parent)).Types();
    if (types.IsDerivedFrom(
            parent.RuntimeType(), Controls::Panel::StaticTypeId())) {
        auto& panel = static_cast<Controls::Panel&>(parent);
        const std::uint32_t count = (panel).ChildCountCore();
        for (std::uint32_t index = 0U; index < count; ++index) {
            if ((panel).ChildAtCore( index).Get() == childElement) {
                return;
            }
        }
        Base::Ref<Base::Object> borrowed =
            Base::Ref<Base::Object>::FromBorrowed(*childElement);
        (panel).AddChildCore( borrowed, *childElement);
        return;
    }
    if (types.IsDerivedFrom(
            parent.RuntimeType(), Controls::ContentPresenter::StaticTypeId())) {
        auto& presenter = static_cast<Controls::ContentPresenter&>(parent);
        if (presenter.GetContent() == nullptr) {
            presenter.SetContent(childElement);
        }
        return;
    }
    if (types.IsDerivedFrom(
            parent.RuntimeType(), Controls::ContentControl::StaticTypeId())) {
        auto& control = static_cast<Controls::ContentControl&>(parent);
        if (childElement == control.GetFallbackPresenter()) {
            return;
        }
        UIElement* existing =
            (control).GetContentElement();
        if (existing == childElement) {
            return;
        }
        const Base::Ref<Controls::ControlTemplate> templ =
            control.GetValue(Controls::Control::TemplateProperty);
        if (existing == nullptr &&
            (control).GetTemplateRoot() == nullptr &&
            !templ) {
            control.SetContent(childElement);
        }
        return;
    }
    if (types.IsDerivedFrom(
            parent.RuntimeType(), Controls::Primitives::Popup::StaticTypeId())) {
        auto& popup = static_cast<Controls::Primitives::Popup&>(parent);
        if (popup.GetChild() == nullptr || popup.GetChild() == childElement) {
            popup.SetChild(childElement);
        }
        return;
    }
    if (types.IsDerivedFrom(
            parent.RuntimeType(), Controls::Decorator::StaticTypeId())) {
        auto& decorator = static_cast<Controls::Decorator&>(parent);
        if (decorator.GetChild() == nullptr || decorator.GetChild() == childElement) {
            decorator.SetChild(childElement);
        }
        return;
    }
    if (types.IsDerivedFrom(
            parent.RuntimeType(), Controls::BulletDecorator::StaticTypeId())) {
        auto& bullet = static_cast<Controls::BulletDecorator&>(parent);
        if (bullet.GetChild() == childElement ||
            bullet.GetBullet() == childElement) {
            return;
        }
        Base::Ref<UIElement> borrowed =
            Base::Ref<UIElement>::FromBorrowed(*childElement);
        if (bullet.GetChild() == nullptr) {
            bullet.SetChild(std::move(borrowed));
            return;
        }
        if (bullet.GetBullet() == nullptr) {
            bullet.SetBullet(std::move(borrowed));
        }
        return;
    }
    return;
}

void ElementTree::AttachVisualControlTemplateRoot(
    Media::Visual& parent,
    Media::Visual& child) noexcept {
    if (DependencyObjectAccess::PropertyRegistry(parent).Types().IsDerivedFrom(
            parent.RuntimeType(), Controls::ContentControl::StaticTypeId())) {
        return;
    }
    if (DependencyObjectAccess::PropertyRegistry(parent).Types().IsDerivedFrom(
            parent.RuntimeType(), Controls::Primitives::Track::StaticTypeId())) {
        return;
    }
    if (DependencyObjectAccess::PropertyRegistry(parent).Types().IsDerivedFrom(
            parent.RuntimeType(), Controls::Control::StaticTypeId()) &&
        ::Aero::TryCast<::Aero::UIElement>(&child) != nullptr) {
        auto& control = static_cast<Controls::Control&>(parent);
        if (control.GetTemplateRoot() == nullptr) {
            control.SetTemplateChildCore(::Aero::TryCast<::Aero::UIElement>(&child));
        }
    }
}

void ElementTree::CleanVisualChildStorage(
    Media::Visual& parent,
    Media::Visual& child) noexcept {
    if (UIElement* childElement = ::Aero::TryCast<::Aero::UIElement>(&child)) {
        if (DependencyObjectAccess::PropertyRegistry((parent)).Types().IsDerivedFrom(
                parent.RuntimeType(), Controls::Panel::StaticTypeId())) {
            auto& panel = static_cast<Controls::Panel&>(parent);
            (void)(panel).RemoveChildCore( *childElement);
        } else if (DependencyObjectAccess::PropertyRegistry((parent)).Types().IsDerivedFrom(
                parent.RuntimeType(), Controls::Primitives::Popup::StaticTypeId())) {
            auto& popup = static_cast<Controls::Primitives::Popup&>(parent);
            if (popup.GetChild() == childElement) {
                popup.SetChild(nullptr);
            }
        }
    }
}

} // namespace Aero

namespace Aero {

using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero;

EventRouter::EventRouter(
    void* eventState) noexcept
    : eventState_(eventState),
      classHandlersByEvent_() {}

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
        return TreeInvalidState(
            "RoutedEventTable must be frozen before handlers");
    }
    const RoutedEventTable::Definition* definition =
        catalog.Find(event);
    if (definition == nullptr ||
        catalog.Types().FindType(classType) == nullptr ||
        definition->eventArgsType != eventArgsType) {
        return TreeInvalidArgument(
            "Class handler registration is invalid");
    }
    return {};
}

void EventRouter::InvokeNode(
    DependencyObject& node,
    RoutedEventArgs& args) noexcept {
    RoutedEventTable& catalog = Events(eventState_);
    if (const Base::Vector<ClassHandlerRecord>* handlers =
            classHandlersByEvent_.Find(args.GetRoutedEvent().value)) {
        for (const ClassHandlerRecord& record : *handlers) {
            if (catalog.Types().IsDerivedFrom(
                    node.RuntimeType(), record.classType) &&
                (!args.GetHandled() || record.handledEventsToo)) {
                record.handler.Invoke(&node, args);
            }
        }
    }

    if (catalog.Types().IsDerivedFrom(
            node.RuntimeType(), UIElement::StaticTypeId())) {
        (static_cast<UIElement&>(node)).InvokeHandlers( args.GetRoutedEvent(), args);
        return;
    }
    if (catalog.Types().IsDerivedFrom(
            node.RuntimeType(), ContentElement::StaticTypeId())) {
        (static_cast<ContentElement&>(node)).InvokeHandlers( args.GetRoutedEvent(), args);
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
        return TreeInvalidState(
            "RoutedEventTable must be frozen before dispatch");
    }
    if (raiseDepth_ == 64U) {
        return TreeInvalidState(
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
        return TreeInvalidArgument(
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
    classHandlersByEvent_.Clear();
}

} // namespace Aero
