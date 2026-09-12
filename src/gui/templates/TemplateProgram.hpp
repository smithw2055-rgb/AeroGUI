#pragma once

#include <Aero/Base/Result.hpp>
#include <Aero/Controls.hpp>
#include <Aero/Data/Binding.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Controls/ControlTemplate.hpp>
#include <Aero/Visual.hpp>
#include <Aero/Media/Animation/Storyboard.hpp>
#include <Aero/Media/Animation/EasingFunctionBase.hpp>
#include "gui/controls/VisualStateManagerExecution.hpp"

#include <cstdint>

namespace Aero { class BindingEngine; }

namespace Aero::Controls {

struct TemplateHandle {
    std::uint64_t value = 0U;
    constexpr bool IsValid() const noexcept { return value != 0U; }
};

struct VisualStateSetterPlan {
    Base::String targetName;
    DependencyPropertyHandle property;
    Meta::PropertyValue value;
};

struct VisualStatePlan {
    Base::String name;
    Base::Vector<VisualStateSetterPlan> setters;
    Base::Ref<Media::Animation::Storyboard> storyboard;
};

struct VisualTransitionPlan {
    Base::String from;
    Base::String to;
    Media::Animation::AnimationTime generatedDurationMicroseconds = 0U;
    Base::Ref<Media::Animation::EasingFunctionBase>
        generatedEasingFunction;
    Base::Ref<Media::Animation::Storyboard> storyboard;
};

struct VisualStateGroupPlan {
    Base::String name;
    Base::Vector<VisualStatePlan> states;
    Base::Vector<VisualTransitionPlan> transitions;
};

} // namespace Aero::Controls

namespace Aero { class VisualStateManager; class AnimationEngine; }
namespace Aero::Controls { class TemplateEngine; class Control; }

namespace Aero::Controls {

class ContentPresenter;
class ItemsPanelTemplate;
class ItemsPresenter;

class TemplateBuilder {
public:
    Base::Result<void> SetRoot(Base::Ref<Base::Object> owner, ::Aero::Media::Visual& root) noexcept;
    Base::Result<void> SetRoot(Base::StringView name, Base::Ref<Base::Object> owner, ::Aero::Media::Visual& root) noexcept;
    Base::Result<void> AddPart(Base::StringView name, ::Aero::Media::Visual& parent, Base::Ref<Base::Object> owner, ::Aero::Media::Visual& part) noexcept;
    Base::Result<void> AddObjectPart(
        Base::StringView name,
        Base::Ref<Base::Object> owner,
        DependencyObject& object) noexcept;
    Base::Result<bool> ProjectContent(ContentControl& owner, ContentPresenter& presenter) noexcept;
    Base::Result<bool> ProjectContent(ContentControl& owner, ContentControl& presenter) noexcept;
    Control& TemplatedParent() const noexcept;
    ::Aero::Media::Visual* RootVisual() const noexcept;
    UIElement* RootElement() const noexcept;
    Aero::BindingEngine& Bindings() const noexcept;

private:
    friend class Aero::Controls::TemplateEngine;
    friend struct Aero::Controls::FrameworkTemplateState;
    explicit TemplateBuilder(void* state) noexcept : state_(state) {}
    DependencyObject* FindObject(Base::StringView name) const noexcept;
    Base::Result<void> AddOwnedPart(Base::StringView name, Base::Ref<Base::Object> owner, ::Aero::Media::Visual& visual, void* mount) noexcept;
    Base::Result<void> PopulateItemsPresenter(ItemsPresenter& presenter, const ItemsPanelTemplate* itemsPanel) noexcept;
    Base::Result<void> PopulateContentPresenter(ContentPresenter& presenter) noexcept;
    Base::Result<bool> ProjectContentCore(ContentControl& owner, ::Aero::Media::Visual& presenterVisual, ContentPresenter* presenter, ContentControl* contentHost) noexcept;
    void Rollback() noexcept;
    void* state_ = nullptr;
};

using TemplateFactoryCallback = Base::Result<void> (*)(TemplateBuilder& context, void* factoryContext) noexcept;

struct TemplateNamespace {
    Base::String prefix;
    Base::String uri;
};

struct TemplateBindingPlan {
    Base::String targetName;
    DependencyPropertyHandle sourceProperty;
    DependencyPropertyHandle targetProperty;
};

struct TemplateMetadataBindingPlan {
    Base::String targetName;
    Base::String path;
    Base::String stringFormat;
    DependencyPropertyHandle targetProperty;
    Data::BindingMode mode = Data::BindingMode::Default;
    UpdateSourceTrigger updateSourceTrigger = UpdateSourceTrigger::PropertyChanged;
    Base::Ref<Data::IValueConverter> converter;
    Meta::PropertyValue converterParameter;
};

struct TemplateDynamicResourcePlan {
    Base::String targetName;
    Base::String key;
    DependencyPropertyHandle targetProperty;
};

struct TemplateTriggerSetter {
    Base::String targetName;
    DependencyPropertyHandle property;
    Meta::PropertyValue value;
};

struct TemplateTriggerCondition {
    Base::String sourceName;
    DependencyPropertyHandle property;
    Meta::PropertyValue value;
    // Per-condition eval (ToggleButton IsChecked null sentinel included).
    bool IsMet(
        DependencyObject& source,
        const Meta::PropertyValue& current) const noexcept;
};

struct TemplatePropertyTrigger {
    Base::Vector<TemplateTriggerCondition> conditions;
    Base::Vector<TemplateTriggerSetter> setters;
};

} // namespace Aero::Controls

