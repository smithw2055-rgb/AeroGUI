#pragma once

// Private element state and direct Gui runtime declarations.

#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Events/RoutedEvent.hpp>
#include <Aero/Visual.hpp>
#include <Aero/ContentElement.hpp>
#include <Aero/UIElement.hpp>
#include <Aero/FrameworkElement.hpp>

#include <cstdint>
#include <utility>

namespace Aero::Controls {
class Control;
class Decorator;
class MenuItem;
class Panel;
class TreeViewItem;
}

namespace Aero { class VisualStateManager; }

namespace Aero::Controls::Primitives { class Selector; }

namespace Aero::Shapes { class Path; }

namespace Aero::GuiPrivate::Detail {

class EventRouter;
class InputRouter;
class LayoutEngine;
class BindingEngine;
class AnimationEngine;
class StyleEngine;

struct ElementHost {
    EventRouter* events = nullptr;
    InputRouter* input = nullptr;
    void* templates = nullptr;
    void* visualStates = nullptr;
    void* textLayout = nullptr;
    void* controlBehaviors = nullptr;
    void* meshResources = nullptr;
    void* nameScopeContext = nullptr;
    Base::Object* (*findName)(
        void*, Base::StringView, Meta::TypeId) noexcept = nullptr;
};

} // namespace Aero::GuiPrivate::Detail

namespace Aero {

struct VisualHandle {
    std::uint32_t index = UINT32_MAX;
    std::uint32_t generation = 0U;

    constexpr bool IsValid() const noexcept {
        return index != UINT32_MAX && generation != 0U;
    }
};

struct ElementTreeLifecycleEvent {
    Visual* node = nullptr;
    bool loaded = false;
    std::uint64_t treeVersion = 0U;
};

using ElementTreeLifecycleHandler = void (*)(
    const ElementTreeLifecycleEvent& event,
    void* context) noexcept;

} // namespace Aero

namespace Aero {

using namespace ::Aero::GuiPrivate::Detail;

// One private entry point owns all element implementation state. Public WPF
// classes friend this type instead of exposing one Access class per base type.
struct Visual::Impl {
public:
    static void SetViewServices(
        Aero::UIElement& element,
        ElementHost*) noexcept {
        // View services are owned once by ElementTree. Per-element service
        // pointers are deliberately absent from the public object layout.
        static_cast<void>(element);
    }

    static ElementHost* Host(const Visual& visual) noexcept;

    static EventRouter* EventRouterFor(
        const Aero::UIElement& element) noexcept {
        ElementHost* services = Host(element);
        return services != nullptr ? services->events : nullptr;
    }

    static InputRouter* InputRouterFor(
        const Aero::UIElement& element) noexcept {
        ElementHost* services = Host(element);
        return services != nullptr ? services->input : nullptr;
    }

    static Base::Object* FindName(
        const Aero::UIElement& element,
        Base::StringView name,
        Meta::TypeId expectedType = Meta::InvalidTypeId) noexcept {
        ElementHost* services = Host(element);
        return services != nullptr && services->findName != nullptr
            ? services->findName(
                  services->nameScopeContext,
                  name,
                  expectedType)
            : nullptr;
    }

