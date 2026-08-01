#pragma once

// Private element access and direct GUI runtime declarations.

#include <Aero/Base/Result.hpp>
#include <Aero/RoutedEvent.hpp>

namespace Aero {
class UIElement;
}

namespace Aero::Controls {
class Control;
class VisualStateManager;
}

namespace Aero::Detail {

class EventRouter;
class InputService;
class ViewUiServices;
class LayoutManager;
class BindingManager;
class AnimationManager;
class StyleManager;
class ThemeStyleManager;

struct UiElementServices final {
    EventRouter* events = nullptr;
    InputService* input = nullptr;
    void* nameScopeContext = nullptr;
    Base::Object* (*findName)(
        void*, Base::StringView, Core::TypeId) noexcept = nullptr;
};

// Narrow bridge for private UIElement state shared across the GUI runtime.
class UiRuntimeAccess final {
public:
    static void SetViewServices(Aero::UIElement& element, UiElementServices* services) noexcept;
    static EventRouter* EventRouterFor(const Aero::UIElement& element) noexcept;
    static InputService* InputServiceFor(const Aero::UIElement& element) noexcept;
    static Base::Object* FindName(
        const Aero::UIElement& element,
        Base::StringView name,
        Core::TypeId expectedType = Core::InvalidTypeId) noexcept;
    static Base::Result<void> SetMouseOver(Aero::UIElement& element, bool value) noexcept;
    static Base::Result<void> SetPressed(Aero::UIElement& element, bool value) noexcept;
    static Base::Result<void> SetKeyboardFocused(Aero::UIElement& element, bool value) noexcept;
    static Base::Result<void> SetKeyboardFocusWithin(Aero::UIElement& element, bool value) noexcept;
    static void InvokeHandlers(Aero::UIElement& element, RoutedEventHandle event, RoutedEventArgs& args) noexcept;
};

// Narrow bridge for private Control state. Runtime classes are direct types;
// this helper does not own nested implementation classes.
class ControlRuntimeAccess final {
public:
    class ControlInteractionManager;
    class TextBoxInteractionManager;
    class ScrollInteractionManager;
    class SliderInteractionManager;
    class TreeViewInteractionManager;
    class ComboBoxInteractionManager;
    class ListBoxInteractionManager;
    class TemplateManager;
    class MenuInteractionManager;

    static void SetVisualStateManager(Controls::Control& control, Controls::VisualStateManager* visualStates) noexcept;
};

} // namespace Aero::Detail

namespace Aero::Controls {
using ControlInteractionManager = Aero::Detail::ControlRuntimeAccess::ControlInteractionManager;
using TextBoxInteractionManager = Aero::Detail::ControlRuntimeAccess::TextBoxInteractionManager;
using ScrollInteractionManager = Aero::Detail::ControlRuntimeAccess::ScrollInteractionManager;
using SliderInteractionManager = Aero::Detail::ControlRuntimeAccess::SliderInteractionManager;
using TreeViewInteractionManager = Aero::Detail::ControlRuntimeAccess::TreeViewInteractionManager;
using ComboBoxInteractionManager = Aero::Detail::ControlRuntimeAccess::ComboBoxInteractionManager;
using ListBoxInteractionManager = Aero::Detail::ControlRuntimeAccess::ListBoxInteractionManager;
using TemplateManager = Aero::Detail::ControlRuntimeAccess::TemplateManager;
using MenuInteractionManager = Aero::Detail::ControlRuntimeAccess::MenuInteractionManager;
} // namespace Aero::Controls

#include <Aero/Visual.hpp>
#include <Aero/ContentElement.hpp>
#include <Aero/UIElement.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Base/Span.hpp>

#include <utility>

#include <cstdint>

