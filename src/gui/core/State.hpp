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

#pragma once

#include <Aero/Freezable.hpp>

namespace Aero {

struct DependencyObject::Access {
    using FreezableVisitor = Base::Result<void> (*)(
        void* context,
        Freezable& child) noexcept;

    static bool HasUnfreezableValueState(
        const DependencyObject& object) noexcept;
    static Base::Result<void> VisitFreezableChildren(
        DependencyObject& object,
        void* context,
        FreezableVisitor visitor) noexcept;
    static Base::Result<void> PrepareConsumerChange(
        DependencyObject& consumer,
        Meta::DependencyPropertyHandle property,
        const Meta::PropertyValue& oldValue,
        const Meta::PropertyValue& newValue) noexcept;
    static void CommitConsumerChange(
        DependencyObject& consumer,
        Meta::DependencyPropertyHandle property,
        const Meta::PropertyValue& oldValue,
        const Meta::PropertyValue& newValue) noexcept;
    static void InvalidateSubProperty(
        DependencyObject& object,
        Meta::DependencyPropertyHandle property) noexcept;
};

struct Freezable::Access {
    struct ConsumerRecord {
        Base::WeakRef<DependencyObject> object;
        DependencyObject* unmanagedObject = nullptr;
        Meta::DependencyPropertyHandle property;
    };

    struct HandlerRecord {
        FreezableChangedHandler handler;
        bool active = false;
    };

    explicit Access(Base::IAllocator& allocator) noexcept
        : consumers(&allocator), handlers(&allocator) {}

    Base::Vector<ConsumerRecord> consumers;
    Base::Vector<HandlerRecord> handlers;
    std::uint64_t revision = 0U;
    std::uint32_t notificationDepth = 0U;
    bool frozen = false;
    bool freezing = false;

    static Base::Result<void> AttachConsumer(
        Freezable& value,
        DependencyObject& object,
        Meta::DependencyPropertyHandle property) noexcept;
    static void DetachConsumer(
        Freezable& value,
        DependencyObject& object,
        Meta::DependencyPropertyHandle property) noexcept;
    static std::uint64_t Revision(const Freezable& value) noexcept;
    static bool CheckCore(Freezable& value) noexcept {
        return value.FreezeCore(true);
    }
};

} // namespace Aero

#pragma once


#include <Aero/Layout.hpp>

namespace Aero {

struct UIElement::Access {
    static void* LayoutManager(
        const UIElement& element) noexcept {
        ElementTree* tree = ::Aero::Media::Visual::Access::Tree(element);
        return tree != nullptr ? tree->Layout() : nullptr;
    }
    static bool& LayoutAttached(UIElement& element) noexcept {
        return element.layoutAttached_;
    }
    static bool& MeasureValid(UIElement& element) noexcept {
        return element.measureValid_;
    }
    static bool& ArrangeValid(UIElement& element) noexcept {
        return element.arrangeValid_;
    }
    static bool& MeasureQueued(UIElement& element) noexcept {
        return element.measureQueued_;
    }
    static bool& ArrangeQueued(UIElement& element) noexcept {
        return element.arrangeQueued_;
    }
    static bool& Measuring(UIElement& element) noexcept {
        return element.measuring_;
    }
    static bool& Arranging(UIElement& element) noexcept {
        return element.arranging_;
    }
    static Size& DesiredSize(UIElement& element) noexcept {
        return element.desiredSize_;
    }
    static Size& RenderSize(UIElement& element) noexcept {
        return element.renderSize_;
    }
    static Size& UntransformedDesiredSize(UIElement& element) noexcept {
        return element.untransformedDesiredSize_;
    }
    static Size& PreviousMeasureConstraint(UIElement& element) noexcept {
        return element.previousMeasureConstraint_;
    }
    static Rect& LayoutSlot(UIElement& element) noexcept {
        return element.layoutSlot_;
    }
    static Rect& LayoutClip(UIElement& element) noexcept {
        return element.layoutClip_;
    }
    static std::uint64_t& LayoutRevision(UIElement& element) noexcept {
        return element.layoutRevision_;
    }
    static Size MeasureOverride(
        UIElement& element,
        Size availableSize) noexcept {
        return element.MeasureOverride(availableSize);
    }
    static Size ArrangeOverride(
        UIElement& element,
        Size finalSize) noexcept {
        return element.ArrangeOverride(finalSize);
    }
    static void SetActualSize(
        FrameworkElement& element,
        double width,
        double height) noexcept {
        element.SetReadOnlyCurrentValue(
            FrameworkElement::ActualWidthProperty, width);
        element.SetReadOnlyCurrentValue(
            FrameworkElement::ActualHeightProperty, height);
    }
};

} // namespace Aero

