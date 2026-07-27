#include "XamlPresentationObjectModelInternal.hpp"

#include <Aero/Base/String.hpp>
#include <Aero/Controls/ControlPrimitives.hpp>
#include <Aero/Controls/Items.hpp>
#include <Aero/Controls/Templates.hpp>
#include <Aero/Markup/Schema/XamlSchemaContext.hpp>
#include <Aero/Presentation/Style.hpp>

#include "XamlTemplateCompiler.hpp"

#include <new>
#include <utility>

namespace Aero::Markup {
namespace {

using namespace Aero::Controls;
using namespace Aero::Core;
using namespace Aero::Presentation;

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

Base::Result<TypeId> ResolveTargetType(
    const XamlServiceProvider& services,
    Base::StringView name) noexcept {
    if (services.schema == nullptr || name.Empty()) {
        return InvalidTemplateXaml(
            "ControlTemplate TargetType is invalid");
    }
    std::uint32_t colon = name.SizeBytes();
    for (std::uint32_t index = 0U;
         index < name.SizeBytes(); ++index) {
        if (name[index] != ':') {
            continue;
        }
        if (colon != name.SizeBytes()) {
            return InvalidTemplateXaml(
                "ControlTemplate TargetType has multiple prefixes");
        }
        colon = index;
    }
    Base::StringView prefix;
    Base::StringView localName = name;
    if (colon != name.SizeBytes()) {
        if (colon == 0U ||
            colon + 1U >= name.SizeBytes()) {
            return InvalidTemplateXaml(
                "ControlTemplate TargetType prefix is malformed");
        }
        prefix = name.Substr(0U, colon);
        localName = name.Substr(
            colon + 1U,
            name.SizeBytes() - colon - 1U);
    }
    Base::Result<Base::StringView> xamlNamespace =
        services.namespaces.Lookup(prefix);
    if (!xamlNamespace) {
        return xamlNamespace.GetStatus();
    }
    Base::Result<const MetadataTypeDescriptor*> resolved =
        services.schema->ResolveType(
            xamlNamespace.Value(),
            localName);
    if (!resolved ||
        HasTypeFlag(
            resolved.Value()->Flags(),
            TypeFlags::ValueType)) {
        return resolved
            ? Base::Result<TypeId>(
                  InvalidTemplateXaml(
                      "ControlTemplate TargetType must be an object type"))
            : Base::Result<TypeId>(
                  resolved.GetStatus());
    }
    return resolved.Value()->Id();
}

ResourceDictionary* ResolveTemplateResources(
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

Base::Result<ResourceKey> ResolveTemplateImplicitKey(
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
    return ResourceKey::FromType(key);
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
    XamlSchemaContext* schema = nullptr;

    static Base::Result<void> EndTemplate(
        Base::Object& object,
        const XamlServiceProvider& services,
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
        Base::Vector<XamlDeferredContentEdge> edges(
            self->allocator);
        Base::Result<void> copied =
            services.deferredContent->CopyForOwner(
                object, edges);
        if (!copied) return copied.GetStatus();

        if (services.baseUri != nullptr) {
            Base::Result<void> baseUri;
            if (object.RuntimeType() ==
                    ControlTemplate::StaticTypeId()) {
                auto& program =
                    static_cast<ControlTemplate&>(
                        object).Program();
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
                        {
                            edges.Data(),
                            edges.Size()},
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
                static_cast<DataTemplate&>(object)
                    .ClearAuthoredVisualTree();
            } else {
                static_cast<ItemsPanelTemplate&>(object)
                    .ClearAuthoredVisualTree();
            }
            return {};
        }
        auto& controlTemplate =
            static_cast<ControlTemplate&>(object);
        Base::Result<Detail::CompiledTemplateDefinition>
            compiled =
                Detail::CompileControlTemplateDefinition(
                    controlTemplate,
                    {
                        edges.Data(),
                        edges.Size()},
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
            controlTemplate.ConfigureProgram(
                &Detail::BuildCompiledTemplate,
                programContext,
                std::move(programOwner));
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
                controlTemplate.Seal(
                    *self->properties);
        }
        if (!configured) {
            return configured.GetStatus();
        }

        services.deferredContent->ReleaseOwner(
            object);
        controlTemplate.ClearAuthoredVisualTree();
        controlTemplate.ClearAuthoredVisualStateGroups();
        controlTemplate.ClearAuthoredNames();
        return {};
    }

    static Base::Result<void> SetTargetType(
        Base::Object& object,
        const XamlValue& value,
        const XamlServiceProvider& services,
        void* context) noexcept {
        auto* self = static_cast<Impl*>(context);
        if (self == nullptr ||
            object.RuntimeType() !=
                ControlTemplate::StaticTypeId() ||
            value.Kind() != ValueKind::String) {
            return InvalidTemplateXaml(
                "ControlTemplate TargetType value is invalid");
        }
        Base::Result<TypeId> type =
            ResolveTargetType(
                services,
                value.AsString());
        if (!type) {
            return type.GetStatus();
        }
        if (!self->runtime->Descriptors().IsDerivedFrom(
                type.Value(),
                Control::StaticTypeId())) {
            return InvalidTemplateXaml(
                "ControlTemplate TargetType is not a Control");
        }
        Base::Result<void> assigned =
            static_cast<ControlTemplate&>(object)
                .TrySetTargetType(type.Value());
        if (!assigned) {
            return assigned.GetStatus();
        }
        if (services.baseUri != nullptr) {
            return static_cast<ControlTemplate&>(object)
                .Program().SetBaseUri(
                    *services.baseUri);
        }
        return {};
    }

    static Base::Result<void> SetDataType(
        Base::Object& object,
        const XamlValue& value,
        const XamlServiceProvider& services,
        void* context) noexcept {
        auto* self = static_cast<Impl*>(context);
        if (self == nullptr ||
            object.RuntimeType() !=
                DataTemplate::StaticTypeId() ||
            value.Kind() != ValueKind::String) {
            return InvalidTemplateXaml(
                "DataTemplate DataType value is invalid");
        }
        Base::Result<TypeId> type =
            ResolveTargetType(
                services,
                value.AsString());
        if (!type) return type.GetStatus();
        Base::Result<void> assigned =
            static_cast<DataTemplate&>(object)
                .SetDataType(type.Value());
        if (!assigned) return assigned.GetStatus();
        if (services.baseUri != nullptr) {
            return static_cast<DataTemplate&>(object)
                .Program().SetBaseUri(
                    *services.baseUri);
        }
        return {};
    }

    static Base::Result<void> SetSetterTargetName(
        Base::Object& object,
        const XamlValue& value,
        void* context) noexcept {
        if (context == nullptr ||
            object.RuntimeType() !=
                Setter::StaticTypeId() ||
            value.Kind() != ValueKind::String) {
            return InvalidTemplateXaml(
                "Template Setter.TargetName value is invalid");
        }
        return static_cast<Setter&>(object)
            .SetTargetName(value.AsString());
    }

    static Base::Result<void> RegisterTemplateName(
        Base::Object& scopeOwner,
        Base::StringView name,
        Base::Object& object,
        void* context) noexcept {
        if (context == nullptr ||
            scopeOwner.RuntimeType() !=
                ControlTemplate::StaticTypeId()) {
            return InvalidTemplateXaml(
                "ControlTemplate name scope is invalid");
        }
        return static_cast<ControlTemplate&>(
            scopeOwner).RegisterAuthoredName(
                name,
                object);
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
    XamlSchemaContext& schema) noexcept {
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
    const MetadataPropertyDescriptor* targetType =
        schema.Descriptors().FindProperty(
            ControlTemplate::StaticTypeId(),
            Base::StringView("TargetType"),
            false);
    const MetadataPropertyDescriptor* targetName =
        schema.Descriptors().FindProperty(
            Setter::StaticTypeId(),
            Base::StringView("TargetName"),
            false);
    const MetadataPropertyDescriptor* dataType =
        schema.Descriptors().FindProperty(
            DataTemplate::StaticTypeId(),
            Base::StringView("DataType"),
            false);
    if (targetType == nullptr ||
        dataType == nullptr ||
        targetName == nullptr) {
        return InvalidTemplateXaml(
            "Template XAML metadata is incomplete");
    }

    impl_->schema = &schema;
    Base::Result<void> status =
        schema.TryRegisterMemberAdapter({
            targetType->Id(),
            XamlMemberWriteMode::SetOnce,
            nullptr,
            impl_,
            &Impl::SetTargetType,
            true});
    if (status) {
        status = schema.TryRegisterMemberAdapter({
            targetName->Id(),
            XamlMemberWriteMode::SetOnce,
            &Impl::SetSetterTargetName,
            impl_});
    }
    if (status) {
        status = schema.TryRegisterMemberAdapter({
            dataType->Id(),
            XamlMemberWriteMode::SetOnce,
            nullptr,
            impl_,
            &Impl::SetDataType,
            true});
    }
    if (status) {
        status = schema.TryRegisterTypeAdapter({
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
    }
    if (status) {
        status = schema.TryRegisterTypeAdapter({
            DataTemplate::StaticTypeId(),
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
            &ResolveTemplateImplicitKey});
    }
    if (status) {
        status = schema.TryRegisterTypeAdapter({
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