namespace Aero::Detail {

inline void UiRuntimeAccess::SetViewServices(
    Aero::UIElement& element,
    UiElementServices* services) noexcept {
    element.viewServices_ = services;
}

inline EventRouter* UiRuntimeAccess::EventRouterFor(
    const Aero::UIElement& element) noexcept {
    auto* services = static_cast<UiElementServices*>(element.viewServices_);
    return services != nullptr ? services->events : nullptr;
}

inline InputService* UiRuntimeAccess::InputServiceFor(
    const Aero::UIElement& element) noexcept {
    auto* services = static_cast<UiElementServices*>(element.viewServices_);
    return services != nullptr ? services->input : nullptr;
}

inline Base::Object* UiRuntimeAccess::FindName(
    const Aero::UIElement& element,
    Base::StringView name,
    Core::TypeId expectedType) noexcept {
    auto* services = static_cast<UiElementServices*>(element.viewServices_);
    return services != nullptr && services->findName != nullptr
        ? services->findName(
              services->nameScopeContext, name, expectedType)
        : nullptr;
}

} // namespace Aero::Detail

namespace Aero {

struct VisualHandle final {
    std::uint32_t index = UINT32_MAX;
    std::uint32_t generation = 0U;

    constexpr bool IsValid() const noexcept {
        return index != UINT32_MAX && generation != 0U;
    }
};

struct GuiContextLifecycleEvent final {
    Visual* node = nullptr;
    bool loaded = false;
    std::uint64_t treeVersion = 0U;
};

using GuiContextLifecycleHandler = void (*)(const GuiContextLifecycleEvent& event, void* context) noexcept;

} // namespace Aero

namespace Aero::Detail {

class VisualLifetime final : public Base::Object {
public:
    explicit VisualLifetime(Visual& node) noexcept : node_(&node) {}
    ~VisualLifetime() override = default;

    Visual* Node() const noexcept { return node_; }
    void Invalidate() noexcept { node_ = nullptr; }

private:
    Visual* node_ = nullptr;
};

struct VisualLease final {
    Base::Ref<Visual> strong;
    Base::Ref<VisualLifetime> lifetime;

    static Base::Result<VisualLease> Acquire(Visual& node) noexcept;

    Visual* Resolve() const noexcept {
        return strong ? strong.Get() : (lifetime ? lifetime->Node() : nullptr);
    }
};

class VisualAccess final {
public:
    static GuiContext* Tree(const Visual& visual) noexcept { return visual.tree_; }
    static Visual* LogicalParent(const Visual& visual) noexcept { return visual.logicalParent_; }
    static Visual* VisualParent(const Visual& visual) noexcept { return visual.visualParent_; }
    static Base::Span<Visual* const> LogicalChildren(const Visual& visual) noexcept { return {visual.logicalChildren_.Data(), visual.logicalChildren_.Size()}; }
    static Base::Span<Visual* const> VisualChildren(const Visual& visual) noexcept { return {visual.visualChildren_.Data(), visual.visualChildren_.Size()}; }
    static bool IsLoaded(const Visual& visual) noexcept { return visual.loaded_; }
    static VisualHandle Handle(const Visual& visual) noexcept { return {visual.handleIndex_, visual.handleGeneration_}; }
    static void SetHandle(Visual& visual, VisualHandle handle) noexcept { visual.handleIndex_ = handle.index; visual.handleGeneration_ = handle.generation; }
    static UIElement* AsUIElement(Visual& visual) noexcept { return visual.AsUIElement(); }
    static const UIElement* AsUIElement(const Visual& visual) noexcept { return visual.AsUIElement(); }
    static FrameworkElement* AsFrameworkElement(Visual& visual) noexcept { return visual.AsFrameworkElement(); }
    static const FrameworkElement* AsFrameworkElement(const Visual& visual) noexcept { return visual.AsFrameworkElement(); }
    static Base::Result<Base::Ref<Base::Object>> AcquireLifetime(Visual& visual) noexcept { return visual.AcquireLifetime(); }
};


class FrameworkElementAccess final {
public:
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
        return element.SetTemplatedParent(value);
    }
    static Base::Result<void> TryAddAuthoredTrigger(
        FrameworkElement& element,
        Base::Ref<Base::Object> trigger) noexcept {
        return element.TryAddAuthoredTrigger(std::move(trigger));
    }
    static Base::Result<void> ClearAuthoredTriggers(
        FrameworkElement& element) noexcept {
        return element.ClearAuthoredTriggers();
    }
    static Base::Span<const Base::Ref<Base::Object>> AuthoredTriggers(
        const FrameworkElement& element) noexcept {
        return element.AuthoredTriggers();
    }
    static bool IsRenderValid(
        const FrameworkElement& element) noexcept {
        return element.IsRenderValid();
    }
    static std::uint64_t RenderRevision(
        const FrameworkElement& element) noexcept {
        return element.RenderRevision();
    }
};