    static Base::Result<void> SetMouseOver(
        Aero::UIElement& element,
        bool value) noexcept;
    static Base::Result<void> SetPressed(
        Aero::UIElement& element,
        bool value) noexcept;
    static Base::Result<void> SetKeyboardFocused(
        Aero::UIElement& element,
        bool value) noexcept;
    static Base::Result<void> SetKeyboardFocusWithin(
        Aero::UIElement& element,
        bool value) noexcept;
    static void InvokeHandlers(
        Aero::UIElement& element,
        RoutedEventHandle event,
        RoutedEventArgs& args) noexcept;
    static void InvokeContentHandlers(
        Aero::ContentElement& element,
        RoutedEventHandle event,
        RoutedEventArgs& args) noexcept {
        element.InvokeHandlers(event, args);
    }
    static std::uint32_t PanelChildCount(
        const Aero::Controls::Panel& panel) noexcept;
    static Base::Ref<Base::Object> PanelChildAt(
        const Aero::Controls::Panel& panel,
        std::uint32_t index) noexcept;
    static Base::Result<void> PanelAddChild(
        Aero::Controls::Panel& panel,
        const Base::Ref<Base::Object>& owner,
        Aero::UIElement& child) noexcept;
    static Base::Result<bool> PanelRemoveChild(
        Aero::Controls::Panel& panel,
        Aero::UIElement& child) noexcept;
    static void PanelClearChildren(
        Aero::Controls::Panel& panel) noexcept;
    static const Base::Ref<Base::Object>& DecoratorOwnedChild(
        const Aero::Controls::Decorator& decorator) noexcept;
    static Base::Result<void> DecoratorSetOwnedChild(
        Aero::Controls::Decorator& decorator,
        const Base::Ref<Base::Object>& owner,
        Aero::UIElement& child) noexcept;
    static void PathInvalidateGeometry(
        Aero::Shapes::Path& path) noexcept;
    static void PathAttachMeshResources(
        Aero::Shapes::Path& path,
        void* services,
        bool invalidate) noexcept;
    static void SetMenuItemHighlighted(
        Aero::Controls::MenuItem& item,
        bool value) noexcept;
    static void SyncSelectorContainers(
        Aero::Controls::Primitives::Selector& selector) noexcept;
    static std::uint32_t TreeViewItemCount(
        const Aero::Controls::TreeViewItem& item) noexcept;

    static ElementTree* Tree(const Visual& visual) noexcept {
        return visual.tree_;
    }
    static Visual* LogicalParent(const Visual& visual) noexcept {
        return visual.logicalParent_;
    }
    static Visual* VisualParent(const Visual& visual) noexcept {
        return visual.visualParent_;
    }
    static Base::Span<Visual* const> LogicalChildren(
        const Visual& visual) noexcept {
        return {
            visual.logicalChildren_.Data(),
            visual.logicalChildren_.Size()};
    }
    static Base::Span<Visual* const> VisualChildren(
        const Visual& visual) noexcept {
        return {
            visual.visualChildren_.Data(),
            visual.visualChildren_.Size()};
    }
    static bool IsLoaded(const Visual& visual) noexcept {
        return visual.loaded_;
    }
    static VisualHandle Handle(const Visual& visual) noexcept {
        return {visual.handleIndex_, visual.handleGeneration_};
    }
    static void SetHandle(
        Visual& visual,
        VisualHandle handle) noexcept {
        visual.handleIndex_ = handle.index;
        visual.handleGeneration_ = handle.generation;
    }
    static UIElement* AsUIElement(Visual& visual) noexcept {
        return visual.AsUIElement();
    }
    static const UIElement* AsUIElement(
        const Visual& visual) noexcept {
        return visual.AsUIElement();
    }
    static FrameworkElement* AsFrameworkElement(
        Visual& visual) noexcept {
        return visual.AsFrameworkElement();
    }
    static const FrameworkElement* AsFrameworkElement(
        const Visual& visual) noexcept {
        return visual.AsFrameworkElement();
    }
    static Base::Result<Base::Ref<Base::Object>> AcquireLifetime(
        Visual& visual) noexcept {
        return visual.AcquireLifetime();
    }

