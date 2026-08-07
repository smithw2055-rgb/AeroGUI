#pragma once

#include <Aero/Base/Result.hpp>
#include <Aero/Controls/Items.hpp>
#include <Aero/Data.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Styling.hpp>
#include <Aero/Visual.hpp>

#include <cstdint>

namespace Aero::GuiPrivate::Detail { class BindingEngine; }

namespace Aero::Controls::Detail {

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

} // namespace Aero::Controls::Detail

namespace Aero::Controls::Detail { class TemplateEngine; }

namespace Aero::Controls {

class ContentPresenter;
class ItemsPanelTemplate;
class ItemsPresenter;

class TemplateBuilder {
public:
    Base::Result<void> SetRoot(Base::Ref<Base::Object> owner, Visual& root) noexcept;
    Base::Result<void> SetRoot(Base::StringView name, Base::Ref<Base::Object> owner, Visual& root) noexcept;
    Base::Result<void> AddPart(Base::StringView name, Visual& parent, Base::Ref<Base::Object> owner, Visual& part) noexcept;
    Base::Result<void> AddObjectPart(
        Base::StringView name,
        Base::Ref<Base::Object> owner,
        DependencyObject& object) noexcept;
    Base::Result<bool> ProjectContent(ContentControl& owner, ContentPresenter& presenter) noexcept;
    Base::Result<bool> ProjectContent(ContentControl& owner, ContentControl& presenter) noexcept;
    Control& TemplatedParent() const noexcept;
    Visual* RootVisual() const noexcept;
    UIElement* RootElement() const noexcept;
    Aero::GuiPrivate::Detail::BindingEngine& Bindings() const noexcept;

private:
    friend class Aero::Controls::Detail::TemplateEngine;
    explicit TemplateBuilder(void* state) noexcept : state_(state) {}
    DependencyObject* FindObject(Base::StringView name) const noexcept;
    Base::Result<void> AddOwnedPart(Base::StringView name, Base::Ref<Base::Object> owner, Visual& visual, void* mount) noexcept;
    Base::Result<void> PopulateItemsPresenter(ItemsPresenter& presenter, const ItemsPanelTemplate* itemsPanel) noexcept;
    Base::Result<void> PopulateContentPresenter(ContentPresenter& presenter) noexcept;
    Base::Result<bool> ProjectContentCore(ContentControl& owner, Visual& presenterVisual, ContentPresenter* presenter, ContentControl* contentHost) noexcept;
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
    Data::BindingMode mode = Data::BindingMode::OneWay;
    UpdateSourceTrigger updateSourceTrigger = UpdateSourceTrigger::PropertyChanged;
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
};

struct TemplatePropertyTrigger {
    Base::Vector<TemplateTriggerCondition> conditions;
    Base::Vector<TemplateTriggerSetter> setters;
};

} // namespace Aero::Controls

namespace Aero::Controls::Detail {

struct TemplateProgram {
    TemplateProgram() noexcept = default;
    TemplateProgram(TemplateFactoryCallback valueFactory, void* valueFactoryContext = nullptr) noexcept
        : factory(valueFactory), factoryContext(valueFactoryContext) {}

    Base::Result<void> Configure(TemplateFactoryCallback valueFactory, void* valueFactoryContext = nullptr, Base::Ref<Base::Object> valueFactoryOwner = {}) noexcept;
    Base::Result<void> SetBaseUri(const Base::ResourceUri& value) noexcept;
    Base::Result<void> AddNamespace(Base::StringView prefix, Base::StringView uri) noexcept;
    Base::Result<void> Seal() noexcept;
    Base::Result<void> FreezeRuntimePlan(Meta::TypeId valueTargetType, Base::Vector<TemplateBindingPlan>&& valueBindings, Base::Vector<TemplateMetadataBindingPlan>&& valueMetadataBindings, Base::Vector<TemplatePropertyTrigger>&& valueTriggers, Base::Vector<VisualStateGroupPlan>&& valueVisualStateGroups) noexcept;

    TemplateFactoryCallback factory = nullptr;
    void* factoryContext = nullptr;
    Base::Ref<Base::Object> factoryOwner;
    Base::ResourceUri baseUri;
    Base::Vector<TemplateNamespace> namespaces;
    Meta::TypeId targetType = Meta::InvalidTypeId;
    Base::Vector<TemplateBindingPlan> bindings;
    Base::Vector<TemplateMetadataBindingPlan> metadataBindings;
    Base::Vector<TemplatePropertyTrigger> triggers;
    Base::Vector<VisualStateGroupPlan> visualStateGroups;
    bool sealed = false;
};

struct FrameworkTemplateState {
    Meta::TypeId targetType = Meta::InvalidTypeId;
    TemplateProgram program;
    ResourceDictionary resources;
    Base::Vector<TemplateBindingPlan> bindings;
    Base::Vector<TemplateMetadataBindingPlan> metadataBindings;
    Base::Vector<TemplatePropertyTrigger> triggers;
    Base::Vector<VisualStateGroupPlan> visualStateGroups;
    Base::Vector<Base::Ref<Base::Object>> authoredTriggers;
    Base::Ref<Base::Object> authoredVisualTree;
    Base::Vector<Base::Ref<Base::Object>> authoredVisualStateGroups;
    NameScope authoredNames;
    std::uint32_t generatedNameSequence = 0U;
    bool sealed = false;
};

using DeferredObjectFactory = Base::Result<Base::Ref<Base::Object>> (*)(
    const Base::Ref<Base::Object>& item, void* context,
    Aero::GuiPrivate::Detail::BindingEngine* bindings) noexcept;

struct DeferredObjectProgram {
    Base::Result<void> Configure(DeferredObjectFactory factory, void* context = nullptr) noexcept;
    Base::Result<void> Configure(DeferredObjectFactory factory, void* context, Base::Ref<Base::Object> factoryOwner) noexcept;
    Base::Result<void> SetBaseUri(const Base::ResourceUri& value) noexcept;
    Base::Result<void> Seal() noexcept;
    Base::Result<Base::Ref<Base::Object>> Instantiate(
        const Base::Ref<Base::Object>& payload = {},
        Aero::GuiPrivate::Detail::BindingEngine* bindings = nullptr) const noexcept;

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

} // namespace Aero::Controls::Detail
