#pragma once

#include "VisualAccess.hpp"
#include <Aero/Threading.hpp>
#include "core/property/EffectiveValueEngine.hpp"

namespace Aero {

class AERO_API ObjectTree final {
public:
    ObjectTree(
        Core::Dispatcher& dispatcher,
        Core::EffectiveValueEngine& values) noexcept;
    ~ObjectTree() noexcept;

    ObjectTree(const ObjectTree&) = delete;
    ObjectTree& operator=(const ObjectTree&) = delete;

    Base::Result<void> Initialize() noexcept;
    Base::Result<void> SetRoot(Visual* root) noexcept;
    Visual* Root() const noexcept { return root_; }
    Base::Result<VisualHandle> GetHandle(
        const Visual& node) const noexcept;
    Visual* ResolveHandle(VisualHandle handle) const noexcept;

    Base::Result<void> AttachLogical(
        Visual& parent, Visual& child) noexcept;
    Base::Result<void> DetachLogical(
        Visual& parent, Visual& child) noexcept;
    Base::Result<void> AttachVisual(
        Visual& parent, Visual& child) noexcept;
    Base::Result<void> DetachVisual(
        Visual& parent, Visual& child) noexcept;
    Base::Result<void> DetachNode(Visual& node) noexcept;

    void SetLifecycleHandler(
        ObjectTreeLifecycleHandler handler,
        void* context = nullptr) noexcept {
        lifecycleHandler_ = handler;
        lifecycleContext_ = context;
    }

    std::uint64_t Version() const noexcept { return version_; }
    bool IsMutating() const noexcept { return mutating_; }
    std::uint32_t PendingLifecycleCount() const noexcept {
        return lifecycleQueue_.Size();
    }

private:
    struct LifecycleRecord final {
        Aero::Detail::VisualLease node;
        bool loaded = false;
        std::uint64_t sequence = 0U;
        std::uint64_t treeVersion = 0U;
    };
    struct HandleEntry final {
        Visual* node = nullptr;
        std::uint32_t generation = 1U;
    };

    Core::Dispatcher* dispatcher_ = nullptr;
    Core::EffectiveValueEngine* values_ = nullptr;
    Visual* root_ = nullptr;
    Base::Vector<LifecycleRecord> lifecycleQueue_;
    Base::Vector<HandleEntry> handles_;
    Core::DispatcherFrameHookHandle lifecycleHook_;
    ObjectTreeLifecycleHandler lifecycleHandler_ = nullptr;
    void* lifecycleContext_ = nullptr;
    DependencyPropertyChangedEventHandler dataContextChangedHandler_;
    std::uint64_t nextLifecycleSequence_ = 1U;
    std::uint64_t version_ = 0U;
    bool mutating_ = false;

    Base::Result<void> VerifyMutation(
        const Visual& first,
        const Visual* second = nullptr) const noexcept;
    bool IsLogicalAncestor(
        const Visual& possibleAncestor,
        const Visual& node) const noexcept;
    bool IsVisualAncestor(
        const Visual& possibleAncestor,
        const Visual& node) const noexcept;
    Base::Result<void> CollectLogicalSubtree(
        Visual& node,
        Base::Vector<Visual*>& nodes) noexcept;
    Base::Result<void> StageLifecycleSubtree(
        Visual& node,
        bool loaded,
        Base::Vector<LifecycleRecord>& staged) noexcept;
    void PublishLifecycle(
        Base::Vector<LifecycleRecord>& staged) noexcept;
    void ApplyLoadedSubtree(Visual& node, bool loaded) noexcept;
    void SetTreeSubtree(Visual& node, ObjectTree* tree) noexcept;
    Base::Result<std::uint32_t> FlushLifecycle() noexcept;
    Base::Result<void> RegisterHandleSubtree(Visual& node) noexcept;
    void InvalidateHandleSubtree(Visual& node) noexcept;
    Base::Result<void> TrackInheritedValues(Visual& node) noexcept;
    void UntrackInheritedValues(Visual& node) noexcept;
    void OnDataContextChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs& args) noexcept;
    void RemoveChild(Base::Vector<Visual*>& children, Visual& child) noexcept;
    static void LifecycleHook(void* context) noexcept;
};

} // namespace Aero
