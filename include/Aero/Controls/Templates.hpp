#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Controls/ControlPrimitives.hpp>
#include <Aero/Presentation/MountService.hpp>
#include <Aero/Presentation/ObjectTree.hpp>

namespace Aero::Presentation {
class LayoutManager;
class RenderManager;
}

namespace Aero::Controls {

using namespace Aero::Core;
using namespace Aero::Presentation;

class ContentPresenter;

struct TemplateHandle final {
    std::uint64_t value = 0U;
    constexpr bool IsValid() const noexcept {
        return value != 0U;
    }
};

struct TemplatePart final {
    Base::String name;
    Base::Ref<Base::Object> owner;
    Visual* visual = nullptr;
    DependencyObject* object = nullptr;
    FrameworkElement* frameworkElement = nullptr;
    MountEdgeState mount;
};

struct TemplateContentProjection final {
    ContentControl* owner = nullptr;
    ContentPresenter* presenter = nullptr;
    UIElement* content = nullptr;
    Visual* originalVisualParent = nullptr;
    PresentationMountState projectedMount;
    bool attachedLogical = false;
    bool detachedOriginalPresentation = false;
};

class AERO_API TemplateBuildContext final {
public:
    Base::Result<void> SetRoot(
        Base::Ref<Base::Object> owner,
        Visual& root) noexcept;
    Base::Result<void> SetRoot(
        Base::StringView name,
        Base::Ref<Base::Object> owner,
        Visual& root) noexcept;
    Base::Result<void> AddPart(
        Base::StringView name,
        Visual& parent,
        Base::Ref<Base::Object> owner,
        Visual& part) noexcept;
    // Projects a ContentControl's logical content into a template-owned
    // ContentPresenter without changing the logical parent.
    Base::Result<bool> ProjectContent(
        ContentControl& owner,
        ContentPresenter& presenter) noexcept;

    Control& TemplatedParent() const noexcept {
        return *parent_;
    }
    Visual* RootVisual() const noexcept {
        return rootVisual_;
    }
    UIElement* RootElement() const noexcept {
        return rootElement_;
    }
    Base::Span<const TemplatePart> Parts() const noexcept {
        return {parts_.Data(), parts_.Size()};
    }

private:
    friend class TemplateManager;

    TemplateBuildContext(
        ObjectTree& tree,
        Control& parent,
        LayoutManager* layout,
        RenderManager* renderer) noexcept
        : tree_(&tree),
          layout_(layout),
          renderer_(renderer),
          mounts_(tree, layout, renderer),
          parent_(&parent) {}

    DependencyObject* FindObject(
        Base::StringView name) const noexcept;
    Base::Result<void> AddOwnedPart(
        Base::StringView name,
        Base::Ref<Base::Object> owner,
        Visual& visual,
        MountEdgeState mount) noexcept;
    void Rollback() noexcept;

    ObjectTree* tree_ = nullptr;
    LayoutManager* layout_ = nullptr;
    RenderManager* renderer_ = nullptr;
    MountService mounts_;
    Control* parent_ = nullptr;
    Visual* rootVisual_ = nullptr;
    UIElement* rootElement_ = nullptr;
    Base::Vector<TemplatePart> parts_;
    Base::Vector<TemplateContentProjection> projections_;
};

using TemplateFactoryCallback = Base::Result<void> (*)(
    TemplateBuildContext& context,
    void* factoryContext) noexcept;

struct TemplateNamespace final {
    Base::String prefix;
    Base::String uri;
};

// Immutable deferred construction program shared by native and XAML-authored
// templates. Markup may retain node IR behind factoryContext; Controls only
// depends on the stable execution callback and origin/environment metadata.
class AERO_API TemplateProgram final {
public:
    TemplateProgram() noexcept = default;
    TemplateProgram(
        TemplateFactoryCallback factory,
        void* factoryContext = nullptr) noexcept
        : factory_(factory),
          factoryContext_(factoryContext) {}

    Base::Result<void> Configure(
        TemplateFactoryCallback factory,
        void* factoryContext = nullptr,
        Base::Ref<Base::Object> factoryOwner = {}) noexcept;
    Base::Result<void> SetBaseUri(
        const Base::ResourceUri& value) noexcept;
    Base::Result<void> TryAddNamespace(
        Base::StringView prefix,
        Base::StringView uri) noexcept;
    Base::Result<void> Seal() noexcept;

