#include "Extensions.hpp"
#include "../data/BindingRuntime.hpp"
#include "../ui/RuntimeManagers.hpp"

// Binding markup-extension implementation.
#include "DeferredContent.hpp"
#include "SchemaInternal.hpp"

#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Styling.hpp>
#include <Aero/Controls/Items.hpp>
#include <Aero/Animation.hpp>

#include <new>

namespace Aero::Markup {
namespace {

constexpr Base::StringView ElementNameKey("ElementName");
constexpr Base::StringView SourceKey("Source");
constexpr Base::StringView PathKey("Path");
constexpr Base::StringView ModeKey("Mode");
constexpr Base::StringView RelativeSourceKey("RelativeSource");
constexpr Base::StringView StringFormatKey("StringFormat");
constexpr Base::StringView FallbackValueKey("FallbackValue");
constexpr Base::StringView UpdateSourceTriggerKey("UpdateSourceTrigger");
constexpr Base::StringView OneTimeMode("OneTime");
constexpr Base::StringView OneWayMode("OneWay");
constexpr Base::StringView TwoWayMode("TwoWay");
constexpr Base::StringView OneWayToSourceMode("OneWayToSource");
constexpr Base::StringView PropertyChangedTrigger("PropertyChanged");
constexpr Base::StringView ExplicitTrigger("Explicit");
constexpr Base::StringView SelfValue("Self");
constexpr Base::StringView TemplatedParentValue("TemplatedParent");
constexpr Base::StringView RelativeSourcePrefix("{RelativeSource");
constexpr Base::StringView StaticResourcePrefix("{StaticResource");

enum class RelativeSourceKind : std::uint8_t {
    None = 0U,
    Self,
    TemplatedParent,
    Ancestor
};

Base::StringView TrimAscii(Base::StringView value) noexcept {
    std::uint32_t first = 0U;
    std::uint32_t last = value.SizeBytes();
    while (first < last &&
           (value[first] == ' ' || value[first] == '\t' ||
            value[first] == '\r' || value[first] == '\n')) {
        ++first;
    }
    while (last > first &&
           (value[last - 1U] == ' ' || value[last - 1U] == '\t' ||
            value[last - 1U] == '\r' || value[last - 1U] == '\n')) {
        --last;
    }
    return value.Substr(first, last - first);
}


Base::Result<void> ParseArguments(
    Base::StringView arguments,
    Base::StringView& elementName,
    Base::StringView& sourceResource,
    Base::StringView& path,
    Base::StringView& stringFormat,
    Base::StringView& fallbackValue,
    Base::StringView& ancestorType,
    RelativeSourceKind& relativeSource,
    Data::BindingMode& mode,
    Core::UpdateSourceTrigger& updateSourceTrigger) noexcept {
    elementName = {};
    sourceResource = {};
    path = {};
    stringFormat = {};
    fallbackValue = {};
    ancestorType = {};
    relativeSource = RelativeSourceKind::None;
    mode = Data::BindingMode::OneWay;
    updateSourceTrigger = Core::UpdateSourceTrigger::PropertyChanged;

    std::uint32_t begin = 0U;
    while (begin < arguments.SizeBytes()) {
        std::uint32_t end = begin;
        std::uint32_t depth = 0U;
        char quote = '\0';
        while (end < arguments.SizeBytes()) {
            const char character = arguments[end];
            if (quote != '\0') {
                if (character == quote) quote = '\0';
            } else if (character == '\'' || character == '"') {
                quote = character;
            } else if (character == '{') {
                ++depth;
            } else if (character == '}') {
                if (depth == 0U) {
                    return Base::Status::Failure(
                        Base::ErrorCode::ValidationFailed,
                        "Binding contains an unmatched closing brace");
                }
                --depth;
            } else if (character == ',' && depth == 0U) {
                break;
            }
            ++end;
        }
        if (depth != 0U || quote != '\0') {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Binding contains an incomplete nested markup extension");
        }
        const Base::StringView item = TrimAscii(arguments.Substr(begin, end - begin));
        const std::uint32_t equals = [&item]() noexcept {
            std::uint32_t depth = 0U;
            char quote = '\0';
            for (std::uint32_t index = 0U; index < item.SizeBytes(); ++index) {
                const char character = item[index];
                if (quote != '\0') {
                    if (character == quote) quote = '\0';
                } else if (character == '\'' || character == '"') {
                    quote = character;
                } else if (character == '{') {
                    ++depth;
                } else if (character == '}') {
                    if (depth > 0U) --depth;
                } else if (character == '=' && depth == 0U) {
                    return index;
                }
            }
            return item.SizeBytes();
        }();
        if (item.Empty()) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Binding argument is empty");
        }
        if (equals == item.SizeBytes()) {
            if (!path.Empty()) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Binding positional Path is specified more than once");
            }
            path = item;
            begin = end + 1U;
            continue;
        }
        const Base::StringView key = TrimAscii(item.Substr(0U, equals));
        const Base::StringView value = TrimAscii(item.Substr(
            equals + 1U,
            item.SizeBytes() - equals - 1U));
        if (value.Empty()) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Binding argument value is empty");
        }
        if (key == ElementNameKey) {
            if (!elementName.Empty()) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Binding ElementName is specified more than once");
            }
            elementName = value;
        } else if (key == SourceKey) {
            if (!sourceResource.Empty()) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Binding Source is specified more than once");
            }
            if (value.SizeBytes() <=
                    StaticResourcePrefix.SizeBytes() + 1U ||
                value.Substr(0U, StaticResourcePrefix.SizeBytes()) !=
                    StaticResourcePrefix ||
                value[value.SizeBytes() - 1U] != '}') {
                return Base::Status::Failure(
                    Base::ErrorCode::Unsupported,
                    "Binding Source currently requires a StaticResource");
            }
            sourceResource = TrimAscii(value.Substr(
                StaticResourcePrefix.SizeBytes(),
                value.SizeBytes() - StaticResourcePrefix.SizeBytes() - 1U));
            if (sourceResource.Empty()) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Binding StaticResource key is empty");
            }
        } else if (key == RelativeSourceKey) {
            if (relativeSource != RelativeSourceKind::None) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Binding RelativeSource is specified more than once");
            }
            Base::StringView relative = TrimAscii(value);
            if (relative == TemplatedParentValue) {
                relativeSource =
                    RelativeSourceKind::TemplatedParent;
            } else if (
                relative.SizeBytes() >
                    RelativeSourcePrefix.SizeBytes() + 1U &&
                relative.Substr(
                    0U,
                    RelativeSourcePrefix.SizeBytes()) ==
                    RelativeSourcePrefix &&
                relative[relative.SizeBytes() - 1U] == '}') {
                Base::StringView relativeMode = TrimAscii(
                    relative.Substr(
                        RelativeSourcePrefix.SizeBytes(),
                        relative.SizeBytes() -
                            RelativeSourcePrefix.SizeBytes() - 1U));
                constexpr Base::StringView ModePrefix("Mode=");
                if (relativeMode.SizeBytes() >= ModePrefix.SizeBytes() &&
                    relativeMode.Substr(0U, ModePrefix.SizeBytes()) ==
                        ModePrefix) {
                    relativeMode = TrimAscii(relativeMode.Substr(
                        ModePrefix.SizeBytes(),
                        relativeMode.SizeBytes() - ModePrefix.SizeBytes()));
                }
                constexpr Base::StringView AncestorPrefix("AncestorType=");
                if (relativeMode == SelfValue) {
                    relativeSource = RelativeSourceKind::Self;
                } else if (relativeMode == TemplatedParentValue) {
                    relativeSource = RelativeSourceKind::TemplatedParent;
                } else if (relativeMode.SizeBytes() >=
                           AncestorPrefix.SizeBytes() &&
                    relativeMode.Substr(0U, AncestorPrefix.SizeBytes()) ==
                        AncestorPrefix) {
                    Base::StringView typeName = TrimAscii(relativeMode.Substr(
                        AncestorPrefix.SizeBytes(),
                        relativeMode.SizeBytes() - AncestorPrefix.SizeBytes()));
                    constexpr Base::StringView TypePrefix("{x:Type");
                    if (typeName.SizeBytes() > TypePrefix.SizeBytes() + 1U &&
                        typeName.Substr(0U, TypePrefix.SizeBytes()) == TypePrefix &&
                        typeName[typeName.SizeBytes() - 1U] == '}') {
                        typeName = TrimAscii(typeName.Substr(
                            TypePrefix.SizeBytes(),
                            typeName.SizeBytes() - TypePrefix.SizeBytes() - 1U));
                    }
                    if (typeName.Empty()) {
                        return Base::Status::Failure(
                            Base::ErrorCode::ValidationFailed,
                            "Binding RelativeSource AncestorType is empty");
                    }
                    ancestorType = typeName;
                    relativeSource = RelativeSourceKind::Ancestor;
                } else {
                    return Base::Status::Failure(
                        Base::ErrorCode::Unsupported,
                        "Binding RelativeSource mode is not supported");
                }
            } else {
                return Base::Status::Failure(
                    Base::ErrorCode::Unsupported,
                    "Binding RelativeSource mode is not supported");
            }
        } else if (key == StringFormatKey) {
            if (!stringFormat.Empty()) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Binding StringFormat is specified more than once");
            }
            stringFormat = value;
        } else if (key == FallbackValueKey) {
            if (!fallbackValue.Empty()) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Binding FallbackValue is specified more than once");
            }
            // The metadata binding engine does not yet need the fallback for
            // a resolvable path, but WPF permits it on every Binding. Parse
            // it here so the declaration remains valid while the fallback is
            // carried by the higher-level binding semantics incrementally.
            fallbackValue = value;
        } else if (key == PathKey) {
            if (!path.Empty()) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Binding Path is specified more than once");
            }
            path = value;
        } else if (key == ModeKey) {
            if (value == OneTimeMode) {
                mode = Data::BindingMode::OneTime;
            } else if (value == OneWayMode) {
                mode = Data::BindingMode::OneWay;
            } else if (value == TwoWayMode) {
                mode = Data::BindingMode::TwoWay;
            } else if (value == OneWayToSourceMode) {
                mode = Data::BindingMode::OneWayToSource;
            } else {
                return Base::Status::Failure(
                    Base::ErrorCode::Unsupported,
                    "Binding mode is not supported");
            }
        } else if (key == UpdateSourceTriggerKey) {
            if (value == PropertyChangedTrigger) {
                updateSourceTrigger = Core::UpdateSourceTrigger::PropertyChanged;
            } else if (value == ExplicitTrigger) {
                updateSourceTrigger = Core::UpdateSourceTrigger::Explicit;
            } else {
                return Base::Status::Failure(
                    Base::ErrorCode::Unsupported,
                    "Binding update trigger is not supported");
            }
        } else {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Binding argument is not supported");
        }
        begin = end + 1U;
    }
    if (path.Empty() && elementName.Empty() &&
        sourceResource.Empty() &&
        relativeSource == RelativeSourceKind::None) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Binding requires Path, Source, ElementName, or RelativeSource");
    }
    return {};
}

