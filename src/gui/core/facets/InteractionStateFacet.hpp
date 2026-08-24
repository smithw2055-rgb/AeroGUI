#pragma once

#include "gui/core/Facet.hpp"
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/FrameworkContentElement.hpp>

namespace Aero {
class DataTemplate;
class Style;
class VisualStateManager;
class LayoutEngine;
class StyleEngine;
namespace Collections { class IItemsSource; }
namespace Render { class RenderTree; }
namespace Meta { class EffectiveValueEngine; class DependencyPropertyChangedEventArgs; }
namespace Controls {
class Control;
class ContentControl;
class ItemsControl;
class ItemsPanelTemplate;
class ItemContainerGenerator;
class TemplateEngine;
class MenuItem;
class TreeViewItem;
namespace Primitives { class Selector; }
enum class ItemSubtreeChange : std::uint8_t { Mounted = 0U, Unmounting };
using ItemSubtreeCallback = Base::Result<void> (*)(
    Aero::Media::Visual& root,
    ItemSubtreeChange change,
    void* context) noexcept;
} // namespace Controls
} // namespace Aero

namespace Aero::Core {

// Interaction State, VisualStateManager & Triggers/Behaviors Facet
class InteractionStateFacet : public Facet {
public:
    static constexpr FacetType StaticType = FacetType::Interaction;

    explicit InteractionStateFacet(::Aero::Media::Visual& owner) noexcept : owner_(&owner) {}

    ::Aero::Media::Visual& Owner() const noexcept { return *owner_; }

    static void* VisualStateRuntime(const ::Aero::Media::Visual& visual) noexcept;
    static void* ControlBehaviorRuntime(const ::Aero::Media::Visual& visual) noexcept;

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

    static void SetMenuItemHighlighted(
        Aero::Controls::MenuItem& item,
        bool value) noexcept;

    static void SyncSelectorContainers(
        Aero::Controls::Primitives::Selector& selector) noexcept;

    static std::uint32_t TreeViewItemCount(
        const Aero::Controls::TreeViewItem& item) noexcept;

    // Control and template lifecycle
    static bool IsTemplateApplied(const Aero::Controls::Control& control) noexcept;
    static std::uint64_t TemplateGeneration(const Aero::Controls::Control& control) noexcept;
    static UIElement* TemplateRoot(const Aero::Controls::Control& control) noexcept;
    static Base::Result<void> SetTemplateRoot(Aero::Controls::Control& control, UIElement* child) noexcept;
    static void AttachTemplateEngine(Aero::Controls::Control& control, void* engine) noexcept;
    static void SetVisualStateManager(Aero::Controls::Control& control, Aero::VisualStateManager* manager) noexcept;
    static void NotifyTemplateApplied(Aero::Controls::Control& control, std::uint64_t handleValue) noexcept;
    static void NotifyTemplateDetached(Aero::Controls::Control& control) noexcept;
    static void InvokeTemplateApplied(Aero::Controls::Control& control) noexcept;

    // ContentControl content and value management
    static UIElement* ContentElement(const Aero::Controls::ContentControl& control) noexcept;
    static const Base::Ref<Base::Object>& OwnedContent(const Aero::Controls::ContentControl& control) noexcept;
    static const Base::Ref<Base::Object>& ContentValue(const Aero::Controls::ContentControl& control) noexcept;
    static Base::Result<void> SetOwnedContent(
        Aero::Controls::ContentControl& control,
        const Base::Ref<Base::Object>& owner,
        UIElement& content) noexcept;
    static Base::Result<void> SetGeneratedTextContent(
        Aero::Controls::ContentControl& container,
        const Base::Ref<Base::Object>& contentObject,
        UIElement& content) noexcept;
    static Base::Result<void> SetContentValue(
        Aero::Controls::ContentControl& control,
        Base::Ref<Base::Object> value) noexcept;
    static Base::Result<void> SetContentValue(
        Aero::Controls::ContentControl& control,
        Meta::Value value) noexcept;
    static void OnContentControlPropertyChanged(
        ::Aero::DependencyObject& object,
        const Meta::DependencyPropertyChangedEventArgs& change) noexcept;

    // ItemsControl items source and container generation
    static bool HasAttachedGenerator(const Aero::Controls::ItemsControl& control) noexcept;
    static void SetItemsSource(
        Aero::Controls::ItemsControl& control,
        Collections::IItemsSource* source) noexcept;
    static void SetItemsSource(
        Aero::Controls::ItemsControl& control,
        Base::Ref<Base::Object> source) noexcept;
    static void SetItemsSourceBorrowed(
        Aero::Controls::ItemsControl& control,
        Collections::IItemsSource* source) noexcept;
    static void SetItemTemplate(
        Aero::Controls::ItemsControl& control,
        const DataTemplate* value) noexcept;
    static void SetItemsPanel(
        Aero::Controls::ItemsControl& control,
        const Aero::Controls::ItemsPanelTemplate* value) noexcept;
    static void SetItemContainerStyle(
        Aero::Controls::ItemsControl& control,
        const Style* value) noexcept;
    static void RefreshDisplayMemberPath(
        Aero::Controls::ItemsControl& control) noexcept;
    static Base::Result<Aero::Controls::ItemContainerGenerator*> CreateItemContainerGenerator(
        ElementTree& tree,
        LayoutEngine& layout,
        Meta::EffectiveValueEngine& values,
        StyleEngine* styles,
        Render::RenderTree* renderer,
        Aero::Controls::TemplateEngine* templates,
        Aero::Controls::ItemSubtreeCallback callback,
        void* context) noexcept;

private:
    ::Aero::Media::Visual* owner_ = nullptr;
};

template<>
struct FacetTrait<InteractionStateFacet> {
    static constexpr std::uint32_t Id = static_cast<std::uint32_t>(FacetId::Interaction);
    static constexpr FacetType Type = FacetType::Interaction;
};

} // namespace Aero::Core