    static Visual* RenderParent(
        const Visual& visual) noexcept {
        return visual.visualParent_;
    }
    static Base::Span<Visual* const> RenderChildren(
        const Visual& visual) noexcept {
        return {
            visual.visualChildren_.Data(),
            visual.visualChildren_.Size()};
    }
    static void* RenderRuntime(const Visual& visual) noexcept;
    static void* TemplateRuntime(const Visual& visual) noexcept {
        ElementHost* host = Host(visual);
        return host != nullptr ? host->templates : nullptr;
    }
    static void* VisualStateRuntime(const Visual& visual) noexcept {
        ElementHost* host = Host(visual);
        return host != nullptr ? host->visualStates : nullptr;
    }
    static void* TextLayoutRuntime(const Visual& visual) noexcept {
        ElementHost* host = Host(visual);
        return host != nullptr ? host->textLayout : nullptr;
    }
    static void* ControlBehaviorRuntime(
        const Visual& visual) noexcept {
        ElementHost* host = Host(visual);
        return host != nullptr ? host->controlBehaviors : nullptr;
    }
    static void* MeshResourcesRuntime(
        const Visual& visual) noexcept {
        ElementHost* host = Host(visual);
        return host != nullptr ? host->meshResources : nullptr;
    }
    static bool& RenderAttached(
        Visual& visual) noexcept {
        return visual.renderAttached_;
    }
    static Base::RenderNodeId& NodeId(
        Visual& visual) noexcept {
        return visual.renderNodeId_;
    }
    static bool& RenderValid(
        Visual& visual) noexcept {
        return visual.renderValid_;
    }
    static bool& RenderQueued(
        Visual& visual) noexcept {
        return visual.renderQueued_;
    }
    static std::uint64_t& RenderRevision(
        Visual& visual) noexcept {
        return visual.renderRevision_;
    }
    static bool& Rendering(
        Visual& visual) noexcept {
        return visual.rendering_;
    }
    static std::uint8_t& RenderDirtyFlags(
        Visual& visual) noexcept {
        return visual.renderDirtyFlags_;
    }
    static void Render(
        Visual& visual,
        DrawingContext& context) noexcept {
        FrameworkElement* element =
            visual.AsFrameworkElement();
        if (element != nullptr) {
            element->OnRender(context);
        }
    }
    static Base::Result<void> InvalidateRenderDrawing(
        Visual& visual) noexcept;
    static Base::Result<void> InvalidateRenderState(
        Visual& visual) noexcept;
    static Base::Result<void> SetTemplatedParent(
        FrameworkElement& element,
        DependencyObject* value) noexcept {
        element.SetTemplatedParent(value);
        return {};
    }
    static Base::Result<void> AddAuthoredTrigger(
        FrameworkElement& element,
        Base::Ref<Base::Object> trigger) noexcept {
        return element.AddAuthoredTrigger(std::move(trigger));
    }
    static Base::Result<void> ClearAuthoredTriggers(
        FrameworkElement& element) noexcept {
        element.ClearAuthoredTriggers();
        return {};
    }
    static Base::Span<const Base::Ref<Base::Object>> AuthoredTriggers(
        const FrameworkElement& element) noexcept {
        return element.AuthoredTriggers();
    }
    static Base::Result<void> AddAuthoredBehavior(
        FrameworkElement& element,
        Base::Ref<Base::Object> behavior) noexcept {
        return element.AddAuthoredBehavior(std::move(behavior));
    }
    static Base::Result<void> ClearAuthoredBehaviors(
        FrameworkElement& element) noexcept {
        element.ClearAuthoredBehaviors();
        return {};
    }
    static Base::Span<const Base::Ref<Base::Object>> AuthoredBehaviors(
        const FrameworkElement& element) noexcept {
        return element.AuthoredBehaviors();
    }
    static Base::Result<void> AddStyleBehaviorPrototype(
        FrameworkElement& element,
        Base::Ref<Base::Object> behavior) noexcept {
        return element.AddStyleBehaviorPrototype(std::move(behavior));
    }
    static Base::Result<void> ClearStyleBehaviorPrototypes(
        FrameworkElement& element) noexcept {
        element.ClearStyleBehaviorPrototypes();
        return {};
    }
    static Base::Span<const Base::Ref<Base::Object>> StyleBehaviorPrototypes(
        const FrameworkElement& element) noexcept {
        return element.StyleBehaviorPrototypes();
    }
    static Base::Result<void> AddAuthoredTrigger(
        FrameworkContentElement& element,
        Base::Ref<Base::Object> trigger) noexcept {
        return element.AddAuthoredTrigger(std::move(trigger));
    }
    static Base::Result<void> ClearAuthoredTriggers(
        FrameworkContentElement& element) noexcept {
        element.ClearAuthoredTriggers();
        return {};
    }
    static Base::Span<const Base::Ref<Base::Object>> AuthoredTriggers(
        const FrameworkContentElement& element) noexcept {
        return element.AuthoredTriggers();
    }
    static bool GetIsRenderValid(
        const Visual& visual) noexcept {
        return visual.renderValid_;
    }
    static std::uint64_t GetRenderRevision(
        const Visual& visual) noexcept {
        return visual.renderRevision_;
    }