struct DeferredBindingState final {
    Aero::Detail::BindingManager* manager = nullptr;
    Core::MetadataRuntime* metadata = nullptr;
    Base::Object* source = nullptr;
    Core::DependencyObject* target = nullptr;
    Core::DependencyPropertyHandle targetProperty;
    Core::DependencyPropertyHandle dataContextProperty;
    Base::String path;
    Base::String stringFormat;
    bool bindsToSource = false;
    Data::BindingMode mode = Data::BindingMode::OneWay;
    Core::UpdateSourceTrigger updateSourceTrigger =
        Core::UpdateSourceTrigger::PropertyChanged;
    Base::IAllocator* allocator = nullptr;
};

Base::Result<std::uint64_t> CommitBinding(void* context) noexcept {
    auto* state = static_cast<DeferredBindingState*>(context);
    if (state == nullptr || state->manager == nullptr ||
        state->metadata == nullptr || state->target == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Deferred Binding state is invalid");
    }
    Data::MetadataBindingDescriptor descriptor;
    descriptor.metadata = state->metadata;
    descriptor.source = state->source;
    descriptor.target = state->target;
    descriptor.targetProperty = state->targetProperty;
    descriptor.dataContextProperty = state->dataContextProperty;
    descriptor.path = state->path.View();
    descriptor.stringFormat =
        state->stringFormat.View();
    descriptor.bindsToSource = state->bindsToSource;
    descriptor.mode = state->mode;
    descriptor.updateSourceTrigger = state->updateSourceTrigger;
    Base::Result<Data::BindingHandle> attached =
        state->manager->Attach(descriptor);
    return attached
        ? Base::Result<std::uint64_t>(attached.Value().value)
        : Base::Result<std::uint64_t>(attached.GetStatus());
}