namespace Aero {

using namespace Aero::Meta;
using namespace Aero::Threading;

class LayoutEngine {
public:
    explicit LayoutEngine(Dispatcher& dispatcher) noexcept;
    ~LayoutEngine() noexcept;
    LayoutEngine(const LayoutEngine&) = delete;
    LayoutEngine& operator=(const LayoutEngine&) = delete;

    Base::Result<void> Initialize() noexcept;
    Base::Result<void> Attach(UIElement& parent, UIElement& child) noexcept;
    Base::Result<void> Detach(UIElement& parent, UIElement& child) noexcept;
    Base::Result<void> SetRoot(UIElement* root, Size availableSize) noexcept;
    Base::Result<void> InvalidateMeasure(UIElement& element) noexcept;
    Base::Result<void> InvalidateArrange(UIElement& element) noexcept;
    Base::Result<std::uint32_t> Flush() noexcept;
    LayoutDiagnostics Diagnostics() const noexcept;
    std::uint64_t PassVersion() const noexcept { return passVersion_; }
    Base::Status LastFlushStatus() const noexcept {
        return lastFlushStatus_;
    }

private:
    friend class Aero::UIElement;
    Dispatcher* dispatcher_ = nullptr;
    UIElement* root_ = nullptr;
    Size rootAvailableSize_;
    Base::Vector<Aero::VisualLease> measureQueue_;
    Base::Vector<Aero::VisualLease> arrangeQueue_;
    DispatcherFrameHookHandle phaseHook_;
    std::uint64_t passVersion_ = 0U;
    std::uint32_t measuredCount_ = 0U;
    std::uint32_t arrangedCount_ = 0U;
    Base::Status lastFlushStatus_;
    bool flushing_ = false;

    Base::Result<void> VerifyElement(const UIElement& element) const noexcept;
    Base::Result<void> QueueMeasure(UIElement& element) noexcept;
    Base::Result<void> QueueArrange(UIElement& element) noexcept;
    void RemoveQueued(UIElement& element) noexcept;
    Base::Result<void> MeasureElement(UIElement& element, Size constraint) noexcept;
    Base::Result<void> ArrangeElement(UIElement& element, Rect slot) noexcept;
    static void LayoutHook(void* context) noexcept;
};

} // namespace Aero

#pragma once

// Dependency-property evaluation, ambient services and provider sessions.

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Diagnostics/PropertyValueSource.hpp>
#include <Aero/Threading.hpp>

#include <cstdint>

namespace Aero::Meta { class Registry; class Registration; }

namespace Aero::Meta {

using ::Aero::Threading::Dispatcher;
using ::Aero::Threading::DispatcherFrameHookHandle;
using ::Aero::Threading::DispatcherThreadToken;
using ::Aero::Threading::CurrentDispatcherThreadToken;

using EffectiveValueDiagnostics = PropertyValueSourceInfo;

class EffectiveValueEngine {
public:
    EffectiveValueEngine(
        Dispatcher& dispatcher,
        DependencyPropertyRegistry& registry) noexcept;
    ~EffectiveValueEngine() noexcept;

    EffectiveValueEngine(const EffectiveValueEngine&) = delete;
    EffectiveValueEngine& operator=(const EffectiveValueEngine&) = delete;
    EffectiveValueEngine(EffectiveValueEngine&&) = delete;
    EffectiveValueEngine& operator=(EffectiveValueEngine&&) = delete;

    Base::Result<void> Initialize() noexcept;
    bool IsInitialized() const noexcept {
        return phaseHook_.IsValid();
    }

    Base::Result<void> SetInheritanceParent(
        DependencyObject& child,
        DependencyObject* parent) noexcept;
    DependencyObject* InheritanceParent(
        const DependencyObject& child) const noexcept;

    // Allocates a process-local provider origin unique to this value engine.
    // Provider sessions allocate lazily on the owning Dispatcher and retain the
    // origin for the lifetime of their Style, Theme or Template application.
    Base::Result<std::uint32_t> AllocateProviderOrigin() noexcept {
        Base::Result<void> access = dispatcher_->VerifyAccess();
        if (!access) return access.GetStatus();
        return providerOrigins_.Allocate();
    }

    // Canonical contribution API. Style, Template, Theme and Trigger runtimes
    // allocate a stable origin and use declaration ordinal within that origin.
    Base::Result<void> SetProviderContribution(
        DependencyObject& object,
        DependencyPropertyHandle property,
        PropertyProviderToken token,
        const PropertyValue& value) noexcept;
    Base::Result<bool> ClearProviderContribution(
        DependencyObject& object,
        DependencyPropertyHandle property,
        PropertyProviderToken token) noexcept;
    Base::Result<std::uint32_t> ClearProviderOrigin(
        DependencyObject& object,
        std::uint32_t origin) noexcept;

