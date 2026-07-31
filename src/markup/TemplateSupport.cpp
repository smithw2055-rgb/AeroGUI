#include "UiObjectModelInternal.hpp"

#include <Aero/Base/String.hpp>
#include <Aero/Controls/Base.hpp>
#include <Aero/Controls/Items.hpp>
#include <Aero/Styling.hpp>
#include "SchemaInternal.hpp"
#include <Aero/Markup/Schema.hpp>

#include "TemplateCompiler.hpp"
#include "../controls/FrameworkTemplateAccess.hpp"

#include <new>
#include <utility>
#include "../ui/RuntimeManagers.hpp"

namespace Aero::Markup {
namespace {

using namespace Aero::Controls;
using namespace Aero::Core;


class CompiledTemplateProgramOwner final
    : public Base::Object {
public:
    explicit CompiledTemplateProgramOwner(
        Detail::CompiledTemplateBlueprint blueprint) noexcept
        : blueprint_(std::move(blueprint)) {}
    ~CompiledTemplateProgramOwner() noexcept override = default;

    Detail::CompiledTemplateBlueprint& Blueprint() noexcept {
        return blueprint_;
    }

private:
    Detail::CompiledTemplateBlueprint blueprint_;
};

Base::Status InvalidTemplateXaml(
    const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::ValidationFailed,
        message);
}

bool HasTypeFlag(
    TypeFlags value,
    TypeFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

Aero::ResourceDictionary* ResolveTemplateResources(
    Base::Object& object,
    void*) noexcept {
    if (object.RuntimeType() ==
            ControlTemplate::StaticTypeId()) {
        return &static_cast<ControlTemplate&>(
            object).Resources();
    }
    if (object.RuntimeType() ==
            DataTemplate::StaticTypeId()) {
        return &static_cast<DataTemplate&>(
            object).Resources();
    }
    if (object.RuntimeType() ==
            ItemsPanelTemplate::StaticTypeId()) {
        return &static_cast<ItemsPanelTemplate&>(
            object).Resources();
    }
    return nullptr;
}

Base::Result<Aero::ResourceKey>
ResolveTemplateImplicitKey(
    const Base::Object& object,
    void*) noexcept {
    Core::TypeId key = Core::InvalidTypeId;
    if (object.RuntimeType() == ControlTemplate::StaticTypeId()) {
        key = static_cast<const ControlTemplate&>(object).TargetType();
    } else if (object.RuntimeType() == DataTemplate::StaticTypeId()) {
        key = static_cast<const DataTemplate&>(object).DataType();
    }
    if (key == Core::InvalidTypeId) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Template type has no implicit resource key");
    }
    return Aero::ResourceKey::FromType(key);
}

} // namespace

namespace Detail {

struct XamlTemplateSchemaFacet::Impl final {
    Impl(
        MetadataRuntime& metadataRuntime,
        DependencyPropertyRegistry& dependencyProperties,
        Base::IAllocator& programAllocator) noexcept
        : allocator(&programAllocator),
          runtime(&metadataRuntime),
          properties(&dependencyProperties) {}

    Base::IAllocator* allocator = nullptr;
    MetadataRuntime* runtime = nullptr;
    DependencyPropertyRegistry* properties = nullptr;
    Schema* schema = nullptr;

