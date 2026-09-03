// ===== TemplateSupport =====



#include <Aero/Controls/ControlTemplate.hpp>
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

template <typename TTemplate>
inline Base::Result<void> ApplyTemplateBaseUriIfEmpty(
    TTemplate& tpl, const Base::ResourceUri* uri) noexcept {
    if (uri != nullptr && ::Aero::Controls::TemplatePrivate::BaseUri(tpl).Empty()) {
        return ::Aero::Controls::TemplatePrivate::SetBaseUri(tpl, *uri);
    }
    return {};
}

Aero::ResourceDictionary* ResolveTemplateResources(
    Base::Object& object,
    void*) noexcept {
    if (object.RuntimeType() ==
            ControlTemplate::StaticTypeId()) {
        return &static_cast<ControlTemplate&>(
            object).GetResources();
    }
    if (object.RuntimeType() ==
            DataTemplate::StaticTypeId()) {
        return &static_cast<DataTemplate&>(
            object).GetResources();
    }
    if (object.RuntimeType() ==
            ItemsPanelTemplate::StaticTypeId()) {
        return &static_cast<ItemsPanelTemplate&>(
            object).GetResources();
    }
    return nullptr;
}

Base::Result<Aero::ResourceKey>
ResolveTemplateImplicitKey(
    const Base::Object& object,
    void*) noexcept {
    Meta::TypeId key = Meta::InvalidTypeId;
    if (object.RuntimeType() == ControlTemplate::StaticTypeId()) {
        key = static_cast<const ControlTemplate&>(object).GetTargetType();
    } else if (const DataTemplate* dataTemplate =
                   TryCast<DataTemplate>(
                       const_cast<Base::Object*>(&object))) {
        key = dataTemplate->GetDataType();
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

struct XamlTemplateSchemaFacetState {
    XamlTemplateSchemaFacetState(
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
        auto* self = static_cast<XamlTemplateSchemaFacetState*>(context);
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
                    isControlTemplate
                    ? static_cast<const ControlTemplate&>(object)
                          .GetIsSealed()
                    : isDataTemplateFamily
                          ? static_cast<const DataTemplate&>(object)
                                .GetIsSealed()
                          : static_cast<const ItemsPanelTemplate&>(object)
                                .GetIsSealed();
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
                isControlTemplate
                ? static_cast<ControlTemplate&>(object)
                      .GetTargetType()
                : static_cast<DataTemplate&>(object)
                      .GetDataType();
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
            Base::Result<void> baseUri =
                isControlTemplate
                    ? ApplyTemplateBaseUriIfEmpty(static_cast<ControlTemplate&>(object), services.baseUri)
                    : isDataTemplateFamily
                          ? ApplyTemplateBaseUriIfEmpty(static_cast<DataTemplate&>(object), services.baseUri)
                          : ApplyTemplateBaseUriIfEmpty(static_cast<ItemsPanelTemplate&>(object), services.baseUri);
            if (!baseUri) return baseUri.GetStatus();
        }
        if (isDataTemplateFamily || isItemsPanelTemplate) {
            const Base::Ref<Base::Object>* authored = nullptr;
            if (isDataTemplateFamily) {
                authored =
                    &::Aero::Controls::TemplatePrivate::AuthoredVisualTree(static_cast<DataTemplate&>(object));
            } else {
                authored =
                    &::Aero::Controls::TemplatePrivate::AuthoredVisualTree(static_cast<ItemsPanelTemplate&>(object));
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
                        ::Aero::Controls::TemplatePrivate::Seal(dataTemplate);
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
                            ? &::Aero::Controls::TemplatePrivate::AuthoredNames(static_cast<DataTemplate&>(object))
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
            if (isDataTemplateFamily) {
                auto& dataTemplate =
                    static_cast<DataTemplate&>(object);
                Base::Result<void> reserved =
                    compiled.Value().
                        dataTemplateTriggers.Reserve(
                            ::Aero::Controls::TemplatePrivate::AuthoredTriggers(dataTemplate).Size());
                if (!reserved) {
                    return reserved.GetStatus();
                }
                for (const Base::Ref<
                         Aero::TriggerBase>& trigger :
                     ::Aero::Controls::TemplatePrivate::AuthoredTriggers(dataTemplate)) {
                    Base::Result<void> retained =
                        compiled.Value().
                            dataTemplateTriggers.
                                PushBack(trigger);
                    if (!retained) {
                        return retained.GetStatus();
                    }
                }
            }
            Base::Result<Base::Ref<CompiledTemplateProgramOwner>>
                program =
                    Base::MakeRefWithAllocator<
                        CompiledTemplateProgramOwner>(
                        *self->allocator,
                        std::move(compiled).Value());
            if (!program) {
                return program.GetStatus();
            }
            CompiledTemplateBlueprint* programContext =
                &program.Value()->Blueprint();
            Base::Ref<Base::Object> programOwner =
                program.Value();
            Base::Result<void> configured;
            if (isDataTemplateFamily) {
                auto& dataTemplate =
                    static_cast<DataTemplate&>(object);
                configured = ::Aero::Controls::TemplatePrivate::Configure(dataTemplate,
                    &BuildCompiledDeferredTemplate,
                    programContext,
                    std::move(programOwner));
                if (configured) {
                    configured = ::Aero::Controls::TemplatePrivate::Seal(dataTemplate);
                }
            } else {
                auto& itemsPanel =
                    static_cast<ItemsPanelTemplate&>(object);
                configured = ::Aero::Controls::TemplatePrivate::Configure(itemsPanel,
                    &BuildCompiledDeferredTemplate,
                    programContext,
                    std::move(programOwner));
                if (configured) {
                    configured = ::Aero::Controls::TemplatePrivate::Seal(itemsPanel);
                }
            }
            if (!configured) {
                return configured.GetStatus();
            }
            services.deferredContent->ReleaseOwner(
                object);
            if (isDataTemplateFamily) {
                auto& dataTemplate =
                    static_cast<DataTemplate&>(object);
                ::Aero::Controls::TemplatePrivate::ClearAuthoredVisualTree(dataTemplate);
                ::Aero::Controls::TemplatePrivate::ClearAuthoredTriggers(dataTemplate);
                ::Aero::Controls::TemplatePrivate::ClearAuthoredNames(dataTemplate);
            } else {
                ::Aero::Controls::TemplatePrivate::ClearAuthoredVisualTree(
                    static_cast<ItemsPanelTemplate&>(object));
            }
            return {};
        }
        auto& controlTemplate =
            static_cast<ControlTemplate&>(object);
        if (controlTemplate.GetTargetType() ==
            Meta::InvalidTypeId) {
            // WPF permits a keyed ControlTemplate to omit TargetType. The
            // consuming Style supplies the concrete control at apply time;
            // compile against the common Control contract so its authored
            // bindings and triggers remain valid until then.
            Base::Result<void> inferred =
                ::Aero::Controls::TemplatePrivate::SetTargetType(controlTemplate,
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

        Base::Result<Base::Ref<CompiledTemplateProgramOwner>>
            program =
                Base::MakeRefWithAllocator<
                    CompiledTemplateProgramOwner>(
                    *self->allocator,
                    std::move(
                        compiled.Value().blueprint));
        if (!program) {
            return program.GetStatus();
        }
        CompiledTemplateBlueprint* programContext =
            &program.Value()->Blueprint();
        Base::Ref<Base::Object> programOwner =
            program.Value();

        Base::Result<void> configured =
            ::Aero::Controls::TemplatePrivate::ConfigureFactory(controlTemplate,
                &BuildCompiledTemplate,
                programContext,
                std::move(programOwner));
        if (configured) {
            for (const TemplateBindingPlan& binding :
                 compiled.Value().
                     contentSourceBindings) {
                configured =
                    ::Aero::Controls::TemplatePrivate::AddTemplateBinding(controlTemplate,
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
                    ::Aero::Controls::TemplatePrivate::AddPropertyTrigger(controlTemplate,
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
                    ::Aero::Controls::TemplatePrivate::AddVisualStateGroup(controlTemplate,
                        std::move(group));
                if (!configured) {
                    break;
                }
            }
        }
        if (configured) {
            configured =
                ::Aero::Controls::TemplatePrivate::Seal(
                    controlTemplate,
                    *self->properties);
        }
        if (!configured) {
            return configured.GetStatus();
        }

        services.deferredContent->ReleaseOwner(
            object);
        ::Aero::Controls::TemplatePrivate::ClearAuthoredVisualTree(controlTemplate);
        ::Aero::Controls::TemplatePrivate::ClearAuthoredVisualStateGroups(controlTemplate);
        ::Aero::Controls::TemplatePrivate::ClearAuthoredTriggers(controlTemplate);
        ::Aero::Controls::TemplatePrivate::ClearAuthoredNames(controlTemplate);
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
        return scopeOwner.RuntimeType() ==
                ControlTemplate::StaticTypeId()
            ? ::Aero::Controls::TemplatePrivate::RegisterAuthoredName(
                  static_cast<ControlTemplate&>(scopeOwner), name, object)
            : ::Aero::Controls::TemplatePrivate::RegisterAuthoredName(
                  static_cast<DataTemplate&>(scopeOwner), name, object);
    }
};

static_assert(
    sizeof(XamlTemplateSchemaFacetState) <= 1024,
    "XamlTemplateSchemaFacet inline state storage is too small");
static_assert(
    alignof(XamlTemplateSchemaFacetState) <= alignof(std::max_align_t),
    "XamlTemplateSchemaFacet inline state alignment is insufficient");

XamlTemplateSchemaFacet::XamlTemplateSchemaFacet(
    Meta::Registry& runtime,
    DependencyPropertyRegistry& properties,
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {
    state_ = new (stateStorage_) XamlTemplateSchemaFacetState(
        runtime,
        properties,
        *allocator_);
}

XamlTemplateSchemaFacet::~XamlTemplateSchemaFacet() noexcept {
    if (state_ == nullptr) {
        return;
    }
    state_->~XamlTemplateSchemaFacetState();
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
    const PropertyInfo* targetType =
        schema.Types().FindProperty(
            ControlTemplate::StaticTypeId(),
            Base::StringView("TargetType"),
            false);
    const PropertyInfo* targetName =
        schema.Types().FindProperty(
            Setter::StaticTypeId(),
            Base::StringView("TargetName"),
            false);
    const PropertyInfo* dataType =
        schema.Types().FindProperty(
            DataTemplate::StaticTypeId(),
            Base::StringView("DataType"),
            false);
    if (targetType == nullptr ||
        dataType == nullptr ||
        targetName == nullptr ||
        targetType->ValueType() !=
            TypeOf<TypeReference>() ||
        dataType->ValueType() !=
            TypeOf<TypeReference>() ||
        targetName->ValueType() !=
            TypeOf<Base::String>()) {
        return InvalidTemplateXaml(
            "Template XAML metadata is incomplete");
    }

    state_->schema = &schema;
    Base::Result<void> status =
        SchemaPrivate::AddType(schema, {
            ControlTemplate::StaticTypeId(),
            nullptr,
            nullptr,
            nullptr,
            state_,
            true,
            true,
            &XamlTemplateSchemaFacetState::RegisterTemplateName,
            nullptr,
            &ResolveTemplateResources,
            &XamlTemplateSchemaFacetState::EndTemplate,
            true,
            &ResolveTemplateImplicitKey});
    if (status) {
        status = SchemaPrivate::AddType(schema, {
            DataTemplate::StaticTypeId(),
            nullptr,
            nullptr,
            nullptr,
            state_,
            true,
            true,
            &XamlTemplateSchemaFacetState::RegisterTemplateName,
            nullptr,
            &ResolveTemplateResources,
            &XamlTemplateSchemaFacetState::EndTemplate,
            true,
            &ResolveTemplateImplicitKey});
    }
    if (status) {
        status = SchemaPrivate::AddType(schema, {
            HierarchicalDataTemplate::StaticTypeId(),
            nullptr,
            nullptr,
            nullptr,
            state_,
            true,
            true,
            &XamlTemplateSchemaFacetState::RegisterTemplateName,
            nullptr,
            &ResolveTemplateResources,
            &XamlTemplateSchemaFacetState::EndTemplate,
            true,
            &ResolveTemplateImplicitKey});
    }
    if (status) {
        status = SchemaPrivate::AddType(schema, {
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
            &XamlTemplateSchemaFacetState::EndTemplate,
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


