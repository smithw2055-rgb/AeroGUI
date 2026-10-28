#include "gui/core/TypeRegistryCore.hpp"
#include "gui/templates/TemplateBlueprint.hpp"
#include "gui/markup/XamlObjectWriterState.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/templates/TemplateInstance.hpp"
#include "gui/markup/XamlBytecode.hpp"
#include <cstdio>

// ===== TemplateSupport =====



#include <Aero/Controls/Control.hpp>
#include <Aero/DataTemplate.hpp>
#include <Aero/VisualStateManager.hpp>






namespace Aero::Markup {
namespace {

using namespace Aero::Controls;
using namespace Aero::Meta;
using namespace Aero::Threading;


class CompiledTemplateProgramOwner
    : public Base::Object {
public:
    explicit CompiledTemplateProgramOwner(
        CompiledTemplateBlueprint blueprint) noexcept
        : blueprint_(std::move(blueprint)) {}
    ~CompiledTemplateProgramOwner() noexcept override = default;

    CompiledTemplateBlueprint& Blueprint() noexcept {
        return blueprint_;
    }

private:
    CompiledTemplateBlueprint blueprint_;
};

Base::Status InvalidTemplateXaml(
    const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::ValidationFailed,
        message);
}

bool TemplateHasTypeFlag(
    TypeFlags value,
    TypeFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

inline void ApplyTemplateBaseUriIfEmpty(
    FrameworkTemplate& tpl, const Base::ResourceUri* uri) noexcept {
    if (uri != nullptr && ::Aero::Controls::FrameworkTemplateState::BaseUri(tpl).Empty()) {
        ::Aero::Controls::FrameworkTemplateState::SetBaseUri(tpl, *uri);
    }
}

Aero::ResourceDictionary* ResolveTemplateResources(
    Base::Object& object,
    void*) noexcept {
    if (auto* tpl = TryCast<FrameworkTemplate>(&object)) {
        return &tpl->GetResources();
    }
    return nullptr;
}

Base::Result<Aero::ResourceKey>
ResolveTemplateImplicitKey(
    const Base::Object& object,
    void*) noexcept {
    Meta::TypeId key = Meta::InvalidTypeId;
    if (const auto* tpl = TryCast<FrameworkTemplate>(&object)) {
        key = tpl->GetTargetType();
    }
    if (key == Meta::InvalidTypeId) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Template type has no implicit resource key");
    }
    return Aero::ResourceKey::FromType(key);
}

} // namespace

} // namespace Aero::Markup

namespace Aero::Markup {

struct XamlTemplateSchemaFacet::State {
    State(
        Meta::Registry& metadata,
        DependencyPropertyRegistry& dependencyProperties,
        Base::IAllocator& programAllocator) noexcept
        : allocator(&programAllocator),
          runtime(&metadata),
          properties(&dependencyProperties) {}

    Base::IAllocator* allocator = nullptr;
    Meta::Registry* runtime = nullptr;
    DependencyPropertyRegistry* properties = nullptr;
    Schema* schema = nullptr;