    static Base::Result<void> EndTemplate(
        Base::Object& object,
        const ExtensionContext& services,
        void* context) noexcept {
        auto* self = static_cast<Impl*>(context);
        const TypeId type = object.RuntimeType();
        if (self == nullptr || self->runtime == nullptr ||
            self->properties == nullptr ||
            (type != ControlTemplate::StaticTypeId() &&
             type != DataTemplate::StaticTypeId() &&
             type != ItemsPanelTemplate::StaticTypeId()) ||
            services.deferredContentOwner != &object ||
            services.deferredContent == nullptr) {
            return InvalidTemplateXaml(
                "Template deferred-content scope is invalid");
        }
        if (type == ControlTemplate::StaticTypeId() ||
            type == DataTemplate::StaticTypeId()) {
            const Core::TypeId targetType =
                type == ControlTemplate::StaticTypeId()
                ? static_cast<ControlTemplate&>(object)
                      .TargetType()
                : static_cast<DataTemplate&>(object)
                      .DataType();
            // A keyed WPF ControlTemplate may deliberately omit TargetType.
            // Its target is inferred from the Style/Setter that consumes it,
            // so only validate an explicitly authored type here. The apply
            // path still checks that the eventual target is a Control.
            if (targetType != Core::InvalidTypeId) {
                const Core::TypeInfo* targetInfo =
                    self->runtime->Types().FindType(targetType);
                if (targetInfo == nullptr ||
                    HasTypeFlag(
                        targetInfo->Flags(),
                        Core::TypeFlags::ValueType)) {
                    return InvalidTemplateXaml(
                        "Template type constraint must identify an object type");
                }
                if (type == ControlTemplate::StaticTypeId() &&
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
            Base::Result<void> baseUri;
            if (object.RuntimeType() ==
                    ControlTemplate::StaticTypeId()) {
                auto& program =
                    static_cast<ControlTemplate&>(
                        object).RuntimeData();
                if (program.BaseUri().Empty()) {
                    baseUri = program.SetBaseUri(
                        *services.baseUri);
                }
            } else if (object.RuntimeType() ==
                       DataTemplate::StaticTypeId()) {
                auto& program =
                    static_cast<DataTemplate&>(
                        object).Program();
                if (program.BaseUri().Empty()) {
                    baseUri = program.SetBaseUri(
                        *services.baseUri);
                }
            } else if (object.RuntimeType() ==
                       ItemsPanelTemplate::StaticTypeId()) {
                auto& program =
                    static_cast<ItemsPanelTemplate&>(
                        object).Program();
                if (program.BaseUri().Empty()) {
                    baseUri = program.SetBaseUri(
                        *services.baseUri);
                }
            }
            if (!baseUri) return baseUri.GetStatus();
        }
        if (object.RuntimeType() ==
                DataTemplate::StaticTypeId() ||
            object.RuntimeType() ==
                ItemsPanelTemplate::StaticTypeId()) {
            const Base::Ref<Base::Object>* authored = nullptr;
            if (object.RuntimeType() ==
                    DataTemplate::StaticTypeId()) {
                authored =
                    &static_cast<DataTemplate&>(
                         object).AuthoredVisualTree();
            } else {
                authored =
                    &static_cast<ItemsPanelTemplate&>(
                         object).AuthoredVisualTree();
            }
            Base::Result<Detail::CompiledTemplateBlueprint>
                compiled =
                    Detail::CompileDeferredTemplateBlueprint(
                        *authored,
                        object.RuntimeType() ==
                                DataTemplate::StaticTypeId()
                            ? &static_cast<DataTemplate&>(
                                  object).AuthoredNames()
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
            if (object.RuntimeType() ==
                    DataTemplate::StaticTypeId()) {
                auto& dataTemplate =
                    static_cast<DataTemplate&>(object);
                Base::Result<void> reserved =
                    compiled.Value().
                        dataTemplateTriggers.TryReserve(
                            dataTemplate.
                                AuthoredTriggers().Size());
                if (!reserved) {
                    return reserved.GetStatus();
                }
                for (const Base::Ref<
                         Aero::TriggerBase>& trigger :
                     dataTemplate.AuthoredTriggers()) {
                    Base::Result<void> retained =
                        compiled.Value().
                            dataTemplateTriggers.
                                TryPushBack(trigger);
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
            Detail::CompiledTemplateBlueprint* programContext =
                &program.Value()->Blueprint();
            Base::Ref<Base::Object> programOwner =
                program.Value();
            Base::Result<void> configured;
            if (object.RuntimeType() ==
                    DataTemplate::StaticTypeId()) {
                auto& dataTemplate =
                    static_cast<DataTemplate&>(object);
                configured = dataTemplate.Configure(
                    &Detail::BuildCompiledDeferredTemplate,
                    programContext,
                    std::move(programOwner));
                if (configured) {
                    configured = dataTemplate.Seal();
                }
            } else {
                auto& itemsPanel =
                    static_cast<ItemsPanelTemplate&>(object);
                configured = itemsPanel.Configure(
                    &Detail::BuildCompiledDeferredTemplate,
                    programContext,
                    std::move(programOwner));
                if (configured) {
                    configured = itemsPanel.Seal();
                }
            }
            if (!configured) {
                return configured.GetStatus();
            }
            services.deferredContent->ReleaseOwner(
                object);
            if (object.RuntimeType() ==
                    DataTemplate::StaticTypeId()) {
                auto& dataTemplate =
                    static_cast<DataTemplate&>(object);
                dataTemplate.ClearAuthoredVisualTree();
                dataTemplate.ClearAuthoredTriggers();
                dataTemplate.ClearAuthoredNames();
            } else {
                static_cast<ItemsPanelTemplate&>(object)
                    .ClearAuthoredVisualTree();
            }
            return {};
        }
        auto& controlTemplate =
            static_cast<ControlTemplate&>(object);
        if (controlTemplate.TargetType() ==
            Core::InvalidTypeId) {
            // WPF permits a keyed ControlTemplate to omit TargetType. The
            // consuming Style supplies the concrete control at apply time;
            // compile against the common Control contract so its authored
            // bindings and triggers remain valid until then.
            Base::Result<void> inferred =
                controlTemplate.TrySetTargetType(
                    Control::StaticTypeId());
            if (!inferred) return inferred.GetStatus();
        }
        Base::Result<Detail::CompiledTemplateDefinition>
            compiled =
                Detail::CompileControlTemplateDefinition(
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
        Detail::CompiledTemplateBlueprint* programContext =
            &program.Value()->Blueprint();
        Base::Ref<Base::Object> programOwner =
            program.Value();

        Base::Result<void> configured =
            controlTemplate.ConfigureFactory(
                &Detail::BuildCompiledTemplate,
                programContext,
                std::move(programOwner));
        if (configured) {
            for (const TemplateBindingPlan& binding :
                 compiled.Value().
                     contentSourceBindings) {
                configured =
                    controlTemplate.
                        TryAddTemplateBinding(
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
                    controlTemplate.TryAddPropertyTrigger(
                        std::move(trigger));
                if (!configured) {
                    break;
                }
            }
        }
        if (configured) {
            for (VisualStateGroup& group :
                 compiled.Value().visualStateGroups) {
                configured =
                    controlTemplate.TryAddVisualStateGroup(
                        std::move(group));
                if (!configured) {
                    break;
                }
            }
        }
        if (configured) {
            configured =
                Controls::Detail::FrameworkTemplateAccess::Seal(
                    controlTemplate,
                    *self->properties);
        }
        if (!configured) {
            return configured.GetStatus();
        }

        services.deferredContent->ReleaseOwner(
            object);
        controlTemplate.ClearAuthoredVisualTree();
        controlTemplate.ClearAuthoredVisualStateGroups();
        controlTemplate.ClearAuthoredTriggers();
        controlTemplate.ClearAuthoredNames();
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
                 DataTemplate::StaticTypeId())) {
            return InvalidTemplateXaml(
                "Template name scope is invalid");
        }
        return scopeOwner.RuntimeType() ==
                ControlTemplate::StaticTypeId()
            ? static_cast<ControlTemplate&>(
                  scopeOwner).RegisterAuthoredName(
                  name, object)
            : static_cast<DataTemplate&>(
                  scopeOwner).RegisterAuthoredName(
                  name, object);
    }
};

XamlTemplateSchemaFacet::XamlTemplateSchemaFacet(
    MetadataRuntime& runtime,
    DependencyPropertyRegistry& properties,
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {
    void* memory = allocator_->Allocate({
        sizeof(Impl),
        alignof(Impl),
        Base::MemoryTag::Markup});
    if (memory != nullptr) {
        impl_ = new (memory) Impl(
            runtime,
            properties,
            *allocator_);
    }
}

XamlTemplateSchemaFacet::~XamlTemplateSchemaFacet() noexcept {
    if (impl_ == nullptr) {
        return;
    }
    impl_->~Impl();
    allocator_->Deallocate(
        impl_,
        sizeof(Impl),
        alignof(Impl),
        Base::MemoryTag::Markup);
    impl_ = nullptr;
}

Base::Result<void> XamlTemplateSchemaFacet::Register(
    Schema& schema) noexcept {
    if (impl_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "XAML template extension allocation failed");
    }
    if (schema.IsFrozen() ||
        impl_->schema != nullptr) {
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

    impl_->schema = &schema;
    Base::Result<void> status =
        Detail::SchemaAccess::AddType(schema, {
            ControlTemplate::StaticTypeId(),
            nullptr,
            nullptr,
            nullptr,
            impl_,
            true,
            true,
            &Impl::RegisterTemplateName,
            nullptr,
            &ResolveTemplateResources,
            &Impl::EndTemplate,
            true,
            &ResolveTemplateImplicitKey});
    if (status) {
        status = Detail::SchemaAccess::AddType(schema, {
            DataTemplate::StaticTypeId(),
            nullptr,
            nullptr,
            nullptr,
            impl_,
            true,
            true,
            &Impl::RegisterTemplateName,
            nullptr,
            &ResolveTemplateResources,
            &Impl::EndTemplate,
            true,
            &ResolveTemplateImplicitKey});
    }
    if (status) {
        status = Detail::SchemaAccess::AddType(schema, {
            ItemsPanelTemplate::StaticTypeId(),
            nullptr,
            nullptr,
            nullptr,
            impl_,
            true,
            true,
            nullptr,
            nullptr,
            &ResolveTemplateResources,
            &Impl::EndTemplate,
            true,
            nullptr});
    }
    if (!status) {
        impl_->schema = nullptr;
        return status.GetStatus();
    }
    return {};
}

} // namespace Detail
} // namespace Aero::Markup
