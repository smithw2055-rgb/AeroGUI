#pragma once

// Kernel-private operations. Not installed. Implementation TUs include this
// header. Public WPF types grant friendship only to AeroGuiInternal.

#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Controls/Control.hpp>
#include <Aero/Controls/ContentControl.hpp>
#include <Aero/Controls/Decorator.hpp>
#include <Aero/Controls/Image.hpp>
#include <Aero/Controls/ItemContainerGenerator.hpp>
#include <Aero/Controls/ItemsControl.hpp>
#include <Aero/Controls/Menu.hpp>
#include <Aero/Controls/Panel.hpp>
#include <Aero/Controls/PasswordBox.hpp>
#include <Aero/Controls/TextBlock.hpp>
#include <Aero/Controls/TextBox.hpp>
#include <Aero/Controls/TreeView.hpp>
#include <Aero/FrameworkContentElement.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Freezable.hpp>
#include <Aero/RoutedEvent.hpp>
#include <Aero/Shapes.hpp>
#include <Aero/UIElement.hpp>
#include <Aero/Visual.hpp>

#include "gui/core/VisualHandle.hpp"
#include "gui/core/state/ElementTree.hpp"
#include "gui/internal/PropertyStore.hpp"

namespace Aero {
class AnimationEngine;
class BindingEngine;
class EventRouter;
class InputRouter;
class LayoutEngine;
class StyleEngine;
class VisualStateManager;
class DataTemplate;
class Style;
namespace Controls {
class TemplateEngine;
class ControlBehavior;
class TextBlockLayout;
class ItemsPanelTemplate;
enum class ItemSubtreeChange : std::uint8_t { Mounted = 0U, Unmounting };
using ItemSubtreeCallback = Base::Result<void> (*)(
    ::Aero::Media::Visual& root,
    ItemSubtreeChange change,
    void* context) noexcept;
namespace Primitives { class Selector; }
} // namespace Controls
namespace Collections { class IItemsSource; }
namespace Render { struct MeshResources; class RenderTree; }
namespace Media { class DrawingContext; }
} // namespace Aero

namespace Aero {

class AeroGuiInternal {
public:
    AeroGuiInternal() = delete;

