#pragma once

namespace Aero {


class VisualLifetime : public Base::Object {
public:
    explicit VisualLifetime(::Aero::Media::Visual& node) noexcept : node_(&node) {}
    ~VisualLifetime() override = default;

    ::Aero::Media::Visual* Node() const noexcept { return node_; }
    void Invalidate() noexcept { node_ = nullptr; }

private:
    ::Aero::Media::Visual* node_ = nullptr;
};

struct VisualLease {
    Base::Ref<::Aero::Media::Visual> strong;
    Base::Ref<VisualLifetime> lifetime;

    static Base::Result<VisualLease> Acquire(::Aero::Media::Visual& node) noexcept;

    ::Aero::Media::Visual* Resolve() const noexcept {
        return strong
            ? strong.Get()
            : (lifetime ? lifetime->Node() : nullptr);
    }
};

} // namespace Aero

// Per-view Gui context and element attachment state.

#include <Aero/Threading.hpp>
#include <Aero/Layout.hpp>

namespace Aero::Render { class RenderTree; }

namespace Aero {

struct ElementAttachment {
    ::Aero::Media::Visual* logicalParent = nullptr;
    ::Aero::Media::Visual* visualParent = nullptr;
    ::Aero::Media::Visual* child = nullptr;
    VisualHandle childHandle;
    bool logicalAttached = false;
    bool visualAttached = false;
    bool layoutAttached = false;
    bool renderAttached = false;

    bool IsAttached() const noexcept {
        return logicalAttached || visualAttached || layoutAttached || renderAttached;
    }
};

struct VisualAttachment {
    ::Aero::Media::Visual* visualParent = nullptr;
    ::Aero::Media::Visual* child = nullptr;
    bool visualAttached = false;
    bool layoutAttached = false;
    bool renderAttached = false;

    bool IsAttached() const noexcept {
        return visualAttached || layoutAttached || renderAttached;
    }
};

struct RootAttachment {
    ::Aero::Media::Visual* root = nullptr;
    Size availableSize;
    bool contextAttached = false;
    bool layoutAttached = false;
    bool renderAttached = false;

    bool IsAttached() const noexcept {
        return contextAttached || layoutAttached || renderAttached;
    }
};

} // namespace Aero

namespace Aero {

class ElementTree {
public:
    ElementTree(
        ::Aero::Threading::Dispatcher& dispatcher,
        Meta::EffectiveValueEngine& values) noexcept;
    ~ElementTree() noexcept;

    ElementTree(const ElementTree&) = delete;
    ElementTree& operator=(const ElementTree&) = delete;

    Base::Result<void> Initialize() noexcept;
    Base::Result<void> SetRoot(::Aero::Media::Visual* root) noexcept;
    ::Aero::Media::Visual* Root() const noexcept { return root_; }
    Base::Result<VisualHandle> GetHandle(
        const ::Aero::Media::Visual& node) const noexcept;
    ::Aero::Media::Visual* ResolveHandle(VisualHandle handle) const noexcept;

    Base::Result<void> AttachLogical(
        ::Aero::Media::Visual& parent, ::Aero::Media::Visual& child) noexcept;
    Base::Result<void> DetachLogical(
        ::Aero::Media::Visual& parent, ::Aero::Media::Visual& child) noexcept;
    Base::Result<void> AttachVisual(
        ::Aero::Media::Visual& parent, ::Aero::Media::Visual& child) noexcept;
    Base::Result<void> DetachVisual(
        ::Aero::Media::Visual& parent, ::Aero::Media::Visual& child) noexcept;
    Base::Result<void> DetachNode(::Aero::Media::Visual& node) noexcept;

    void AttachPresentation(
        Aero::LayoutEngine* layout,
        ::Aero::Render::RenderTree* renderer) noexcept {
        layout_ = layout;
        renderer_ = renderer;
    }

    Base::Result<Aero::ElementAttachment> AttachElement(
        ::Aero::Media::Visual& parent, ::Aero::Media::Visual& child) noexcept {
        return AttachElement(parent, parent, child);
    }
    Base::Result<Aero::ElementAttachment> AttachElement(
        ::Aero::Media::Visual& logicalParent, ::Aero::Media::Visual& visualParent, ::Aero::Media::Visual& child) noexcept;
    Base::Result<void> DetachElement(
        Aero::ElementAttachment& state) noexcept;
    Base::Result<void> DetachVisual(
        Aero::ElementAttachment& state) noexcept;
    Base::Result<void> AttachVisual(
        Aero::ElementAttachment& state, ::Aero::Media::Visual& newVisualParent) noexcept;
    Base::Result<Aero::VisualAttachment> AttachVisualChild(
        ::Aero::Media::Visual& visualParent, ::Aero::Media::Visual& child) noexcept;
    Base::Result<void> DetachVisual(
        Aero::VisualAttachment& state) noexcept;
    Base::Result<Aero::VisualAttachment> ReparentVisual(
        Aero::VisualAttachment& current, ::Aero::Media::Visual& newVisualParent) noexcept;
    Base::Result<Aero::RootAttachment> AttachRoot(
        ::Aero::Media::Visual& root, Size availableSize) noexcept;
    Base::Result<void> DetachRoot(
        Aero::RootAttachment& state) noexcept;