    static void Attach(
        ContentElement& element,
        DependencyObject* logicalParent,
        UIElement* contentHost,
        EventRouter* eventRouter) noexcept {
        element.logicalParent_ = logicalParent;
        element.contentHost_ = contentHost;
        element.eventRouter_ = eventRouter;
    }
    static void Detach(ContentElement& element) noexcept {
        element.logicalParent_ = nullptr;
        element.contentHost_ = nullptr;
        element.eventRouter_ = nullptr;
    }
    static DependencyObject* Parent(
        const ContentElement& element) noexcept {
        return element.logicalParent_;
    }
    static UIElement* ContentHost(
        const ContentElement& element) noexcept {
        return element.contentHost_;
    }
    static std::uint32_t LogicalChildrenCount(
        const FrameworkContentElement& element) noexcept {
        return element.GetLogicalChildrenCount();
    }
    static DependencyObject* LogicalChild(
        const FrameworkContentElement& element,
        std::uint32_t index) noexcept {
        return element.GetLogicalChild(index);
    }
};

} // namespace Aero

namespace Aero::GuiPrivate::Detail {

using ElementPrivate = ::Aero::Visual::Impl;

class VisualLifetime : public Base::Object {
public:
    explicit VisualLifetime(Visual& node) noexcept : node_(&node) {}
    ~VisualLifetime() override = default;

    Visual* Node() const noexcept { return node_; }
    void Invalidate() noexcept { node_ = nullptr; }

private:
    Visual* node_ = nullptr;
};

struct VisualLease {
    Base::Ref<Visual> strong;
    Base::Ref<VisualLifetime> lifetime;

    static Base::Result<VisualLease> Acquire(Visual& node) noexcept;

    Visual* Resolve() const noexcept {
        return strong
            ? strong.Get()
            : (lifetime ? lifetime->Node() : nullptr);
    }
};

} // namespace Aero::GuiPrivate::Detail

// Per-view Gui context and element attachment state.

#include <Aero/Threading.hpp>
#include "gui/GuiPrivate.hpp"
#include <Aero/Layout.hpp>

namespace Aero::Render::Detail { class RenderTree; }

namespace Aero::GuiPrivate::Detail {

struct ElementAttachment {
    Visual* logicalParent = nullptr;
    Visual* visualParent = nullptr;
    Visual* child = nullptr;
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
    Visual* visualParent = nullptr;
    Visual* child = nullptr;
    bool visualAttached = false;
    bool layoutAttached = false;
    bool renderAttached = false;

    bool IsAttached() const noexcept {
        return visualAttached || layoutAttached || renderAttached;
    }
};

struct RootAttachment {
    Visual* root = nullptr;
    Size availableSize;
    bool contextAttached = false;
    bool layoutAttached = false;
    bool renderAttached = false;

    bool IsAttached() const noexcept {
        return contextAttached || layoutAttached || renderAttached;
    }
};

} // namespace Aero::GuiPrivate::Detail

namespace Aero {

class AERO_API ElementTree {
public:
    ElementTree(
        ::Aero::Threading::Dispatcher& dispatcher,
        Meta::EffectiveValueEngine& values) noexcept;
    ~ElementTree() noexcept;

    ElementTree(const ElementTree&) = delete;
    ElementTree& operator=(const ElementTree&) = delete;

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

    void AttachPresentation(
        Aero::GuiPrivate::Detail::LayoutEngine* layout,
        ::Aero::Render::Detail::RenderTree* renderer) noexcept {
        layout_ = layout;
        renderer_ = renderer;
    }