    // --- View / ElementTree services ---
    static ElementTree* Tree(const ::Aero::Media::Visual& visual) noexcept {
        return visual.tree_;
    }
    static ElementTree* Tree(const UIElement& element) noexcept {
        return element.GetTree();
    }
    static LayoutEngine* LayoutEngineOf(const ::Aero::Media::Visual& visual) noexcept {
        ElementTree* tree = visual.GetTree();
        return tree != nullptr ? tree->Layout() : nullptr;
    }
    static LayoutEngine* LayoutEngineOf(const UIElement& element) noexcept {
        return LayoutEngineOf(static_cast<const ::Aero::Media::Visual&>(element));
    }
    static EventRouter* EventRouterOf(const UIElement& element) noexcept {
        ElementTree* tree = element.GetTree();
        return tree != nullptr ? tree->Events() : nullptr;
    }
    static InputRouter* InputRouterOf(const UIElement& element) noexcept {
        ElementTree* tree = element.GetTree();
        return tree != nullptr ? tree->Input() : nullptr;
    }
    static BindingEngine* BindingEngineOf(const UIElement& element) noexcept {
        ElementTree* tree = element.GetTree();
        return tree != nullptr ? tree->Bindings() : nullptr;
    }
    static StyleEngine* StyleEngineOf(const UIElement& element) noexcept {
        ElementTree* tree = element.GetTree();
        return tree != nullptr ? tree->Styles() : nullptr;
    }
    static AnimationEngine* AnimationEngineOf(const UIElement& element) noexcept {
        ElementTree* tree = element.GetTree();
        return tree != nullptr ? tree->Animations() : nullptr;
    }
    static Controls::TemplateEngine* TemplatesOf(
        const ::Aero::Media::Visual& visual) noexcept {
        ElementTree* tree = visual.GetTree();
        return tree != nullptr ? tree->Templates() : nullptr;
    }
    static VisualStateManager* VisualStatesOf(
        const ::Aero::Media::Visual& visual) noexcept {
        ElementTree* tree = visual.GetTree();
        return tree != nullptr ? tree->VisualStates() : nullptr;
    }
    static Controls::TextBlockLayout* TextLayoutOf(
        const ::Aero::Media::Visual& visual) noexcept {
        ElementTree* tree = visual.GetTree();
        return tree != nullptr ? tree->TextLayout() : nullptr;
    }
    static Controls::ControlBehavior* ControlBehaviorsOf(
        const ::Aero::Media::Visual& visual) noexcept {
        ElementTree* tree = visual.GetTree();
        return tree != nullptr ? tree->ControlBehaviors() : nullptr;
    }
    static Render::MeshResources* MeshResourcesOf(
        const ::Aero::Media::Visual& visual) noexcept {
        ElementTree* tree = visual.GetTree();
        return tree != nullptr ? tree->MeshResources() : nullptr;
    }
    static Base::Object* FindName(
        const UIElement& element,
        Base::StringView name,
        Meta::TypeId expectedType = Meta::InvalidTypeId) noexcept {
        ElementTree* tree = element.GetTree();
        return tree != nullptr ? tree->FindName(name, expectedType) : nullptr;
    }
    static void* TemplateRuntime(const ::Aero::Media::Visual& visual) noexcept {
        return static_cast<void*>(TemplatesOf(visual));
    }
    static void* MeshResourcesRuntime(const ::Aero::Media::Visual& visual) noexcept {
        return static_cast<void*>(MeshResourcesOf(visual));
    }
    static void* VisualStateRuntime(const ::Aero::Media::Visual& visual) noexcept {
        return static_cast<void*>(VisualStatesOf(visual));
    }
    static void* ControlBehaviorRuntime(const ::Aero::Media::Visual& visual) noexcept {
        return static_cast<void*>(ControlBehaviorsOf(visual));
    }
    static void* TextLayoutRuntime(const ::Aero::Media::Visual& visual) noexcept {
        return static_cast<void*>(TextLayoutOf(visual));
    }
    template<class TRuntime = void>
    static TRuntime* TypedTextLayoutRuntime(
        const ::Aero::Media::Visual& visual) noexcept {
        return static_cast<TRuntime*>(TextLayoutRuntime(visual));
    }

