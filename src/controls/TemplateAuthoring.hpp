#pragma once

#include <Aero/Styling.hpp>
#include <Aero/Data.hpp>
#include <Aero/Visual.hpp>
#include <type_traits>

namespace Aero::Detail { class ControlRuntimeAccess; }

namespace Aero::Controls {

class ContentPresenter;
class ItemsPanelTemplate;
class ItemsPresenter;

class TemplateBuildContext final {
public:
    Base::Result<void> SetRoot(Base::Ref<Base::Object> owner, Visual& root) noexcept;
    Base::Result<void> SetRoot(Base::StringView name, Base::Ref<Base::Object> owner, Visual& root) noexcept;
    Base::Result<void> AddPart(Base::StringView name, Visual& parent, Base::Ref<Base::Object> owner, Visual& part) noexcept;
    Base::Result<bool> ProjectContent(ContentControl& owner, ContentPresenter& presenter) noexcept;
    Base::Result<bool> ProjectContent(ContentControl& owner, ContentControl& presenter) noexcept;
    Control& TemplatedParent() const noexcept;
    Visual* RootVisual() const noexcept;
    UIElement* RootElement() const noexcept;

private:
    friend class Aero::Detail::ControlRuntimeAccess;
    explicit TemplateBuildContext(void* state) noexcept : state_(state) {}
    DependencyObject* FindObject(Base::StringView name) const noexcept;
    Base::Result<void> AddOwnedPart(Base::StringView name, Base::Ref<Base::Object> owner, Visual& visual, void* mount) noexcept;
    Base::Result<void> PopulateItemsPresenter(ItemsPresenter& presenter, const ItemsPanelTemplate* itemsPanel) noexcept;
    Base::Result<void> PopulateContentPresenter(ContentPresenter& presenter) noexcept;
    Base::Result<bool> ProjectContentCore(ContentControl& owner, Visual& presenterVisual, ContentPresenter* presenter, ContentControl* contentHost) noexcept;
    void Rollback() noexcept;
    void* state_ = nullptr;
};

using TemplateFactoryCallback = Base::Result<void> (*)(TemplateBuildContext& context, void* factoryContext) noexcept;

struct TemplateNamespace final {
    Base::String prefix;
    Base::String uri;
};

struct TemplateBindingPlan final {
    Base::String targetName;
    DependencyPropertyHandle sourceProperty;
    DependencyPropertyHandle targetProperty;
};

struct TemplateMetadataBindingPlan final {
    Base::String targetName;
    Base::String path;
    Base::String stringFormat;
    DependencyPropertyHandle targetProperty;
    Data::BindingMode mode = Data::BindingMode::OneWay;
    UpdateSourceTrigger updateSourceTrigger = UpdateSourceTrigger::PropertyChanged;
};

struct TemplateTriggerSetter final {
    Base::String targetName;
    DependencyPropertyHandle property;
    Core::PropertyValue value;
};

struct TemplateTriggerCondition final {
    Base::String sourceName;
    DependencyPropertyHandle property;
    Core::PropertyValue value;
};

struct TemplatePropertyTrigger final {
    Base::Vector<TemplateTriggerCondition> conditions;
    Base::Vector<TemplateTriggerSetter> setters;
};

namespace Detail {

struct TemplateProgram final {
    TemplateProgram() noexcept = default;
    TemplateProgram(TemplateFactoryCallback valueFactory, void* valueFactoryContext = nullptr) noexcept
        : factory(valueFactory), factoryContext(valueFactoryContext) {}

    Base::Result<void> Configure(TemplateFactoryCallback valueFactory, void* valueFactoryContext = nullptr, Base::Ref<Base::Object> valueFactoryOwner = {}) noexcept;
    Base::Result<void> SetBaseUri(const Base::ResourceUri& value) noexcept;
    Base::Result<void> TryAddNamespace(Base::StringView prefix, Base::StringView uri) noexcept;
    Base::Result<void> Seal() noexcept;
    Base::Result<void> FreezeRuntimePlan(Core::TypeId valueTargetType, Base::Vector<TemplateBindingPlan>&& valueBindings, Base::Vector<TemplateMetadataBindingPlan>&& valueMetadataBindings, Base::Vector<TemplatePropertyTrigger>&& valueTriggers, Base::Vector<VisualStateGroup>&& valueVisualStateGroups) noexcept;

    TemplateFactoryCallback factory = nullptr;
    void* factoryContext = nullptr;
    Base::Ref<Base::Object> factoryOwner;
    Base::ResourceUri baseUri;
    Base::Vector<TemplateNamespace> namespaces;
    Core::TypeId targetType = Core::InvalidTypeId;
    Base::Vector<TemplateBindingPlan> bindings;
    Base::Vector<TemplateMetadataBindingPlan> metadataBindings;
    Base::Vector<TemplatePropertyTrigger> triggers;
    Base::Vector<VisualStateGroup> visualStateGroups;
    bool sealed = false;
};

struct FrameworkTemplateState final {
    Core::TypeId targetType = Core::InvalidTypeId;
    TemplateProgram program;
    ResourceDictionary resources;
    Base::Vector<TemplateBindingPlan> bindings;
    Base::Vector<TemplateMetadataBindingPlan> metadataBindings;
    Base::Vector<TemplatePropertyTrigger> triggers;
    Base::Vector<VisualStateGroup> visualStateGroups;
    Base::Vector<Base::Ref<Base::Object>> authoredTriggers;
    Base::Ref<Base::Object> authoredVisualTree;
    Base::Vector<Base::Ref<Base::Object>> authoredVisualStateGroups;
    NameScope authoredNames;
    std::uint32_t generatedNameSequence = 0U;
    bool sealed = false;
};

} // namespace Detail
} // namespace Aero::Controls