    static Base::Result<void> EndTemplate(
        Base::Object& object,
        const ExtensionServices& services,
        void* context) noexcept {
        auto* self = static_cast<XamlTemplateSchemaFacet::State*>(context);
        const TypeId type = object.RuntimeType();
        const bool isControlTemplate =
            type == ControlTemplate::StaticTypeId();
        const bool isDataTemplateFamily =
            type == DataTemplate::StaticTypeId() ||
            type == HierarchicalDataTemplate::StaticTypeId();
        const bool isItemsPanelTemplate =
            type == ItemsPanelTemplate::StaticTypeId();
        if (self == nullptr || self->runtime == nullptr ||
            self->properties == nullptr ||
            (!isControlTemplate &&
             !isDataTemplateFamily &&
             !isItemsPanelTemplate) ||
            services.deferredContentOwner != &object ||
            services.deferredContent == nullptr) {
            if (isControlTemplate ||
                isDataTemplateFamily ||
                isItemsPanelTemplate) {
                const bool sealed =
                    static_cast<const FrameworkTemplate&>(object).GetIsSealed();
                if (sealed) {
                    return {};
                }
                // WPF allows an empty ControlTemplate/DataTemplate. Failing
                // EndInit here aborts the document and drops visual children.
                return {};
            }
            return InvalidTemplateXaml(
                "Template deferred-content scope is invalid");
        }
        if (isControlTemplate || isDataTemplateFamily) {
            const Meta::TypeId targetType =
                static_cast<const FrameworkTemplate&>(object).GetTargetType();
            // A keyed WPF ControlTemplate may deliberately omit TargetType.
            // Its target is inferred from the Style/Setter that consumes it,
            // so only validate an explicitly authored type here. The apply
            // path still checks that the eventual target is a Control.
            if (targetType != Meta::InvalidTypeId) {
                const Meta::TypeInfo* targetInfo =
                    self->runtime->Types().FindType(targetType);
                if (targetInfo == nullptr ||
                    TemplateHasTypeFlag(
                        targetInfo->Flags(),
                        Meta::TypeFlags::ValueType)) {
                    return InvalidTemplateXaml(
                        "Template type constraint must identify an object type");
                }
                if (isControlTemplate &&
                    !self->runtime->Types().IsDerivedFrom(
                        targetType,
                        Control::StaticTypeId())) {
                    return InvalidTemplateXaml(
                        "ControlTemplate TargetType is not a Control");
                }
            }
        }
        Base::Vector<DeferredContentEdge> edges(
            self->allocator);
        Base::Result<void> copied =
            services.deferredContent->CopyForOwner(
                object, edges);
        if (!copied) return copied.GetStatus();
        Base::Vector<DeferredBindingEdge> bindings(
            self->allocator);
        copied =
            services.deferredContent->
                CopyBindingsForOwner(
                    object, bindings);
        if (!copied) return copied.GetStatus();

        if (services.baseUri != nullptr) {
            if (isControlTemplate) {
                ApplyTemplateBaseUriIfEmpty(static_cast<ControlTemplate&>(object), services.baseUri);
            } else if (isDataTemplateFamily) {
                ApplyTemplateBaseUriIfEmpty(static_cast<DataTemplate&>(object), services.baseUri);
            } else {
                ApplyTemplateBaseUriIfEmpty(static_cast<ItemsPanelTemplate&>(object), services.baseUri);
            }
        }
        if (isDataTemplateFamily || isItemsPanelTemplate) {
            const Base::Ref<Base::Object>* authored = nullptr;
            if (isDataTemplateFamily) {
                authored =
                    &::Aero::Controls::FrameworkTemplateState::AuthoredVisualTree(static_cast<DataTemplate&>(object));
            } else {
                authored =
                    &::Aero::Controls::FrameworkTemplateState::AuthoredVisualTree(static_cast<ItemsPanelTemplate&>(object));
            }
            Base::Ref<Base::Object> visualTree =
                authored != nullptr ? *authored : Base::Ref<Base::Object>{};
            if (!visualTree) {
                for (std::uint32_t index = 0U; index < edges.Size(); ++index) {
                    if (edges[index].owner == &object && edges[index].child) {
                        visualTree = edges[index].child;
                        break;
                    }
                }
            }
            if (!visualTree) {
                // WPF allows a DataTemplate with no visual tree. HierarchicalDataTemplate
                // is a DataTemplate subtype; an empty tree must still seal.
                if (isDataTemplateFamily) {
                    auto& dataTemplate =
                        static_cast<DataTemplate&>(object);
                    Base::Result<void> sealedDt =
                        ::Aero::Controls::FrameworkTemplateState::Seal(dataTemplate);
                    if (!sealedDt) return sealedDt.GetStatus();
                    services.deferredContent->ReleaseOwner(object);
                    return {};
                }
                return InvalidTemplateXaml("Template requires a VisualTree");
            }
            Base::Result<CompiledTemplateBlueprint>
                compiled =
                    CompileDeferredTemplateBlueprint(
                        visualTree,
                        isDataTemplateFamily
                            ? &::Aero::Controls::FrameworkTemplateState::AuthoredNames(static_cast<DataTemplate&>(object))
                            : nullptr,
                        {
                            edges.Data(),
                            edges.Size()},
                        {
                            bindings.Data(),
                            bindings.Size()},
                        *self->runtime,
                        *self->properties);
            if (!compiled) {
                return compiled.GetStatus();
            }
            auto& templateObj = static_cast<FrameworkTemplate&>(object);
            if (isDataTemplateFamily) {
                auto authoredTriggers =
                    ::Aero::Controls::FrameworkTemplateState::AuthoredTriggers(templateObj);
                compiled.Value().
                    dataTemplateTriggers.Reserve(authoredTriggers.Size());
                for (const Base::Ref<Base::Object>& triggerObj : authoredTriggers) {
                    if (triggerObj) {
                        auto retained = Base::Ref<Aero::TriggerBase>::TryFromBorrowed(
                            static_cast<Aero::TriggerBase&>(*triggerObj));
                        if (retained) {
                            Base::Result<CompiledTriggerPlan> plan = CompileTriggerPlan(
                                std::move(retained), compiled.Value(),
                                *self->runtime, *self->properties);
                            if (!plan) return plan.GetStatus();
                            compiled.Value().dataTemplateTriggers.PushBack(std::move(plan).Value());
                        }
                    }
                }
            }
            EnsureBytecode(compiled.Value());
            for (auto& node : compiled.Value().nodes) {
                node.properties.Clear();
                node.gridColumns.Clear();
                node.gridRows.Clear();
                node.streamGeometryData.Clear();
                node.gradientStops.Clear();
                node.authoredBehaviors.Clear();
                node.authoredTriggers.Clear();
            }
            Base::Ref<CompiledTemplateProgramOwner>
                program =
                    Base::MakeRefWithAllocator<
                        CompiledTemplateProgramOwner>(
                        *self->allocator,
                        std::move(compiled).Value());
            if (!program) {
                return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "OOM");
            }
            CompiledTemplateBlueprint* programContext =
                &program->Blueprint();
            Base::Ref<Base::Object> programOwner =
                program;
            Base::Result<void> configured =
                ::Aero::Controls::FrameworkTemplateState::Configure(
                    templateObj,
                    &BuildCompiledDeferredTemplate,
                    programContext,
                    std::move(programOwner));
            if (configured) {
                configured = ::Aero::Controls::FrameworkTemplateState::Seal(templateObj);
            }
            if (!configured) {
                return configured.GetStatus();
            }
            services.deferredContent->ReleaseOwner(
                object);
            ::Aero::Controls::FrameworkTemplateState::ClearAuthoredVisualTree(templateObj);
            if (isDataTemplateFamily) {
                ::Aero::Controls::FrameworkTemplateState::ClearAuthoredTriggers(templateObj);
                ::Aero::Controls::FrameworkTemplateState::ClearAuthoredNames(templateObj);
            }
            return {};
        }
        auto& controlTemplate =
            static_cast<ControlTemplate&>(object);
        // BasedOn inheritance without own VisualTree: the derived template
        // shares the sealed base tree/factory and only authors incremental
        // triggers or visual states (resolved against the base blueprint).
        // Seal() prepends the base plans base-first.
        bool hasOwnTree =
            static_cast<bool>(
                ::Aero::Controls::FrameworkTemplateState::AuthoredVisualTree(
                    controlTemplate));
        if (!hasOwnTree) {
            for (std::uint32_t index = 0U; index < edges.Size(); ++index) {
                if (edges[index].owner == &object && edges[index].child) {
                    hasOwnTree = true;
                    break;
                }
            }
        }
        FrameworkTemplate* templateBase =
            ::Aero::Controls::FrameworkTemplateState::BasedOn(
                static_cast<FrameworkTemplate&>(controlTemplate));
        if (!hasOwnTree && templateBase != nullptr) {
            auto* baseControl =
                TryCast<ControlTemplate>(templateBase);
            if (baseControl == nullptr || !baseControl->GetIsSealed()) {
                return InvalidTemplateXaml(
                    "BasedOn template must be a sealed ControlTemplate");
            }
            const bool addsTriggers =
                !::Aero::Controls::FrameworkTemplateState::AuthoredTriggers(
                    static_cast<FrameworkTemplate&>(controlTemplate)).Empty();
            const bool addsStates =
                !::Aero::Controls::FrameworkTemplateState::AuthoredVisualStateGroups(
                    controlTemplate).Empty();
            const auto baseFactory =
                ::Aero::Controls::FrameworkTemplateState::Factory(*baseControl);
            void* const baseFactoryContext =
                ::Aero::Controls::FrameworkTemplateState::FactoryContext(
                    *baseControl);
            if (addsTriggers || addsStates) {
                if (baseFactory != &BuildCompiledTemplate ||
                    baseFactoryContext == nullptr) {
                    return InvalidTemplateXaml(
                        "Inherited ControlTemplate triggers require a XAML-compiled BasedOn template");
                }
                Base::Result<void> inherited =
                    CompileInheritedControlTemplate(
                        controlTemplate,
                        *static_cast<const CompiledTemplateBlueprint*>(
                            baseFactoryContext),
                        *self->runtime,
                        *self->properties);
                if (!inherited) {
                    return inherited.GetStatus();
                }
            }
            if (baseFactory == nullptr) {
                return InvalidTemplateXaml(
                    "BasedOn template has no factory to inherit");
            }
            Base::Ref<Base::Object> baseOwner =
                ::Aero::Controls::FrameworkTemplateState::FactoryOwner(
                    *baseControl);
            Base::Result<void> configured =
                ::Aero::Controls::FrameworkTemplateState::ConfigureFactory(
                    controlTemplate,
                    baseFactory,
                    baseFactoryContext,
                    std::move(baseOwner));
            if (configured) {
                configured =
                    ::Aero::Controls::FrameworkTemplateState::Seal(
                        controlTemplate,
                        *self->properties);
            }
            if (!configured) {
                return configured.GetStatus();
            }
            services.deferredContent->ReleaseOwner(
                object);
            ::Aero::Controls::FrameworkTemplateState::ClearAuthoredVisualTree(controlTemplate);
            ::Aero::Controls::FrameworkTemplateState::ClearAuthoredVisualStateGroups(controlTemplate);
            ::Aero::Controls::FrameworkTemplateState::ClearAuthoredTriggers(controlTemplate);
            ::Aero::Controls::FrameworkTemplateState::ClearAuthoredNames(controlTemplate);
            return {};
        }
        if (controlTemplate.GetTargetType() ==
            Meta::InvalidTypeId) {
            // WPF permits a keyed ControlTemplate to omit TargetType. The
            // consuming Style supplies the concrete control at apply time;
            // compile against the common Control contract so its authored
            // bindings and triggers remain valid until then.
            Base::Result<void> inferred =
                ::Aero::Controls::FrameworkTemplateState::SetTargetType(controlTemplate,
                    Control::StaticTypeId());
            if (!inferred) return inferred.GetStatus();
        }
        Base::Result<CompiledTemplateDefinition>
            compiled =
                CompileControlTemplateDefinition(
                    controlTemplate,
                    {
                        edges.Data(),
                        edges.Size()},
                    {
                        bindings.Data(),
                        bindings.Size()},
                    *self->runtime,
                    *self->properties);
        if (!compiled) {
            return compiled.GetStatus();
        }

