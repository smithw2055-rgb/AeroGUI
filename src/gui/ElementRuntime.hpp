#pragma once

// Private element state and direct Gui runtime declarations.

#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/RoutedEvent.hpp>
#include <Aero/Visual.hpp>
#include <Aero/FrameworkContentElement.hpp>
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

namespace Aero {

class EventRouter;
class InputRouter;
class LayoutEngine;
class BindingEngine;
class AnimationEngine;
class StyleEngine;

struct ElementHost {
    EventRouter* events = nullptr;
    InputRouter* input = nullptr;
    BindingEngine* bindings = nullptr;
    void* templates = nullptr;
    void* visualStates = nullptr;
    void* textLayout = nullptr;
    void* controlBehaviors = nullptr;
    void* meshResources = nullptr;
    void* nameScopeContext = nullptr;
    Base::Object* (*findName)(
        void*, Base::StringView, Meta::TypeId) noexcept = nullptr;
};

} // namespace Aero

namespace Aero {

struct VisualHandle {
    std::uint32_t index = UINT32_MAX;
    std::uint32_t generation = 0U;

    constexpr bool IsValid() const noexcept {
        return index != UINT32_MAX && generation != 0U;
    }
};

struct ElementTreeLifecycleEvent {
    ::Aero::Media::Visual* node = nullptr;
    bool loaded = false;
    std::uint64_t treeVersion = 0U;
};

using ElementTreeLifecycleHandler = void (*)(
    const ElementTreeLifecycleEvent& event,
    void* context) noexcept;

} // namespace Aero

namespace Aero {

using namespace ::Aero;

// One private entry point owns all element implementation state. Public WPF
// classes friend this type instead of exposing one Access class per base type.
struct Media::Visual::Access {
public:
    static void SetViewServices(
        Aero::UIElement& element,
        ElementHost*) noexcept {
        // View services are owned once by ElementTree. Per-element service
        // pointers are deliberately absent from the public object layout.
        static_cast<void>(element);
    }

    static ElementHost* Host(const ::Aero::Media::Visual& visual) noexcept;

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

