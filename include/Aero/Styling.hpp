#pragma once

#include <Aero/Style.hpp>
#include <Aero/Controls/Base.hpp>
#include <Aero/Animation.hpp>
#include <Aero/Data.hpp>
#include <Aero/Visual.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <type_traits>

namespace Aero::Markup::Detail {
class XamlTemplateSchemaFacet;
}

namespace Aero::Controls::Detail {
class FrameworkTemplateAccess;
class VisualStateManagerAccess;
}

namespace Aero::Controls {

using namespace Aero::Core;
using namespace Aero;
using namespace Aero::Data;
using namespace Aero::Input;
using namespace Aero::Media;
using namespace Aero::Render;

class ContentPresenter;
class ItemsPanelTemplate;
class ItemsPresenter;

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
    Base::Result<bool> ProjectContent(
        ContentControl& owner,
        ContentPresenter& presenter) noexcept;
    Base::Result<bool> ProjectContent(
        ContentControl& owner,
        ContentControl& presenter) noexcept;

    Control& TemplatedParent() const noexcept;
    Visual* RootVisual() const noexcept;
    UIElement* RootElement() const noexcept;

private:
    friend class Aero::Detail::ControlRuntimeAccess;

    explicit TemplateBuildContext(void* state) noexcept
        : state_(state) {}

    DependencyObject* FindObject(
        Base::StringView name) const noexcept;
    Base::Result<void> AddOwnedPart(
        Base::StringView name,
        Base::Ref<Base::Object> owner,
        Visual& visual,
        void* mount) noexcept;
    Base::Result<void> PopulateItemsPresenter(
        ItemsPresenter& presenter,
        const ItemsPanelTemplate* itemsPanel) noexcept;
    Base::Result<void> PopulateContentPresenter(
        ContentPresenter& presenter) noexcept;
    Base::Result<bool> ProjectContentCore(
        ContentControl& owner,
        Visual& presenterVisual,
        ContentPresenter* presenter,
        ContentControl* contentHost) noexcept;
    void Rollback() noexcept;

    void* state_ = nullptr;
};

using TemplateFactoryCallback = Base::Result<void> (*)(
    TemplateBuildContext& context,
    void* factoryContext) noexcept;

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
    Aero::Data::BindingMode mode =
        Aero::Data::BindingMode::OneWay;
    Core::UpdateSourceTrigger updateSourceTrigger =
        Core::UpdateSourceTrigger::PropertyChanged;
};

struct TemplateTriggerSetter final {
    Base::String targetName;
    DependencyPropertyHandle property;
    PropertyValue value;
};

struct TemplateTriggerCondition final {
    Base::String sourceName;
    DependencyPropertyHandle property;
    PropertyValue value;
};

struct TemplatePropertyTrigger final {
    Base::Vector<TemplateTriggerCondition> conditions;
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
    Base::Ref<Media::Animation::Storyboard> storyboard;
};

struct VisualTransition final {
    Base::String from;
    Base::String to;
    Media::Animation::AnimationTime generatedDurationMicroseconds = 0U;
    Base::Ref<Media::Animation::EasingFunctionBase> generatedEasingFunction;
    Base::Ref<Media::Animation::Storyboard> storyboard;
};

struct VisualStateGroup final {
    Base::String name;
    Base::Vector<VisualState> states;
    Base::Vector<VisualTransition> transitions;
};