    // Ownership of expression.context transfers only after this call succeeds.
    Base::Result<void> SetLocalExpression(
        DependencyObject& object,
        DependencyPropertyHandle property,
        const PropertyExpression& expression) noexcept;
    Base::Result<void> ClearLocalExpression(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept;

    Base::Result<void> SetAnimationValue(
        DependencyObject& object,
        DependencyPropertyHandle property,
        const PropertyValue& value) noexcept;
    Base::Result<void> ClearAnimationValue(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept;

    Base::Result<void> Invalidate(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept;

    // Processes only the queue snapshot that existed at entry. Changes queued by
    // inheritance propagation are deferred to the next PropertyChanges phase.
    Base::Result<std::uint32_t> Flush() noexcept;

    Base::Result<EffectiveValueDiagnostics> Diagnostics(
        const DependencyObject& object,
        DependencyPropertyHandle property) const noexcept;

    // Tracked objects are non-owning. Hosts must detach an object before its
    // destruction unless the engine itself is destroyed first.
    Base::Result<void> DetachObject(
        DependencyObject& object) noexcept;

    std::uint32_t TrackedPropertyCount() const noexcept {
        return entries_.Size();
    }
    std::uint32_t PendingPropertyCount() const noexcept;
    bool IsFlushing() const noexcept {
        return flushing_;
    }

private:
    struct Entry {
        DependencyObject* object = nullptr;
        DependencyPropertyHandle property;
        std::uint64_t queueSequence = 0U;
        bool queued = false;
    };

    struct ParentLink {
        DependencyObject* child = nullptr;
        DependencyObject* parent = nullptr;
    };

    Dispatcher* dispatcher_ = nullptr;
    DependencyPropertyRegistry* registry_ = nullptr;
    Base::Vector<Entry> entries_;
    Base::Vector<ParentLink> parents_;
    Base::Vector<DependencyObject*> inheritanceSubscriptions_;
    DependencyPropertyChangedEventHandler
        inheritanceChangedHandler_;
    DispatcherFrameHookHandle phaseHook_;
    PropertyProviderOriginAllocator providerOrigins_;
    std::uint64_t nextQueueSequence_ = 1U;
    bool flushing_ = false;

    Base::Result<void> VerifyMutable() const noexcept;
    std::uint32_t FindEntryIndex(
        const DependencyObject& object,
        DependencyPropertyHandle property) const noexcept;
    Base::Result<std::uint32_t> EnsureEntry(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept;
    std::uint32_t FindParentIndex(
        const DependencyObject& child) const noexcept;

    Base::Result<void> QueueEntry(
        std::uint32_t index) noexcept;
    Base::Result<void> QueueDescendants(
        DependencyObject& parent,
        DependencyPropertyHandle property) noexcept;
    Base::Result<void> EnsureInheritanceSubscription(
        DependencyObject& object) noexcept;
    void RemoveInheritanceSubscription(
        DependencyObject& object) noexcept;
    void OnInheritancePropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs&
            args) noexcept;
    Base::Result<void> Apply(
        Entry& entry) noexcept;

    void RemoveEntry(std::uint32_t index) noexcept;
    void RemoveParent(std::uint32_t index) noexcept;

    static bool IsMutableBaseRank(PropertyValueRank rank) noexcept;
    static void PropertyChangesHook(void* context) noexcept;
};

} // namespace Aero::Meta


namespace Aero::Meta {


struct ObjectFactoryState {
    Dispatcher* dispatcher = nullptr;
    DependencyPropertyRegistry* dependencyProperties = nullptr;
    Meta::Registry* metadata = nullptr;

    bool IsValid() const noexcept {
        return dispatcher != nullptr && dependencyProperties != nullptr;
    }
};

ObjectFactoryState CurrentObjectFactory() noexcept;
bool HasObjectFactory() noexcept;

Base::Result<Value> TryEncodeValue(
    TypeId type,
    const void* source) noexcept;

class ObjectFactoryScope {
public:
    ObjectFactoryScope(
        Dispatcher& dispatcher,
        DependencyPropertyRegistry& properties) noexcept;
    ObjectFactoryScope(
        Dispatcher& dispatcher,
        DependencyPropertyRegistry& properties,
        Meta::Registry& runtime) noexcept;
    ObjectFactoryScope(
        Dispatcher& dispatcher,
        DependencyPropertyRegistry& properties,
        Meta::Registry* runtime) noexcept;
    ~ObjectFactoryScope();

    ObjectFactoryScope(const ObjectFactoryScope&) = delete;
    ObjectFactoryScope& operator=(const ObjectFactoryScope&) = delete;
    ObjectFactoryScope(ObjectFactoryScope&&) = delete;
    ObjectFactoryScope& operator=(ObjectFactoryScope&&) = delete;

private:
    ObjectFactoryState state_;
    ObjectFactoryState* previous_ = nullptr;
    DispatcherThreadToken ownerThread_ = 0U;
};

} // namespace Aero::Meta


#include <utility>

namespace Aero {

using namespace ::Aero::Meta;

// Canonical expression-to-DP boundary. Binding and MultiBinding use this
// after their explicit converters; other expression runtimes can share it
// without depending on Data. Type-specific conversions belong to metadata
// codecs or an authored converter; this helper only normalizes text,
// null-object typing and object covariance.
Base::Result<PropertyValue> NormalizeValueForProperty(
    Meta::Registry* metadata,
    const DependencyProperty& property,
    PropertyValue value) noexcept;

// Manager-owned provider state. One session belongs to one StyleEngine,
// StyleEngine or TemplateEngine allocates all provider origins through the
// shared EffectiveValueEngine, preventing cross-manager token collisions.
class PropertyProviderSession {
public:
    PropertyProviderSession(
        EffectiveValueEngine& engine,
        PropertyValueRank setterRank,
        PropertyValueRank triggerRank) noexcept
        : engine_(&engine),
          setterRank_(setterRank),
          triggerRank_(triggerRank) {}

    PropertyProviderSession(const PropertyProviderSession&) = delete;
    PropertyProviderSession& operator=(const PropertyProviderSession&) = delete;

    Base::Result<void> SetSetterValue(
        DependencyObject& object,
        DependencyPropertyHandle property,
        const PropertyValue& value) noexcept {
        ContributionRecord* existing = FindRecord(
            setterRecords_, object, property);
        if (existing != nullptr) {
            return engine_->SetProviderContribution(
                object, property, existing->token, value);
        }

        Base::Result<ObjectState*> stateResult = EnsureState(object);
        if (!stateResult) return stateResult.GetStatus();
        ObjectState& state = *stateResult.Value();
        Base::Result<std::uint32_t> origin = EnsureOrigin(
            state.setterOrigin);
        if (!origin) return origin.GetStatus();
        Base::Result<std::uint32_t> ordinal = NextOrdinal(
            state.nextSetterOrdinal,
            "Property setter ordinal limit reached");
        if (!ordinal) return ordinal.GetStatus();

        const PropertyProviderToken token{
            setterRank_, origin.Value(), ordinal.Value()};
        Base::Result<void> applied = engine_->SetProviderContribution(
            object, property, token, value);
        if (!applied) return applied.GetStatus();

        ContributionRecord record;
        record.object = &object;
        record.property = property;
        record.token = token;
        Base::Result<void> retained = setterRecords_.PushBack(
            std::move(record));
        if (!retained) {
            static_cast<void>(engine_->ClearProviderContribution(
                object, property, token));
            return retained.GetStatus();
        }
        return {};
    }

    Base::Result<void> ClearSetterValue(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept {
        Base::Result<std::uint32_t> cleared = ClearRecords(
            setterRecords_, object, property);
        if (!cleared) return cleared.GetStatus();
        PruneState(object);
        return {};
    }

    Base::Result<void> SetTriggerValue(
        DependencyObject& object,
        DependencyPropertyHandle property,
        const PropertyValue& value) noexcept {
        Base::Result<ObjectState*> stateResult = EnsureState(object);
        if (!stateResult) return stateResult.GetStatus();
        ObjectState& state = *stateResult.Value();
        Base::Result<std::uint32_t> origin = EnsureOrigin(
            state.triggerOrigin);
        if (!origin) return origin.GetStatus();
        Base::Result<std::uint32_t> ordinal = NextOrdinal(
            state.nextTriggerOrdinal,
            "Property trigger ordinal limit reached");
        if (!ordinal) return ordinal.GetStatus();

        const PropertyProviderToken token{
            triggerRank_, origin.Value(), ordinal.Value()};
        Base::Result<void> applied = engine_->SetProviderContribution(
            object, property, token, value);
        if (!applied) return applied.GetStatus();

        ContributionRecord record;
        record.object = &object;
        record.property = property;
        record.token = token;
        Base::Result<void> retained = triggerRecords_.PushBack(
            std::move(record));
        if (!retained) {
            static_cast<void>(engine_->ClearProviderContribution(
                object, property, token));
            return retained.GetStatus();
        }
        return {};
    }

    Base::Result<void> ClearTriggerValue(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept {
        Base::Result<std::uint32_t> cleared = ClearRecords(
            triggerRecords_, object, property);
        if (!cleared) return cleared.GetStatus();
        PruneState(object);
        return {};
    }

    Base::Result<void> ClearObjectProviders(
        DependencyObject& object) noexcept {
        Base::Result<std::uint32_t> setters = ClearObjectRecords(
            setterRecords_, object);
        if (!setters) return setters.GetStatus();
        Base::Result<std::uint32_t> triggers = ClearObjectRecords(
            triggerRecords_, object);
        if (!triggers) return triggers.GetStatus();
        RemoveState(object);
        return {};
    }

    Base::Result<void> DetachObject(
        DependencyObject& object) noexcept {
        Base::Result<void> cleared = ClearObjectProviders(object);
        if (!cleared) return cleared.GetStatus();
        return engine_->DetachObject(object);
    }

    Base::Result<std::uint32_t> Flush() noexcept {
        return engine_->Flush();
    }

    bool IsFlushing() const noexcept {
        return engine_->IsFlushing();
    }

private:
    struct ContributionRecord {
        DependencyObject* object = nullptr;
        DependencyPropertyHandle property;
        PropertyProviderToken token;
    };

    struct ObjectState {
        DependencyObject* object = nullptr;
        std::uint32_t setterOrigin = 0U;
        std::uint32_t triggerOrigin = 0U;
        std::uint32_t nextSetterOrdinal = 0U;
        std::uint32_t nextTriggerOrdinal = 0U;
    };

    EffectiveValueEngine* engine_ = nullptr;
    PropertyValueRank setterRank_ = PropertyValueRank::Default;
    PropertyValueRank triggerRank_ = PropertyValueRank::Default;
    Base::Vector<ContributionRecord> setterRecords_;
    Base::Vector<ContributionRecord> triggerRecords_;
    Base::Vector<ObjectState> states_;

    Base::Result<ObjectState*> EnsureState(
        DependencyObject& object) noexcept {
        for (ObjectState& state : states_) {
            if (state.object == &object) return &state;
        }
        ObjectState state;
        state.object = &object;
        Base::Result<void> retained = states_.PushBack(
            std::move(state));
        if (!retained) return retained.GetStatus();
        return &states_[states_.Size() - 1U];
    }

    Base::Result<std::uint32_t> EnsureOrigin(
        std::uint32_t& origin) noexcept {
        if (origin != 0U) return origin;
        Base::Result<std::uint32_t> allocated =
            engine_->AllocateProviderOrigin();
        if (!allocated) return allocated.GetStatus();
        origin = allocated.Value();
        return origin;
    }

    static Base::Result<std::uint32_t> NextOrdinal(
        std::uint32_t& next,
        const char* message) noexcept {
        if (next == UINT32_MAX) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                message);
        }
        return next++;
    }

    static ContributionRecord* FindRecord(
        Base::Vector<ContributionRecord>& records,
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept {
        for (ContributionRecord& record : records) {
            if (record.object == &object && record.property == property) {
                return &record;
            }
        }
        return nullptr;
    }

    Base::Result<std::uint32_t> ClearRecords(
        Base::Vector<ContributionRecord>& records,
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept {
        std::uint32_t removed = 0U;
        std::uint32_t index = 0U;
        while (index < records.Size()) {
            ContributionRecord& record = records[index];
            if (record.object != &object || record.property != property) {
                ++index;
                continue;
            }
            Base::Result<bool> cleared =
                engine_->ClearProviderContribution(
                    object, property, record.token);
            if (!cleared) return cleared.GetStatus();
            RemoveAt(records, index);
            ++removed;
        }
        return removed;
    }

    Base::Result<std::uint32_t> ClearObjectRecords(
        Base::Vector<ContributionRecord>& records,
        DependencyObject& object) noexcept {
        std::uint32_t removed = 0U;
        std::uint32_t index = 0U;
        while (index < records.Size()) {
            ContributionRecord& record = records[index];
            if (record.object != &object) {
                ++index;
                continue;
            }
            Base::Result<bool> cleared =
                engine_->ClearProviderContribution(
                    object, record.property, record.token);
            if (!cleared) return cleared.GetStatus();
            RemoveAt(records, index);
            ++removed;
        }
        return removed;
    }

    bool HasRecords(const DependencyObject& object) const noexcept {
        for (const ContributionRecord& record : setterRecords_) {
            if (record.object == &object) return true;
        }
        for (const ContributionRecord& record : triggerRecords_) {
            if (record.object == &object) return true;
        }
        return false;
    }

    void PruneState(DependencyObject& object) noexcept {
        if (!HasRecords(object)) RemoveState(object);
    }

    void RemoveState(DependencyObject& object) noexcept {
        for (std::uint32_t index = 0U; index < states_.Size(); ++index) {
            if (states_[index].object != &object) continue;
            RemoveAt(states_, index);
            return;
        }
    }

    template<class T>
    static void RemoveAt(
        Base::Vector<T>& values,
        std::uint32_t index) noexcept {
        for (std::uint32_t next = index + 1U;
             next < values.Size();
             ++next) {
            values[next - 1U] = std::move(values[next]);
        }
        values.PopBack();
    }
};

class StyleProviderSession {
public:
    explicit StyleProviderSession(
        EffectiveValueEngine& engine) noexcept
        : session_(
              engine,
              PropertyValueRank::StyleSetter,
              PropertyValueRank::StyleTrigger) {}

    Base::Result<void> SetStyleValue(
        DependencyObject& object,
        DependencyPropertyHandle property,
        const PropertyValue& value) noexcept {
        return session_.SetSetterValue(object, property, value);
    }
    Base::Result<void> ClearStyleValue(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept {
        return session_.ClearSetterValue(object, property);
    }
    Base::Result<void> SetTriggerValue(
        DependencyObject& object,
        DependencyPropertyHandle property,
        const PropertyValue& value) noexcept {
        return session_.SetTriggerValue(object, property, value);
    }
    Base::Result<void> ClearTriggerValue(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept {
        return session_.ClearTriggerValue(object, property);
    }
    Base::Result<std::uint32_t> Flush() noexcept {
        return session_.Flush();
    }
    bool IsFlushing() const noexcept {
        return session_.IsFlushing();
    }

private:
    PropertyProviderSession session_;
};

class TemplatedParentProviderSession {
public:
    explicit TemplatedParentProviderSession(
        EffectiveValueEngine& engine) noexcept
        : session_(
              engine,
              PropertyValueRank::TemplatedParentSetter,
              PropertyValueRank::TemplatedParentTrigger) {}

    Base::Result<void> SetTemplateValue(
        DependencyObject& object,
        DependencyPropertyHandle property,
        const PropertyValue& value) noexcept {
        return session_.SetSetterValue(object, property, value);
    }
    Base::Result<void> ClearTemplateValue(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept {
        return session_.ClearSetterValue(object, property);
    }
    Base::Result<void> SetTriggerValue(
        DependencyObject& object,
        DependencyPropertyHandle property,
        const PropertyValue& value) noexcept {
        return session_.SetTriggerValue(object, property, value);
    }
    Base::Result<void> ClearTriggerValue(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept {
        return session_.ClearTriggerValue(object, property);
    }
    Base::Result<void> DetachObject(
        DependencyObject& object) noexcept {
        return session_.DetachObject(object);
    }

private:
    PropertyProviderSession session_;
};

} // namespace Aero

#pragma once

// Internal routed-event storage, route snapshots and dispatch.

#include <Aero/Base/Assert.hpp>
#include <Aero/Base/Delegate.hpp>
#include <Aero/Events/EventArgs.hpp>
#include <Aero/RoutedEvent.hpp>

#include "gui/meta/MetadataState.hpp"

#include <cstddef>
#include <new>
#include <type_traits>

namespace Aero {

class RoutedHandlerStorage {
public:
    RoutedHandlerStorage() noexcept = default;

    RoutedHandlerStorage(
        const void* value,
        std::size_t size,
        std::size_t alignment,
        Meta::TypeId argsType,
        void (*copy)(void*, const void*) noexcept,
        void (*destroy)(void*) noexcept,
        bool (*equals)(const void*, const void*) noexcept,
        void (*invoke)(const void*, Base::Object*, RoutedEventArgs&) noexcept) noexcept
        : size_(size), alignment_(alignment), argsType_(argsType),
          copy_(copy), destroy_(destroy), equals_(equals), invoke_(invoke) {
        AERO_ASSERT(value != nullptr && size_ <= sizeof(storage_) && alignment_ <= alignof(void*));
        copy_(storage_, value);
    }

    template<class TArgs>
    explicit RoutedHandlerStorage(
        const Base::Delegate<void(Base::Object*, TArgs&)>& handler) noexcept
        : RoutedHandlerStorage(
              &handler,
              sizeof(handler),
              alignof(decltype(handler)),
              TArgs::StaticTypeId(),
              [](void* destination, const void* source) noexcept {
                  using Handler = Base::Delegate<void(Base::Object*, TArgs&)>;
                  new (destination) Handler(*static_cast<const Handler*>(source));
              },
              [](void* value) noexcept {
                  using Handler = Base::Delegate<void(Base::Object*, TArgs&)>;
                  static_cast<Handler*>(value)->~Handler();
              },
              [](const void* left, const void* right) noexcept {
                  using Handler = Base::Delegate<void(Base::Object*, TArgs&)>;
                  return *static_cast<const Handler*>(left) == *static_cast<const Handler*>(right);
              },
              [](const void* value, Base::Object* sender, RoutedEventArgs& args) noexcept {
                  using Handler = Base::Delegate<void(Base::Object*, TArgs&)>;
                  static_cast<const Handler*>(value)->Invoke(sender, static_cast<TArgs&>(args));
              }) {
        static_assert(std::is_base_of<RoutedEventArgs, TArgs>::value,
            "Routed event arguments must derive from RoutedEventArgs");
    }

    RoutedHandlerStorage(const RoutedHandlerStorage& other) noexcept { CopyFrom(other); }
    RoutedHandlerStorage(RoutedHandlerStorage&& other) noexcept { CopyFrom(other); other.Reset(); }

    RoutedHandlerStorage& operator=(const RoutedHandlerStorage& other) noexcept {
        if (this != &other) {
            Reset();
            CopyFrom(other);
        }
        return *this;
    }

    RoutedHandlerStorage& operator=(RoutedHandlerStorage&& other) noexcept {
        if (this != &other) {
            Reset();
            CopyFrom(other);
            other.Reset();
        }
        return *this;
    }

    ~RoutedHandlerStorage() noexcept { Reset(); }

    bool Empty() const noexcept { return copy_ == nullptr; }
    Meta::TypeId ArgsType() const noexcept { return argsType_; }

    bool Equals(const RoutedHandlerStorage& other) const noexcept {
        return copy_ == other.copy_ && destroy_ == other.destroy_ &&
            equals_ == other.equals_ && invoke_ == other.invoke_ &&
            argsType_ == other.argsType_ &&
            (Empty() || equals_(storage_, other.storage_));
    }

    void Invoke(Base::Object* sender, RoutedEventArgs& args) const noexcept {
        AERO_ASSERT(
            !Empty() &&
            (args.GetEventArgsType() == argsType_ ||
             argsType_ == RoutedEventArgs::StaticTypeId()));
        invoke_(storage_, sender, args);
    }

private:
    void CopyFrom(const RoutedHandlerStorage& other) noexcept {
        size_ = other.size_;
        alignment_ = other.alignment_;
        argsType_ = other.argsType_;
        copy_ = other.copy_;
        destroy_ = other.destroy_;
        equals_ = other.equals_;
        invoke_ = other.invoke_;
        if (!other.Empty()) copy_(storage_, other.storage_);
    }

    void Reset() noexcept {
        if (!Empty()) destroy_(storage_);
        size_ = 0U;
        alignment_ = 0U;
        argsType_ = Meta::InvalidTypeId;
        copy_ = nullptr;
        destroy_ = nullptr;
        equals_ = nullptr;
        invoke_ = nullptr;
    }

    alignas(void*) unsigned char storage_[4U * sizeof(void*)]{};
    std::size_t size_ = 0U;
    std::size_t alignment_ = 0U;
    Meta::TypeId argsType_ = Meta::InvalidTypeId;
    void (*copy_)(void*, const void*) noexcept = nullptr;
    void (*destroy_)(void*) noexcept = nullptr;
    bool (*equals_)(const void*, const void*) noexcept = nullptr;
    void (*invoke_)(const void*, Base::Object*, RoutedEventArgs&) noexcept = nullptr;
};

} // namespace Aero

#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/FrameworkContentElement.hpp>
#include <Aero/Visual.hpp>
#include <Aero/UIElement.hpp>

#include <utility>

namespace Aero {

struct EventRouteNode {
    Base::Ref<DependencyObject> retained;
    DependencyObject* borrowed = nullptr;

    static EventRouteNode Acquire(DependencyObject& value) noexcept {
        EventRouteNode node;
        node.retained = Base::Ref<DependencyObject>::TryFromBorrowed(value);
        node.borrowed = &value;
        return node;
    }

    DependencyObject* Resolve() const noexcept {
        return retained ? retained.Get() : borrowed;
    }
};

class EventRoute {
public:
    explicit EventRoute(Base::IAllocator* allocator = nullptr) noexcept
        : nodes_(allocator != nullptr
              ? allocator
              : &Base::GetDefaultAllocator()) {}

    Base::Result<void> Build(
        DependencyObject& source,
        RoutingStrategy strategy) noexcept {
        nodes_.Clear();

        DependencyObject* current = &source;
        while (current != nullptr) {
            Base::Result<void> appended =
                nodes_.PushBack(EventRouteNode::Acquire(*current));
            if (!appended) return appended.GetStatus();
            if (strategy == RoutingStrategy::Direct) break;
            current = GetParent(*current);
        }

        if (strategy == RoutingStrategy::Tunnel && nodes_.Size() > 1U) {
            for (std::uint32_t left = 0U, right = nodes_.Size() - 1U;
                 left < right; ++left, --right) {
                EventRouteNode temporary = std::move(nodes_[left]);
                nodes_[left] = std::move(nodes_[right]);
                nodes_[right] = std::move(temporary);
            }
        }
        return {};
    }

    Base::Span<const EventRouteNode> Nodes() const noexcept {
        return nodes_.AsSpan();
    }
    std::uint32_t Size() const noexcept { return nodes_.Size(); }
    bool Empty() const noexcept { return nodes_.Empty(); }

private:
    static DependencyObject* GetParent(
        DependencyObject& object) noexcept {
        const Meta::TypeRegistry& types = object.PropertyRegistry().Types();
        if (types.IsDerivedFrom(
                object.RuntimeType(), ContentElement::StaticTypeId())) {
            auto& content = static_cast<ContentElement&>(object);
            DependencyObject* parent = content.GetParent();
            return parent != nullptr
                ? parent
                : static_cast<DependencyObject*>(content.GetContentHost());
        }
        if (types.IsDerivedFrom(
                object.RuntimeType(), ::Aero::Media::Visual::StaticTypeId())) {
            auto& visual = static_cast<::Aero::Media::Visual&>(object);
            if (visual.GetVisualParent() != nullptr) {
                return visual.GetVisualParent();
            }
        }
        return LogicalTreeHelper::GetParent(object);
    }

    Base::Vector<EventRouteNode> nodes_;
};

} // namespace Aero


namespace Aero { class ContentElement; }


namespace Aero {

using namespace Aero::Meta;
using namespace Aero::Threading;

class EventRouter {
public:
    explicit EventRouter(void* eventState) noexcept;
    ~EventRouter() noexcept;

    template<class TArgs>
    Base::Result<void> RegisterClassHandler(
        RoutedEventHandle event,
        TypeId classType,
        const Base::Delegate<void(Base::Object*, TArgs&)>& handler,
        bool handledEventsToo = false) noexcept;

    Base::Result<void> RaiseEvent(
        DependencyObject& source,
        RoutedEventHandle event,
        RoutedEventArgs* args = nullptr) noexcept;

    template<class TVisitor>
    Base::Result<void> VisitRoute(
        DependencyObject& source,
        RoutingStrategy strategy,
        TVisitor&& visitor) noexcept {
        EventRoute route;
        Base::Result<void> built = route.Build(source, strategy);
        if (!built) return built.GetStatus();
        for (const EventRouteNode& lease : route.Nodes()) {
            DependencyObject* node = lease.Resolve();
            if (node != nullptr && !visitor(*node)) break;
        }
        return {};
    }

private:
    struct ClassHandlerRecord {
        RoutedEventHandle event;
        TypeId classType = InvalidTypeId;
        Aero::RoutedHandlerStorage handler;
        std::uint64_t sequence = 0U;
        bool handledEventsToo = false;
    };

    void* eventState_ = nullptr;
    Base::Vector<ClassHandlerRecord> classHandlers_;
    std::uint64_t nextClassSequence_ = 1U;
    std::uint32_t raiseDepth_ = 0U;

    void InvokeNode(DependencyObject& node, RoutedEventArgs& args) noexcept;
    void CleanupClassHandlers() noexcept;
    Base::Result<void> ValidateClassHandler(
        RoutedEventHandle event,
        TypeId classType,
        TypeId eventArgsType) const noexcept;
};

template<class TArgs>
Base::Result<void> EventRouter::RegisterClassHandler(
    RoutedEventHandle event,
    TypeId classType,
    const Base::Delegate<void(Base::Object*, TArgs&)>& handler,
    bool handledEventsToo) noexcept {
    static_assert(std::is_base_of<RoutedEventArgs, TArgs>::value,
        "Routed event arguments must derive from RoutedEventArgs");
    if (raiseDepth_ != 0U) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Cannot mutate class handlers during routed event dispatch");
    }
    if (handler.Empty()) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Class handler registration is invalid");
    }
    Base::Result<void> valid = ValidateClassHandler(
        event, classType, TArgs::StaticTypeId());
    if (!valid) return valid.GetStatus();
    ClassHandlerRecord value;
    value.event = event;
    value.classType = classType;
    value.handler = Aero::RoutedHandlerStorage(handler);
    value.handledEventsToo = handledEventsToo;
    value.sequence = nextClassSequence_++;
    return classHandlers_.PushBack(std::move(value));
}

} // namespace Aero