namespace Aero { class FrameworkTemplate; }

namespace Aero::Controls {

struct TemplateProgram {
    TemplateProgram() noexcept = default;
    TemplateProgram(TemplateFactoryCallback valueFactory, void* valueFactoryContext = nullptr) noexcept
        : factory(valueFactory), factoryContext(valueFactoryContext) {}

    Base::Result<void> Configure(TemplateFactoryCallback valueFactory, void* valueFactoryContext = nullptr, Base::Ref<Base::Object> valueFactoryOwner = {}) noexcept;
    Base::Result<void> SetBaseUri(const Base::ResourceUri& value) noexcept;
    Base::Result<void> AddNamespace(Base::StringView prefix, Base::StringView uri) noexcept;
    Base::Result<void> Seal() noexcept;
    Base::Result<void> FreezeRuntimePlan(Meta::TypeId valueTargetType, Base::Vector<TemplateBindingPlan>&& valueBindings, Base::Vector<TemplateMetadataBindingPlan>&& valueMetadataBindings, Base::Vector<TemplateDynamicResourcePlan>&& valueDynamicResources, Base::Vector<TemplatePropertyTrigger>&& valueTriggers, Base::Vector<VisualStateGroupPlan>&& valueVisualStateGroups) noexcept;

    TemplateFactoryCallback factory = nullptr;
    void* factoryContext = nullptr;
    Base::Ref<Base::Object> factoryOwner;
    Base::ResourceUri baseUri;
    Base::Vector<TemplateNamespace> namespaces;
    Meta::TypeId targetType = Meta::InvalidTypeId;
    Base::Vector<TemplateBindingPlan> bindings;
    Base::Vector<TemplateMetadataBindingPlan> metadataBindings;
    Base::Vector<TemplateDynamicResourcePlan> dynamicResources;
    Base::Vector<TemplatePropertyTrigger> triggers;
    Base::Vector<VisualStateGroupPlan> visualStateGroups;
    bool sealed = false;
};

struct DataTemplateState;
struct ItemsPanelTemplateState;
struct TemplateBuildState;

using DeferredObjectFactory = Base::Result<Base::Ref<Base::Object>> (*)(
    const Base::Ref<Base::Object>& item, void* context,
    Aero::BindingEngine* bindings) noexcept;

struct FrameworkTemplateState {

