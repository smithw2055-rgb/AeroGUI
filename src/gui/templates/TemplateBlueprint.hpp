#pragma once

// Compiled control and data template blueprint.
#include "gui/markup/XamlObjectWriterState.hpp"

// ===== TemplateCompiler contract =====

// Private template compiler used by ObjectWriter finalization.



#include <Aero/Controls/ControlTemplate.hpp>
#include <Aero/Controls.hpp>

#include <Aero/Triggers.hpp>




namespace Aero::Markup {

struct TemplatePrototypeProperty {
    Meta::DependencyPropertyHandle property;
    Meta::Value value;
    // A dependency-object property participating in a template Binding is
    // cloned as part of the instance graph rather than shared with the
    // authored prototype (for example SolidColorBrush.Color in a DataTemplate).
    std::uint32_t objectNode = UINT32_MAX;
};

struct TemplatePrototypeGradientStop {
    double offset = 0.0;
    ::Aero::Base::Color color{};
    // EnsureAuthoredName / x:Name for a TemplatedParent Binding on this stop.
    // Apply clones a new GradientStop into the brush and registers this name
    // so AttachMetadataBindings FindTarget can resolve the bound child.
    Base::String name;
};

struct TemplatePrototypeNode {
    Meta::TypeId type = Meta::InvalidTypeId;
    Base::String name;
    std::uint32_t parent = UINT32_MAX;
    Meta::MemberId contentMember = Meta::InvalidMemberId;
    Base::Vector<TemplatePrototypeProperty> properties;
    Base::Vector<::Aero::GridLength> gridColumns;
    Base::Vector<::Aero::GridLength> gridRows;
    Base::String streamGeometryData;
    Base::Vector<TemplatePrototypeGradientStop> gradientStops;
    Base::Vector<Base::Ref<Base::Object>> authoredBehaviors;
    Base::Vector<Base::Ref<Base::Object>> authoredTriggers;
};

struct TemplatePrototypeBinding {
    std::uint32_t target = UINT32_MAX;
    std::uint32_t source = UINT32_MAX;
    // resolved from the templated parent's document NameScope for each
    // ControlTemplate instance. Internal names continue to use source above.
    Base::String sourceName;
    // FindAncestor sources are instance-relative and cannot be captured from
    // the authored prototype graph. Resolve them after the cloned visual tree
    // has been connected.
    Base::String relativeAncestorType;
    std::uint32_t relativeAncestorLevel = 0U;
    ::Aero::Meta::Registry* metadata = nullptr;
    Meta::DependencyPropertyHandle targetProperty;
    Meta::DependencyPropertyHandle dataContextProperty;
    Base::String path;
    Base::String stringFormat;
    bool bindsToSource = false;
    Data::BindingMode mode =
        Data::BindingMode::Default;
    Meta::UpdateSourceTrigger updateSourceTrigger =
        Meta::UpdateSourceTrigger::PropertyChanged;
    Base::Ref<Data::IValueConverter> converter;
    Meta::PropertyValue converterParameter;
};

struct CompiledTemplateBlueprint {
    ::Aero::Meta::Registry* runtime = nullptr;
    Meta::DependencyPropertyRegistry* properties = nullptr;
    Base::Vector<TemplatePrototypeNode> nodes;
    Base::Vector<TemplatePrototypeBinding> bindings;
    Base::Vector<Base::Ref<Aero::TriggerBase>>
        dataTemplateTriggers;
    // Non-property triggers are retained on the compiled blueprint so every
    // control-template instance can materialize its own sources, name scope,
    // subscriptions, and animation actions.
    Base::Vector<Base::Ref<Aero::TriggerBase>>
        controlTemplateDataTriggers;
    Base::Vector<Base::Ref<Media::Animation::EventTrigger>>
        controlTemplateEventTriggers;
    std::uint32_t contentPresenter = UINT32_MAX;
};

struct CompiledTemplateDefinition {
    Meta::TypeId targetType = Meta::InvalidTypeId;
    CompiledTemplateBlueprint blueprint;
    Base::Vector<Controls::TemplatePropertyTrigger>
        propertyTriggers;
    Base::Vector<Controls::TemplateBindingPlan>
        contentSourceBindings;
    Base::Vector<Controls::VisualStateGroupPlan>
        visualStateGroups;
};

Base::Result<void> BuildCompiledTemplate(
    Controls::TemplateBuilder& context,
    void* factoryContext) noexcept;

Base::Result<Base::Ref<Base::Object>>
BuildCompiledDeferredTemplate(
    const Base::Ref<Base::Object>& payload,
    void* factoryContext,
    Aero::BindingEngine* bindings) noexcept;

Base::Result<CompiledTemplateBlueprint>
CompileDeferredTemplateBlueprint(
    const Base::Ref<Base::Object>& visualTree,
    const Aero::NameScope* names,
    Base::Span<const DeferredContentEdge> edges,
    Base::Span<const DeferredBindingEdge> bindings,
    ::Aero::Meta::Registry& runtime,
    Meta::DependencyPropertyRegistry& properties) noexcept;

Base::Result<CompiledTemplateDefinition>
CompileControlTemplateDefinition(
    Controls::ControlTemplate& controlTemplate,
    Base::Span<const DeferredContentEdge> edges,
    Base::Span<const DeferredBindingEdge> bindings,
    ::Aero::Meta::Registry& runtime,
    Meta::DependencyPropertyRegistry& properties) noexcept;

// BasedOn inheritance for tree-less derived templates: compiles
// derived-authored property triggers / visual states against the sealed base
// blueprint and stages them on the derived template. Seal() then inherits
// the base factory and prepends base plans.
Base::Result<void>
CompileInheritedControlTemplate(
    Controls::ControlTemplate& derivedTemplate,
    const CompiledTemplateBlueprint& baseBlueprint,
    ::Aero::Meta::Registry& runtime,
    Meta::DependencyPropertyRegistry& properties) noexcept;

} // namespace Aero::Markup

// Markup implementation sources historically referred to the template
// compiler contracts through Markup. Keep that source-only bridge
// while the canonical declarations live in Aero::Base; no aliases are
// exposed by installed headers.
namespace Aero::Markup {
using ::Aero::Markup::CompiledTemplateBlueprint;
using ::Aero::Markup::CompiledTemplateDefinition;
using ::Aero::Markup::BuildCompiledTemplate;
using ::Aero::Markup::BuildCompiledDeferredTemplate;
using ::Aero::Markup::CompileDeferredTemplateBlueprint;
using ::Aero::Markup::CompileControlTemplateDefinition;
using ::Aero::Markup::CompileInheritedControlTemplate;
}
