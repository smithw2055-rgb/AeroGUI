#pragma once

#include "TemplateInstance.hpp"

namespace Aero::Controls::Detail {

class DeferredTemplateAccess final {
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
    static Base::Result<void> TryAddAuthoredTrigger(DataTemplate& value, Base::Ref<Aero::TriggerBase> trigger) noexcept;
    static void ClearAuthoredTriggers(DataTemplate& value) noexcept;
    static Base::Span<const Base::Ref<Aero::TriggerBase>> AuthoredTriggers(const DataTemplate& value) noexcept;
    static Base::Result<void> RegisterAuthoredName(DataTemplate& value, Base::StringView name, Base::Object& object) noexcept;
    static void ClearAuthoredNames(DataTemplate& value) noexcept;
    static const Aero::NameScope& AuthoredNames(const DataTemplate& value) noexcept;
    static const Base::Ref<Base::Object>& AuthoredVisualTree(const DataTemplate& value) noexcept;
    static const Base::Ref<Base::Object>& AuthoredVisualTree(const ItemsPanelTemplate& value) noexcept;
    static Base::Result<void> Seal(DataTemplate& value) noexcept;
    static Base::Result<void> Seal(ItemsPanelTemplate& value) noexcept;
    static Base::Result<Base::Ref<Base::Object>> Instantiate(const DataTemplate& value, const Base::Ref<Base::Object>& item) noexcept;
    static Base::Result<Base::Ref<Base::Object>> Instantiate(const ItemsPanelTemplate& value) noexcept;
};

class FrameworkTemplateAccess final {
public:
    static FrameworkTemplateState* State(FrameworkTemplate& value) noexcept;
    static const FrameworkTemplateState* State(const FrameworkTemplate& value) noexcept;
    static Base::Result<void> TrySetTargetType(FrameworkTemplate& value, Core::TypeId type) noexcept;
    static Base::Result<void> ConfigureFactory(FrameworkTemplate& value, TemplateFactoryCallback factory, void* context = nullptr, Base::Ref<Base::Object> owner = {}) noexcept;
    static Base::Result<void> TryAddTemplateBinding(FrameworkTemplate& value, Base::StringView targetName, DependencyPropertyHandle sourceProperty, DependencyPropertyHandle targetProperty) noexcept;
    static Base::Result<void> TryAddTemplatedParentBinding(FrameworkTemplate& value, Base::StringView targetName, Base::StringView path, Base::StringView stringFormat, DependencyPropertyHandle targetProperty, Data::BindingMode mode, UpdateSourceTrigger updateSourceTrigger) noexcept;
    static Base::Result<void> TryAddPropertyTrigger(FrameworkTemplate& value, TemplatePropertyTrigger trigger) noexcept;
    static Base::Result<void> TryAddVisualStateGroup(FrameworkTemplate& value, VisualStateGroup group) noexcept;
    static Base::Result<void> TryAddAuthoredTrigger(FrameworkTemplate& value, Base::Ref<Base::Object> trigger) noexcept;
    static Base::Result<void> SetAuthoredVisualTree(ControlTemplate& value, const Base::Ref<Base::Object>& tree) noexcept;
    static Base::Result<void> TryAddAuthoredVisualStateGroup(ControlTemplate& value, const Base::Ref<Base::Object>& group) noexcept;
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
    static Base::Result<void> TryAddNamespace(FrameworkTemplate& value, Base::StringView prefix, Base::StringView uri) noexcept;
    static Base::Span<const TemplateNamespace> Namespaces(const FrameworkTemplate& value) noexcept;
    static Base::Span<const TemplateBindingPlan> Bindings(const FrameworkTemplate& value) noexcept;
    static Base::Span<const TemplateMetadataBindingPlan> MetadataBindings(const FrameworkTemplate& value) noexcept;
    static Base::Span<const TemplatePropertyTrigger> Triggers(const FrameworkTemplate& value) noexcept;
    static Base::Span<const VisualStateGroup> VisualStateGroups(const FrameworkTemplate& value) noexcept;
    static Base::Result<void> Seal(FrameworkTemplate& value, const Core::DependencyPropertyRegistry& properties) noexcept;
};

} // namespace Aero::Controls::Detail