    // --- Layout hot state ---
    static UIElement::LayoutHot& Layout(UIElement& element) noexcept {
        return element.layout_;
    }
    static const UIElement::LayoutHot& Layout(const UIElement& element) noexcept {
        return element.layout_;
    }
    static Size MeasureOverride(UIElement& element, Size availableSize) noexcept {
        return element.MeasureOverride(availableSize);
    }
    static Size ArrangeOverride(UIElement& element, Size finalSize) noexcept {
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

    // --- Visual / render hot fields ---
    static VisualHandle Handle(const ::Aero::Media::Visual& visual) noexcept {
        return {visual.handleIndex_, visual.handleGeneration_};
    }
    static void SetHandle(
        ::Aero::Media::Visual& visual, VisualHandle handle) noexcept {
        visual.handleIndex_ = handle.index;
        visual.handleGeneration_ = handle.generation;
    }
    static Base::Result<Base::Ref<Base::Object>> AcquireLifetime(
        ::Aero::Media::Visual& visual) noexcept {
        return visual.AcquireLifetime();
    }
    static Base::RenderNodeId& NodeId(::Aero::Media::Visual& visual) noexcept {
        return visual.renderNodeId_;
    }
    static bool& RenderAttached(::Aero::Media::Visual& visual) noexcept {
        return visual.renderAttached_;
    }
    static bool& RenderValid(::Aero::Media::Visual& visual) noexcept {
        return visual.renderValid_;
    }
    static bool& RenderQueued(::Aero::Media::Visual& visual) noexcept {
        return visual.renderQueued_;
    }
    static bool& Rendering(::Aero::Media::Visual& visual) noexcept {
        return visual.rendering_;
    }
    static std::uint8_t& RenderDirtyFlags(::Aero::Media::Visual& visual) noexcept {
        return visual.renderDirtyFlags_;
    }
    static std::uint64_t& RenderRevision(::Aero::Media::Visual& visual) noexcept {
        return visual.renderRevision_;
    }
    static ::Aero::Media::Visual* RenderParent(
        const ::Aero::Media::Visual& visual) noexcept {
        return visual.visualParent_;
    }
    static Base::Span<::Aero::Media::Visual* const> RenderChildren(
        const ::Aero::Media::Visual& visual) noexcept {
        return {visual.visualChildren_.Data(), visual.visualChildren_.Size()};
    }
    static void* RenderRuntime(const ::Aero::Media::Visual& visual) noexcept {
        return visual.tree_ != nullptr &&
            visual.renderNodeId_ != Base::InvalidRenderNodeId
            ? static_cast<void*>(visual.tree_->Renderer())
            : nullptr;
    }
    static void Render(
        ::Aero::Media::Visual& visual,
        ::Aero::Media::DrawingContext& context) noexcept {
        FrameworkElement* element = visual.AsFrameworkElement();
        if (element != nullptr) {
            element->OnRender(context);
        }
    }
    static Base::Result<void> InvalidateRenderDrawing(
        ::Aero::Media::Visual& visual) noexcept;
    static Base::Result<void> InvalidateRenderState(
        ::Aero::Media::Visual& visual) noexcept;
    static Base::Result<void> SetImageRuntimeData(
        Controls::Image& image,
        std::uint64_t renderImage,
        std::uint32_t pixelWidth,
        std::uint32_t pixelHeight) noexcept;

    // --- Input / routed events ---
    static Base::Result<void> SetMouseOver(UIElement& element, bool value) noexcept {
        element.SetMouseOverState(value);
        return {};
    }
    static Base::Result<void> SetPressed(UIElement& element, bool value) noexcept {
        element.SetPressedState(value);
        return {};
    }
    static Base::Result<void> SetKeyboardFocused(
        UIElement& element, bool value) noexcept {
        element.SetKeyboardFocusedState(value);
        return {};
    }
    static Base::Result<void> SetKeyboardFocusWithin(
        UIElement& element, bool value) noexcept {
        element.SetKeyboardFocusWithinState(value);
        return {};
    }
    static void InvokeHandlers(
        UIElement& element,
        RoutedEventHandle event,
        RoutedEventArgs& args) noexcept {
        element.InvokeHandlers(event, args);
    }
    static void InvokeContentHandlers(
        ContentElement& element,
        RoutedEventHandle event,
        RoutedEventArgs& args) noexcept;

    // --- Content element attach ---
    static void Attach(
        ContentElement& element,
        DependencyObject* logicalParent,
        UIElement* contentHost,
        EventRouter* eventRouter) noexcept;
    static void Detach(ContentElement& element) noexcept;
    static DependencyObject* Parent(const ContentElement& element) noexcept;
    static UIElement* ContentHost(const ContentElement& element) noexcept;
    static std::uint32_t LogicalChildrenCount(
        const FrameworkContentElement& element) noexcept;
    static DependencyObject* LogicalChild(
        const FrameworkContentElement& element,
        std::uint32_t index) noexcept;

    // --- FrameworkElement interaction lists ---
    static Base::Result<void> SetTemplatedParent(
        FrameworkElement& element,
        DependencyObject* value) noexcept {
        element.templatedParent_ = value;
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

    // --- Panel / decorator ---
    static std::uint32_t PanelChildCount(const Controls::Panel& panel) noexcept {
        return panel.ChildCountCore();
    }
    static Base::Ref<Base::Object> PanelChildAt(
        const Controls::Panel& panel,
        std::uint32_t index) noexcept {
        return panel.ChildAtCore(index);
    }
    static Base::Result<void> PanelAddChild(
        Controls::Panel& panel,
        const Base::Ref<Base::Object>& owner,
        UIElement& child) noexcept {
        return panel.AddChildCore(owner, child);
    }
    static Base::Result<bool> PanelRemoveChild(
        Controls::Panel& panel,
        UIElement& child) noexcept {
        return panel.RemoveChildCore(child);
    }
    static void PanelClearChildren(Controls::Panel& panel) noexcept {
        panel.ClearChildrenCore();
    }
    static const Base::Ref<Base::Object>& DecoratorOwnedChild(
        const Controls::Decorator& decorator) noexcept {
        return decorator.ownedChild_;
    }
    static Base::Result<void> DecoratorSetOwnedChild(
        Controls::Decorator& decorator,
        const Base::Ref<Base::Object>& owner,
        UIElement& child) noexcept {
        decorator.SetOwnedChild(owner, child);
        return {};
    }

    // --- Control / template / items ---
    static void SetMenuItemHighlighted(
        Controls::MenuItem& item, bool value) noexcept;
    static void SyncSelectorContainers(
        Controls::Primitives::Selector& selector) noexcept;
    static std::uint32_t TreeViewItemCount(
        const Controls::TreeViewItem& item) noexcept;
    static bool IsTemplateApplied(const Controls::Control& control) noexcept {
        return control.templateHandleValue_ != 0U;
    }
    static std::uint64_t TemplateGeneration(
        const Controls::Control& control) noexcept {
        return control.templateGeneration_;
    }
    static UIElement* TemplateRoot(const Controls::Control& control) noexcept {
        return control.templateChild_;
    }
    static Base::Result<void> SetTemplateRoot(
        Controls::Control& control, UIElement* child) noexcept {
        control.SetTemplateChildCore(child);
        return {};
    }
    static void AttachTemplateEngine(
        Controls::Control& control, void* engine) noexcept {
        static_cast<void>(control);
        static_cast<void>(engine);
    }
    static void SetVisualStateManager(
        Controls::Control& control, VisualStateManager* manager) noexcept {
        static_cast<void>(control);
        static_cast<void>(manager);
    }
    static void NotifyTemplateApplied(
        Controls::Control& control, std::uint64_t handleValue) noexcept {
        control.NotifyTemplateApplied(handleValue);
    }
    static void NotifyTemplateDetached(Controls::Control& control) noexcept {
        control.NotifyTemplateDetached();
    }
    static void InvokeTemplateApplied(Controls::Control& control) noexcept {
        control.OnApplyTemplate();
    }
    static UIElement* ContentControlContent(
        const Controls::ContentControl& control) noexcept {
        return control.content_;
    }
    static const Base::Ref<Base::Object>& OwnedContent(
        const Controls::ContentControl& control) noexcept {
        return control.ownedContent_;
    }
    static const Base::Ref<Base::Object>& ContentValue(
        const Controls::ContentControl& control) noexcept {
        return control.contentValue_;
    }
    static Base::Result<void> SetOwnedContent(
        Controls::ContentControl& control,
        const Base::Ref<Base::Object>& owner,
        UIElement& content) noexcept {
        control.SetOwnedContent(owner, content);
        return {};
    }
    static Base::Result<void> SetGeneratedTextContent(
        Controls::ContentControl& container,
        const Base::Ref<Base::Object>& contentObject,
        UIElement& content) noexcept {
        container.SetGeneratedTextContent(contentObject, content);
        return {};
    }
    static Base::Result<void> SetContentValue(
        Controls::ContentControl& control,
        Base::Ref<Base::Object> value) noexcept {
        control.SetContentValue(std::move(value));
        return {};
    }
    static Base::Result<void> SetContentValue(
        Controls::ContentControl& control,
        Meta::Value value) noexcept {
        control.SetContentValue(std::move(value));
        return {};
    }
    static void OnContentControlPropertyChanged(
        DependencyObject& object,
        const Meta::DependencyPropertyChangedEventArgs& change) noexcept;
    static bool HasAttachedGenerator(
        const Controls::ItemsControl& control) noexcept {
        return control.generator_ != nullptr;
    }
    static void SetItemsSource(
        Controls::ItemsControl& control,
        Collections::IItemsSource* source) noexcept;
    static void SetItemsSource(
        Controls::ItemsControl& control,
        Base::Ref<Base::Object> source) noexcept;
    static void SetItemsSourceBorrowed(
        Controls::ItemsControl& control,
        Collections::IItemsSource* source) noexcept;
    static void SetItemTemplate(
        Controls::ItemsControl& control,
        const DataTemplate* value) noexcept;
    static void SetItemsPanel(
        Controls::ItemsControl& control,
        const Controls::ItemsPanelTemplate* value) noexcept;
    static void SetItemContainerStyle(
        Controls::ItemsControl& control,
        const Style* value) noexcept;
    static void RefreshDisplayMemberPath(
        Controls::ItemsControl& control) noexcept;
    static Base::Result<Controls::ItemContainerGenerator*>
    CreateItemContainerGenerator(
        ElementTree& tree,
        LayoutEngine& layout,
        Meta::EffectiveValueEngine& values,
        StyleEngine* styles,
        Render::RenderTree* renderer,
        Controls::TemplateEngine* templates,
        Controls::ItemSubtreeCallback callback,
        void* context) noexcept;

    // --- Text ---
    static void AttachTextLayout(
        Controls::TextBlock& element,
        void* service,
        bool invalidate = false) noexcept;
    static void AttachTextLayout(
        Controls::TextBox& element,
        void* service,
        bool invalidate = false) noexcept;
    static void AttachTextLayout(
        Controls::PasswordBox& element,
        void* service,
        bool invalidate = false) noexcept;

    // --- Path / freezable / DP consumers ---
    static void PathInvalidateGeometry(Shapes::Path& path) noexcept;
    static void PathAttachMeshResources(
        Shapes::Path& path,
        void* services,
        bool invalidate) noexcept;
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
        DependencyObject& object,
        Meta::DependencyPropertyHandle property,
        const Meta::PropertyValue& oldValue,
        const Meta::PropertyValue& newValue) noexcept;
    static void CommitConsumerChange(
        DependencyObject& object,
        Meta::DependencyPropertyHandle property,
        const Meta::PropertyValue& oldValue,
        const Meta::PropertyValue& newValue) noexcept;
    static void InvalidateSubProperty(
        DependencyObject& object,
        Meta::DependencyPropertyHandle property) noexcept;
    static Base::Result<void> AttachFreezableConsumer(
        Freezable& value,
        DependencyObject& object,
        Meta::DependencyPropertyHandle property) noexcept;
    static void DetachFreezableConsumer(
        Freezable& value,
        DependencyObject& object,
        Meta::DependencyPropertyHandle property) noexcept;
    static std::uint64_t FreezableRevision(const Freezable& value) noexcept;
    static bool FreezableCheckCore(Freezable& value) noexcept;

    // --- Property store ---
    static PropertyStore* Store(DependencyObject& object) noexcept {
        return static_cast<PropertyStore*>(object.valueStore_);
    }
    static const PropertyStore* Store(const DependencyObject& object) noexcept {
        return static_cast<const PropertyStore*>(object.valueStore_);
    }
    static MemberId CanonicalKey(
        const DependencyObject& object,
        DependencyPropertyHandle property) noexcept {
        return object.CanonicalPropertyKey(property);
    }
    static StoredValueEntry* FindEntry(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept {
        return object.FindStoredEntry(property);
    }
    static const StoredValueEntry* FindEntry(
        const DependencyObject& object,
        DependencyPropertyHandle property) noexcept {
        return object.FindStoredEntry(property);
    }
    static Base::Result<StoredValueEntry*> EnsureEntry(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept {
        return object.EnsureStoredEntry(property);
    }
    static void RemoveEntry(
        DependencyObject& object,
        MemberId key) noexcept {
        object.RemoveStoredEntry(key);
    }
    static void ReleaseExpression(StoredValueEntry& entry) noexcept {
        // Implemented as DependencyObject::ReleaseExpression; callers pass
        // the owning object when they have one.
        static_cast<void>(entry);
    }
    static Base::Result<void> ApplyProviderContribution(
        DependencyObject& object,
        DependencyPropertyHandle property,
        PropertyProviderToken token,
        const PropertyValue& value) noexcept {
        return object.ApplyProviderContributionInternal(property, token, value);
    }
    static Base::Result<bool> ClearProviderContribution(
        DependencyObject& object,
        DependencyPropertyHandle property,
        PropertyProviderToken token) noexcept {
        return object.ClearProviderContributionInternal(property, token);
    }
    static Base::Result<bool> ClearProviderOrigin(
        DependencyObject& object,
        DependencyPropertyHandle property,
        std::uint32_t origin) noexcept {
        return object.ClearProviderOriginInternal(property, origin);
    }
    static Base::Result<std::uint32_t> ClearProviderOrigin(
        DependencyObject& object,
        std::uint32_t origin) noexcept {
        std::uint32_t removed = 0U;
        Base::Vector<MemberId> keys;
        ForEachStoredKey(
            object,
            [](void* context, MemberId key) noexcept {
                static_cast<Base::Vector<MemberId>*>(context)->PushBack(key);
            },
            &keys);
        for (MemberId key : keys) {
            Base::Result<bool> cleared =
                object.ClearProviderOriginInternal(
                    DependencyPropertyHandle{key}, origin);
            if (!cleared) return cleared.GetStatus();
            if (cleared.Value()) ++removed;
        }
        return removed;
    }
    static Base::Result<void> ApplyLocalExpression(
        DependencyObject& object,
        DependencyPropertyHandle property,
        const PropertyExpression& expression) noexcept {
        return object.ApplyLocalExpressionInternal(property, expression);
    }
    static Base::Result<bool> ClearLocalExpression(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept {
        return object.ClearLocalExpressionInternal(property);
    }
    static Base::Result<bool> InvalidateBaseValue(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept {
        return object.InvalidateBaseValueInternal(property);
    }
    static Base::Result<void> ApplyAnimationValue(
        DependencyObject& object,
        DependencyPropertyHandle property,
        const PropertyValue& value) noexcept {
        return object.ApplyAnimationValueInternal(property, value);
    }
    static Base::Result<bool> ClearAnimationValue(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept {
        return object.ClearAnimationValueInternal(property);
    }
    static Base::Result<void> ApplyInheritedValue(
        DependencyObject& object,
        DependencyPropertyHandle property,
        const PropertyValue* value) noexcept {
        return object.ApplyInheritedValueInternal(property, value);
    }
    static Base::Result<void> RecomputeEffectiveValue(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept {
        return object.RecomputeEffectiveValueInternal(property);
    }
    static Base::Result<void> DropEngineValueState(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept {
        return object.DropEngineValueStateInternal(property);
    }
    static Base::Result<void> DropAllEngineValueState(
        DependencyObject& object) noexcept {
        Base::Vector<MemberId> keys;
        ForEachStoredKey(
            object,
            [](void* context, MemberId key) noexcept {
                static_cast<Base::Vector<MemberId>*>(context)->PushBack(key);
            },
            &keys);
        for (MemberId key : keys) {
            Base::Result<void> dropped =
                object.DropEngineValueStateInternal(
                    DependencyPropertyHandle{key});
            if (!dropped) return dropped.GetStatus();
        }
        return {};
    }
    static void ForEachStoredKey(
        DependencyObject& object,
        void (*visitor)(void*, MemberId) noexcept,
        void* context) noexcept {
        PropertyStore* store = Store(object);
        if (store == nullptr || visitor == nullptr) return;
        for (auto& record : store->entries) {
            visitor(context, record.Key());
        }
    }

    // --- Rare data ---
    static void* RoutedHandlers(const UIElement& element) noexcept {
        return element.rare_ != nullptr ? element.rare_->routedHandlers : nullptr;
    }
    static void SetRoutedHandlers(UIElement& element, void* handlers) noexcept {
        element.EnsureRare().routedHandlers = handlers;
    }
};

} // namespace Aero