    static DataTemplateState* State(DataTemplate& value) noexcept;
    static const DataTemplateState* State(const DataTemplate& value) noexcept;
    static ItemsPanelTemplateState* State(ItemsPanelTemplate& value) noexcept;
    static const ItemsPanelTemplateState* State(const ItemsPanelTemplate& value) noexcept;
    static Base::Result<void> Configure(DataTemplate& value, DeferredObjectFactory factory, void* context = nullptr, Base::Ref<Base::Object> owner = {}) noexcept;
    static Base::Result<void> Configure(ItemsPanelTemplate& value, DeferredObjectFactory factory, void* context = nullptr, Base::Ref<Base::Object> owner = {}) noexcept;
    static Base::Result<void> SetBaseUri(DataTemplate& value, const Base::ResourceUri& uri) noexcept;
    static Base::Result<void> SetBaseUri(ItemsPanelTemplate& value, const Base::ResourceUri& uri) noexcept;
    static const Base::ResourceUri& BaseUri(const DataTemplate& value) noexcept;
    static const Base::ResourceUri& BaseUri(const ItemsPanelTemplate& value) noexcept;
    static Base::Result<void> SetAuthoredVisualTree(DataTemplate& value, const Base::Ref<Base::Object>& tree) noexcept;
    static Base::Result<void> SetAuthoredVisualTree(ItemsPanelTemplate& value, const Base::Ref<Base::Object>& tree) noexcept;
    static void ClearAuthoredVisualTree(DataTemplate& value) noexcept;
    static void ClearAuthoredVisualTree(ItemsPanelTemplate& value) noexcept;
    static Base::Result<void> AddAuthoredTrigger(DataTemplate& value, Base::Ref<Aero::TriggerBase> trigger) noexcept;
    static void ClearAuthoredTriggers(DataTemplate& value) noexcept;
    static Base::Span<const Base::Ref<Aero::TriggerBase>> AuthoredTriggers(const DataTemplate& value) noexcept;
    static Base::Result<void> RegisterAuthoredName(DataTemplate& value, Base::StringView name, Base::Object& object) noexcept;
    static void ClearAuthoredNames(DataTemplate& value) noexcept;
    static const Aero::NameScope& AuthoredNames(const DataTemplate& value) noexcept;
    static const Base::Ref<Base::Object>& AuthoredVisualTree(const DataTemplate& value) noexcept;
    static const Base::Ref<Base::Object>& AuthoredVisualTree(const ItemsPanelTemplate& value) noexcept;
    static Base::Result<void> Seal(DataTemplate& value) noexcept;
    static Base::Result<void> Seal(ItemsPanelTemplate& value) noexcept;
    static Base::Result<Base::Ref<Base::Object>> Instantiate(
        const DataTemplate& value, const Base::Ref<Base::Object>& item,
        Aero::BindingEngine* bindings = nullptr) noexcept;
    static Base::Result<Base::Ref<Base::Object>> Instantiate(const ItemsPanelTemplate& value) noexcept;