class AERO_API FrameworkTemplate : public Base::Object {
    AERO_DECLARE_TYPE(FrameworkTemplate, Base::Object)
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
    Base::Result<void> ConfigureFactory(
        TemplateFactoryCallback factory,
        void* factoryContext = nullptr,
        Base::Ref<Base::Object> factoryOwner = {}) noexcept;
    Base::Result<void> TryAddTemplateBinding(
        Base::StringView targetName,
        DependencyPropertyHandle sourceProperty,
        DependencyPropertyHandle targetProperty) noexcept;
    Base::Result<void> TryAddTemplatedParentBinding(
        Base::StringView targetName,
        Base::StringView path,
        Base::StringView stringFormat,
        DependencyPropertyHandle targetProperty,
        Aero::Data::BindingMode mode,
        Core::UpdateSourceTrigger updateSourceTrigger) noexcept;
    template<
        class TSourceOwner,
        class TSourceValue,
        class TTargetOwner,
        class TTargetValue>
    Base::Result<void> Bind(
        Base::StringView targetName,
        const Core::DependencyPropertyRef<
            TSourceOwner, TSourceValue>& sourceProperty,
        const Core::DependencyPropertyRef<
            TTargetOwner, TTargetValue>& targetProperty) noexcept {
        static_assert(
            std::is_same_v<TSourceValue, TTargetValue>,
            "Template binding source and target values must match");
        return TryAddTemplateBinding(
            targetName,
            sourceProperty.Handle(),
            targetProperty.Handle());
    }
    template<
        class TSourceOwner,
        class TSourceValue,
        class TTargetOwner,
        class TTargetValue>
    Base::Result<void> Bind(
        Base::StringView targetName,
        const Core::ReadOnlyPropertyRef<
            TSourceOwner, TSourceValue>& sourceProperty,
        const Core::DependencyPropertyRef<
            TTargetOwner, TTargetValue>& targetProperty) noexcept {
        static_assert(
            std::is_same_v<TSourceValue, TTargetValue>,
            "Template binding source and target values must match");
        return TryAddTemplateBinding(
            targetName,
            sourceProperty.Handle(),
            targetProperty.Handle());
    }
    Base::Result<void> TryAddPropertyTrigger(
        TemplatePropertyTrigger trigger) noexcept;
    Base::Result<void> TryAddVisualStateGroup(
        VisualStateGroup group) noexcept;
    TypeId TargetType() const noexcept {
        return sealed_ ? program_.TargetType() : targetType_;
    }
    bool IsSealed() const noexcept { return sealed_; }
    TemplateFactoryCallback Factory() const noexcept {
        return program_.Factory();
    }
    void* FactoryContext() const noexcept {
        return program_.FactoryContext();
    }
    ResourceDictionary& Resources() noexcept {
        return resources_;
    }
    const ResourceDictionary& Resources() const noexcept {
        return resources_;
    }
    Base::Result<void> SetResources(
        Base::Ref<ResourceDictionary> value) noexcept;
    Base::Span<const TemplateBindingPlan> Bindings() const noexcept {
        return sealed_ ? program_.Bindings()
            : Base::Span<const TemplateBindingPlan>{
                  bindings_.Data(), bindings_.Size()};
    }
    Base::Span<const TemplateMetadataBindingPlan>
    MetadataBindings() const noexcept {
        return sealed_ ? program_.MetadataBindings()
            : Base::Span<const TemplateMetadataBindingPlan>{
                  metadataBindings_.Data(),
                  metadataBindings_.Size()};
    }
    Base::Span<const TemplatePropertyTrigger> Triggers() const noexcept {
        return sealed_ ? program_.Triggers()
            : Base::Span<const TemplatePropertyTrigger>{
                  triggers_.Data(), triggers_.Size()};
    }
    Base::Span<const VisualStateGroup> VisualStateGroups() const noexcept {
        return sealed_ ? program_.VisualStateGroups()
            : Base::Span<const VisualStateGroup>{
                  visualStateGroups_.Data(), visualStateGroups_.Size()};
    }
    Base::Result<void> TryAddAuthoredTrigger(
        Base::Ref<Base::Object> trigger) noexcept;
    void ClearAuthoredTriggers() noexcept {
        authoredTriggers_.Clear();
    }
    Base::Span<const Base::Ref<Base::Object>>
    AuthoredTriggers() const noexcept {
        return {
            authoredTriggers_.Data(),
            authoredTriggers_.Size()};
    }

private:
    friend class ::Aero::Controls::Detail::FrameworkTemplateAccess;
    friend class Markup::Detail::XamlTemplateSchemaFacet;

    Base::Result<void> SealRuntime(
        const void* properties) noexcept;

    struct Impl final {
        Impl() noexcept = default;
        Impl(
            TemplateFactoryCallback valueFactory,
            void* valueFactoryContext = nullptr) noexcept
            : factory(valueFactory),
              factoryContext(valueFactoryContext) {}

