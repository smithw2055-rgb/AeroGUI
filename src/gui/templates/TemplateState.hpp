#pragma once

#include "gui/templates/TemplateInstance.hpp"
#include <Aero/VisualStateManager.hpp>

namespace Aero::Controls {
class TemplateEngine;
}

namespace Aero { class AnimationEngine; }

namespace Aero {

struct DataTemplateRuntime {
    static Controls::DataTemplateState* State(
        DataTemplate& value) noexcept;
    static const Controls::DataTemplateState* State(
        const DataTemplate& value) noexcept;
    static Base::Result<void> Configure(
        DataTemplate& value,
        Controls::DeferredObjectFactory factory,
        void* context = nullptr,
        Base::Ref<Base::Object> owner = {}) noexcept;
    static Base::Result<void> SetBaseUri(
        DataTemplate& value,
        const Base::ResourceUri& uri) noexcept;
    static const Base::ResourceUri& BaseUri(
        const DataTemplate& value) noexcept;
    static Base::Result<void> SetAuthoredVisualTree(
        DataTemplate& value,
        const Base::Ref<Base::Object>& tree) noexcept;
    static void ClearAuthoredVisualTree(
        DataTemplate& value) noexcept;
    static Base::Result<void> AddAuthoredTrigger(
        DataTemplate& value,
        Base::Ref<Aero::TriggerBase> trigger) noexcept;
    static void ClearAuthoredTriggers(
        DataTemplate& value) noexcept;
    static Base::Span<const Base::Ref<Aero::TriggerBase>>
        AuthoredTriggers(const DataTemplate& value) noexcept;
    static Base::Result<void> RegisterAuthoredName(
        DataTemplate& value,
        Base::StringView name,
        Base::Object& object) noexcept;
    static void ClearAuthoredNames(
        DataTemplate& value) noexcept;
    static const Aero::NameScope& AuthoredNames(
        const DataTemplate& value) noexcept;
    static const Base::Ref<Base::Object>& AuthoredVisualTree(
        const DataTemplate& value) noexcept;
    static Base::Result<void> Seal(
        DataTemplate& value) noexcept;
    static Base::Result<Base::Ref<Base::Object>> Instantiate(
        const DataTemplate& value,
        const Base::Ref<Base::Object>& item,
        Aero::BindingEngine* bindings = nullptr) noexcept;
};

} // namespace Aero

namespace Aero::Controls {

struct ItemsPanelTemplateRuntime {
    static ItemsPanelTemplateState* State(
        ItemsPanelTemplate& value) noexcept;
    static const ItemsPanelTemplateState* State(
        const ItemsPanelTemplate& value) noexcept;
    static Base::Result<void> Configure(
        ItemsPanelTemplate& value,
        DeferredObjectFactory factory,
        void* context = nullptr,
        Base::Ref<Base::Object> owner = {}) noexcept;
    static Base::Result<void> SetBaseUri(
        ItemsPanelTemplate& value,
        const Base::ResourceUri& uri) noexcept;
    static const Base::ResourceUri& BaseUri(
        const ItemsPanelTemplate& value) noexcept;
    static Base::Result<void> SetAuthoredVisualTree(
        ItemsPanelTemplate& value,
        const Base::Ref<Base::Object>& tree) noexcept;
    static void ClearAuthoredVisualTree(
        ItemsPanelTemplate& value) noexcept;
    static Base::Result<void> Seal(
        ItemsPanelTemplate& value) noexcept;
    static Base::Result<Base::Ref<Base::Object>> Instantiate(
        const ItemsPanelTemplate& value) noexcept;
    static const Base::Ref<Base::Object>& AuthoredVisualTree(
        const ItemsPanelTemplate& value) noexcept;
};

} // namespace Aero::Controls

namespace Aero {

struct FrameworkTemplateRuntime {
    static Controls::FrameworkTemplateState* State(
        FrameworkTemplate& value) noexcept;
    static const Controls::FrameworkTemplateState* State(
        const FrameworkTemplate& value) noexcept;
};

struct VisualStateManagerRuntime {
    static Base::Result<VisualStateManager*> Create(
        Meta::EffectiveValueEngine& values,
        ::Aero::Controls::TemplateEngine& templates,
        ::Aero::AnimationEngine& animations,
        Meta::DependencyPropertyRegistry& properties) noexcept;
    static void*& Runtime(
        VisualStateManager& value) noexcept {
        return value.impl_;
    }
    static const void* Runtime(
        const VisualStateManager& value) noexcept {
        return value.impl_;
    }
};

} // namespace Aero

namespace Aero::Controls {
class TemplateEngine;
class AnimationEngine;
}

namespace Aero::Controls {

using namespace ::Aero::Controls;
using namespace ::Aero::Controls;

// Single private entry point for template authoring, compiled template state
// and visual-state execution. It has no storage or lifetime of its own.
class TemplatePrivate {
public:
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
        static Base::Result<void> ConfigureFactory(FrameworkTemplate& value, TemplateFactoryCallback factory, void* context = nullptr, Base::Ref<Base::Object> owner = {}) noexcept;
        static Base::Result<void> AddTemplateBinding(FrameworkTemplate& value, Base::StringView targetName, DependencyPropertyHandle sourceProperty, DependencyPropertyHandle targetProperty) noexcept;
        static Base::Result<void> AddTemplatedParentBinding(FrameworkTemplate& value, Base::StringView targetName, Base::StringView path, Base::StringView stringFormat, DependencyPropertyHandle targetProperty, Data::BindingMode mode, UpdateSourceTrigger updateSourceTrigger) noexcept;
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

        static Base::Result<VisualStateManager*> Create(Meta::EffectiveValueEngine& values,
            Aero::Controls::TemplateEngine& templates, Aero::AnimationEngine& animations,
            Meta::DependencyPropertyRegistry& properties) noexcept;
        static Base::Result<bool> GoToState(VisualStateManager& manager, Control& control,
            Base::StringView groupName, Base::StringView stateName,
            bool useTransitions = true) noexcept;
        static Base::Result<bool> ClearState(VisualStateManager& manager, Control& control,
            Base::StringView groupName) noexcept;
        static Base::Result<std::uint32_t> Clear(VisualStateManager& manager,
            Control& control) noexcept;
        static Base::StringView GetCurrentState(const VisualStateManager& manager,
            const Control& control, Base::StringView groupName) noexcept;
};

} // namespace Aero::Controls