    TemplateFactoryCallback Factory() const noexcept {
        return factory_;
    }
    void* FactoryContext() const noexcept {
        return factoryContext_;
    }
    const Base::Ref<Base::Object>& FactoryOwner() const noexcept {
        return factoryOwner_;
    }
    const Base::ResourceUri& BaseUri() const noexcept {
        return baseUri_;
    }
    Base::Span<const TemplateNamespace>
    Namespaces() const noexcept {
        return {namespaces_.Data(), namespaces_.Size()};
    }
    bool IsSealed() const noexcept {
        return sealed_;
    }

private:
    TemplateFactoryCallback factory_ = nullptr;
    void* factoryContext_ = nullptr;
    Base::Ref<Base::Object> factoryOwner_;
    Base::ResourceUri baseUri_;
    Base::Vector<TemplateNamespace> namespaces_;
    bool sealed_ = false;
};

struct TemplateBindingPlan final {
    Base::String targetName;
    DependencyPropertyHandle sourceProperty;
    DependencyPropertyHandle targetProperty;
};

struct TemplateTriggerSetter final {
    Base::String targetName;
    DependencyPropertyHandle property;
    PropertyValue value;
};

struct TemplatePropertyTrigger final {
    DependencyPropertyHandle property;
    PropertyValue value;
    Base::Vector<TemplateTriggerSetter> setters;
};

struct VisualStateSetter final {
    Base::String targetName;
    DependencyPropertyHandle property;
    PropertyValue value;
};

struct VisualState final {
    Base::String name;
    Base::Vector<VisualStateSetter> setters;
};

struct VisualStateGroup final {
    Base::String name;
    Base::Vector<VisualState> states;
};

class AERO_API FrameworkTemplate : public Base::Object {
    AERO_TYPED_META(FrameworkTemplate, Base::Object)
public:
    FrameworkTemplate() noexcept = default;
    FrameworkTemplate(
        TypeId targetType,
        TemplateFactoryCallback factory,
        void* factoryContext = nullptr) noexcept
        : targetType_(targetType),
          program_(factory, factoryContext) {}

    FrameworkTemplate(const FrameworkTemplate&) = delete;
    FrameworkTemplate& operator=(const FrameworkTemplate&) = delete;

    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Base::Result<void> TrySetTargetType(
        TypeId value) noexcept;
    Base::Result<void> ConfigureProgram(
        TemplateFactoryCallback factory,
        void* factoryContext = nullptr,
        Base::Ref<Base::Object> factoryOwner = {}) noexcept;
    Base::Result<void> TryAddTemplateBinding(
        Base::StringView targetName,
        DependencyPropertyHandle sourceProperty,
        DependencyPropertyHandle targetProperty) noexcept;
    Base::Result<void> TryAddPropertyTrigger(
        TemplatePropertyTrigger trigger) noexcept;
    Base::Result<void> TryAddVisualStateGroup(
        VisualStateGroup group) noexcept;
    Base::Result<void> Seal(
        const DependencyPropertyRegistry& properties) noexcept;

    TypeId TargetType() const noexcept { return targetType_; }
    bool IsSealed() const noexcept { return sealed_; }
    TemplateFactoryCallback Factory() const noexcept {
        return program_.Factory();
    }
    void* FactoryContext() const noexcept {
        return program_.FactoryContext();
    }
    TemplateProgram& Program() noexcept {
        return program_;
    }
    const TemplateProgram& Program() const noexcept {
        return program_;
    }
    ResourceDictionary& Resources() noexcept {
        return resources_;
    }
    const ResourceDictionary& Resources() const noexcept {
        return resources_;
    }
    Base::Span<const TemplateBindingPlan> Bindings() const noexcept {
        return {bindings_.Data(), bindings_.Size()};
    }
    Base::Span<const TemplatePropertyTrigger> Triggers() const noexcept {
        return {triggers_.Data(), triggers_.Size()};
    }
    Base::Span<const VisualStateGroup> VisualStateGroups() const noexcept {
        return {visualStateGroups_.Data(), visualStateGroups_.Size()};
    }

private:
    TypeId targetType_ = InvalidTypeId;
    TemplateProgram program_;
    ResourceDictionary resources_;
    Base::Vector<TemplateBindingPlan> bindings_;
    Base::Vector<TemplatePropertyTrigger> triggers_;
    Base::Vector<VisualStateGroup> visualStateGroups_;
    bool sealed_ = false;
};

class AERO_API ControlTemplate final : public FrameworkTemplate {
    AERO_TYPED_META(ControlTemplate, FrameworkTemplate)
public:
    using FrameworkTemplate::FrameworkTemplate;

    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Base::Result<void> SetAuthoredVisualTree(
        const Base::Ref<Base::Object>& value) noexcept;
    const Base::Ref<Base::Object>&
    AuthoredVisualTree() const noexcept {
        return authoredVisualTree_;
    }
    void ClearAuthoredVisualTree() noexcept {
        authoredVisualTree_.Reset();
    }
    Base::Result<void> TryAddAuthoredVisualStateGroup(
        const Base::Ref<Base::Object>& value) noexcept;
    void ClearAuthoredVisualStateGroups() noexcept {
        authoredVisualStateGroups_.Clear();
    }
    Base::Span<const Base::Ref<Base::Object>>
    AuthoredVisualStateGroups() const noexcept {
        return {
            authoredVisualStateGroups_.Data(),
            authoredVisualStateGroups_.Size()};
    }
    Base::Result<void> RegisterAuthoredName(
        Base::StringView name,
        Base::Object& object) noexcept {
        return authoredNames_.TryRegister(
            name, object);
    }
    const NameScope& AuthoredNames() const noexcept {
        return authoredNames_;
    }
    void ClearAuthoredNames() noexcept {
        authoredNames_.Clear();
    }

private:
    Base::Ref<Base::Object> authoredVisualTree_;
    Base::Vector<Base::Ref<Base::Object>>
        authoredVisualStateGroups_;
    NameScope authoredNames_;
};

class AERO_API TemplateManager final {
public:
    TemplateManager(
        ObjectTree& tree,
        EffectiveValueEngine& values,
        DependencyPropertyRegistry& properties,
        LayoutManager* layout = nullptr,
        RenderManager* renderer = nullptr) noexcept
        : tree_(&tree),
          values_(&values),
          properties_(&properties),
          layout_(layout),
          renderer_(renderer),
          mounts_(tree, layout, renderer),
          propertyChangedHandler_(
              this, &TemplateManager::OnPropertyChanged) {}
    ~TemplateManager() noexcept;

