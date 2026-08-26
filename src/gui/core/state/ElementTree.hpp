#pragma once

#include "gui/core/VisualHandle.hpp"

#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>

namespace Aero {

namespace Markup { struct VisualEdge; }


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
#include <Aero/Visual.hpp>
#include <Aero/UIElement.hpp>

namespace Aero::Render { class RenderTree; struct MeshResources; }
namespace Aero::Meta { class EffectiveValueEngine; }

namespace Aero {

class LayoutEngine;
class BindingEngine;
class StyleEngine;
class EventRouter;
class InputRouter;
class AnimationEngine;
class VisualStateManager;
namespace Controls {
class TemplateEngine;
class TextBlockLayout;
class ControlBehavior;
} // namespace Controls

struct ElementTreeLifecycleEvent {
    ::Aero::Media::Visual* node = nullptr;
    bool loaded = false;
    std::uint64_t treeVersion = 0U;
};

using ElementTreeLifecycleHandler = void (*)(
    const ElementTreeLifecycleEvent& event,
    void* context) noexcept;

constexpr std::uint32_t InvalidIndex = UINT32_MAX;

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
    Base::Result<void> AttachVisualGraph(
        ::Aero::Media::Visual& visualRoot,
        Base::Span<Markup::VisualEdge> edges,
        Size availableSize,
        RootAttachment& outAttachment) noexcept;
    Base::Result<void> CompleteVisualEdges(
        Base::Span<Markup::VisualEdge> edges) noexcept;
    Base::Result<void> DetachVisualGraph(
        RootAttachment& attachment,
        Base::Span<Markup::VisualEdge> edges) noexcept;
    Base::Result<void> ResizeRoot(
        UIElement& layoutRoot,
        Size availableSize,
        ::Aero::Media::Visual* renderRoot) noexcept;

    Aero::LayoutEngine* Layout() const noexcept { return layout_; }
    ::Aero::Render::RenderTree* Renderer() const noexcept { return renderer_; }
    Aero::BindingEngine* Bindings() const noexcept { return bindings_; }
    Aero::StyleEngine* Styles() const noexcept { return styles_; }
    Aero::EventRouter* Events() const noexcept { return events_; }
    Aero::InputRouter* Input() const noexcept { return input_; }
    Aero::AnimationEngine* Animations() const noexcept { return animations_; }
    VisualStateManager* VisualStates() const noexcept { return visualStates_; }
    Controls::TemplateEngine* Templates() const noexcept { return templates_; }
    Controls::TextBlockLayout* TextLayout() const noexcept { return textLayout_; }
    Controls::ControlBehavior* ControlBehaviors() const noexcept {
        return controlBehaviors_;
    }
    Render::MeshResources* MeshResources() const noexcept {
        return meshResources_;
    }

    void SetLayout(Aero::LayoutEngine* layout) noexcept { layout_ = layout; }
    void SetRenderer(::Aero::Render::RenderTree* renderer) noexcept {
        renderer_ = renderer;
    }
    void SetBindings(Aero::BindingEngine* bindings) noexcept {
        bindings_ = bindings;
    }
    void SetStyles(Aero::StyleEngine* styles) noexcept { styles_ = styles; }
    void SetEvents(Aero::EventRouter* events) noexcept { events_ = events; }
    void SetInput(Aero::InputRouter* input) noexcept { input_ = input; }
    void SetAnimations(Aero::AnimationEngine* animations) noexcept {
        animations_ = animations;
    }
    void SetVisualStates(VisualStateManager* visualStates) noexcept {
        visualStates_ = visualStates;
    }
    void SetTemplates(Controls::TemplateEngine* templates) noexcept {
        templates_ = templates;
    }
    void SetTextLayout(Controls::TextBlockLayout* textLayout) noexcept {
        textLayout_ = textLayout;
    }
    void SetControlBehaviors(Controls::ControlBehavior* behaviors) noexcept {
        controlBehaviors_ = behaviors;
    }
    void SetMeshResources(Render::MeshResources* resources) noexcept {
        meshResources_ = resources;
    }

    using FindNameFn = Base::Object* (*)(
        void* context,
        Base::StringView name,
        Meta::TypeId expectedType) noexcept;
    void SetNameScope(void* context, FindNameFn findName) noexcept {
        nameScopeContext_ = context;
        findName_ = findName;
    }
    Base::Object* FindName(
        Base::StringView name,
        Meta::TypeId expectedType = Meta::InvalidTypeId) const noexcept {
        return findName_ != nullptr
            ? findName_(nameScopeContext_, name, expectedType)
            : nullptr;
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
    Aero::BindingEngine* bindings_ = nullptr;
    Aero::StyleEngine* styles_ = nullptr;
    Aero::EventRouter* events_ = nullptr;
    Aero::InputRouter* input_ = nullptr;
    Aero::AnimationEngine* animations_ = nullptr;
    VisualStateManager* visualStates_ = nullptr;
    Controls::TemplateEngine* templates_ = nullptr;
    Controls::TextBlockLayout* textLayout_ = nullptr;
    Controls::ControlBehavior* controlBehaviors_ = nullptr;
    Render::MeshResources* meshResources_ = nullptr;
    void* nameScopeContext_ = nullptr;
    FindNameFn findName_ = nullptr;
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