        Base::Ref<CompiledTemplateProgramOwner>
            program =
                Base::MakeRefWithAllocator<
                    CompiledTemplateProgramOwner>(
                    *self->allocator,
                    std::move(
                        compiled.Value().blueprint));
        if (!program) {
            return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "OOM");
        }
        CompiledTemplateBlueprint* programContext =
            &program->Blueprint();
        Base::Ref<Base::Object> programOwner =
            program;

        Base::Result<void> configured =
            ::Aero::Controls::FrameworkTemplateState::ConfigureFactory(controlTemplate,
                &BuildCompiledTemplate,
                programContext,
                std::move(programOwner));
        if (configured) {
            for (const TemplateBindingPlan& binding :
                 compiled.Value().
                     contentSourceBindings) {
                configured =
                    ::Aero::Controls::FrameworkTemplateState::AddTemplateBinding(controlTemplate,
                            binding.targetName.View(),
                            binding.sourceProperty,
                            binding.targetProperty);
                if (!configured) {
                    break;
                }
            }
        }
        if (configured) {
            for (TemplatePropertyTrigger& trigger :
                 compiled.Value().propertyTriggers) {
                configured =
                    ::Aero::Controls::FrameworkTemplateState::AddPropertyTrigger(controlTemplate,
                        std::move(trigger));
                if (!configured) {
                    break;
                }
            }
        }
        if (configured) {
            for (Controls::VisualStateGroupPlan& group :
                 compiled.Value().visualStateGroups) {
                configured =
                    ::Aero::Controls::FrameworkTemplateState::AddVisualStateGroup(controlTemplate,
                        std::move(group));
                if (!configured) {
                    break;
                }
            }
        }
        if (configured) {
            configured =
                ::Aero::Controls::FrameworkTemplateState::Seal(
                    controlTemplate,
                    *self->properties);
        }
        if (!configured) {
            return configured.GetStatus();
        }