    Base::Result<TemplateHandle> Apply(
        Control& control,
        const ControlTemplate& plan) noexcept;
    Base::Result<bool> Clear(
        TemplateHandle handle) noexcept;
    Base::Result<bool> Clear(
        Control& control) noexcept;
    DependencyObject* FindName(
        TemplateHandle handle,
        Base::StringView name) const noexcept;
    TemplateHandle AppliedHandle(
        const Control& control) const noexcept;
    const ControlTemplate* AppliedTemplate(
        TemplateHandle handle) const noexcept;

private:
    struct Instance final {
        TemplateHandle handle;
        Control* parent = nullptr;
        const ControlTemplate* plan = nullptr;
        Visual* rootVisual = nullptr;
        UIElement* rootElement = nullptr;
        Base::Vector<TemplatePart> parts;
        Base::Vector<TemplateContentProjection> projections;
        NameScope names;
    };

    ObjectTree* tree_ = nullptr;
    EffectiveValueEngine* values_ = nullptr;
    DependencyPropertyRegistry* properties_ = nullptr;
    LayoutManager* layout_ = nullptr;
    RenderManager* renderer_ = nullptr;
    MountService mounts_;
    Base::Vector<Instance> instances_;
    DependencyPropertyChangedEventHandler propertyChangedHandler_;
    std::uint64_t nextHandle_ = 1U;

    std::uint32_t FindInstance(
        TemplateHandle handle) const noexcept;
    std::uint32_t FindInstance(
        const Control& control) const noexcept;
    DependencyObject* FindTarget(
        const Instance& instance,
        Base::StringView name) const noexcept;
    Base::Result<void> Subscribe(
        Instance& instance) noexcept;
    void Unsubscribe(Instance& instance) noexcept;
    Base::Result<void> ApplyBindings(
        Instance& instance,
        DependencyPropertyHandle changed =
            DependencyPropertyHandle{}) noexcept;
    Base::Result<void> EvaluateTriggers(
        Instance& instance) noexcept;
    Base::Result<void> ClearProviders(
        Instance& instance) noexcept;
    Base::Result<void> ClearAt(
        std::uint32_t index) noexcept;
    void OnPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs& args) noexcept;
};

// Applies setter-only visual states through the Animation provider. State
// changes are immediate; animated transitions intentionally remain a later
// extension. A sealed template rejects property conflicts between groups so
// clearing one group cannot disturb another.
class AERO_API VisualStateManager final {
public:
    VisualStateManager(
        EffectiveValueEngine& values,
        TemplateManager& templates) noexcept
        : values_(&values), templates_(&templates) {}

    Base::Result<bool> GoToState(
        Control& control,
        Base::StringView groupName,
        Base::StringView stateName) noexcept;
    Base::Result<bool> ClearState(
        Control& control,
        Base::StringView groupName) noexcept;
    Base::Result<std::uint32_t> Clear(
        Control& control) noexcept;
    Base::StringView CurrentState(
        const Control& control,
        Base::StringView groupName) const noexcept;

private:
    struct ActiveGroup final {
        std::uint64_t templateValue = 0U;
        Base::String groupName;
        Base::String stateName;
    };

    EffectiveValueEngine* values_ = nullptr;
    TemplateManager* templates_ = nullptr;
    Base::Vector<ActiveGroup> active_;

    std::uint32_t FindActive(
        TemplateHandle handle,
        Base::StringView groupName) const noexcept;
    static const VisualStateGroup* FindGroup(
        const ControlTemplate& plan,
        Base::StringView groupName) noexcept;
    static const VisualState* FindState(
        const VisualStateGroup& group,
        Base::StringView stateName) noexcept;
    Base::Result<void> ApplyState(
        TemplateHandle handle,
        const VisualState& state) noexcept;
    Base::Result<void> ClearStateValues(
        TemplateHandle handle,
        const VisualState& state) noexcept;
    void PruneStale() noexcept;
    void RemoveActiveAt(std::uint32_t index) noexcept;
};

} // namespace Aero::Controls
