#pragma once

#include "../runtime/RuntimeFwd.hpp"
#include <cstdint>
#include <Aero/Styling.hpp>
#include <Aero/Data.hpp>
#include <Aero/Visual.hpp>
#include <type_traits>
#include "../ui/MountService.hpp"
#include <Aero/Controls/Items.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Base/Result.hpp>

namespace Aero::Controls::Detail {

struct TemplateHandle final {
    std::uint64_t value = 0U;
    constexpr bool IsValid() const noexcept { return value != 0U; }
};

} // namespace Aero::Controls::Detail

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

namespace Aero::Controls { class ContentPresenter; }

namespace Aero::Controls::Detail {

struct TemplatePart final {
    Base::String name;
    Base::Ref<Base::Object> owner;
    Visual* visual = nullptr;
    DependencyObject* object = nullptr;
    FrameworkElement* frameworkElement = nullptr;
    Aero::Detail::MountEdgeState mount;
};

struct TemplateContentProjection final {
    ContentControl* owner = nullptr;
    ContentPresenter* presenter = nullptr;
    ContentControl* contentHost = nullptr;
    UIElement* content = nullptr;
    Visual* originalVisualParent = nullptr;
    Aero::Detail::UiMountState projectedMount;
    bool attachedLogical = false;
    bool detachedOriginalVisual = false;
};

struct TemplateBuildState final {
    TemplateBuildState(
        ObjectTree& tree,
        Control& parent,
        Aero::Detail::LayoutManager* layout,
        Render::RenderTree* renderer) noexcept
        : tree(&tree),
          layout(layout),
          renderer(renderer),
          mounts(tree, layout, renderer),
          parent(&parent) {}

    ObjectTree* tree = nullptr;
    Aero::Detail::LayoutManager* layout = nullptr;
    Render::RenderTree* renderer = nullptr;
    Aero::Detail::MountService mounts;
    Control* parent = nullptr;
    Visual* rootVisual = nullptr;
    UIElement* rootElement = nullptr;
    Base::Vector<TemplatePart> parts;
    Base::Vector<TemplateContentProjection> projections;
};


} // namespace Aero::Controls::Detail

namespace Aero::Controls::Detail {

using DeferredObjectFactory = Base::Result<Base::Ref<Base::Object>> (*)(const Base::Ref<Base::Object>& item, void* context) noexcept;

struct DeferredObjectProgram final {
    Base::Result<void> Configure(DeferredObjectFactory factory, void* context = nullptr) noexcept;
    Base::Result<void> Configure(DeferredObjectFactory factory, void* context, Base::Ref<Base::Object> factoryOwner) noexcept;
    Base::Result<void> SetBaseUri(const Base::ResourceUri& value) noexcept;
    Base::Result<void> Seal() noexcept;
    Base::Result<Base::Ref<Base::Object>> Instantiate(const Base::Ref<Base::Object>& payload = {}) const noexcept;

    DeferredObjectFactory factory = nullptr;
    void* context = nullptr;
    Base::Ref<Base::Object> factoryOwner;
    Base::ResourceUri baseUri;
    bool sealed = false;
};

struct DataTemplateState final {
    DeferredObjectProgram program;
    TypeId dataType = InvalidTypeId;
    Base::Ref<Base::Object> hierarchicalItemsSource;
    Base::Ref<Base::Object> hierarchicalItemTemplate;
    ResourceDictionary resources;
    Base::Ref<Base::Object> authoredVisualTree;
    Base::Vector<Base::Ref<Aero::TriggerBase>> authoredTriggers;
    Aero::NameScope authoredNames;
};

struct ItemsPanelTemplateState final {
    DeferredObjectProgram program;
    ResourceDictionary resources;
    Base::Ref<Base::Object> authoredVisualTree;
};

} // namespace Aero::Controls::Detail

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

} // namespace Aero::Controls::Detail

namespace Aero::Controls::Detail {

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

namespace Aero::Controls::Detail {

class VisualStateManagerAccess final {
public:
    static Base::Result<VisualStateManager*> Create(
        Core::EffectiveValueEngine& values,
        TemplateManager& templates,
        Aero::Detail::AnimationManager& animations,
        Core::DependencyPropertyRegistry& properties) noexcept;
    static Base::Result<bool> GoToState(VisualStateManager& manager, Control& control, Base::StringView groupName, Base::StringView stateName, bool useTransitions = true) noexcept;
    static Base::Result<bool> ClearState(VisualStateManager& manager, Control& control, Base::StringView groupName) noexcept;
    static Base::Result<std::uint32_t> Clear(VisualStateManager& manager, Control& control) noexcept;
    static Base::StringView GetCurrentState(const VisualStateManager& manager, const Control& control, Base::StringView groupName) noexcept;
};

} // namespace Aero::Controls::Detail