    static BindingEngine* BindingEngineFor(
        const Aero::UIElement& element) noexcept {
        ElementHost* services = Host(element);
        return services != nullptr ? services->bindings : nullptr;
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

    static ElementTree* Tree(const ::Aero::Media::Visual& visual) noexcept {
        return visual.tree_;
    }
    static ::Aero::Media::Visual* LogicalParent(const ::Aero::Media::Visual& visual) noexcept {
        return visual.logicalParent_;
    }
    static ::Aero::Media::Visual* VisualParent(const ::Aero::Media::Visual& visual) noexcept {
        return visual.visualParent_;
    }
    static Base::Span<::Aero::Media::Visual* const> LogicalChildren(
        const ::Aero::Media::Visual& visual) noexcept {
        return {
            visual.logicalChildren_.Data(),
            visual.logicalChildren_.Size()};
    }
    static Base::Span<::Aero::Media::Visual* const> VisualChildren(
        const ::Aero::Media::Visual& visual) noexcept {
        return {
            visual.visualChildren_.Data(),
            visual.visualChildren_.Size()};
    }
    static bool IsLoaded(const ::Aero::Media::Visual& visual) noexcept {
        return visual.loaded_;
    }
    static VisualHandle Handle(const ::Aero::Media::Visual& visual) noexcept {
        return {visual.handleIndex_, visual.handleGeneration_};
    }
    static void SetHandle(
        ::Aero::Media::Visual& visual,
        VisualHandle handle) noexcept {
        visual.handleIndex_ = handle.index;
        visual.handleGeneration_ = handle.generation;
    }
    static UIElement* AsUIElement(::Aero::Media::Visual& visual) noexcept {
        return visual.AsUIElement();
    }
    static const UIElement* AsUIElement(
        const ::Aero::Media::Visual& visual) noexcept {
        return visual.AsUIElement();
    }
    static FrameworkElement* AsFrameworkElement(
        ::Aero::Media::Visual& visual) noexcept {
        return visual.AsFrameworkElement();
    }
    static const FrameworkElement* AsFrameworkElement(
        const ::Aero::Media::Visual& visual) noexcept {
        return visual.AsFrameworkElement();
    }
    static Base::Result<Base::Ref<Base::Object>> AcquireLifetime(
        ::Aero::Media::Visual& visual) noexcept {
        return visual.AcquireLifetime();
    }

    static ::Aero::Media::Visual* RenderParent(
        const ::Aero::Media::Visual& visual) noexcept {
        return visual.visualParent_;
    }
    static Base::Span<::Aero::Media::Visual* const> RenderChildren(
        const ::Aero::Media::Visual& visual) noexcept {
        return {
            visual.visualChildren_.Data(),
            visual.visualChildren_.Size()};
    }
    static void* RenderRuntime(const ::Aero::Media::Visual& visual) noexcept;
    static void* TemplateRuntime(const ::Aero::Media::Visual& visual) noexcept {
        ElementHost* host = Host(visual);
        return host != nullptr ? host->templates : nullptr;
    }
    static void* VisualStateRuntime(const ::Aero::Media::Visual& visual) noexcept {
        ElementHost* host = Host(visual);
        return host != nullptr ? host->visualStates : nullptr;
    }
    static void* TextLayoutRuntime(const ::Aero::Media::Visual& visual) noexcept {
        ElementHost* host = Host(visual);
        return host != nullptr ? host->textLayout : nullptr;
    }
    static void* ControlBehaviorRuntime(
        const ::Aero::Media::Visual& visual) noexcept {
        ElementHost* host = Host(visual);
        return host != nullptr ? host->controlBehaviors : nullptr;
    }
    static void* MeshResourcesRuntime(
        const ::Aero::Media::Visual& visual) noexcept {
        ElementHost* host = Host(visual);
        return host != nullptr ? host->meshResources : nullptr;
    }
    static bool& RenderAttached(
        ::Aero::Media::Visual& visual) noexcept {
        return visual.renderAttached_;
    }
    static Base::RenderNodeId& NodeId(
        ::Aero::Media::Visual& visual) noexcept {
        return visual.renderNodeId_;
    }
    static bool& RenderValid(
        ::Aero::Media::Visual& visual) noexcept {
        return visual.renderValid_;
    }
    static bool& RenderQueued(
        ::Aero::Media::Visual& visual) noexcept {
        return visual.renderQueued_;
    }
    static std::uint64_t& RenderRevision(
        ::Aero::Media::Visual& visual) noexcept {
        return visual.renderRevision_;
    }
    static bool& Rendering(
        ::Aero::Media::Visual& visual) noexcept {
        return visual.rendering_;
    }
    static std::uint8_t& RenderDirtyFlags(
        ::Aero::Media::Visual& visual) noexcept {
        return visual.renderDirtyFlags_;
    }
    static void Render(
        ::Aero::Media::Visual& visual,
        ::Aero::Media::DrawingContext& context) noexcept {
        FrameworkElement* element =
            visual.AsFrameworkElement();
        if (element != nullptr) {
            element->OnRender(context);
        }
    }
    static Base::Result<void> InvalidateRenderDrawing(
        ::Aero::Media::Visual& visual) noexcept;
    static Base::Result<void> InvalidateRenderState(
        ::Aero::Media::Visual& visual) noexcept;
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
    static Base::Result<void> AddStyleTriggerPrototype(
        FrameworkElement& element,
        Base::Ref<Base::Object> trigger) noexcept {
        return element.AddStyleTriggerPrototype(std::move(trigger));
    }
    static Base::Result<void> ClearStyleTriggerPrototypes(
        FrameworkElement& element) noexcept {
        element.ClearStyleTriggerPrototypes();
        return {};
    }
    static Base::Span<const Base::Ref<Base::Object>> StyleTriggerPrototypes(
        const FrameworkElement& element) noexcept {
        return element.StyleTriggerPrototypes();
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
        const ::Aero::Media::Visual& visual) noexcept {
        return visual.renderValid_;
    }
    static std::uint64_t GetRenderRevision(
        const ::Aero::Media::Visual& visual) noexcept {
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

namespace Aero {

using ElementPrivate = Media::Visual::Access;

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

inline Aero::ElementHost*
Media::Visual::Access::Host(const ::Aero::Media::Visual& visual) noexcept {
    return visual.tree_ != nullptr ? visual.tree_->Host() : nullptr;
}

inline void* Media::Visual::Access::RenderRuntime(
    const ::Aero::Media::Visual& visual) noexcept {
    return visual.tree_ != nullptr &&
        visual.renderNodeId_ != Base::InvalidRenderNodeId
        ? static_cast<void*>(visual.tree_->Renderer())
        : nullptr;
}

} // namespace Aero