    Aero::LayoutEngine* Layout() const noexcept { return layout_; }
    ::Aero::Render::RenderTree* Renderer() const noexcept { return renderer_; }
    void SetHost(Aero::ElementHost* host) noexcept {
        host_ = host;
    }
    Aero::ElementHost* Host() const noexcept {
        return host_;
    }

    void SetLifecycleHandler(
        ElementTreeLifecycleHandler handler,
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
    struct LifecycleRecord {
        Aero::VisualLease node;
        bool loaded = false;
        std::uint64_t sequence = 0U;
        std::uint64_t treeVersion = 0U;
    };
    struct HandleEntry {
        ::Aero::Media::Visual* node = nullptr;
        std::uint32_t generation = 1U;
    };

    ::Aero::Threading::Dispatcher* dispatcher_ = nullptr;
    Meta::EffectiveValueEngine* values_ = nullptr;
    Aero::LayoutEngine* layout_ = nullptr;
    ::Aero::Render::RenderTree* renderer_ = nullptr;
    Aero::ElementHost* host_ = nullptr;
    ::Aero::Media::Visual* root_ = nullptr;
    Base::Vector<LifecycleRecord> lifecycleQueue_;
    Base::Vector<HandleEntry> handles_;
    ::Aero::Threading::DispatcherFrameHookHandle lifecycleHook_;
    ElementTreeLifecycleHandler lifecycleHandler_ = nullptr;
    void* lifecycleContext_ = nullptr;
    DependencyPropertyChangedEventHandler dataContextChangedHandler_;
    std::uint64_t nextLifecycleSequence_ = 1U;
    std::uint64_t version_ = 0U;
    bool mutating_ = false;

    Base::Result<void> VerifyMutation(
        const ::Aero::Media::Visual& first,
        const ::Aero::Media::Visual* second = nullptr) const noexcept;
    bool IsLogicalAncestor(
        const ::Aero::Media::Visual& possibleAncestor,
        const ::Aero::Media::Visual& node) const noexcept;
    bool IsVisualAncestor(
        const ::Aero::Media::Visual& possibleAncestor,
        const ::Aero::Media::Visual& node) const noexcept;
    Base::Result<void> CollectLogicalSubtree(
        ::Aero::Media::Visual& node,
        Base::Vector<::Aero::Media::Visual*>& nodes) noexcept;
    Base::Result<void> StageLifecycleSubtree(
        ::Aero::Media::Visual& node,
        bool loaded,
        Base::Vector<LifecycleRecord>& staged) noexcept;
    void PublishLifecycle(
        Base::Vector<LifecycleRecord>& staged) noexcept;
    void ApplyLoadedSubtree(::Aero::Media::Visual& node, bool loaded) noexcept;
    void SetTreeSubtree(::Aero::Media::Visual& node, ElementTree* tree) noexcept;
    Base::Result<std::uint32_t> FlushLifecycle() noexcept;
    Base::Result<void> RegisterHandleSubtree(::Aero::Media::Visual& node) noexcept;
    void InvalidateHandleSubtree(::Aero::Media::Visual& node) noexcept;
    Base::Result<void> TrackInheritedValues(::Aero::Media::Visual& node) noexcept;
    void UntrackInheritedValues(::Aero::Media::Visual& node) noexcept;
    void OnDataContextChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs& args) noexcept;
    void RemoveChild(Base::Vector<::Aero::Media::Visual*>& children, ::Aero::Media::Visual& child) noexcept;
    Base::Result<void> AttachLayout(::Aero::Media::Visual& parent, ::Aero::Media::Visual& child, bool& attached) noexcept;
    Base::Result<void> AttachRender(::Aero::Media::Visual& parent, ::Aero::Media::Visual& child, bool& attached) noexcept;
    Base::Result<void> DetachLayout(::Aero::Media::Visual& parent, ::Aero::Media::Visual& child, bool& attached) noexcept;
    Base::Result<void> DetachRender(::Aero::Media::Visual& parent, ::Aero::Media::Visual& child, bool& attached) noexcept;
    static void LifecycleHook(void* context) noexcept;
};

} // namespace Aero