void RollbackBinding(
    void* context,
    std::uint64_t token) noexcept {
    auto* state = static_cast<DeferredBindingState*>(context);
    if (state != nullptr && state->manager != nullptr && token != 0U) {
        static_cast<void>(state->manager->Detach({token}));
    }
}

void CleanupBinding(void* context) noexcept {
    auto* state = static_cast<DeferredBindingState*>(context);
    if (state == nullptr) return;
    Base::IAllocator* allocator = state->allocator;
    state->~DeferredBindingState();
    allocator->Deallocate(
        state, sizeof(DeferredBindingState),
        alignof(DeferredBindingState), Base::MemoryTag::Markup);
}

} // namespace

BindingExtension::BindingExtension(
    const BindingExtensionOptions& options) noexcept
    : options_(options) {}

Base::Result<void> BindingExtension::Register(
    Schema& schema,
    Core::TypeId bindingExtensionType) noexcept {
    return Detail::SchemaAccess::AddMarkupExtension(schema, {
        bindingExtensionType,
        &BindingExtension::ProvideValue,
        this});
}

Base::Result<ProvidedValue> BindingExtension::ProvideValue(
    Base::StringView arguments,
    const ExtensionContext& services,
    void* context) noexcept {
    BindingExtension* extension =
        static_cast<BindingExtension*>(context);
    if (extension == nullptr ||
        services.schema == nullptr || services.targetObject == nullptr ||
        services.nameScope == nullptr ||
        services.targetMember == Core::InvalidMemberId) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Binding markup extension has no target service context");
    }

    Base::StringView elementName;
    Base::StringView sourceResource;
    Base::StringView path;
    Base::StringView stringFormat;
    Base::StringView fallbackValue;
    Base::StringView ancestorType;
    RelativeSourceKind relativeSource =
        RelativeSourceKind::None;
    Data::BindingMode mode = Data::BindingMode::OneWay;
    Core::UpdateSourceTrigger updateSourceTrigger =
        Core::UpdateSourceTrigger::PropertyChanged;
    Base::Result<void> parsed = ParseArguments(
        arguments,
        elementName,
        sourceResource,
        path,
        stringFormat,
        fallbackValue,
        ancestorType,
        relativeSource,
        mode,
        updateSourceTrigger);
    if (!parsed) {
        return parsed.GetStatus();
    }
    (void)fallbackValue;
    if ((!elementName.Empty() &&
         relativeSource != RelativeSourceKind::None) ||
        (!sourceResource.Empty() &&
         (!elementName.Empty() ||
          relativeSource != RelativeSourceKind::None))) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Binding Source, ElementName, and RelativeSource are mutually exclusive");
    }

    Core::MetadataRuntime* metadata =
        Detail::SchemaAccess::Runtime(
            *services.schema);
    const Core::PropertyInfo* targetMember =
        metadata != nullptr
        ? metadata->Types().FindProperty(
            services.targetMember)
        : nullptr;
    // Setter is only an authored declaration. Its target object is created
    // when the Style is applied, so preserve the binding specification here.
    const bool authoredSetterValue =
        targetMember != nullptr &&
        targetMember->OwnerType() == Aero::Setter::StaticTypeId() &&
        targetMember->Name() == Base::StringView("Value");
    const bool authoredHierarchicalItemsSource =
        targetMember != nullptr &&
        targetMember->OwnerType() == Controls::DataTemplate::StaticTypeId() &&
        targetMember->Name() == Base::StringView("ItemsSource");
    const bool authoredLaunchPath =
        targetMember != nullptr &&
        services.targetObject->RuntimeType() ==
            Media::Animation::LaunchUriOrFileAction::StaticTypeId() &&
        targetMember->Name() == Base::StringView("Path");
    if ((targetMember != nullptr &&
         targetMember->ValueType() ==
             Data::Binding::StaticTypeId()) ||
        authoredSetterValue || authoredHierarchicalItemsSource ||
        authoredLaunchPath) {
        if (!sourceResource.Empty()) {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Data::Binding does not support an explicit Source");
        }
        Base::Result<Base::Ref<
            Data::Binding>> binding =
                Base::MakeRef<
                    Data::Binding>();
        if (!binding) {
            return binding.GetStatus();
        }
        Base::Result<void> configured =
            binding.Value()->Configure(
                path,
                elementName,
                mode,
                updateSourceTrigger,
                stringFormat,
                relativeSource == RelativeSourceKind::Self
                    ? Data::RelativeSourceMode::Self
                    : relativeSource == RelativeSourceKind::TemplatedParent
                        ? Data::RelativeSourceMode::TemplatedParent
                        : relativeSource == RelativeSourceKind::Ancestor
                            ? Data::RelativeSourceMode::Ancestor
                            : Data::RelativeSourceMode::None,
                ancestorType);
        if (!configured) {
            return configured.GetStatus();
        }
        if (authoredLaunchPath) {
            Base::Result<void> assigned =
                static_cast<Media::Animation::LaunchUriOrFileAction*>(
                    services.targetObject)->SetPathBinding(
                        std::move(binding).Value());
            return assigned
                ? Base::Result<ProvidedValue>(ProvidedValue::Handled())
                : Base::Result<ProvidedValue>(assigned.GetStatus());
        }
        Base::Result<Core::Value> value =
            Core::Value::FromObject(
                authoredHierarchicalItemsSource
                    ? targetMember->ValueType()
                    : Data::Binding::StaticTypeId(),
                Base::Ref<Base::Object>(
                    std::move(binding).Value()));
        if (!value) return value.GetStatus();
        return ProvidedValue::FromValue(
            std::move(value).Value());
    }

    Base::Result<Core::DependencyObject*> targetResult =
        Detail::SchemaAccess::ResolvePropertyTarget(
            *services.schema,
            *services.targetObject);
    if (!targetResult) {
        return targetResult.GetStatus();
    }
    Core::DependencyObject* target = targetResult.Value();

    const Core::DependencyPropertyHandle targetHandle{
        services.targetMember};
    const Core::DependencyProperty* targetProperty =
        target->PropertyRegistry().Find(targetHandle);
    if (targetProperty == nullptr ||
        Detail::SchemaAccess::Runtime(
            *services.schema) == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Binding target property or metadata runtime was not found");
    }

    if (relativeSource ==
            RelativeSourceKind::TemplatedParent &&
        services.deferredContentOwner != nullptr &&
        services.deferredContentOwner->RuntimeType() ==
            Controls::ControlTemplate::StaticTypeId()) {
        auto& controlTemplate =
            static_cast<Controls::ControlTemplate&>(
                *services.deferredContentOwner);
        Base::String targetName;
        Base::StringView authoredName =
            services.nameScope->NameOf(
                *services.targetObject);
        if (authoredName.Empty()) {
            Base::Result<Base::String> generated =
                controlTemplate.EnsureAuthoredName(
                    *services.targetObject);
            if (!generated) {
                return generated.GetStatus();
            }
            targetName =
                std::move(generated).Value();
            authoredName = targetName.View();
        }
        Base::Result<void> added =
            controlTemplate.TryAddTemplatedParentBinding(
                authoredName,
                path,
                stringFormat,
                targetHandle,
                mode,
                updateSourceTrigger);
        return added
            ? Base::Result<ProvidedValue>(
                  ProvidedValue::Handled())
            : Base::Result<ProvidedValue>(
                  added.GetStatus());
    }

    Base::Object* source = nullptr;
    if (!sourceResource.Empty()) {
        if (!services.resources.IsAvailable()) {
            return Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "Binding Source requires an active resource scope");
        }
        Base::Result<Aero::ResourceValue> resource =
            services.resources.Lookup(sourceResource);
        if (!resource) return resource.GetStatus();
        if (resource.Value().Kind() != Core::ValueKind::Object ||
            resource.Value().IsNullObject() ||
            !resource.Value().AsObject()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Binding Source StaticResource must be an object");
        }
        source = resource.Value().AsObject().Get();
    } else if (!elementName.Empty()) {
        source = services.nameScope->Find(elementName);
        if (source == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Binding ElementName was not found in the active NameScope");
        }
    } else if (relativeSource == RelativeSourceKind::Self) {
        source = services.targetObject;
    } else if (relativeSource ==
               RelativeSourceKind::TemplatedParent) {
        source = services.templatedParent;
        if (source == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Binding TemplatedParent is unavailable");
        }
    } else {
        if (!extension->options_.dataContextProperty.IsValid()) {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Binding without ElementName requires a DataContext property");
        }
    }

    Aero::Detail::BindingManager* bindings =
        services.bindings != nullptr
        ? services.bindings
        : extension->options_.bindings;
    if (bindings == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Binding requires a load-scoped BindingManager");
    }

    if (services.deferredContentOwner != nullptr &&
        services.deferredContent != nullptr) {
        Base::Result<void> staged =
            services.deferredContent->StageBinding(
                *services.deferredContentOwner,
                source,
                *target,
                *bindings,
                *Detail::SchemaAccess::Runtime(
                    *services.schema),
                targetHandle,
                extension->options_.dataContextProperty,
                path,
                stringFormat,
                mode,
                updateSourceTrigger,
                path.Empty());
        return staged
            ? Base::Result<ProvidedValue>(
                  ProvidedValue::Handled())
            : Base::Result<ProvidedValue>(
                  staged.GetStatus());
    }

    Base::IAllocator& allocator = Base::GetDefaultAllocator();
    void* memory = allocator.Allocate({
        sizeof(DeferredBindingState),
        alignof(DeferredBindingState),
        Base::MemoryTag::Markup});
    if (memory == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Deferred Binding allocation failed");
    }
    auto* state = new (memory) DeferredBindingState();
    state->manager = bindings;
    state->metadata = Detail::SchemaAccess::Runtime(
        *services.schema);
    state->source = source;
    state->target = target;
    state->targetProperty = targetHandle;
    state->dataContextProperty = extension->options_.dataContextProperty;
    state->mode = mode;
    state->bindsToSource = path.Empty();
    state->updateSourceTrigger = updateSourceTrigger;
    state->allocator = &allocator;
    Base::Result<void> assigned = state->path.TryAssign(path);
    if (!assigned) {
        CleanupBinding(state);
        return assigned.GetStatus();
    }
    assigned = state->stringFormat.TryAssign(
        stringFormat);
    if (!assigned) {
        CleanupBinding(state);
        return assigned.GetStatus();
    }
    return ProvidedValue::Deferred(
        state, &CommitBinding, &RollbackBinding, &CleanupBinding);
}

} // namespace Aero::Markup