    static FrameworkTemplateState* State(FrameworkTemplate& value) noexcept;
    static const FrameworkTemplateState* State(const FrameworkTemplate& value) noexcept;
    static Base::Result<void> SetTargetType(FrameworkTemplate& value, Meta::TypeId type) noexcept;
    static Base::Result<void> SetBasedOn(FrameworkTemplate& value, ::Aero::FrameworkTemplate* basedOn) noexcept;
    static Base::Result<void> SetBasedOn(FrameworkTemplate& value, Base::Ref<Base::Object> basedOn) noexcept;
    static ::Aero::FrameworkTemplate* BasedOn(FrameworkTemplate& value) noexcept;
    static const ::Aero::FrameworkTemplate* BasedOn(const FrameworkTemplate& value) noexcept;
    static Base::Result<void> ConfigureFactory(FrameworkTemplate& value, TemplateFactoryCallback factory, void* context = nullptr, Base::Ref<Base::Object> owner = {}) noexcept;
    static Base::Result<void> AddTemplateBinding(FrameworkTemplate& value, Base::StringView targetName, DependencyPropertyHandle sourceProperty, DependencyPropertyHandle targetProperty) noexcept;
    static Base::Result<void> AddTemplatedParentBinding(FrameworkTemplate& value, Base::StringView targetName, Base::StringView path, Base::StringView stringFormat, DependencyPropertyHandle targetProperty, Data::BindingMode mode, UpdateSourceTrigger updateSourceTrigger, const Base::Ref<Data::IValueConverter>& converter = {}, const Meta::PropertyValue& converterParameter = {}) noexcept;
    static Base::Result<void> AddDynamicResource(FrameworkTemplate& value, Base::StringView targetName, Base::StringView key, DependencyPropertyHandle targetProperty) noexcept;
    static Base::Result<void> AddPropertyTrigger(FrameworkTemplate& value, TemplatePropertyTrigger trigger) noexcept;
    static Base::Result<void> AddVisualStateGroup(FrameworkTemplate& value, VisualStateGroupPlan group) noexcept;
    static Base::Result<void> AddAuthoredTrigger(FrameworkTemplate& value, Base::Ref<Base::Object> trigger) noexcept;
    static Base::Result<void> SetAuthoredVisualTree(ControlTemplate& value, const Base::Ref<Base::Object>& tree) noexcept;
    static Base::Result<void> AddAuthoredVisualStateGroup(ControlTemplate& value, const Base::Ref<Base::Object>& group) noexcept;
    static void ClearAuthoredVisualTree(ControlTemplate& value) noexcept;
    static void ClearAuthoredVisualStateGroups(ControlTemplate& value) noexcept;
    static void ClearAuthoredTriggers(FrameworkTemplate& value) noexcept;
    static Base::Result<void> RegisterAuthoredName(ControlTemplate& value, Base::StringView name, Base::Object& object) noexcept;
    static Base::Result<Base::String> EnsureAuthoredName(ControlTemplate& value, Base::Object& object) noexcept;
    static void ClearAuthoredNames(ControlTemplate& value) noexcept;
    static const Base::Ref<Base::Object>& AuthoredVisualTree(const ControlTemplate& value) noexcept;
    static Base::Span<const Base::Ref<Base::Object>> AuthoredVisualStateGroups(const ControlTemplate& value) noexcept;
    static const NameScope& AuthoredNames(const ControlTemplate& value) noexcept;
    static Base::Span<const Base::Ref<Base::Object>> AuthoredTriggers(const FrameworkTemplate& value) noexcept;
    static TemplateFactoryCallback Factory(const FrameworkTemplate& value) noexcept;
    static void* FactoryContext(const FrameworkTemplate& value) noexcept;
    static const Base::Ref<Base::Object>& FactoryOwner(const FrameworkTemplate& value) noexcept;
    static const Base::ResourceUri& BaseUri(const FrameworkTemplate& value) noexcept;
    static Base::Result<void> SetBaseUri(FrameworkTemplate& value, const Base::ResourceUri& uri) noexcept;
    static Base::Result<void> AddNamespace(FrameworkTemplate& value, Base::StringView prefix, Base::StringView uri) noexcept;
    static Base::Span<const TemplateNamespace> Namespaces(const FrameworkTemplate& value) noexcept;
    static Base::Span<const TemplateBindingPlan> Bindings(const FrameworkTemplate& value) noexcept;
    static Base::Span<const TemplateMetadataBindingPlan> MetadataBindings(const FrameworkTemplate& value) noexcept;
    static Base::Span<const TemplateDynamicResourcePlan> DynamicResources(const FrameworkTemplate& value) noexcept;
    static Base::Span<const TemplatePropertyTrigger> Triggers(const FrameworkTemplate& value) noexcept;
    static Base::Span<const VisualStateGroupPlan> VisualStateGroups(const FrameworkTemplate& value) noexcept;
    static Base::Result<void> Seal(FrameworkTemplate& value, const Meta::DependencyPropertyRegistry& properties) noexcept;
    // Expand a sealed ControlTemplate into buildState (factory + presenters).
    // TemplateEngine keeps instance registry / bindings / trigger Flush.
    static Base::Result<void> Materialize(
        const ControlTemplate& plan,
        TemplateBuildState& buildState,
        TemplateBuilder& context,
        const Meta::DependencyPropertyRegistry& properties) noexcept;