        services.deferredContent->ReleaseOwner(
            object);
        ::Aero::Controls::FrameworkTemplateState::ClearAuthoredVisualTree(controlTemplate);
        ::Aero::Controls::FrameworkTemplateState::ClearAuthoredVisualStateGroups(controlTemplate);
        ::Aero::Controls::FrameworkTemplateState::ClearAuthoredTriggers(controlTemplate);
        ::Aero::Controls::FrameworkTemplateState::ClearAuthoredNames(controlTemplate);
        return {};
    }

    static Base::Result<void> RegisterTemplateName(
        Base::Object& scopeOwner,
        Base::StringView name,
        Base::Object& object,
        void* context) noexcept {
        if (context == nullptr ||
            (scopeOwner.RuntimeType() !=
                 ControlTemplate::StaticTypeId() &&
             scopeOwner.RuntimeType() !=
                 DataTemplate::StaticTypeId() &&
             scopeOwner.RuntimeType() !=
                 HierarchicalDataTemplate::StaticTypeId())) {
            return InvalidTemplateXaml(
                "Template name scope is invalid");
        }
        return ::Aero::Controls::FrameworkTemplateState::RegisterAuthoredName(
            static_cast<FrameworkTemplate&>(scopeOwner), name, object);
    }
};

XamlTemplateSchemaFacet::XamlTemplateSchemaFacet(
    Meta::Registry& runtime,
    DependencyPropertyRegistry& properties,
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {
    static_cast<void>(Base::AllocateObject(
        *allocator_, Base::MemoryTag::General, state_, runtime, properties, *allocator_));
}

XamlTemplateSchemaFacet::~XamlTemplateSchemaFacet() noexcept {
    if (state_ == nullptr) {
        return;
    }
    Base::FreeObject(*allocator_, Base::MemoryTag::General, state_);
    state_ = nullptr;
}

Base::Result<void> XamlTemplateSchemaFacet::Register(
    Schema& schema) noexcept {
    if (state_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "XAML template extension allocation failed");
    }
    if (schema.IsFrozen() ||
        state_->schema != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML template extension registration is invalid");
    }

    state_->schema = &schema;
    Base::Result<void> status =
        schema.AddType({
            ControlTemplate::StaticTypeId(),
            nullptr,
            nullptr,
            nullptr,
            state_,
            true,
            true,
            &XamlTemplateSchemaFacet::State::RegisterTemplateName,
            nullptr,
            &ResolveTemplateResources,
            &XamlTemplateSchemaFacet::State::EndTemplate,
            true,
            &ResolveTemplateImplicitKey});
    if (status) {
        status = schema.AddType({
            DataTemplate::StaticTypeId(),
            nullptr,
            nullptr,
            nullptr,
            state_,
            true,
            true,
            &XamlTemplateSchemaFacet::State::RegisterTemplateName,
            nullptr,
            &ResolveTemplateResources,
            &XamlTemplateSchemaFacet::State::EndTemplate,
            true,
            &ResolveTemplateImplicitKey});
    }
    if (status) {
        status = schema.AddType({
            HierarchicalDataTemplate::StaticTypeId(),
            nullptr,
            nullptr,
            nullptr,
            state_,
            true,
            true,
            &XamlTemplateSchemaFacet::State::RegisterTemplateName,
            nullptr,
            &ResolveTemplateResources,
            &XamlTemplateSchemaFacet::State::EndTemplate,
            true,
            &ResolveTemplateImplicitKey});
    }
    if (status) {
        status = schema.AddType({
            ItemsPanelTemplate::StaticTypeId(),
            nullptr,
            nullptr,
            nullptr,
            state_,
            true,
            true,
            nullptr,
            nullptr,
            &ResolveTemplateResources,
            &XamlTemplateSchemaFacet::State::EndTemplate,
            true,
            nullptr});
    }
    if (!status) {
        state_->schema = nullptr;
        return status.GetStatus();
    }
    return {};
}

} // namespace Aero::Markup