class ContentElementAccess final {
public:
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

    static EventRouter* EventRouterFor(
        const UIElement& element) noexcept {
        return UiRuntimeAccess::EventRouterFor(element);
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

} // namespace Aero::Detail

// Per-view GUI context and element attachment state.

#include <Aero/Threading.hpp>
#include "gui/PropertyInternal.hpp"
#include <Aero/Layout.hpp>
#include <Aero/FrameworkElement.hpp>

namespace Aero::Render { class RenderTree; }

namespace Aero::Detail {

struct ElementAttachment final {
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

struct VisualAttachment final {
    Visual* visualParent = nullptr;
    Visual* child = nullptr;
    bool visualAttached = false;
    bool layoutAttached = false;
    bool renderAttached = false;

    bool IsAttached() const noexcept {
        return visualAttached || layoutAttached || renderAttached;
    }
};

struct RootAttachment final {
    Visual* root = nullptr;
    Size availableSize;
    bool contextAttached = false;
    bool layoutAttached = false;
    bool renderAttached = false;

    bool IsAttached() const noexcept {
        return contextAttached || layoutAttached || renderAttached;
    }
};

} // namespace Aero::Detail

namespace Aero {

class AERO_API GuiContext final {
public:
    GuiContext(
        Core::Dispatcher& dispatcher,
        Core::EffectiveValueEngine& values) noexcept;
    ~GuiContext() noexcept;

    GuiContext(const GuiContext&) = delete;
    GuiContext& operator=(const GuiContext&) = delete;

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

    void SetPresentationServices(
        Aero::Detail::LayoutManager* layout,
        Render::RenderTree* renderer) noexcept {
        layout_ = layout;
        renderer_ = renderer;
    }

    Base::Result<Aero::Detail::ElementAttachment> AttachElement(
        Visual& parent, Visual& child) noexcept {
        return AttachElement(parent, parent, child);
    }
    Base::Result<Aero::Detail::ElementAttachment> AttachElement(
        Visual& logicalParent, Visual& visualParent, Visual& child) noexcept;
    Base::Result<void> DetachElement(
        Aero::Detail::ElementAttachment& state) noexcept;
    Base::Result<void> DetachVisual(
        Aero::Detail::ElementAttachment& state) noexcept;
    Base::Result<void> AttachVisual(
        Aero::Detail::ElementAttachment& state, Visual& newVisualParent) noexcept;
    Base::Result<Aero::Detail::VisualAttachment> AttachVisualChild(
        Visual& visualParent, Visual& child) noexcept;
    Base::Result<void> DetachVisual(
        Aero::Detail::VisualAttachment& state) noexcept;
    Base::Result<Aero::Detail::VisualAttachment> ReparentVisual(
        Aero::Detail::VisualAttachment& current, Visual& newVisualParent) noexcept;
    Base::Result<Aero::Detail::RootAttachment> AttachRoot(
        Visual& root, Size availableSize) noexcept;
    Base::Result<void> DetachRoot(
        Aero::Detail::RootAttachment& state) noexcept;

    Aero::Detail::LayoutManager* Layout() const noexcept { return layout_; }
    Render::RenderTree* Renderer() const noexcept { return renderer_; }

    void SetLifecycleHandler(
        GuiContextLifecycleHandler handler,
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
    Aero::Detail::LayoutManager* layout_ = nullptr;
    Render::RenderTree* renderer_ = nullptr;
    Visual* root_ = nullptr;
    Base::Vector<LifecycleRecord> lifecycleQueue_;
    Base::Vector<HandleEntry> handles_;
    Core::DispatcherFrameHookHandle lifecycleHook_;
    GuiContextLifecycleHandler lifecycleHandler_ = nullptr;
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
    void SetTreeSubtree(Visual& node, GuiContext* tree) noexcept;
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
