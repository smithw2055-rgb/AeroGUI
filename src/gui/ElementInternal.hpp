#pragma once

// Private element state and direct Gui runtime declarations.

#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/RoutedEvent.hpp>
#include <Aero/Visual.hpp>
#include <Aero/ContentElement.hpp>
#include <Aero/UIElement.hpp>
#include <Aero/FrameworkElement.hpp>

#include <cstdint>
#include <utility>

namespace Aero::Controls {
class Control;
class VisualStateManager;
}

namespace Aero::Internal {

class EventRouter;
class InputRouter;
class LayoutEngine;
class BindingEngine;
class AnimationEngine;
class StyleEngine;

struct ElementHost {
    EventRouter* events = nullptr;
    InputRouter* input = nullptr;
    void* nameScopeContext = nullptr;
    Base::Object* (*findName)(
        void*, Base::StringView, Meta::TypeId) noexcept = nullptr;
};

} // namespace Aero::Internal

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

namespace Aero::Internal {

// One private entry point owns all element implementation state. Public WPF
// classes friend this type instead of exposing one Access class per base type.
class ElementPrivate {
public:
    static void SetViewServices(
        Aero::UIElement& element,
        ElementHost* services) noexcept {
        element.viewServices_ = services;
    }

    static EventRouter* EventRouterFor(
        const Aero::UIElement& element) noexcept {
        auto* services = static_cast<ElementHost*>(element.viewServices_);
        return services != nullptr ? services->events : nullptr;
    }

    static InputRouter* InputRouterFor(
        const Aero::UIElement& element) noexcept {
        auto* services = static_cast<ElementHost*>(element.viewServices_);
        return services != nullptr ? services->input : nullptr;
    }

    static Base::Object* FindName(
        const Aero::UIElement& element,
        Base::StringView name,
        Meta::TypeId expectedType = Meta::InvalidTypeId) noexcept {
        auto* services = static_cast<ElementHost*>(element.viewServices_);
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

    static FrameworkElement* RenderParent(
        const FrameworkElement& element) noexcept {
        return element.GetRenderParent();
    }
    static FrameworkElementChildRange RenderChildren(
        const FrameworkElement& element) noexcept {
        return element.GetRenderChildren();
    }
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
    static bool GetIsRenderValid(
        const FrameworkElement& element) noexcept {
        return element.GetIsRenderValid();
    }
    static std::uint64_t GetRenderRevision(
        const FrameworkElement& element) noexcept {
        return element.GetRenderRevision();
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

} // namespace Aero::Internal

// Per-view Gui context and element attachment state.

#include <Aero/Threading.hpp>
#include "gui/PropertyInternal.hpp"
#include <Aero/Layout.hpp>

namespace Aero::Internal { class RenderTree; }

namespace Aero::Internal {

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

} // namespace Aero::Internal

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
        Aero::Internal::LayoutEngine* layout,
        Internal::RenderTree* renderer) noexcept {
        layout_ = layout;
        renderer_ = renderer;
    }

    Base::Result<Aero::Internal::ElementAttachment> AttachElement(
        Visual& parent, Visual& child) noexcept {
        return AttachElement(parent, parent, child);
    }
    Base::Result<Aero::Internal::ElementAttachment> AttachElement(
        Visual& logicalParent, Visual& visualParent, Visual& child) noexcept;
    Base::Result<void> DetachElement(
        Aero::Internal::ElementAttachment& state) noexcept;
    Base::Result<void> DetachVisual(
        Aero::Internal::ElementAttachment& state) noexcept;
    Base::Result<void> AttachVisual(
        Aero::Internal::ElementAttachment& state, Visual& newVisualParent) noexcept;
    Base::Result<Aero::Internal::VisualAttachment> AttachVisualChild(
        Visual& visualParent, Visual& child) noexcept;
    Base::Result<void> DetachVisual(
        Aero::Internal::VisualAttachment& state) noexcept;
    Base::Result<Aero::Internal::VisualAttachment> ReparentVisual(
        Aero::Internal::VisualAttachment& current, Visual& newVisualParent) noexcept;
    Base::Result<Aero::Internal::RootAttachment> AttachRoot(
        Visual& root, Size availableSize) noexcept;
    Base::Result<void> DetachRoot(
        Aero::Internal::RootAttachment& state) noexcept;

    Aero::Internal::LayoutEngine* Layout() const noexcept { return layout_; }
    Internal::RenderTree* Renderer() const noexcept { return renderer_; }

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
        Aero::Internal::VisualLease node;
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
    Aero::Internal::LayoutEngine* layout_ = nullptr;
    Internal::RenderTree* renderer_ = nullptr;
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

} // namespace Aero