    // VisualStateManager execution path (forwarding to VisualStateManagerExecution).
    static Base::Result<::Aero::VisualStateManager*> CreateVisualStateManager(
        Meta::EffectiveValueEngine& values,
        ::Aero::Controls::TemplateEngine& templates,
        ::Aero::AnimationEngine& animations,
        Meta::DependencyPropertyRegistry& properties) noexcept {
        return VisualStateManagerExecution::Create(values, templates, animations, properties);
    }
    static Base::Result<bool> GoToState(
        ::Aero::VisualStateManager& manager,
        ::Aero::Controls::Control& control,
        Base::StringView groupName,
        Base::StringView stateName,
        bool useTransitions = true) noexcept {
        return VisualStateManagerExecution::GoToState(
            manager, control, groupName, stateName, useTransitions);
    }
    static Base::Result<bool> ClearState(
        ::Aero::VisualStateManager& manager,
        ::Aero::Controls::Control& control,
        Base::StringView groupName) noexcept {
        return VisualStateManagerExecution::ClearState(manager, control, groupName);
    }
    static Base::Result<std::uint32_t> Clear(
        ::Aero::VisualStateManager& manager,
        ::Aero::Controls::Control& control) noexcept {
        return VisualStateManagerExecution::Clear(manager, control);
    }
    static Base::StringView CurrentState(
        const ::Aero::VisualStateManager& manager,
        const ::Aero::Controls::Control& control,
        Base::StringView groupName) noexcept {
        return VisualStateManagerExecution::CurrentState(manager, control, groupName);
    }

    Meta::TypeId targetType = Meta::InvalidTypeId;
    TemplateProgram program;
    ResourceDictionary resources;
    // Optional inheritance link. When set, Seal() requires the base template
    // to be sealed first, inherits its factory when this template authors no
    // VisualTree, and prepends its compiled plans base-first. The link is
    // retained after sealing so GetBasedOn() keeps working (Style parity).
    ::Aero::FrameworkTemplate* basedOn = nullptr;
    Base::Ref<Base::Object> basedOnOwner;
    Base::Vector<TemplateBindingPlan> bindings;
    Base::Vector<TemplateMetadataBindingPlan> metadataBindings;
    Base::Vector<TemplateDynamicResourcePlan> dynamicResources;
    Base::Vector<TemplatePropertyTrigger> triggers;
    Base::Vector<VisualStateGroupPlan> visualStateGroups;
    Base::Vector<Base::Ref<Base::Object>> authoredTriggers;
    Base::Ref<Base::Object> authoredVisualTree;
    Base::Vector<Base::Ref<Base::Object>> authoredVisualStateGroups;
    NameScope authoredNames;
    std::uint32_t generatedNameSequence = 0U;
    bool sealed = false;
};

struct DeferredObjectProgram {
    Base::Result<void> Configure(DeferredObjectFactory factory, void* context = nullptr) noexcept;
    Base::Result<void> Configure(DeferredObjectFactory factory, void* context, Base::Ref<Base::Object> factoryOwner) noexcept;
    Base::Result<void> SetBaseUri(const Base::ResourceUri& value) noexcept;
    Base::Result<void> Seal() noexcept;
    Base::Result<Base::Ref<Base::Object>> Instantiate(
        const Base::Ref<Base::Object>& payload = {},
        Aero::BindingEngine* bindings = nullptr) const noexcept;

    DeferredObjectFactory factory = nullptr;
    void* context = nullptr;
    Base::Ref<Base::Object> factoryOwner;
    Base::ResourceUri baseUri;
    bool sealed = false;
};

struct DataTemplateState {
    DeferredObjectProgram program;
    TypeId dataType = InvalidTypeId;
    Base::Ref<Base::Object> hierarchicalItemsSource;
    Base::Ref<Base::Object> hierarchicalItemTemplate;
    ResourceDictionary resources;
    Base::Ref<Base::Object> authoredVisualTree;
    Base::Vector<Base::Ref<Aero::TriggerBase>> authoredTriggers;
    Aero::NameScope authoredNames;
};

struct ItemsPanelTemplateState {
    DeferredObjectProgram program;
    ResourceDictionary resources;
    Base::Ref<Base::Object> authoredVisualTree;
};

} // namespace Aero::Controls