    Base::Result<Aero::GuiPrivate::Detail::ElementAttachment> AttachElement(
        Visual& parent, Visual& child) noexcept {
        return AttachElement(parent, parent, child);
    }
    Base::Result<Aero::GuiPrivate::Detail::ElementAttachment> AttachElement(
        Visual& logicalParent, Visual& visualParent, Visual& child) noexcept;
    Base::Result<void> DetachElement(
        Aero::GuiPrivate::Detail::ElementAttachment& state) noexcept;
    Base::Result<void> DetachVisual(
        Aero::GuiPrivate::Detail::ElementAttachment& state) noexcept;
    Base::Result<void> AttachVisual(
        Aero::GuiPrivate::Detail::ElementAttachment& state, Visual& newVisualParent) noexcept;
    Base::Result<Aero::GuiPrivate::Detail::VisualAttachment> AttachVisualChild(
        Visual& visualParent, Visual& child) noexcept;
    Base::Result<void> DetachVisual(
        Aero::GuiPrivate::Detail::VisualAttachment& state) noexcept;
    Base::Result<Aero::GuiPrivate::Detail::VisualAttachment> ReparentVisual(
        Aero::GuiPrivate::Detail::VisualAttachment& current, Visual& newVisualParent) noexcept;
    Base::Result<Aero::GuiPrivate::Detail::RootAttachment> AttachRoot(
        Visual& root, Size availableSize) noexcept;
    Base::Result<void> DetachRoot(
        Aero::GuiPrivate::Detail::RootAttachment& state) noexcept;

    Aero::GuiPrivate::Detail::LayoutEngine* Layout() const noexcept { return layout_; }
    ::Aero::Render::Detail::RenderTree* Renderer() const noexcept { return renderer_; }
    void SetHost(Aero::GuiPrivate::Detail::ElementHost* host) noexcept {
        host_ = host;
    }
    Aero::GuiPrivate::Detail::ElementHost* Host() const noexcept {
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
        Aero::GuiPrivate::Detail::VisualLease node;
        bool loaded = false;
        std::uint64_t sequence = 0U;
        std::uint64_t treeVersion = 0U;
    };
    struct HandleEntry {
        Visual* node = nullptr;
        std::uint32_t generation = 1U;
    };

    ::Aero::Threading::Dispatcher* dispatcher_ = nullptr;
    Meta::EffectiveValueEngine* values_ = nullptr;
    Aero::GuiPrivate::Detail::LayoutEngine* layout_ = nullptr;
    ::Aero::Render::Detail::RenderTree* renderer_ = nullptr;
    Aero::GuiPrivate::Detail::ElementHost* host_ = nullptr;
    Visual* root_ = nullptr;
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
    void SetTreeSubtree(Visual& node, ElementTree* tree) noexcept;
    Base::Result<std::uint32_t> FlushLifecycle() noexcept;
    Base::Result<void> RegisterHandleSubtree(Visual& node) noexcept;
    void InvalidateHandleSubtree(Visual& node) noexcept;
    Base::Result<void> TrackInheritedValues(Visual& node) noexcept;
    void UntrackInheritedValues(Visual& node) noexcept;
    void OnDataContextChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs& args) noexcept;
    void RemoveChild(Base::Vector<Visual*>& children, Visual& child) noexcept;
    Base::Result<void> AttachLayout(Visual& parent, Visual& child, bool& attached) noexcept;
    Base::Result<void> AttachRender(Visual& parent, Visual& child, bool& attached) noexcept;
    Base::Result<void> DetachLayout(Visual& parent, Visual& child, bool& attached) noexcept;
    Base::Result<void> DetachRender(Visual& parent, Visual& child, bool& attached) noexcept;
    static void LifecycleHook(void* context) noexcept;
};

inline Aero::GuiPrivate::Detail::ElementHost*
Visual::Impl::Host(const Visual& visual) noexcept {
    return visual.tree_ != nullptr ? visual.tree_->Host() : nullptr;
}

inline void* Visual::Impl::RenderRuntime(
    const Visual& visual) noexcept {
    return visual.tree_ != nullptr &&
        visual.renderNodeId_ != Base::InvalidRenderNodeId
        ? static_cast<void*>(visual.tree_->Renderer())
        : nullptr;
}

} // namespace Aero