        Base::Result<void> Configure(
            TemplateFactoryCallback valueFactory,
            void* valueFactoryContext = nullptr,
            Base::Ref<Base::Object> valueFactoryOwner = {}) noexcept;
        Base::Result<void> SetBaseUri(
            const Base::ResourceUri& value) noexcept;
        Base::Result<void> TryAddNamespace(
            Base::StringView prefix,
            Base::StringView uri) noexcept;
        Base::Result<void> Seal() noexcept;
        Base::Result<void> FreezeRuntimePlan(
            TypeId valueTargetType,
            Base::Vector<TemplateBindingPlan>&& valueBindings,
            Base::Vector<TemplateMetadataBindingPlan>&&
                valueMetadataBindings,
            Base::Vector<TemplatePropertyTrigger>&& valueTriggers,
            Base::Vector<VisualStateGroup>&& valueVisualStateGroups) noexcept;

        TemplateFactoryCallback Factory() const noexcept { return factory; }
        void* FactoryContext() const noexcept { return factoryContext; }
        const Base::Ref<Base::Object>& FactoryOwner() const noexcept {
            return factoryOwner;
        }
        const Base::ResourceUri& BaseUri() const noexcept { return baseUri; }
        Base::Span<const TemplateNamespace> Namespaces() const noexcept {
            return {namespaces.Data(), namespaces.Size()};
        }
        TypeId TargetType() const noexcept { return targetType; }
        Base::Span<const TemplateBindingPlan> Bindings() const noexcept {
            return {bindings.Data(), bindings.Size()};
        }
        Base::Span<const TemplateMetadataBindingPlan>
        MetadataBindings() const noexcept {
            return {
                metadataBindings.Data(),
                metadataBindings.Size()};
        }
        Base::Span<const TemplatePropertyTrigger> Triggers() const noexcept {
            return {triggers.Data(), triggers.Size()};
        }
        Base::Span<const VisualStateGroup> VisualStateGroups() const noexcept {
            return {
                visualStateGroups.Data(),
                visualStateGroups.Size()};
        }

        TemplateFactoryCallback factory = nullptr;
        void* factoryContext = nullptr;
        Base::Ref<Base::Object> factoryOwner;
        Base::ResourceUri baseUri;
        Base::Vector<TemplateNamespace> namespaces;
        TypeId targetType = InvalidTypeId;
        Base::Vector<TemplateBindingPlan> bindings;
        Base::Vector<TemplateMetadataBindingPlan>
            metadataBindings;
        Base::Vector<TemplatePropertyTrigger> triggers;
        Base::Vector<VisualStateGroup> visualStateGroups;
        bool sealed = false;
    };

    Impl& RuntimeData() noexcept { return program_; }
    const Impl& RuntimeData() const noexcept { return program_; }

    TypeId targetType_ = InvalidTypeId;
    Impl program_;
    ResourceDictionary resources_;
    Base::Vector<TemplateBindingPlan> bindings_;
    Base::Vector<TemplateMetadataBindingPlan>
        metadataBindings_;
    Base::Vector<TemplatePropertyTrigger> triggers_;
    Base::Vector<VisualStateGroup> visualStateGroups_;
    Base::Vector<Base::Ref<Base::Object>>
        authoredTriggers_;
    bool sealed_ = false;
};

class AERO_API ControlTemplate final : public FrameworkTemplate {
    AERO_DECLARE_TYPE(ControlTemplate, FrameworkTemplate)
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
    Base::Result<Base::String>
    EnsureAuthoredName(
        Base::Object& object) noexcept;
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
    std::uint32_t generatedNameSequence_ = 0U;
};


// Applies a named VisualState to a Control. Runtime coordination and
// animation bookkeeping are private to the Controls implementation.
class AERO_API VisualStateManager final {
public:
    static bool GoToState(Control& control, Base::StringView stateName, bool useTransitions = true) noexcept;

    ~VisualStateManager() noexcept;

    VisualStateManager(const VisualStateManager&) = delete;
    VisualStateManager& operator=(const VisualStateManager&) = delete;

    Base::Result<bool> GoToState(
        Control& control,
        Base::StringView groupName,
        Base::StringView stateName,
        bool useTransitions = true) noexcept;
    Base::Result<bool> ClearState(
        Control& control,
        Base::StringView groupName) noexcept;
    Base::Result<std::uint32_t> Clear(
        Control& control) noexcept;
    Base::StringView CurrentState(
        const Control& control,
        Base::StringView groupName) const noexcept;

private:
    friend class Detail::VisualStateManagerAccess;

    VisualStateManager() noexcept = default;

    void* impl_ = nullptr;
};

} // namespace Aero::Controls
