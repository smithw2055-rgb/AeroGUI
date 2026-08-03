#include "gui/MetadataInternal.hpp"
#include "markup/MarkupWriterInternal.hpp"
// Consolidated implementation. Keep sections ordered by dependency.

// ===== BindingExtension =====


#include "gui/BindingInternal.hpp"

// Binding markup-extension implementation.



#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include "../controls/TemplateInternals.hpp"

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
    Meta::UpdateSourceTrigger& updateSourceTrigger) noexcept {
    elementName = {};
    sourceResource = {};
    path = {};
    stringFormat = {};
    fallbackValue = {};
    ancestorType = {};
    relativeSource = RelativeSourceKind::None;
    mode = Data::BindingMode::OneWay;
    updateSourceTrigger = Meta::UpdateSourceTrigger::PropertyChanged;

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
                updateSourceTrigger = Meta::UpdateSourceTrigger::PropertyChanged;
            } else if (value == ExplicitTrigger) {
                updateSourceTrigger = Meta::UpdateSourceTrigger::Explicit;
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

struct DeferredBindingState {
    Aero::Internal::BindingEngine* manager = nullptr;
    ::Aero::Meta::Registry* metadata = nullptr;
    Base::Object* source = nullptr;
    ::Aero::DependencyObject* target = nullptr;
    Meta::DependencyPropertyHandle targetProperty;
    Meta::DependencyPropertyHandle dataContextProperty;
    Base::String path;
    Base::String stringFormat;
    bool bindsToSource = false;
    Data::BindingMode mode = Data::BindingMode::OneWay;
    Meta::UpdateSourceTrigger updateSourceTrigger =
        Meta::UpdateSourceTrigger::PropertyChanged;
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
    Meta::TypeId bindingExtensionType) noexcept {
    return Detail::SchemaPrivate::AddMarkupExtension(schema, {
        bindingExtensionType,
        &BindingExtension::ProvideValue,
        this});
}

Base::Result<ProvidedValue> BindingExtension::ProvideValue(
    Base::StringView arguments,
    const ExtensionServices& services,
    void* context) noexcept {
    BindingExtension* extension =
        static_cast<BindingExtension*>(context);
    if (extension == nullptr ||
        services.schema == nullptr || services.targetObject == nullptr ||
        services.nameScope == nullptr ||
        services.targetMember == Meta::InvalidMemberId) {
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
    Meta::UpdateSourceTrigger updateSourceTrigger =
        Meta::UpdateSourceTrigger::PropertyChanged;
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

    ::Aero::Meta::Registry* metadata =
        Detail::SchemaPrivate::Metadata(
            *services.schema);
    const Meta::PropertyInfo* targetMember =
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
        binding.Value()->SetPath(path);
        binding.Value()->SetElementName(elementName);
        binding.Value()->SetStringFormat(stringFormat);
        binding.Value()->SetMode(mode);
        binding.Value()->SetUpdateSourceTrigger(updateSourceTrigger);
        if (relativeSource != RelativeSourceKind::None) {
            const Data::RelativeSourceMode sourceMode =
                relativeSource == RelativeSourceKind::Self
                    ? Data::RelativeSourceMode::Self
                    : relativeSource == RelativeSourceKind::TemplatedParent
                        ? Data::RelativeSourceMode::TemplatedParent
                        : Data::RelativeSourceMode::FindAncestor;
            Base::Result<Base::Ref<Data::RelativeSource>> source =
                Base::MakeRef<Data::RelativeSource>(sourceMode);
            if (!source) return source.GetStatus();
            if (sourceMode == Data::RelativeSourceMode::FindAncestor) {
                source.Value()->SetAncestorType(ancestorType);
            }
            binding.Value()->SetRelativeSource(std::move(source).Value());
        }
        if (authoredLaunchPath) {
            static_cast<Media::Animation::LaunchUriOrFileAction*>(
                services.targetObject)->SetPathBinding(
                    std::move(binding).Value());
            return ProvidedValue::Handled();
        }
        Base::Result<Meta::Value> value =
            Meta::Value::FromObject(
                authoredHierarchicalItemsSource
                    ? targetMember->ValueType()
                    : Data::Binding::StaticTypeId(),
                Base::Ref<Base::Object>(
                    std::move(binding).Value()));
        if (!value) return value.GetStatus();
        return ProvidedValue::FromValue(
            std::move(value).Value());
    }

    Base::Result<::Aero::DependencyObject*> targetResult =
        Detail::SchemaPrivate::ResolvePropertyTarget(
            *services.schema,
            *services.targetObject);
    if (!targetResult) {
        return targetResult.GetStatus();
    }
    ::Aero::DependencyObject* target = targetResult.Value();

    const Meta::DependencyPropertyHandle targetHandle{
        services.targetMember};
    const Meta::DependencyProperty* targetProperty =
        target->PropertyRegistry().Find(targetHandle);
    if (targetProperty == nullptr ||
        Detail::SchemaPrivate::Metadata(
            *services.schema) == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Binding target property or metadata program was not found");
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
                Internal::TemplatePrivate::EnsureAuthoredName(controlTemplate,
                    *services.targetObject);
            if (!generated) {
                return generated.GetStatus();
            }
            targetName =
                std::move(generated).Value();
            authoredName = targetName.View();
        }
        Base::Result<void> added =
            Internal::TemplatePrivate::AddTemplatedParentBinding(controlTemplate,
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
        if (resource.Value().Kind() != Meta::ValueKind::Object ||
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

    Aero::Internal::BindingEngine* bindings =
        services.bindings != nullptr
        ? services.bindings
        : extension->options_.bindings;
    if (bindings == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Binding requires a load-scoped BindingEngine");
    }

    if (services.deferredContentOwner != nullptr &&
        services.deferredContent != nullptr) {
        Base::Result<void> staged =
            services.deferredContent->StageBinding(
                *services.deferredContentOwner,
                source,
                *target,
                *bindings,
                *Detail::SchemaPrivate::Metadata(
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
    state->metadata = Detail::SchemaPrivate::Metadata(
        *services.schema);
    state->source = source;
    state->target = target;
    state->targetProperty = targetHandle;
    state->dataContextProperty = extension->options_.dataContextProperty;
    state->mode = mode;
    state->bindsToSource = path.Empty();
    state->updateSourceTrigger = updateSourceTrigger;
    state->allocator = &allocator;
    Base::Result<void> assigned = state->path.Assign(path);
    if (!assigned) {
        CleanupBinding(state);
        return assigned.GetStatus();
    }
    assigned = state->stringFormat.Assign(
        stringFormat);
    if (!assigned) {
        CleanupBinding(state);
        return assigned.GetStatus();
    }
    return ProvidedValue::Deferred(
        state, &CommitBinding, &RollbackBinding, &CleanupBinding);
}

} // namespace Aero::Markup


// ===== DynamicResourceExtension =====




// Dynamic-resource markup-extension implementation.


#include <Aero/Styling.hpp>

#include <new>
#include <utility>

namespace Aero::Markup {
using Aero::ResourceChangeSubscription;
using Aero::ResourceDictionary;

namespace {

struct DynamicResourceState {
    DynamicResourceState(
        Meta::EffectiveValueEngine& effectiveValues,
        ::Aero::DependencyObject& dependencyObject,
        Meta::DependencyPropertyHandle dependencyProperty) noexcept
        : engine(&effectiveValues),
          target(&dependencyObject),
          property(dependencyProperty),
          key(),
          sources(),
          allocator(&Base::GetDefaultAllocator()) {}

    struct Source {
        ResourceDictionary* resources = nullptr;
        ResourceChangeSubscription subscription;
    };

    Meta::EffectiveValueEngine* engine = nullptr;
    ::Aero::DependencyObject* target = nullptr;
    Meta::DependencyPropertyHandle property;
    Base::String key;
    Base::Vector<Source> sources;
    Base::IAllocator* allocator = nullptr;
};

Base::StringView TrimDynamicResourceText(Base::StringView value) noexcept {
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

Base::Result<Meta::PropertyValue> EvaluateDynamicResource(
    void* context,
    ::Aero::DependencyObject& object,
    Meta::DependencyPropertyHandle property) noexcept {
    DynamicResourceState* state = static_cast<DynamicResourceState*>(context);
    if (state == nullptr || state->sources.Empty() ||
        state->target != &object || state->property != property) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "DynamicResource expression state is invalid");
    }
    for (const DynamicResourceState::Source& source :
         state->sources) {
        if (source.resources == nullptr) {
            continue;
        }
        Base::Result<Aero::ResourceValue> resource =
            source.resources->Lookup(state->key.View());
        if (resource) {
            return resource.Value();
        }
        if (resource.GetStatus().code !=
            Base::ErrorCode::NotFound) {
            return resource.GetStatus();
        }
    }
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        "DynamicResource key is not available in the active resource chain");
}

void ResourceChanged(
    void* context,
    Base::StringView key,
    Aero::ResourceChangeKind,
    std::uint64_t) noexcept {
    DynamicResourceState* state = static_cast<DynamicResourceState*>(context);
    if (state == nullptr || state->engine == nullptr || state->target == nullptr) {
        return;
    }
    if (!key.Empty() && key != state->key.View()) {
        return;
    }
    static_cast<void>(state->engine->Invalidate(*state->target, state->property));
}

void CleanupDynamicResource(void* context) noexcept {
    DynamicResourceState* state = static_cast<DynamicResourceState*>(context);
    if (state == nullptr) {
        return;
    }
    Base::IAllocator* allocator = state->allocator;
    for (DynamicResourceState::Source& source :
         state->sources) {
        if (source.resources != nullptr) {
            static_cast<void>(
                source.resources->Unsubscribe(
                    source.subscription));
        }
    }
    state->~DynamicResourceState();
    allocator->Deallocate(
        state,
        sizeof(DynamicResourceState),
        alignof(DynamicResourceState),
        Base::MemoryTag::Markup);
}


struct DeferredDynamicResourceState {
    Meta::EffectiveValueEngine* engine = nullptr;
    ::Aero::DependencyObject* target = nullptr;
    Meta::DependencyPropertyHandle property;
    Base::Vector<const ResourceDictionary*> resources;
    ResourceDictionary* fallbackResources = nullptr;
    Base::String key;
    Base::IAllocator* allocator = nullptr;
};

Base::Result<std::uint64_t> CommitDynamicResource(void* context) noexcept {
    auto* state = static_cast<DeferredDynamicResourceState*>(context);
    if (state == nullptr || state->engine == nullptr ||
        state->target == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Deferred DynamicResource state is invalid");
    }
    Base::Result<void> attached = DynamicResource::Attach(
        *state->engine,
        {state->resources.Data(), state->resources.Size()},
        state->fallbackResources,
        *state->target, state->property, state->key.View());
    return attached
        ? Base::Result<std::uint64_t>(1U)
        : Base::Result<std::uint64_t>(attached.GetStatus());
}

void RollbackDynamicResource(
    void* context, std::uint64_t token) noexcept {
    auto* state = static_cast<DeferredDynamicResourceState*>(context);
    if (state != nullptr && token != 0U && state->engine != nullptr &&
        state->target != nullptr) {
        static_cast<void>(state->engine->ClearLocalExpression(
            *state->target, state->property));
    }
}

void CleanupDeferredDynamicResource(void* context) noexcept {
    auto* state = static_cast<DeferredDynamicResourceState*>(context);
    if (state == nullptr) return;
    Base::IAllocator* allocator = state->allocator;
    state->~DeferredDynamicResourceState();
    allocator->Deallocate(
        state, sizeof(DeferredDynamicResourceState),
        alignof(DeferredDynamicResourceState), Base::MemoryTag::Markup);
}

} // namespace

Base::Result<void> DynamicResource::Attach(
    Meta::EffectiveValueEngine& effectiveValues,
    ResourceDictionary& resources,
    ::Aero::DependencyObject& target,
    Meta::DependencyPropertyHandle property,
    Base::StringView key) noexcept {
    const ResourceDictionary* chain[] = {&resources};
    return Attach(
        effectiveValues,
        {chain, 1U},
        nullptr,
        target,
        property,
        key);
}

Base::Result<Meta::PropertyExpression> DynamicResource::CreateExpression(
    Meta::EffectiveValueEngine& effectiveValues,
    Base::Span<const ResourceDictionary* const> resourceChain,
    ResourceDictionary* fallbackResources,
    ::Aero::DependencyObject& target,
    Meta::DependencyPropertyHandle property,
    Base::StringView key) noexcept {
    const Base::StringView normalizedKey = TrimDynamicResourceText(key);
    if (!property.IsValid() || normalizedKey.Empty() ||
        (resourceChain.Empty() &&
         fallbackResources == nullptr)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "DynamicResource requires resources, a target property, and a non-empty key");
    }
    Base::Result<Aero::ResourceValue> existing =
        Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "DynamicResource key is not available");
    for (const ResourceDictionary* resources :
         resourceChain) {
        if (resources == nullptr) continue;
        existing = resources->Lookup(normalizedKey);
        if (existing ||
            existing.GetStatus().code !=
                Base::ErrorCode::NotFound) {
            break;
        }
    }
    if (!existing && fallbackResources != nullptr) {
        existing = fallbackResources->Lookup(normalizedKey);
    }
    if (!existing) return existing.GetStatus();

    Base::IAllocator* stateAllocator = &Base::GetDefaultAllocator();
    void* memory = stateAllocator->Allocate({
        sizeof(DynamicResourceState),
        alignof(DynamicResourceState),
        Base::MemoryTag::Markup});
    if (memory == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "DynamicResource expression allocation failed");
    }
    DynamicResourceState* state = new (memory) DynamicResourceState(
        effectiveValues, target, property);
    Base::Result<void> assigned = state->key.Assign(normalizedKey);
    if (!assigned) {
        state->~DynamicResourceState();
        stateAllocator->Deallocate(
            memory, sizeof(DynamicResourceState), alignof(DynamicResourceState),
            Base::MemoryTag::Markup);
        return assigned.GetStatus();
    }
    auto subscribe =
        [state](ResourceDictionary* resources) noexcept
            -> Base::Result<void> {
        if (resources == nullptr) return {};
        for (const DynamicResourceState::Source& source :
             state->sources) {
            if (source.resources == resources) return {};
        }
        Base::Result<ResourceChangeSubscription> subscription =
            resources->SubscribeChanged(
                &ResourceChanged, state);
        if (!subscription) {
            return subscription.GetStatus();
        }
        Base::Result<void> added =
            state->sources.PushBack({
                resources, subscription.Value()});
        if (!added) {
            static_cast<void>(
                resources->Unsubscribe(
                    subscription.Value()));
            return added.GetStatus();
        }
        return {};
    };
    for (const ResourceDictionary* resources :
         resourceChain) {
        assigned = subscribe(
            const_cast<ResourceDictionary*>(resources));
        if (!assigned) {
            CleanupDynamicResource(state);
            return assigned.GetStatus();
        }
    }
    assigned = subscribe(fallbackResources);
    if (!assigned) {
        CleanupDynamicResource(state);
        return assigned.GetStatus();
    }

    return Meta::PropertyExpression{
        state,
        &EvaluateDynamicResource,
        &CleanupDynamicResource,
        Meta::PropertyExpressionKind::DynamicResource};
}

Base::Result<void> DynamicResource::Attach(
    Meta::EffectiveValueEngine& effectiveValues,
    Base::Span<const ResourceDictionary* const> resourceChain,
    ResourceDictionary* fallbackResources,
    ::Aero::DependencyObject& target,
    Meta::DependencyPropertyHandle property,
    Base::StringView key) noexcept {
    Base::Result<Meta::PropertyExpression> expression = CreateExpression(
        effectiveValues,
        resourceChain,
        fallbackResources,
        target,
        property,
        key);
    if (!expression) return expression.GetStatus();
    Base::Result<void> installed = effectiveValues.SetLocalExpression(
        target, property, expression.Value());
    if (!installed && expression.Value().cleanup != nullptr) {
        expression.Value().cleanup(expression.Value().context);
    }
    return installed;
}

DynamicResourceExtension::DynamicResourceExtension(
    const DynamicResourceExtensionOptions& options) noexcept
    : options_(options) {}

Base::Result<void> DynamicResourceExtension::Register(
    Schema& schema,
    Meta::TypeId dynamicResourceExtensionType) noexcept {
    return Detail::SchemaPrivate::AddMarkupExtension(schema, {
        dynamicResourceExtensionType,
        &DynamicResourceExtension::ProvideValue,
        this});
}

Base::Result<ProvidedValue> DynamicResourceExtension::ProvideValue(
    Base::StringView arguments,
    const ExtensionServices& services,
    void* context) noexcept {
    DynamicResourceExtension* extension =
        static_cast<DynamicResourceExtension*>(context);
    if (extension == nullptr ||
        services.schema == nullptr || services.targetObject == nullptr ||
        services.targetMember == Meta::InvalidMemberId) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "DynamicResource markup extension has no target service context");
    }
    const Base::StringView key = TrimDynamicResourceText(arguments);
    if (key.Empty() || key.Data() == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "DynamicResource requires a resource key");
    }
    // A Setter is an authored style record rather than a DependencyObject.
    // Resolve its current resource value while the style dictionary is being
    // built; the style finalizer subsequently converts that value for the
    // target dependency property.
    if (services.targetObject->RuntimeType() ==
        Aero::Setter::StaticTypeId()) {
        // Template/style setters are authored before their eventual target
        // exists. Resolve from the complete parse-time resource scope, not
        // only the immediate fallback dictionary: a theme's brushes commonly
        // live in an earlier merged sibling dictionary.
        for (const ResourceDictionary* resources :
             services.ambientResourceChain) {
            if (resources == nullptr) continue;
            Base::Result<Aero::ResourceValue> resource =
                resources->Lookup(key);
            if (resource) {
                return ProvidedValue::FromValue(
                    std::move(resource).Value());
            }
            if (resource.GetStatus().code !=
                Base::ErrorCode::NotFound) {
                return resource.GetStatus();
            }
        }
        if (services.fallbackResources != nullptr) {
            Base::Result<Aero::ResourceValue> resource =
                services.fallbackResources->Lookup(key);
            if (resource) {
                return ProvidedValue::FromValue(
                    std::move(resource).Value());
            }
            if (resource.GetStatus().code !=
                Base::ErrorCode::NotFound) {
                return resource.GetStatus();
            }
        }
        // WPF does not fail dictionary construction for a DynamicResource
        // whose key is currently absent. Preserve an unset object value until
        // the eventual style-instance resource expression can evaluate it.
        return ProvidedValue::FromValue(
            Meta::Value::NullObject(
                Meta::TypeOf<Base::Object>()));
    }
    Base::Result<::Aero::DependencyObject*> targetResult =
        Detail::SchemaPrivate::ResolvePropertyTarget(
            *services.schema,
            *services.targetObject);
    if (!targetResult) {
        return targetResult.GetStatus();
    }
    ::Aero::DependencyObject* target = targetResult.Value();
    const Meta::DependencyPropertyHandle property{services.targetMember};
    Meta::EffectiveValueEngine* effectiveValues =
        services.effectiveValues != nullptr
        ? services.effectiveValues
        : extension->options_.effectiveValues;
    ResourceDictionary* fallbackResources =
        services.fallbackResources != nullptr
        ? services.fallbackResources
        : extension->options_.resources;
    if (effectiveValues == nullptr || fallbackResources == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "DynamicResource requires load-scoped effective-value and resource services");
    }
    Base::IAllocator& allocator = Base::GetDefaultAllocator();
    void* memory = allocator.Allocate({
        sizeof(DeferredDynamicResourceState),
        alignof(DeferredDynamicResourceState),
        Base::MemoryTag::Markup});
    if (memory == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Deferred DynamicResource allocation failed");
    }
    auto* state = new (memory) DeferredDynamicResourceState();
    state->engine = effectiveValues;
    state->target = target;
    state->property = property;
    state->fallbackResources = fallbackResources;
    state->allocator = &allocator;
    Base::Result<void> reserved = state->resources.Reserve(
        services.ambientResourceChain.Size());
    if (reserved) {
        for (const ResourceDictionary* resource :
             services.ambientResourceChain) {
            reserved = state->resources.PushBack(resource);
            if (!reserved) break;
        }
    }
    if (reserved) reserved = state->key.Assign(key);
    if (!reserved) {
        CleanupDeferredDynamicResource(state);
        return reserved.GetStatus();
    }
    return ProvidedValue::Deferred(
        state,
        &CommitDynamicResource,
        &RollbackDynamicResource,
        &CleanupDeferredDynamicResource);
}

} // namespace Aero::Markup


// ===== TemplateBindingExtension =====





#include "../controls/TemplateInternals.hpp"

#include <Aero/Styling.hpp>
#include <Aero/Value.hpp>


namespace Aero::Markup {
namespace {

Base::StringView PropertyLocalName(
    Base::StringView value) noexcept {
    value = ::Aero::Base::Detail::ValueConversion::Trim(value);
    std::uint32_t separator = UINT32_MAX;
    for (std::uint32_t index = 0U;
         index < value.SizeBytes();
         ++index) {
        if (value[index] == '.') {
            separator = index;
        }
    }
    return separator == UINT32_MAX
        ? value
        : value.Substr(
              separator + 1U,
              value.SizeBytes() - separator - 1U);
}

const char* MissingPropertyMessage(
    Base::StringView propertyName,
    bool source) noexcept {
    if (propertyName == Base::StringView("Content")) {
        return source
            ? "TemplateBinding source property Content was not found"
            : "TemplateBinding target property for Content was not found";
    }
    if (propertyName == Base::StringView("ContentTemplate")) {
        return source
            ? "TemplateBinding source property ContentTemplate was not found"
            : "TemplateBinding target property for ContentTemplate was not found";
    }
    if (propertyName == Base::StringView("ContentTemplateSelector")) {
        return source
            ? "TemplateBinding source property ContentTemplateSelector was not found"
            : "TemplateBinding target property for ContentTemplateSelector was not found";
    }
    if (propertyName == Base::StringView("CanContentScroll")) {
        return source
            ? "TemplateBinding source property CanContentScroll was not found"
            : "TemplateBinding target property for CanContentScroll was not found";
    }
    if (propertyName == Base::StringView("Padding")) {
        return source
            ? "TemplateBinding source property Padding was not found"
            : "TemplateBinding target property for Padding was not found";
    }
    return source
        ? "TemplateBinding source property was not found"
        : "TemplateBinding target property was not found";
}

} // namespace

Base::Result<void> TemplateBindingExtension::Register(
    Schema& schema,
    Meta::TypeId markupExtensionType) noexcept {
    if (schema.IsFrozen() ||
        markupExtensionType == Meta::InvalidTypeId) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "TemplateBinding extension registration is invalid");
    }
    return Detail::SchemaPrivate::AddMarkupExtension(
        schema,
        {markupExtensionType, &ProvideValue, nullptr});
}

Base::Result<ProvidedValue>
TemplateBindingExtension::ProvideValue(
    Base::StringView arguments,
    const ExtensionServices& services,
    void* context) noexcept {
    if (context != nullptr ||
        services.schema == nullptr ||
        services.targetObject == nullptr ||
        services.deferredContentOwner == nullptr ||
        services.nameScope == nullptr ||
        services.targetMember ==
            Meta::InvalidMemberId ||
        services.deferredContentOwner->RuntimeType() !=
            Controls::ControlTemplate::StaticTypeId()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "TemplateBinding requires an active ControlTemplate target");
    }

    const Base::StringView propertyName =
        PropertyLocalName(arguments);
    if (propertyName.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "TemplateBinding requires a source property");
    }

    auto& controlTemplate =
        static_cast<Controls::ControlTemplate&>(
            *services.deferredContentOwner);
    Base::Result<::Aero::DependencyObject*> target =
        Detail::SchemaPrivate::ResolvePropertyTarget(
            *services.schema,
            *services.targetObject);
    if (!target) return target.GetStatus();

    const Meta::DependencyProperty* source =
        target.Value()->PropertyRegistry().Find(
            controlTemplate.GetTargetType(),
            propertyName);
    const Meta::DependencyProperty* destination =
        target.Value()->PropertyRegistry().Find(
            Meta::DependencyPropertyHandle{
                services.targetMember});
    if (source == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            MissingPropertyMessage(propertyName, true));
    }
    if (destination == nullptr ||
        destination->MetadataFor(
            target.Value()->RuntimeType()) == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            MissingPropertyMessage(propertyName, false));
    }
    Base::String targetName;
    Base::StringView authoredName =
        services.nameScope->NameOf(
            *services.targetObject);
    if (authoredName.Empty()) {
        Base::Result<Base::String> generated =
            Internal::TemplatePrivate::EnsureAuthoredName(controlTemplate,
                *services.targetObject);
        if (!generated) {
            return generated.GetStatus();
        }
        targetName =
            std::move(generated).Value();
        authoredName = targetName.View();
    }
    Base::Result<void> added =
        Internal::TemplatePrivate::AddTemplateBinding(controlTemplate,
            authoredName,
            source->Handle(),
            destination->Handle());
    return added
        ? Base::Result<ProvidedValue>(
              ProvidedValue::Handled())
        : Base::Result<ProvidedValue>(
              added.GetStatus());
}

} // namespace Aero::Markup


// ===== TypeExtension =====



#include <utility>



namespace Aero::Markup {
Base::Result<void> TypeExtension::Register(
    Schema& schema,
    Meta::TypeId markupExtensionType) noexcept {
    if (schema.IsFrozen() ||
        markupExtensionType == Meta::InvalidTypeId) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "x:Type extension registration is invalid");
    }
    const Meta::TypeInfo* token =
        schema.Types().FindType(
            Meta::TypeOf<Meta::TypeReference>());
    if (token == nullptr ||
        (static_cast<std::uint32_t>(token->Flags()) &
            static_cast<std::uint32_t>(Meta::TypeFlags::ValueType)) == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "x:Type reference token must be a value type");
    }
    return Detail::SchemaPrivate::AddMarkupExtension(schema, {
        markupExtensionType, &ProvideValue, nullptr});
}

Base::Result<ProvidedValue> TypeExtension::ProvideValue(
    Base::StringView arguments,
    const ExtensionServices& services,
    void* context) noexcept {
    if (context != nullptr || services.schema == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "x:Type extension context is invalid");
    }
    Base::Result<Meta::Value> value =
        Detail::SchemaPrivate::ConvertText(
            *services.schema,
            Meta::TypeOf<Meta::TypeReference>(),
            arguments,
            &services);
    return value
        ? ProvidedValue::FromValue(
              std::move(value).Value())
        : Base::Result<ProvidedValue>(
              value.GetStatus());
}

} // namespace Aero::Markup


// ===== StaticExtension =====






namespace Aero::Markup {
namespace {

Base::StringView TrimStaticText(Base::StringView value) noexcept {
    std::uint32_t begin = 0U;
    std::uint32_t end = value.SizeBytes();
    while (begin < end &&
           (value[begin] == ' ' || value[begin] == '\t' ||
            value[begin] == '\r' || value[begin] == '\n')) {
        ++begin;
    }
    while (end > begin &&
           (value[end - 1U] == ' ' || value[end - 1U] == '\t' ||
            value[end - 1U] == '\r' || value[end - 1U] == '\n')) {
        --end;
    }
    return value.Substr(begin, end - begin);
}

} // namespace

Base::Result<void> StaticExtension::Register(
    Schema& schema,
    Meta::TypeId markupExtensionType) noexcept {
    if (schema.IsFrozen() ||
        markupExtensionType == Meta::InvalidTypeId) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "x:Static extension registration is invalid");
    }
    return Detail::SchemaPrivate::AddMarkupExtension(schema, {
        markupExtensionType, &ProvideValue, nullptr});
}

Base::Result<ProvidedValue> StaticExtension::ProvideValue(
    Base::StringView arguments,
    const ExtensionServices& services,
    void* context) noexcept {
    if (context != nullptr ||
        services.schema == nullptr ||
        !services.namespaces.IsAvailable()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "x:Static extension context is invalid");
    }

    const Base::StringView expression = TrimStaticText(arguments);
    std::uint32_t memberSeparator = expression.SizeBytes();
    for (std::uint32_t index = 0U;
         index < expression.SizeBytes(); ++index) {
        if (expression[index] == '.') memberSeparator = index;
    }
    if (memberSeparator == 0U ||
        memberSeparator + 1U >= expression.SizeBytes()) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "x:Static requires Type.Member");
    }

    const Base::StringView qualifiedType =
        expression.Substr(0U, memberSeparator);
    const Base::StringView memberName =
        expression.Substr(
            memberSeparator + 1U,
            expression.SizeBytes() - memberSeparator - 1U);
    std::uint32_t prefixSeparator = qualifiedType.SizeBytes();
    for (std::uint32_t index = 0U;
         index < qualifiedType.SizeBytes(); ++index) {
        if (qualifiedType[index] == ':') {
            if (prefixSeparator != qualifiedType.SizeBytes()) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "x:Static type has multiple namespace separators");
            }
            prefixSeparator = index;
        }
    }
    Base::StringView prefix;
    Base::StringView typeName = qualifiedType;
    if (prefixSeparator != qualifiedType.SizeBytes()) {
        if (prefixSeparator == 0U ||
            prefixSeparator + 1U >= qualifiedType.SizeBytes()) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "x:Static type name is invalid");
        }
        prefix = qualifiedType.Substr(0U, prefixSeparator);
        typeName = qualifiedType.Substr(
            prefixSeparator + 1U,
            qualifiedType.SizeBytes() - prefixSeparator - 1U);
    }

    Base::Result<Base::StringView> xamlNamespace =
        services.namespaces.Lookup(prefix);
    if (!xamlNamespace) return xamlNamespace.GetStatus();
    Base::Result<const Meta::TypeInfo*> type =
        Detail::SchemaPrivate::ResolveType(
            *services.schema,
            xamlNamespace.Value(),
            typeName);
    if (!type) return type.GetStatus();
    if (type.Value()->Kind() != Meta::MetadataTypeKind::Enum) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "x:Static currently supports registered enum members");
    }
    const Meta::EnumValueInfo* value =
        services.schema->Types().FindEnumValue(
            type.Value()->Id(), memberName);
    if (value == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "x:Static enum member was not found");
    }

    const bool signedEnum =
        (static_cast<std::uint32_t>(type.Value()->Flags()) &
         static_cast<std::uint32_t>(Meta::TypeFlags::SignedEnum)) != 0U;
    Meta::Value result = signedEnum
        ? Meta::Value::FromSignedInteger(
              type.Value()->Id(),
              static_cast<std::int64_t>(value->RawValue()))
        : Meta::Value::FromUnsignedInteger(
              type.Value()->Id(),
              value->RawValue());
    return ProvidedValue::FromValue(std::move(result));
}

} // namespace Aero::Markup


// ===== Scopes =====

#include <Aero/Markup.hpp>

// Name and resource scope implementation.

namespace Aero::Markup {
namespace {

constexpr const char* MessageNamespaceUnavailable =
    "XAML namespace scope is not available";
constexpr const char* MessageResourceResolverUnavailable =
    "XAML resource resolver is not available";

} // namespace

Base::Result<Base::StringView> NamespaceScope::Lookup(
    Base::StringView prefix) const noexcept {
    if (lookup_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            MessageNamespaceUnavailable);
    }
    return lookup_(context_, prefix);
}

Base::Result<Aero::ResourceValue> ResourceResolver::Lookup(
    Base::StringView key) const noexcept {
    if (lookup_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            MessageResourceResolverUnavailable);
    }
    return lookup_(context_, key);
}

} // namespace Aero::Markup


// ===== ObjectBuilder =====




#include "markup/MarkupInternal.hpp"
#include <Aero/Media/Brushes.hpp>

#include <utility>

namespace Aero::Markup {

struct NamespaceScope::Impl {
public:
    static ::Aero::Markup::NamespaceScope CreateNamespaceScope(
        ::Aero::Markup::NamespaceScope::LookupCallback lookup,
        void* context) noexcept {
        return ::Aero::Markup::NamespaceScope(lookup, context);
    }

    static ::Aero::Markup::ResourceResolver CreateResourceResolver(
        ::Aero::Markup::ResourceResolver::LookupCallback lookup,
        void* context) noexcept {
        return ::Aero::Markup::ResourceResolver(lookup, context);
    }
};

} // namespace Aero::Markup

// Source-only bridge for the historical Markup::Detail spelling. The
// installed API only friends the opaque NamespaceScope::Impl seam.
namespace Aero::Markup::Detail {
using ResourcePrivate = ::Aero::Markup::NamespaceScope::Impl;
}

namespace Aero::Markup {
class NodeCursor {
public:
    virtual ~NodeCursor() = default;
    virtual Base::Result<const Node*> Read(
        Node& scratch) noexcept = 0;
};

namespace {

class StreamingXamlNodeCursor : public NodeCursor {
public:
    explicit StreamingXamlNodeCursor(
        NodeReader& reader) noexcept
        : reader_(&reader) {}

    Base::Result<const Node*> Read(
        Node& scratch) noexcept override {
        Base::Result<NodeKind> read =
            reader_->Read(scratch);
        return read
            ? Base::Result<const Node*>(&scratch)
            : Base::Result<const Node*>(
                  read.GetStatus());
    }

private:
    NodeReader* reader_ = nullptr;
};

class CompiledXamlNodeCursor : public NodeCursor {
public:
    explicit CompiledXamlNodeCursor(
        const CompiledDocument& document) noexcept
        : nodes_(document.Nodes()) {}

    Base::Result<const Node*> Read(
        Node&) noexcept override {
        if (index_ >= nodes_.Size()) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Compiled XAML node stream ended unexpectedly");
        }
        return &nodes_[index_++];
    }

private:
    Base::Span<const Node> nodes_;
    std::uint32_t index_ = 0U;
};

constexpr Base::StringView MessageSchemaNotReady(
    "XAML object writer requires a frozen schema context");
constexpr Base::StringView MessageUnknownType(
    "XAML object element does not resolve to a registered type");
constexpr Base::StringView MessageTypeNotConstructible(
    "XAML object element resolves to a non-constructible type");
constexpr Base::StringView MessageUnknownMember(
    "XAML member does not resolve on the target object type");
constexpr Base::StringView MessageInvalidAttachedMember(
    "XAML qualified member is not valid for this target object");
constexpr Base::StringView MessageUnsupportedMember(
    "XAML member has no supported object-writer adapter");
constexpr Base::StringView MessageInvalidValue(
    "XAML value conversion or member assignment failed");
constexpr Base::StringView MessageInvalidWriterState(
    "XAML node sequence is invalid for the object writer");
constexpr Base::StringView MessageMissingContentProperty(
    "XAML child object requires a registered content property");
constexpr Base::StringView MessageDuplicateMemberValue(
    "XAML member is assigned more than once");
constexpr Base::StringView MessageInitializationFailed(
    "XAML object initialization callback failed");
constexpr Base::StringView MessageUnexpectedText(
    "XAML text is not valid in the current object context");
constexpr Base::StringView MessageTypeMismatch(
    "XAML object or scalar value is not assignable to the target member");
constexpr Base::StringView MessageFactoryFailed(
    "XAML object factory failed");
constexpr Base::StringView MessageMissingMemberValue(
    "XAML member scope does not contain a value");
constexpr Base::StringView MessageMultipleRoots(
    "XAML document contains more than one root object");
constexpr Base::StringView MessageInvalidDirective(
    "XAML language directive is unsupported or used in an invalid context");
constexpr Base::StringView MessageDuplicateName(
    "x:Name is duplicated in the active XAML name scope");
constexpr Base::StringView MessageDuplicateResourceKey(
    "x:Key is duplicated in the active XAML resource scope");
constexpr Base::StringView MessageStaticResourceNotFound(
    "StaticResource key is not available; forward references are not supported");
constexpr Base::StringView MessageMissingResourceScope(
    "x:Key requires an enclosing XAML resource scope");
constexpr Base::StringView MessageNullNotAllowed(
    "x:Null is not valid for this XAML value or document root");
constexpr Base::StringView MessageInvalidMarkupExtension(
    "XAML markup-extension text is malformed or unsupported");
constexpr Base::StringView MessageNamespaceState(
    "XAML namespace declaration state is invalid");
constexpr Base::StringView MessageNameRegistrationFailed(
    "XAML name registration callback failed");
constexpr Base::StringView MessageResourceRegistrationFailed(
    "XAML resource registration callback failed");
constexpr Base::StringView MessageUnknownMarkupExtension(
    "XAML markup-extension type or provider is not registered");
constexpr Base::StringView MessageMarkupExtensionFailed(
    "XAML markup-extension value provider failed");

constexpr Base::StringView XmlPrefix("xml");
constexpr Base::StringView XmlNamespaceUri(
    "http://www.w3.org/XML/1998/namespace");
constexpr Base::StringView DirectiveName("Name");
constexpr Base::StringView DirectiveKey("Key");
constexpr Base::StringView DirectiveClass("Class");
constexpr Base::StringView DirectiveNull("Null");
constexpr Base::StringView NullMarkup("x:Null");
constexpr Base::StringView StaticResourceMarkup("StaticResource");

Base::Status InvalidStateStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        MessageInvalidWriterState.Data());
}

Base::Status SessionConsumedStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "XAML load session is single use");
}

Base::Result<Base::String> StaticResourceNotFoundMessage(
    Base::StringView key) noexcept {
    Base::String message;
    Base::Result<void> appended = message.Assign(
        "StaticResource key '");
    if (appended) appended = message.Append(key);
    if (appended) appended = message.Append(
        "' is not available; forward references are not supported");
    return appended
        ? Base::Result<Base::String>(std::move(message))
        : Base::Result<Base::String>(appended.GetStatus());
}

bool IsAsciiWhitespace(char value) noexcept {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

Base::StringView TrimBuilderText(Base::StringView value) noexcept {
    std::uint32_t begin = 0U;
    while (begin < value.SizeBytes() && IsAsciiWhitespace(value[begin])) {
        ++begin;
    }
    std::uint32_t end = value.SizeBytes();
    while (end > begin && IsAsciiWhitespace(value[end - 1U])) {
        --end;
    }
    return value.Substr(begin, end - begin);
}

bool HasTypeFlag(Meta::TypeFlags value, Meta::TypeFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

bool IsValueType(
    const Meta::TypeRegistry& descriptors,
    Meta::TypeId type) noexcept {
    const Meta::TypeInfo* info = descriptors.FindType(type);
    return info != nullptr &&
        HasTypeFlag(info->Flags(), Meta::TypeFlags::ValueType);
}

} // namespace

ObjectBuilder::ObjectBuilder(
    ::Aero::Markup::Schema& schema,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept
    : schema_(&schema),
      diagnostics_(diagnostics),
      frames_(),
      created_(),
      assignments_(),
      deferredStaticResources_(),
      nameScopes_(),
      resourceScopes_(),
      serviceResourceChain_(),
      namespaceBindings_(),
      pendingNamespaces_(),
      committedNames_(),
      committedResources_(),
      resultVisualContent_() {}

ObjectBuilder::~ObjectBuilder() noexcept {
    AbortTransaction();
}

Base::Result<LoaderResult> ObjectBuilder::Load(
    NodeReader& reader) noexcept {
    const LoadState* previous = loadContext_;
    loadContext_ = nullptr;
    Base::Result<Base::Ref<Base::Object>> loaded =
        LoadReaderCore(reader);
    Base::Result<LoaderResult> result =
        CompleteLoad(std::move(loaded));
    loadContext_ = previous;
    return result;
}

Base::Result<LoaderResult> ObjectBuilder::Load(
    NodeReader& reader,
    const LoadState& context) noexcept {
    const LoadState* previous = loadContext_;
    loadContext_ = &context;
    Base::Result<Base::Ref<Base::Object>> loaded =
        LoadReaderCore(reader);
    Base::Result<LoaderResult> result =
        CompleteLoad(std::move(loaded));
    loadContext_ = previous;
    return result;
}

Base::Result<LoaderResult> ObjectBuilder::Load(
    const CompiledDocument& document) noexcept {
    const LoadState* previous = loadContext_;
    loadContext_ = nullptr;
    Base::Result<Base::Ref<Base::Object>> loaded =
        LoadCompiledCore(document);
    Base::Result<LoaderResult> result =
        CompleteLoad(std::move(loaded));
    loadContext_ = previous;
    return result;
}

Base::Result<LoaderResult> ObjectBuilder::Load(
    const CompiledDocument& document,
    const LoadState& context) noexcept {
    const LoadState* previous = loadContext_;
    loadContext_ = &context;
    Base::Result<Base::Ref<Base::Object>> loaded =
        LoadCompiledCore(document);
    Base::Result<LoaderResult> result =
        CompleteLoad(std::move(loaded));
    loadContext_ = previous;
    return result;
}

Base::Result<LoaderResult> ObjectBuilder::CompleteLoad(
    Base::Result<Base::Ref<Base::Object>> loaded) noexcept {
    if (!loaded) return loaded.GetStatus();
    if (hasDeferredStaticResources_) {
        Base::Result<void> resolved =
            ResolveDeferredStaticResources();
        if (!resolved) {
            const Base::Status status = resolved.GetStatus();
            AbortTransaction();
            return status;
        }
    }
    LoaderResult result;
    result.root = std::move(loaded).Value();
    result.metadata = schema_->Metadata();
    result.names = std::move(committedNames_);
    result.resources = std::move(committedResources_);
    result.visualContent = std::move(resultVisualContent_);
    result.effects.Items() = std::move(extensionEffects_);
    result.hasDeferredStaticResources = hasDeferredStaticResources_;
    if (loadContext_ != nullptr) {
        result.runtimeLifetime = loadContext_->effectLifetime;
    }
    if (!deferredContent_.Empty()) {
        result.Clear();
        AbortTransaction();
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Deferred XAML content was not finalized");
    }
    if (loadContext_ != nullptr &&
        loadContext_->finalize != nullptr) {
        Base::Result<void> finalized =
            loadContext_->finalize(
                result,
                loadContext_->finalizeContext);
        if (!finalized) {
            const Base::Status status =
                finalized.GetStatus();
            result.Clear();
            AbortTransaction();
            return status;
        }
    }
    ClearTransaction();
    return result;
}

Base::Result<void> ObjectBuilder::ResolveDeferredStaticResources() noexcept {
    for (DeferredStaticResourceRecord& deferred :
         deferredStaticResources_) {
        Base::Result<Aero::ResourceValue> resource =
            LookupResource(deferred.key.View());
        if (!resource) {
            Base::Result<Base::String> message =
                StaticResourceNotFoundMessage(
                    deferred.key.View());
            if (!message) return message.GetStatus();
            return Failure(
                resource.GetStatus(),
                XamlObjectWriterDiagnosticCodes::StaticResourceNotFound,
                message.Value().View(),
                deferred.source);
        }
        Base::Result<void> written = WriteValue(
            deferred.targetObjectIndex,
            deferred.member,
            std::move(resource).Value(),
            deferred.source);
        if (!written) return written.GetStatus();
    }
    deferredStaticResources_.Clear();
    hasDeferredStaticResources_ = false;
    return {};
}

Base::Result<Base::Ref<Base::Object>> ObjectBuilder::LoadReaderCore(
    NodeReader& reader) noexcept {
    StreamingXamlNodeCursor cursor(reader);
    return LoadCursorCore(cursor);
}

Base::Result<Base::Ref<Base::Object>> ObjectBuilder::LoadCursorCore(
    NodeCursor& cursor) noexcept {
    if (consumed_ || loading_) return SessionConsumedStatus();
    consumed_ = true;

    AbortTransaction();
    committedNames_.Clear();
    committedResources_.Clear();
    resultVisualContent_.ReleaseContent();
    resultVisualContent_.Clear();
    if (!schema_->IsFrozen()) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                MessageSchemaNotReady.Data()),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageSchemaNotReady,
            {});
    }

    loading_ = true;
    Node node;
    while (!ended_) {
        Base::Result<const Node*> readResult =
            cursor.Read(node);
        if (!readResult) {
            const Base::Status status = readResult.GetStatus();
            AbortTransaction();
            loading_ = false;
            return status;
        }

        const Node* current = readResult.Value();
        if (current == nullptr) {
            const Base::Status status = Failure(
                InvalidStateStatus(),
                XamlObjectWriterDiagnosticCodes::InvalidWriterState,
                MessageInvalidWriterState,
                {});
            AbortTransaction();
            loading_ = false;
            return status;
        }
        if (loadContext_ != nullptr &&
            loadContext_->recordingNodes != nullptr) {
            Base::Result<Node> cloned = Node::Clone(*current);
            if (!cloned) {
                const Base::Status status = cloned.GetStatus();
                AbortTransaction();
                loading_ = false;
                return status;
            }
            Base::Result<void> recorded =
                loadContext_->recordingNodes->PushBack(
                    std::move(cloned).Value());
            if (!recorded) {
                const Base::Status status = recorded.GetStatus();
                AbortTransaction();
                loading_ = false;
                return status;
            }
        }
        Base::Result<void> processResult =
            ProcessNode(*current);
        if (!processResult) {
            const Base::Status status = processResult.GetStatus();
            AbortTransaction();
            loading_ = false;
            return status;
        }
    }

    if (!frames_.Empty() || !root_ || !pendingNamespaces_.Empty() ||
        !namespaceBindings_.Empty()) {
        const Base::Status status = Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
        AbortTransaction();
        loading_ = false;
        return status;
    }

    CommitDocumentScopes();
    Base::Ref<Base::Object> result = std::move(root_);
    loading_ = false;
    return result;
}

Base::Result<Base::Ref<Base::Object>> ObjectBuilder::LoadCompiledCore(
    const CompiledDocument& document) noexcept {
    if (consumed_ || loading_) return SessionConsumedStatus();
    if (!schema_->IsFrozen() || !document.IsValid()) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                MessageSchemaNotReady.Data()),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageSchemaNotReady,
            {});
    }
    Base::Result<void> compatible =
        ValidateCompiledCacheIdentity(
            document.Identity(), schema_->Domain());
    if (!compatible) return compatible.GetStatus();

    CompiledXamlNodeCursor cursor(document);
    return LoadCursorCore(cursor);
}

Base::Result<Base::Ref<Base::Object>> ObjectBuilder::CreateObject(
    Meta::TypeId type) const noexcept {
    if (loadContext_ != nullptr &&
        created_.Size() >= loadContext_->maxObjects) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "XAML object count exceeds configured limits");
    }
    if (loadContext_ != nullptr &&
        loadContext_->existingRoot &&
        rootObjectIndex_ == InvalidIndex &&
        frames_.Empty()) {
        const Meta::TypeId actual =
            loadContext_->existingRoot->RuntimeType();
        if (actual == Meta::InvalidTypeId ||
            !schema_->Types().IsAssignableFrom(type, actual)) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "LoadComponent root type is incompatible with XAML root type");
        }
        return loadContext_->existingRoot;
    }
    return schema_->CreateObject(type);
}

Base::Result<void> ObjectBuilder::ProcessNode(
    const Node& node) noexcept {
    switch (node.Kind()) {
    case NodeKind::NamespaceDeclaration:
        return QueueNamespaceDeclaration(node);
    case NodeKind::StartObject:
        return StartObject(node);
    case NodeKind::EndObject:
        return EndObject(node);
    case NodeKind::StartMember:
        return StartMember(node);
    case NodeKind::EndMember:
        return EndMember(node);
    case NodeKind::Value:
        return WriteText(node);
    case NodeKind::EndOfDocument:
        if (!frames_.Empty() || !root_ || !pendingNamespaces_.Empty() ||
            !namespaceBindings_.Empty()) {
            return Failure(
                InvalidStateStatus(),
                XamlObjectWriterDiagnosticCodes::InvalidWriterState,
                MessageInvalidWriterState,
                node.Source());
        }
        ended_ = true;
        return {};
    case NodeKind::None:
        break;
    }

    return Failure(
        InvalidStateStatus(),
        XamlObjectWriterDiagnosticCodes::InvalidWriterState,
        MessageInvalidWriterState,
        node.Source());
}

Base::Result<void> ObjectBuilder::QueueNamespaceDeclaration(
    const Node& node) noexcept {
    if (node.NamespaceUri().Empty()) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                MessageNamespaceState.Data()),
            XamlObjectWriterDiagnosticCodes::NamespaceState,
            MessageNamespaceState,
            node.Source());
    }

    PendingNamespaceRecord record;
    Base::Result<void> prefixResult = record.prefix.AssignUnchecked(
        node.NamespacePrefix());
    if (!prefixResult) {
        return prefixResult.GetStatus();
    }
    Base::Result<void> uriResult = record.uri.AssignUnchecked(
        node.NamespaceUri());
    if (!uriResult) {
        return uriResult.GetStatus();
    }
    record.source = node.Source();
    return pendingNamespaces_.PushBack(std::move(record));
}

Base::Result<void> ObjectBuilder::StartObject(
    const Node& node) noexcept {
    if (frames_.Empty() && root_) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                MessageMultipleRoots.Data()),
            XamlObjectWriterDiagnosticCodes::MultipleRootObjects,
            MessageMultipleRoots,
            node.Source());
    }

    std::uint32_t bindingStart = InvalidIndex;
    Base::Result<void> namespaceResult =
        ActivatePendingNamespaces(bindingStart);
    if (!namespaceResult) {
        return Failure(
            namespaceResult.GetStatus(),
            XamlObjectWriterDiagnosticCodes::NamespaceState,
            MessageNamespaceState,
            node.Source());
    }

    if (!frames_.Empty() &&
        frames_.Back().kind == FrameKind::Object &&
        HasPropertyElementSyntax(node.Name())) {
        return StartPropertyElement(
            node,
            frames_.Size() - 1U,
            bindingStart);
    }

    if (IsXamlNullObject(node.Name())) {
        return StartNullObject(node, bindingStart);
    }

    Base::Result<const Meta::TypeInfo*> typeResult =
        schema_->ResolveType(
            node.Name().NamespaceUri(),
            node.Name().LocalName());
    if (!typeResult) {
        return Failure(
            typeResult.GetStatus(),
            XamlObjectWriterDiagnosticCodes::UnknownType,
            MessageUnknownType,
            node.Source());
    }

    const Meta::TypeInfo* type = typeResult.Value();
    if (HasTypeFlag(type->Flags(), Meta::TypeFlags::ValueType)) {
        return StartValueObject(node, bindingStart, type->Id());
    }
    Base::Result<Base::Ref<Base::Object>> createResult =
        CreateObject(type->Id());
    if (!createResult) {
        const bool nonConstructible =
            createResult.GetStatus().code == Base::ErrorCode::Unsupported;
        return Failure(
            createResult.GetStatus(),
            nonConstructible
                ? XamlObjectWriterDiagnosticCodes::TypeNotConstructible
                : XamlObjectWriterDiagnosticCodes::FactoryFailed,
            nonConstructible
                ? MessageTypeNotConstructible
                : MessageFactoryFailed,
            node.Source());
    }

    CreatedObjectRecord record;
    record.object = std::move(createResult).Value();
    record.type = type->Id();
    const std::uint32_t objectIndex = created_.Size();
    Base::Result<void> appendObject =
        created_.PushBack(std::move(record));
    if (!appendObject) {
        return Failure(
            appendObject.GetStatus(),
            XamlObjectWriterDiagnosticCodes::FactoryFailed,
            MessageFactoryFailed,
            node.Source());
    }

    if (rootObjectIndex_ == InvalidIndex) {
        rootObjectIndex_ = objectIndex;
    }

    CreatedObjectRecord& stored = created_[objectIndex];
    stored.beginCalled = true;
    Base::Result<void> beginResult = schema_->BeginInit(
        stored.type,
        *stored.object);
    if (!beginResult) {
        return Failure(
            beginResult.GetStatus(),
            XamlObjectWriterDiagnosticCodes::InitializationFailed,
            MessageInitializationFailed,
            node.Source());
    }

    Frame frame;
    frame.kind = FrameKind::Object;
    frame.objectIndex = objectIndex;
    frame.namespaceBindingStart = bindingStart;
    frame.source = node.Source();
    Base::Result<void> scopeResult = CreateScopesForObject(
        objectIndex,
        frame,
        node.Source());
    if (!scopeResult) {
        return scopeResult.GetStatus();
    }

    Base::Result<void> appendFrame = frames_.PushBack(frame);
    if (!appendFrame) {
        return Failure(
            appendFrame.GetStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
    }
    return {};
}

Base::Result<void> ObjectBuilder::StartValueObject(
    const Node& node,
    std::uint32_t bindingStart,
    Meta::TypeId type) noexcept {
    if (frames_.Empty() ||
        (frames_.Back().kind != FrameKind::Object &&
         frames_.Back().kind != FrameKind::Member)) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                MessageTypeMismatch.Data()),
            XamlObjectWriterDiagnosticCodes::TypeMismatch,
            MessageTypeMismatch,
            node.Source());
    }

    CreatedObjectRecord record;
    record.type = type;
    record.valueElement = true;
    const std::uint32_t objectIndex = created_.Size();
    Base::Result<void> appended = created_.PushBack(
        std::move(record));
    if (!appended) return appended.GetStatus();

    Frame frame;
    frame.kind = FrameKind::ValueObject;
    frame.objectIndex = objectIndex;
    frame.namespaceBindingStart = bindingStart;
    frame.source = node.Source();
    appended = frames_.PushBack(frame);
    if (!appended) return appended.GetStatus();
    return {};
}

Base::Result<void> ObjectBuilder::StartNullObject(
    const Node& node,
    std::uint32_t bindingStart) noexcept {
    if (frames_.Empty() ||
        (frames_.Back().kind != FrameKind::Member &&
         frames_.Back().kind != FrameKind::Object)) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                MessageNullNotAllowed.Data()),
            XamlObjectWriterDiagnosticCodes::NullNotAllowed,
            MessageNullNotAllowed,
            node.Source());
    }

    Frame frame;
    frame.kind = FrameKind::NullObject;
    frame.namespaceBindingStart = bindingStart;
    frame.source = node.Source();
    Base::Result<void> appendResult = frames_.PushBack(frame);
    if (!appendResult) {
        return appendResult.GetStatus();
    }
    return {};
}

Base::Result<void> ObjectBuilder::EndObject(
    const Node& node) noexcept {
    if (frames_.Empty()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
    }

    Frame& frame = frames_.Back();
    if (frame.kind == FrameKind::ValueObject) {
        return CompleteValueObject(node);
    }
    if (frame.kind == FrameKind::NullObject) {
        return CompleteNullObject(node);
    }
    if (frame.kind == FrameKind::Member) {
        if (!frame.propertyElement) {
            return Failure(
                InvalidStateStatus(),
                XamlObjectWriterDiagnosticCodes::InvalidWriterState,
                MessageInvalidWriterState,
                node.Source());
        }
        if (frame.valuesWritten == 0U) {
            const Meta::TypeInfo* valueType =
                schema_->Types().FindType(
                    frame.member.valueType);
            // WPF permits an empty property element for reference-valued
            // properties. It means "leave the property's current/default
            // value in place", which is required by empty
            // Application.Resources declarations.
            if (valueType == nullptr ||
                valueType->Kind() !=
                    Meta::MetadataTypeKind::Object) {
                return Failure(
                    Base::Status::Failure(
                        Base::ErrorCode::ValidationFailed,
                        MessageMissingMemberValue.Data()),
                    XamlObjectWriterDiagnosticCodes::MissingMemberValue,
                    MessageMissingMemberValue,
                    node.Source());
            }
        }
        const std::uint32_t bindingStart = frame.namespaceBindingStart;
        frames_.PopBack();
        PopNamespaceBindings(bindingStart);
        return {};
    }
    if (frame.kind != FrameKind::Object) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
    }

    return CompleteObject(node);
}

Base::Result<void> ObjectBuilder::StartMember(
    const Node& node) noexcept {
    if (frames_.Empty() ||
        (frames_.Back().kind != FrameKind::Object &&
         frames_.Back().kind != FrameKind::ValueObject)) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
    }

    const Frame& objectFrame = frames_.Back();
    if (objectFrame.objectIndex >= created_.Size()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
    }

    if (objectFrame.kind == FrameKind::ValueObject) {
        if (node.Name().NamespaceUri() == LanguageNamespaceUri()) {
            if (IsXamlDirective(node.Name(), DirectiveKey)) {
                return StartDirective(
                    node, DirectiveKind::Key, objectFrame.objectIndex);
            }
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::Unsupported,
                    MessageInvalidDirective.Data()),
                XamlObjectWriterDiagnosticCodes::InvalidDirective,
                MessageInvalidDirective,
                node.Source());
        }
        if (!node.IsFromAttribute() ||
            node.Name().LocalName() != Base::StringView("Value")) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    MessageUnknownMember.Data()),
                XamlObjectWriterDiagnosticCodes::UnknownMember,
                MessageUnknownMember,
                node.Source());
        }
        Frame frame;
        frame.kind = FrameKind::ValueMember;
        frame.targetObjectIndex = objectFrame.objectIndex;
        frame.source = node.Source();
        Base::Result<void> appended = frames_.PushBack(frame);
        if (!appended) return appended.GetStatus();
        return {};
    }

    if (node.Name().NamespaceUri() == LanguageNamespaceUri()) {
        if (IsXamlDirective(node.Name(), DirectiveName)) {
            return StartDirective(
                node,
                DirectiveKind::Name,
                objectFrame.objectIndex);
        }
        if (IsXamlDirective(node.Name(), DirectiveKey)) {
            return StartDirective(
                node,
                DirectiveKind::Key,
                objectFrame.objectIndex);
        }
        if (IsXamlDirective(node.Name(), DirectiveClass)) {
            return StartDirective(
                node,
                DirectiveKind::Class,
                objectFrame.objectIndex);
        }
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                MessageInvalidDirective.Data()),
            XamlObjectWriterDiagnosticCodes::InvalidDirective,
            MessageInvalidDirective,
            node.Source());
    }

    Base::Result<ResolvedMember> memberResult = schema_->ResolveMember(
        created_[objectFrame.objectIndex].type,
        node.Name(),
        MemberSyntax::Attribute);
    if (!memberResult) {
        const bool notFound =
            memberResult.GetStatus().code == Base::ErrorCode::NotFound;
        return Failure(
            memberResult.GetStatus(),
            notFound
                ? XamlObjectWriterDiagnosticCodes::UnknownMember
                : XamlObjectWriterDiagnosticCodes::InvalidAttachedMember,
            notFound ? MessageUnknownMember : MessageInvalidAttachedMember,
            node.Source());
    }

    const ResolvedMember member = memberResult.Value();
    if (member.kind != Meta::MemberKind::Property ||
        !schema_->ResolveMemberWritePolicy(member).writable) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                MessageUnsupportedMember.Data()),
            XamlObjectWriterDiagnosticCodes::UnsupportedMember,
            MessageUnsupportedMember,
            node.Source());
    }

    Frame frame;
    frame.kind = FrameKind::Member;
    frame.targetObjectIndex = objectFrame.objectIndex;
    frame.member = member;
    frame.source = node.Source();
    frame.propertyElement = false;
    Base::Result<void> appendResult = frames_.PushBack(frame);
    if (!appendResult) {
        return Failure(
            appendResult.GetStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
    }
    return {};
}

Base::Result<void> ObjectBuilder::StartDirective(
    const Node& node,
    DirectiveKind directive,
    std::uint32_t targetObjectIndex) noexcept {
    if (!node.IsFromAttribute() || targetObjectIndex >= created_.Size()) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                MessageInvalidDirective.Data()),
            XamlObjectWriterDiagnosticCodes::InvalidDirective,
            MessageInvalidDirective,
            node.Source());
    }

    const CreatedObjectRecord& record = created_[targetObjectIndex];
    if ((directive == DirectiveKind::Name &&
         (!record.name.Empty() || record.nameRegistered)) ||
        (directive == DirectiveKind::Key &&
         (!record.key.Empty() || record.resourceRegistered))) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                MessageInvalidDirective.Data()),
            XamlObjectWriterDiagnosticCodes::InvalidDirective,
            MessageInvalidDirective,
            node.Source());
    }

    Frame frame;
    frame.kind = FrameKind::Directive;
    frame.directive = directive;
    frame.targetObjectIndex = targetObjectIndex;
    frame.source = node.Source();
    Base::Result<void> appendResult = frames_.PushBack(frame);
    if (!appendResult) {
        return appendResult.GetStatus();
    }
    return {};
}

Base::Result<void> ObjectBuilder::EndMember(
    const Node& node) noexcept {
    if (frames_.Empty()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
    }

    const Frame& frame = frames_.Back();
    if (frame.kind == FrameKind::ValueMember) {
        if (frame.valuesWritten != 1U) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    MessageMissingMemberValue.Data()),
                XamlObjectWriterDiagnosticCodes::MissingMemberValue,
                MessageMissingMemberValue,
                node.Source());
        }
        frames_.PopBack();
        return {};
    }
    if (frame.kind == FrameKind::Directive) {
        if (frame.valuesWritten != 1U) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    MessageMissingMemberValue.Data()),
                XamlObjectWriterDiagnosticCodes::MissingMemberValue,
                MessageMissingMemberValue,
                node.Source());
        }
        frames_.PopBack();
        return {};
    }

    if (frame.kind != FrameKind::Member || frame.propertyElement) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
    }
    if (frame.valuesWritten == 0U && !frame.deferredStaticResource) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                MessageMissingMemberValue.Data()),
            XamlObjectWriterDiagnosticCodes::MissingMemberValue,
            MessageMissingMemberValue,
            node.Source());
    }
    frames_.PopBack();
    return {};
}

Base::Result<void> ObjectBuilder::WriteText(
    const Node& node) noexcept {
    if (!node.IsFromAttribute() && IsWhitespaceOnly(node.Value())) {
        return {};
    }
    if (frames_.Empty()) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                MessageUnexpectedText.Data()),
            XamlObjectWriterDiagnosticCodes::UnexpectedText,
            MessageUnexpectedText,
            node.Source());
    }

    Frame& frame = frames_.Back();
    if (frame.kind == FrameKind::Directive) {
        return WriteDirectiveText(frame, node);
    }

    if (frame.kind == FrameKind::ValueMember ||
        frame.kind == FrameKind::ValueObject) {
        const std::uint32_t objectIndex =
            frame.kind == FrameKind::ValueMember
                ? frame.targetObjectIndex
                : frame.objectIndex;
        if (objectIndex >= created_.Size() ||
            !created_[objectIndex].valueElement ||
            !created_[objectIndex].value.IsUnset() ||
            frame.valuesWritten != 0U) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::AlreadyExists,
                    MessageDuplicateMemberValue.Data()),
                XamlObjectWriterDiagnosticCodes::DuplicateMemberValue,
                MessageDuplicateMemberValue,
                node.Source());
        }
        Base::StringView extensionName;
        Base::StringView argument;
        const MarkupValueKind markup = ParseMarkupValue(
            node.Value(), extensionName, argument);
        if (markup != MarkupValueKind::Literal &&
            markup != MarkupValueKind::EscapedLiteral) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    MessageInvalidMarkupExtension.Data()),
                XamlObjectWriterDiagnosticCodes::InvalidMarkupExtension,
                MessageInvalidMarkupExtension,
                node.Source());
        }
        Base::Result<Meta::Value> converted = schema_->ConvertText(
            created_[objectIndex].type,
            markup == MarkupValueKind::EscapedLiteral
                ? argument : node.Value());
        if (!converted) {
            return Failure(
                converted.GetStatus(),
                XamlObjectWriterDiagnosticCodes::InvalidValue,
                MessageInvalidValue,
                node.Source());
        }
        created_[objectIndex].value = std::move(converted).Value();
        ++frame.valuesWritten;
        return {};
    }

    if (frame.kind == FrameKind::Member) {
        const MemberWritePolicy policy =
            schema_->ResolveMemberWritePolicy(frame.member);
        const bool acceptsAnyValue = policy.acceptsAnyValue;
        Base::StringView extensionName;
        Base::StringView argument;
        const MarkupValueKind markup = ParseMarkupValue(
            node.Value(),
            extensionName,
            argument);
        if (markup == MarkupValueKind::Invalid) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    MessageInvalidMarkupExtension.Data()),
                XamlObjectWriterDiagnosticCodes::InvalidMarkupExtension,
                MessageInvalidMarkupExtension,
                node.Source());
        }
        if (markup == MarkupValueKind::Null) {
            if (IsValueType(schema_->Types(), frame.member.valueType) &&
                !acceptsAnyValue) {
                return Failure(
                    Base::Status::Failure(
                        Base::ErrorCode::ValidationFailed,
                        MessageNullNotAllowed.Data()),
                    XamlObjectWriterDiagnosticCodes::NullNotAllowed,
                    MessageNullNotAllowed,
                    node.Source());
            }
            Meta::Value value = Meta::Value::NullObject(frame.member.valueType);
            return WriteValueToMember(frame, std::move(value), node.Source());
        }
        if (markup == MarkupValueKind::StaticResource) {
            Base::Result<Aero::ResourceValue> resource = LookupResource(argument);
            if (!resource) {
                if (loadContext_ != nullptr &&
                    loadContext_->deferUnresolvedStaticResources) {
                    frame.deferredStaticResource = true;
                    hasDeferredStaticResources_ = true;
                    DeferredStaticResourceRecord deferred;
                    deferred.targetObjectIndex =
                        frame.targetObjectIndex;
                    deferred.member = frame.member;
                    deferred.source = node.Source();
                    Base::Result<void> key = deferred.key.Assign(
                        argument);
                    if (!key) return key.GetStatus();
                    Base::Result<void> stored =
                        deferredStaticResources_.PushBack(
                            std::move(deferred));
                    if (!stored) return stored.GetStatus();
                    return {};
                }
                Base::Result<Base::String> message =
                    StaticResourceNotFoundMessage(argument);
                if (!message) return message.GetStatus();
                return Failure(
                    resource.GetStatus(),
                    XamlObjectWriterDiagnosticCodes::StaticResourceNotFound,
                    message.Value().View(),
                    node.Source());
            }
            return WriteValueToMember(
                frame, std::move(resource).Value(), node.Source());
        }

        if (markup == MarkupValueKind::Extension) {
            Base::Result<ProvidedValue> value =
                EvaluateMarkupExtension(
                    frame.targetObjectIndex,
                    frame.member,
                    extensionName,
                    argument,
                    node.Source());
            if (!value) return value.GetStatus();
            return WriteProvidedValueToMember(
                frame,
                std::move(value).Value(),
                node.Source());
        }

        const ExtensionServices services = BuildExtensionServices(
            frame.targetObjectIndex,
            frame.member,
            node.Source());
        Base::Result<Meta::Value> convertResult = schema_->ConvertText(
            frame.member.valueType,
            markup == MarkupValueKind::EscapedLiteral
                ? argument
                : node.Value(),
            &services);
        if (!convertResult) {
            return Failure(
                convertResult.GetStatus(),
                XamlObjectWriterDiagnosticCodes::InvalidValue,
                MessageInvalidValue,
                node.Source());
        }
        return WriteValueToMember(
            frame,
            std::move(convertResult).Value(),
            node.Source());
    }

    if (frame.kind != FrameKind::Object ||
        frame.objectIndex >= created_.Size()) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                MessageUnexpectedText.Data()),
            XamlObjectWriterDiagnosticCodes::UnexpectedText,
            MessageUnexpectedText,
            node.Source());
    }

    Base::Result<ResolvedMember> contentResult =
        schema_->ResolveContentMember(created_[frame.objectIndex].type);
    if (!contentResult) {
        return Failure(
            contentResult.GetStatus(),
            XamlObjectWriterDiagnosticCodes::UnexpectedText,
            MessageUnexpectedText,
            node.Source());
    }

    Base::StringView extensionName;
    Base::StringView argument;
    const MarkupValueKind markup = ParseMarkupValue(
        node.Value(),
        extensionName,
        argument);
    if (markup == MarkupValueKind::Invalid) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                MessageInvalidMarkupExtension.Data()),
            XamlObjectWriterDiagnosticCodes::InvalidMarkupExtension,
            MessageInvalidMarkupExtension,
            node.Source());
    }
    if (markup == MarkupValueKind::Null) {
        if (IsValueType(
                schema_->Types(),
                contentResult.Value().valueType)) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    MessageNullNotAllowed.Data()),
                XamlObjectWriterDiagnosticCodes::NullNotAllowed,
                MessageNullNotAllowed,
                node.Source());
        }
        Meta::Value value = Meta::Value::NullObject(
            contentResult.Value().valueType);
        return WriteValue(
            frame.objectIndex,
            contentResult.Value(),
            std::move(value),
            node.Source());
    }
    if (markup == MarkupValueKind::StaticResource) {
        Base::Result<Aero::ResourceValue> resource = LookupResource(argument);
        if (!resource) {
            if (loadContext_ != nullptr &&
                loadContext_->deferUnresolvedStaticResources) {
                frame.deferredStaticResource = true;
                hasDeferredStaticResources_ = true;
                DeferredStaticResourceRecord deferred;
                deferred.targetObjectIndex = frame.objectIndex;
                deferred.member = contentResult.Value();
                deferred.source = node.Source();
                Base::Result<void> key = deferred.key.Assign(
                    argument);
                if (!key) return key.GetStatus();
                Base::Result<void> stored =
                    deferredStaticResources_.PushBack(
                        std::move(deferred));
                if (!stored) return stored.GetStatus();
                return {};
            }
            Base::Result<Base::String> message =
                StaticResourceNotFoundMessage(argument);
            if (!message) return message.GetStatus();
            return Failure(
                resource.GetStatus(),
                XamlObjectWriterDiagnosticCodes::StaticResourceNotFound,
                message.Value().View(),
                node.Source());
        }
        return WriteValue(
            frame.objectIndex,
            contentResult.Value(),
            std::move(resource).Value(),
            node.Source());
    }

    if (markup == MarkupValueKind::Extension) {
        Base::Result<ProvidedValue> value =
            EvaluateMarkupExtension(
                frame.objectIndex,
                contentResult.Value(),
                extensionName,
                argument,
                node.Source());
        if (!value) return value.GetStatus();
        return WriteProvidedValue(
            frame.objectIndex,
            contentResult.Value(),
            std::move(value).Value(),
            node.Source());
    }

    const ExtensionServices services = BuildExtensionServices(
        frame.objectIndex,
        contentResult.Value(),
        node.Source());
    Base::Result<Meta::Value> convertResult = schema_->ConvertText(
        contentResult.Value().valueType,
        markup == MarkupValueKind::EscapedLiteral
            ? argument
            : node.Value(),
        &services);
    if (!convertResult) {
        return Failure(
            convertResult.GetStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidValue,
            MessageInvalidValue,
            node.Source());
    }
    return WriteValue(
        frame.objectIndex,
        contentResult.Value(),
        std::move(convertResult).Value(),
        node.Source());
}

Base::Result<void> ObjectBuilder::WriteDirectiveText(
    Frame& frame,
    const Node& node) noexcept {
    if (frame.targetObjectIndex >= created_.Size() ||
        frame.valuesWritten != 0U || !node.IsFromAttribute()) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                MessageInvalidDirective.Data()),
            XamlObjectWriterDiagnosticCodes::InvalidDirective,
            MessageInvalidDirective,
            node.Source());
    }

    CreatedObjectRecord& object = created_[frame.targetObjectIndex];
    if (frame.directive == DirectiveKind::Name) {
        if (!Aero::NameScope::IsValidName(node.Value())) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    MessageInvalidDirective.Data()),
                XamlObjectWriterDiagnosticCodes::InvalidDirective,
                MessageInvalidDirective,
                node.Source());
        }
        Base::Result<void> assignResult = object.name.Assign(node.Value());
        if (!assignResult) {
            return assignResult.GetStatus();
        }
        Base::Result<void> registerResult = RegisterObjectName(
            frame.targetObjectIndex,
            node.Source());
        if (!registerResult) {
            return registerResult.GetStatus();
        }
    } else if (frame.directive == DirectiveKind::Key) {
        if (node.Value().Empty()) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    MessageInvalidDirective.Data()),
                XamlObjectWriterDiagnosticCodes::InvalidDirective,
                MessageInvalidDirective,
                node.Source());
        }
        Base::Result<void> assignResult = object.key.Assign(node.Value());
        if (!assignResult) {
            return assignResult.GetStatus();
        }
    } else if (frame.directive == DirectiveKind::Class) {
        // x:Class identifies the code-behind type. Aero loads the authored
        // object graph through its module factories, so retaining the value
        // would not affect runtime construction; accepting it keeps WPF XAML
        // portable without inventing a second type-activation path.
        if (node.Value().Empty()) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    MessageInvalidDirective.Data()),
                XamlObjectWriterDiagnosticCodes::InvalidDirective,
                MessageInvalidDirective,
                node.Source());
        }
    } else {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidDirective,
            MessageInvalidDirective,
            node.Source());
    }

    frame.valuesWritten = 1U;
    return {};
}

Base::Result<void> ObjectBuilder::StartPropertyElement(
    const Node& node,
    std::uint32_t targetFrameIndex,
    std::uint32_t bindingStart) noexcept {
    if (targetFrameIndex >= frames_.Size() ||
        frames_[targetFrameIndex].kind != FrameKind::Object ||
        frames_[targetFrameIndex].objectIndex >= created_.Size()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
    }

    const std::uint32_t targetObjectIndex =
        frames_[targetFrameIndex].objectIndex;
    Base::Result<ResolvedMember> memberResult = schema_->ResolveMember(
        created_[targetObjectIndex].type,
        node.Name(),
        MemberSyntax::PropertyElement);
    if (!memberResult) {
        const bool notFound =
            memberResult.GetStatus().code == Base::ErrorCode::NotFound;
        return Failure(
            memberResult.GetStatus(),
            notFound
                ? XamlObjectWriterDiagnosticCodes::UnknownMember
                : XamlObjectWriterDiagnosticCodes::InvalidAttachedMember,
            notFound ? MessageUnknownMember : MessageInvalidAttachedMember,
            node.Source());
    }

    const ResolvedMember member = memberResult.Value();
    Base::Result<ResolvedMember> contentMember =
        schema_->ResolveContentMember(
            created_[targetObjectIndex].type);
    const bool resourceEntries =
        created_[targetObjectIndex].type ==
            Aero::ResourceDictionary::StaticTypeId() &&
        contentMember &&
        contentMember.Value().id == member.id;
    if (member.kind != Meta::MemberKind::Property ||
        (!schema_->ResolveMemberWritePolicy(member).writable &&
         !resourceEntries)) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                MessageUnsupportedMember.Data()),
            XamlObjectWriterDiagnosticCodes::UnsupportedMember,
            MessageUnsupportedMember,
            node.Source());
    }

    Frame frame;
    frame.kind = FrameKind::Member;
    frame.targetObjectIndex = targetObjectIndex;
    frame.namespaceBindingStart = bindingStart;
    frame.member = member;
    frame.source = node.Source();
    frame.propertyElement = true;
    Base::Result<void> appendResult = frames_.PushBack(frame);
    if (!appendResult) {
        return Failure(
            appendResult.GetStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
    }
    return {};
}

Base::Result<void> ObjectBuilder::CompleteObject(
    const Node& node) noexcept {
    if (frames_.Empty() || frames_.Back().kind != FrameKind::Object) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
    }

    const Frame frame = frames_.Back();
    const std::uint32_t objectIndex = frame.objectIndex;
    if (objectIndex >= created_.Size()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
    }

    CreatedObjectRecord& record = created_[objectIndex];
    if (record.endCalled) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
    }

    if (record.type ==
        Detail::StaticResourceObject::StaticTypeId()) {
        const auto& extension =
            static_cast<const Detail::StaticResourceObject&>(
                *record.object);
        if (extension.ResourceKey().Empty()) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "StaticResource ResourceKey is empty"),
                XamlObjectWriterDiagnosticCodes::StaticResourceNotFound,
                MessageStaticResourceNotFound,
                node.Source());
        }
        Base::Result<Aero::ResourceValue> resource =
            LookupResource(extension.ResourceKey());
        if (!resource) {
            if (loadContext_ != nullptr &&
                loadContext_->deferUnresolvedStaticResources) {
                frames_.PopBack();
                PopNamespaceBindings(frame.namespaceBindingStart);
                if (!frames_.Empty()) {
                    Frame& parent = frames_.Back();
                    parent.deferredStaticResource = true;
                    DeferredStaticResourceRecord deferred;
                    if (parent.kind == FrameKind::Member) {
                        deferred.targetObjectIndex =
                            parent.targetObjectIndex;
                        deferred.member = parent.member;
                    } else if (parent.kind == FrameKind::Object &&
                               parent.objectIndex < created_.Size()) {
                        Base::Result<ResolvedMember> content =
                            schema_->ResolveContentMember(
                                created_[parent.objectIndex].type);
                        if (!content) return content.GetStatus();
                        deferred.targetObjectIndex =
                            parent.objectIndex;
                        deferred.member = content.Value();
                    } else {
                        return Base::Status::Failure(
                            Base::ErrorCode::InvalidState,
                            "StaticResource parent frame is invalid");
                    }
                    deferred.source = node.Source();
                    Base::Result<void> key = deferred.key.Assign(
                        extension.ResourceKey());
                    if (!key) return key.GetStatus();
                    Base::Result<void> stored =
                        deferredStaticResources_.PushBack(
                            std::move(deferred));
                    if (!stored) return stored.GetStatus();
                }
                hasDeferredStaticResources_ = true;
                return {};
            }
            Base::Result<Base::String> message =
                StaticResourceNotFoundMessage(extension.ResourceKey());
            if (!message) return message.GetStatus();
            return Failure(
                resource.GetStatus(),
                XamlObjectWriterDiagnosticCodes::StaticResourceNotFound,
                message.Value().View(),
                node.Source());
        }
        frames_.PopBack();
        PopNamespaceBindings(frame.namespaceBindingStart);
        return WriteValueToParent(
            std::move(resource).Value(), node.Source());
    }

    if (!record.name.Empty() && !record.nameRegistered) {
        Base::Result<void> nameResult = RegisterObjectName(
            objectIndex,
            node.Source());
        if (!nameResult) {
            return nameResult.GetStatus();
        }
    }

    Base::Result<void> endResult = schema_->EndInit(
        record.type,
        *record.object,
        BuildExtensionServices(
            objectIndex,
            {},
            node.Source()));
    if (!endResult) {
        return Failure(
            endResult.GetStatus(),
            XamlObjectWriterDiagnosticCodes::InitializationFailed,
            MessageInitializationFailed,
            node.Source());
    }
    record.endCalled = true;

    frames_.PopBack();
    PopNamespaceBindings(frame.namespaceBindingStart);

    Base::Result<bool> resourceResult = RegisterObjectResource(
        objectIndex,
        node.Source());
    if (!resourceResult) {
        return resourceResult.GetStatus();
    }
    if (resourceResult.Value()) {
        return {};
    }
    return WriteObjectToParent(objectIndex, node.Source());
}

Base::Result<void> ObjectBuilder::CompleteValueObject(
    const Node& node) noexcept {
    if (frames_.Empty() ||
        frames_.Back().kind != FrameKind::ValueObject) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
    }
    const Frame frame = frames_.Back();
    if (frame.objectIndex >= created_.Size() ||
        created_[frame.objectIndex].value.IsUnset()) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                MessageMissingMemberValue.Data()),
            XamlObjectWriterDiagnosticCodes::MissingMemberValue,
            MessageMissingMemberValue,
            node.Source());
    }
    frames_.PopBack();
    PopNamespaceBindings(frame.namespaceBindingStart);
    Base::Result<bool> resource = RegisterObjectResource(
        frame.objectIndex, node.Source());
    if (!resource) return resource.GetStatus();
    if (resource.Value()) return {};
    return WriteValueToParent(
        std::move(created_[frame.objectIndex].value),
        node.Source());
}

Base::Result<void> ObjectBuilder::CompleteNullObject(
    const Node& node) noexcept {
    if (frames_.Empty() || frames_.Back().kind != FrameKind::NullObject) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            node.Source());
    }
    const std::uint32_t bindingStart =
        frames_.Back().namespaceBindingStart;
    frames_.PopBack();
    PopNamespaceBindings(bindingStart);
    return WriteNullToParent(node.Source());
}

Base::Result<void> ObjectBuilder::WriteValueToParent(
    Meta::Value&& value,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    if (frames_.Empty()) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                MessageTypeMismatch.Data()),
            XamlObjectWriterDiagnosticCodes::TypeMismatch,
            MessageTypeMismatch,
            source);
    }
    Frame& parent = frames_.Back();
    if (parent.kind == FrameKind::Member) {
        return WriteValueToMember(parent, std::move(value), source);
    }
    if (parent.kind != FrameKind::Object ||
        parent.objectIndex >= created_.Size()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            source);
    }
    Base::Result<ResolvedMember> content =
        schema_->ResolveContentMember(
            created_[parent.objectIndex].type);
    if (!content) {
        return Failure(
            content.GetStatus(),
            XamlObjectWriterDiagnosticCodes::MissingContentProperty,
            MessageMissingContentProperty,
            source);
    }
    return WriteValue(
        parent.objectIndex,
        content.Value(),
        std::move(value),
        source);
}

Base::Result<void> ObjectBuilder::WriteObjectToParent(
    std::uint32_t objectIndex,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    if (objectIndex >= created_.Size()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            source);
    }

    if (frames_.Empty()) {
        if (root_) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::AlreadyExists,
                    MessageMultipleRoots.Data()),
                XamlObjectWriterDiagnosticCodes::MultipleRootObjects,
                MessageMultipleRoots,
                source);
        }
        root_ = created_[objectIndex].object;
        return {};
    }

    Frame& parent = frames_.Back();
    if (parent.kind == FrameKind::Member) {
        Meta::Value value = Meta::Value::FromObject(
            created_[objectIndex].type,
            created_[objectIndex].object);
        return WriteValueToMember(parent, std::move(value), source);
    }
    if (parent.kind != FrameKind::Object) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            source);
    }

    return WriteObjectToContent(
        parent.objectIndex,
        objectIndex,
        source);
}

Base::Result<void> ObjectBuilder::WriteObjectToContent(
    std::uint32_t parentObjectIndex,
    std::uint32_t childObjectIndex,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    if (parentObjectIndex >= created_.Size() ||
        childObjectIndex >= created_.Size()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            source);
    }

    Base::Result<ResolvedMember> contentResult =
        schema_->ResolveContentMember(created_[parentObjectIndex].type);
    if (!contentResult) {
        return Failure(
            contentResult.GetStatus(),
            XamlObjectWriterDiagnosticCodes::MissingContentProperty,
            MessageMissingContentProperty,
            source);
    }

    Meta::Value value = Meta::Value::FromObject(
        created_[childObjectIndex].type,
        created_[childObjectIndex].object);
    return WriteValue(
        parentObjectIndex,
        contentResult.Value(),
        std::move(value),
        source);
}

Base::Result<void> ObjectBuilder::WriteNullToParent(
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    if (frames_.Empty()) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                MessageNullNotAllowed.Data()),
            XamlObjectWriterDiagnosticCodes::NullNotAllowed,
            MessageNullNotAllowed,
            source);
    }

    Frame& parent = frames_.Back();
    if (parent.kind == FrameKind::Member) {
        const MemberWritePolicy policy =
            schema_->ResolveMemberWritePolicy(parent.member);
        const bool acceptsAnyValue = policy.acceptsAnyValue;
        if (IsValueType(schema_->Types(), parent.member.valueType) &&
            !acceptsAnyValue) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    MessageNullNotAllowed.Data()),
                XamlObjectWriterDiagnosticCodes::NullNotAllowed,
                MessageNullNotAllowed,
                source);
        }
        Meta::Value value = Meta::Value::NullObject(parent.member.valueType);
        return WriteValueToMember(parent, std::move(value), source);
    }
    if (parent.kind != FrameKind::Object ||
        parent.objectIndex >= created_.Size()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            source);
    }

    Base::Result<ResolvedMember> contentResult =
        schema_->ResolveContentMember(created_[parent.objectIndex].type);
    if (!contentResult ||
        IsValueType(schema_->Types(), contentResult.Value().valueType)) {
        return Failure(
            contentResult ? Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                MessageNullNotAllowed.Data()) : contentResult.GetStatus(),
            XamlObjectWriterDiagnosticCodes::NullNotAllowed,
            MessageNullNotAllowed,
            source);
    }

    Meta::Value value = Meta::Value::NullObject(
        contentResult.Value().valueType);
    return WriteValue(
        parent.objectIndex,
        contentResult.Value(),
        std::move(value),
        source);
}

Base::Result<void> ObjectBuilder::WriteValueToMember(
    Frame& memberFrame,
    Meta::Value&& value,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    Base::Result<void> result = WriteValue(
        memberFrame.targetObjectIndex,
        memberFrame.member,
        std::move(value),
        source);
    if (!result) {
        return result.GetStatus();
    }
    if (memberFrame.valuesWritten == UINT32_MAX) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                MessageDuplicateMemberValue.Data()),
            XamlObjectWriterDiagnosticCodes::DuplicateMemberValue,
            MessageDuplicateMemberValue,
            source);
    }
    ++memberFrame.valuesWritten;
    return {};
}

Base::Result<void> ObjectBuilder::WriteProvidedValueToMember(
    Frame& memberFrame,
    ProvidedValue&& provided,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    Base::Result<void> result = WriteProvidedValue(
        memberFrame.targetObjectIndex,
        memberFrame.member,
        std::move(provided),
        source);
    if (!result) return result.GetStatus();
    if (memberFrame.valuesWritten == UINT32_MAX) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                MessageDuplicateMemberValue.Data()),
            XamlObjectWriterDiagnosticCodes::DuplicateMemberValue,
            MessageDuplicateMemberValue,
            source);
    }
    ++memberFrame.valuesWritten;
    return {};
}

Base::Result<void> ObjectBuilder::WriteProvidedValue(
    std::uint32_t targetObjectIndex,
    const ResolvedMember& member,
    ProvidedValue&& provided,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    if (provided.kind == ProvidedValueKind::Value) {
        return WriteValue(
            targetObjectIndex,
            member,
            std::move(provided.value),
            source);
    }
    if (targetObjectIndex >= created_.Size()) {
        provided.Discard();
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            source);
    }

    const MemberWritePolicy policy =
        schema_->ResolveMemberWritePolicy(member);
    AssignmentRecord* assignment = FindAssignment(
        targetObjectIndex, member.id);
    if (!policy.writable ||
        (assignment != nullptr && assignment->count != 0U &&
         policy.mode == MemberWriteMode::SetOnce)) {
        provided.Discard();
        return Failure(
            Base::Status::Failure(
                policy.writable
                    ? Base::ErrorCode::AlreadyExists
                    : Base::ErrorCode::Unsupported,
                policy.writable
                    ? MessageDuplicateMemberValue.Data()
                    : MessageUnsupportedMember.Data()),
            policy.writable
                ? XamlObjectWriterDiagnosticCodes::DuplicateMemberValue
                : XamlObjectWriterDiagnosticCodes::UnsupportedMember,
            policy.writable
                ? MessageDuplicateMemberValue
                : MessageUnsupportedMember,
            source);
    }
    if (assignment == nullptr) {
        Base::Result<void> appended = assignments_.PushBack({
            targetObjectIndex, member.id, 0U});
        if (!appended) {
            provided.Discard();
            return appended.GetStatus();
        }
        assignment = &assignments_.Back();
    }

    CommittedEffect effect;
    if (loadContext_ != nullptr) {
        effect.lifetime = loadContext_->effectLifetime;
    }
    if (provided.kind == ProvidedValueKind::Expression) {
        if (provided.effectiveValues == nullptr ||
            !provided.expression.IsValid()) {
            provided.Discard();
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Markup extension returned an invalid property expression");
        }
        Base::Result<::Aero::DependencyObject*> target =
            schema_->ResolvePropertyTarget(
                *created_[targetObjectIndex].object);
        if (!target) {
            provided.Discard();
            return target.GetStatus();
        }
        effect.effectiveValues = provided.effectiveValues;
        effect.target = target.Value();
        effect.property = Meta::DependencyPropertyHandle{member.id};
        effect.pendingExpression = provided.expression;
        provided.expression = {};
    } else if (provided.kind == ProvidedValueKind::Handled) {
        effect.context = provided.rollbackContext;
        effect.token = provided.rollbackToken;
        effect.rollback = provided.rollback;
        effect.committed = true;
        provided.rollbackContext = nullptr;
        provided.rollback = nullptr;
    } else if (provided.kind == ProvidedValueKind::Deferred) {
        effect.context = provided.rollbackContext;
        effect.commit = provided.commit;
        effect.rollback = provided.rollback;
        effect.cleanup = provided.cleanup;
        provided.rollbackContext = nullptr;
        provided.commit = nullptr;
        provided.rollback = nullptr;
        provided.cleanup = nullptr;
    } else {
        provided.Discard();
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Markup extension returned an unknown value kind");
    }

    const bool immediate = loadContext_ == nullptr ||
        loadContext_->effectCommitMode == EffectCommitMode::Immediate;
    if (immediate && !effect.committed) {
        Base::Result<void> committed = effect.Commit();
        if (!committed) {
            effect.Rollback();
            return committed.GetStatus();
        }
    }
    Base::Result<void> effectStored =
        extensionEffects_.PushBack(std::move(effect));
    if (!effectStored) {
        effect.Rollback();
        return effectStored.GetStatus();
    }
    if (assignment->count == UINT32_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            MessageDuplicateMemberValue.Data());
    }
    ++assignment->count;
    return {};
}

Base::Result<void> ObjectBuilder::WriteValue(
    std::uint32_t targetObjectIndex,
    const ResolvedMember& member,
    Meta::Value&& value,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    if (targetObjectIndex >= created_.Size()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            source);
    }

    const MemberWritePolicy policy =
        schema_->ResolveMemberWritePolicy(member);
    if (!policy.writable) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                MessageUnsupportedMember.Data()),
            XamlObjectWriterDiagnosticCodes::UnsupportedMember,
            MessageUnsupportedMember,
            source);
    }

    AssignmentRecord* assignment = FindAssignment(
        targetObjectIndex,
        member.id);
    if (assignment != nullptr && assignment->count != 0U &&
        policy.mode == MemberWriteMode::SetOnce) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                MessageDuplicateMemberValue.Data()),
            XamlObjectWriterDiagnosticCodes::DuplicateMemberValue,
            MessageDuplicateMemberValue,
            source);
    }

    if (assignment == nullptr) {
        Base::Result<void> appendResult = assignments_.PushBack({
            targetObjectIndex,
            member.id,
            0U});
        if (!appendResult) {
            return Failure(
                appendResult.GetStatus(),
                XamlObjectWriterDiagnosticCodes::InvalidWriterState,
                MessageInvalidWriterState,
                source);
        }
        assignment = &assignments_.Back();
    }

    const ExtensionServices services = BuildExtensionServices(
        targetObjectIndex,
        member,
        source);
    if (member.valueType ==
            Media::Brush::StaticTypeId() &&
        value.Type() ==
            Meta::TypeOf<Base::Color>()) {
        Base::Result<Base::Color> color =
            Meta::ValueCodec<Base::Color>::Decode(
                value);
        if (!color) {
            return Failure(
                color.GetStatus(),
                XamlObjectWriterDiagnosticCodes::
                    InvalidWriterState,
                MessageInvalidWriterState,
                source);
        }
        Base::Result<
            Base::Ref<Media::Brush>>
            brush =
                Media::MakeSolidColorBrush(
                    color.Value());
        if (!brush) {
            return Failure(
                brush.GetStatus(),
                XamlObjectWriterDiagnosticCodes::
                    InvalidWriterState,
                MessageInvalidWriterState,
                source);
        }
        value = Meta::Value::FromObject(
            Media::Brush::StaticTypeId(),
            Base::Ref<Base::Object>(
                std::move(brush).Value()));
    }
    if (member.valueType == Meta::TypeOf<Base::String>() &&
        value.Type() == Media::FontFamily::StaticTypeId() &&
        value.Kind() == Meta::ValueKind::Object && value.AsObject()) {
        const auto& family = static_cast<const Media::FontFamily&>(
            *value.AsObject());
        Base::Result<Meta::Value> converted = Meta::Value::TryFromString(
            Meta::TypeOf<Base::String>(), family.GetSource());
        if (!converted) return Failure(converted.GetStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState, source);
        value = std::move(converted).Value();
    }
    Base::Result<Meta::ContentInfo> content =
        schema_->Metadata()->GetContentInfo(member.id);
    const bool hasVisualContent =
        content && content.Value().writable &&
        content.Value().IsVisual();
    const Meta::PropertyInfo* memberProperty =
        schema_->Types().FindProperty(member.id);
    const auto isUIElementValue =
        [this](const Meta::Value& candidate) noexcept {
            return candidate.Kind() == Meta::ValueKind::Object &&
                !candidate.IsNullObject() &&
                candidate.AsObject() &&
                schema_->Types().IsDerivedFrom(
                    candidate.AsObject()->RuntimeType(),
                    Aero::UIElement::StaticTypeId());
        };
    const bool hasVisualStructuralProperty =
        memberProperty != nullptr &&
        (static_cast<std::uint32_t>(
             memberProperty->Flags()) &
         static_cast<std::uint32_t>(
             Meta::PropertyFlags::Structural)) !=
            0U &&
        schema_->Metadata()->
            CanWriteProperty(member.id);
    const bool stagesVisualContent =
        (hasVisualContent ||
         hasVisualStructuralProperty) &&
        isUIElementValue(value);
    Base::Result<void> setResult = stagesVisualContent
        ? ObjectWriter::StageContent(
              *schema_,
              *created_[targetObjectIndex].object,
              value,
              services)
        : schema_->SetMember(
              *created_[targetObjectIndex].object,
              created_[targetObjectIndex].type,
              member,
              value);
    if (setResult &&
        hasVisualContent &&
        !stagesVisualContent &&
        schema_->Metadata()->CanReadProperty(member.id)) {
        Base::Result<Meta::Value> materialized =
            schema_->Metadata()->GetProperty(
                *created_[targetObjectIndex].object,
                member.id);
        if (materialized &&
            isUIElementValue(materialized.Value())) {
            setResult = ObjectWriter::StageContent(
                *schema_,
                *created_[targetObjectIndex].object,
                materialized.Value(),
                services);
        }
    }
    if (!setResult) {
        ::Aero::Diagnostics::DiagnosticCode code =
            XamlObjectWriterDiagnosticCodes::InvalidValue;
        Base::StringView message = MessageInvalidValue;
        if (setResult.GetStatus().code == Base::ErrorCode::Unsupported) {
            code = XamlObjectWriterDiagnosticCodes::UnsupportedMember;
            message = MessageUnsupportedMember;
        } else if (setResult.GetStatus().code ==
            Base::ErrorCode::InvalidArgument) {
            code = XamlObjectWriterDiagnosticCodes::TypeMismatch;
            message = MessageTypeMismatch;
        }
        return Failure(setResult.GetStatus(), code, message, source);
    }

    if (assignment->count == UINT32_MAX) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                MessageDuplicateMemberValue.Data()),
            XamlObjectWriterDiagnosticCodes::DuplicateMemberValue,
            MessageDuplicateMemberValue,
            source);
    }
    ++assignment->count;
    return {};
}

Base::Result<void> ObjectBuilder::RegisterObjectName(
    std::uint32_t objectIndex,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    if (objectIndex >= created_.Size()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            source);
    }

    CreatedObjectRecord& object = created_[objectIndex];
    if (object.name.Empty() || object.nameRegistered) {
        return {};
    }

    const std::uint32_t scopeIndex =
        FindNameScopeIndexForObject(objectIndex);
    if (scopeIndex == InvalidIndex || scopeIndex >= nameScopes_.Size()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            source);
    }

    NameScopeRecord& scope = nameScopes_[scopeIndex];
    Base::Result<void> localResult = scope.names.Register(
        object.name.View(),
        *object.object);
    if (!localResult) {
        const bool duplicate =
            localResult.GetStatus().code == Base::ErrorCode::AlreadyExists;
        return Failure(
            localResult.GetStatus(),
            duplicate
                ? XamlObjectWriterDiagnosticCodes::DuplicateName
                : XamlObjectWriterDiagnosticCodes::InvalidDirective,
            duplicate ? MessageDuplicateName : MessageInvalidDirective,
            source);
    }

    if (scope.ownerObjectIndex >= created_.Size()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            source);
    }
    CreatedObjectRecord& owner = created_[scope.ownerObjectIndex];
    Base::Result<void> callbackResult = schema_->RegisterName(
        owner.type,
        *owner.object,
        object.name.View(),
        *object.object);
    if (!callbackResult) {
        return Failure(
            callbackResult.GetStatus(),
            XamlObjectWriterDiagnosticCodes::NameRegistrationFailed,
            MessageNameRegistrationFailed,
            source);
    }

    object.nameRegistered = true;
    return {};
}

Base::Result<bool> ObjectBuilder::RegisterObjectResource(
    std::uint32_t objectIndex,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    if (objectIndex >= created_.Size()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            source);
    }

    CreatedObjectRecord& object = created_[objectIndex];
    if (object.resourceRegistered) {
        return true;
    }

    bool dictionaryContent = false;
    if (!frames_.Empty()) {
        const Frame& parent = frames_.Back();
        std::uint32_t parentObjectIndex = InvalidIndex;
        Meta::MemberId targetMember = Meta::InvalidMemberId;
        if (parent.kind == FrameKind::Object) {
            parentObjectIndex = parent.objectIndex;
        } else if (parent.kind == FrameKind::Member) {
            parentObjectIndex = parent.targetObjectIndex;
            targetMember = parent.member.id;
        }
        if (parentObjectIndex < created_.Size() &&
            created_[parentObjectIndex].type ==
                Aero::ResourceDictionary::StaticTypeId()) {
            Base::Result<ResolvedMember> content =
                schema_->ResolveContentMember(
                    created_[parentObjectIndex].type);
            dictionaryContent = content &&
                (targetMember == Meta::InvalidMemberId ||
                 targetMember == content.Value().id);
        } else if (
            parentObjectIndex < created_.Size() &&
            targetMember != Meta::InvalidMemberId &&
            object.type !=
                Aero::ResourceDictionary::
                    StaticTypeId() &&
            schema_->CreatesResourceScope(
                created_[parentObjectIndex].type)) {
            const Meta::PropertyInfo* property =
                schema_->Types().FindProperty(
                    targetMember);
            dictionaryContent =
                property != nullptr &&
                property->ValueType() ==
                    Aero::ResourceDictionary::
                        StaticTypeId();
        }
    }

    const bool explicitKey = !object.key.Empty();
    if (!dictionaryContent) {
        if (!explicitKey) return false;
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                MessageInvalidDirective.Data()),
            XamlObjectWriterDiagnosticCodes::InvalidDirective,
            MessageInvalidDirective,
            source);
    }

    Base::Result<Aero::ResourceKey> resourceKey =
        explicitKey
        ? Aero::ResourceKey::FromString(
              object.key.View())
        : (object.valueElement || !object.object
            ? Base::Result<Aero::ResourceKey>(
                  Base::Status::Failure(
                      Base::ErrorCode::ValidationFailed,
                      "Unkeyed ResourceDictionary entry must be an object"))
            : schema_->ResolveImplicitResourceKey(
                  object.type,
                  *object.object));
    if (!resourceKey) {
        return Failure(
            resourceKey.GetStatus(),
            XamlObjectWriterDiagnosticCodes::ResourceRegistrationFailed,
            MessageResourceRegistrationFailed,
            source);
    }

    const std::uint32_t scopeIndex = FindResourceScopeIndexForParent();
    if (scopeIndex == InvalidIndex || scopeIndex >= resourceScopes_.Size()) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::NotFound,
                MessageMissingResourceScope.Data()),
            XamlObjectWriterDiagnosticCodes::MissingResourceScope,
            MessageMissingResourceScope,
            source);
    }

    ResourceScopeRecord& scope = resourceScopes_[scopeIndex];
    Meta::Value resourceValue = object.valueElement
        ? object.value
        : Meta::Value::FromObject(object.type, object.object);
    Base::Result<void> localResult = scope.resources.Add(
        resourceKey.Value(),
        resourceValue,
        source);
    if (!localResult) {
        const bool duplicate =
            localResult.GetStatus().code == Base::ErrorCode::AlreadyExists;
        return Failure(
            localResult.GetStatus(),
            duplicate
                ? XamlObjectWriterDiagnosticCodes::DuplicateResourceKey
                : XamlObjectWriterDiagnosticCodes::InvalidDirective,
            duplicate ? MessageDuplicateResourceKey : MessageInvalidDirective,
            source);
    }

    if (scope.ownerObjectIndex >= created_.Size()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            source);
    }
    CreatedObjectRecord& owner = created_[scope.ownerObjectIndex];
    Base::Result<void> callbackResult = schema_->AddResource(
        owner.type,
        *owner.object,
        resourceKey.Value(),
        resourceValue);
    if (!callbackResult) {
        return Failure(
            callbackResult.GetStatus(),
            XamlObjectWriterDiagnosticCodes::ResourceRegistrationFailed,
            MessageResourceRegistrationFailed,
            source);
    }

    object.resourceRegistered = true;
    return true;
}

Base::Result<Aero::ResourceValue> ObjectBuilder::LookupResource(
    Base::StringView key) const noexcept {
    Base::Result<Aero::ResourceKey> resourceKey =
        Aero::ResourceKey::FromString(key);
    Base::StringView extensionName;
    Base::StringView typeName;
    if (ParseMarkupValue(key, extensionName, typeName) ==
            MarkupValueKind::Extension &&
        extensionName == Base::StringView("x:Type")) {
        std::uint32_t colon = typeName.SizeBytes();
        for (std::uint32_t index = 0U;
             index < typeName.SizeBytes();
             ++index) {
            if (typeName[index] != ':') continue;
            if (colon != typeName.SizeBytes()) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "StaticResource x:Type key contains multiple namespace prefixes");
            }
            colon = index;
        }
        Base::StringView prefix;
        Base::StringView localName = typeName;
        if (colon != typeName.SizeBytes()) {
            if (colon == 0U || colon + 1U >= typeName.SizeBytes()) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "StaticResource x:Type key namespace prefix is malformed");
            }
            prefix = typeName.Substr(0U, colon);
            localName = typeName.Substr(
                colon + 1U,
                typeName.SizeBytes() - colon - 1U);
        }
        Base::Result<Base::StringView> namespaceUri =
            LookupNamespace(prefix);
        if (!namespaceUri) return namespaceUri.GetStatus();
        Base::Result<const Meta::TypeInfo*> type =
            schema_->ResolveType(
                namespaceUri.Value(), localName);
        if (!type) return type.GetStatus();
        resourceKey = Aero::ResourceKey::FromType(
            type.Value()->Id());
    }
    if (!resourceKey) return resourceKey.GetStatus();

    for (std::uint32_t index = frames_.Size(); index > 0U; --index) {
        const Frame& frame = frames_[index - 1U];
        if (frame.kind != FrameKind::Object ||
            frame.resourceScopeIndex == InvalidIndex ||
            frame.resourceScopeIndex >= resourceScopes_.Size()) {
            continue;
        }
        Base::Result<Aero::ResourceValue> value =
            (resourceScopes_[frame.resourceScopeIndex].external != nullptr
                ? resourceScopes_[frame.resourceScopeIndex].external
                : &resourceScopes_[frame.resourceScopeIndex].resources)
                ->Lookup(resourceKey.Value());
        if (value) {
            return value;
        }
        if (value.GetStatus().code != Base::ErrorCode::NotFound) {
            return value.GetStatus();
        }
    }
    if (frames_.Empty()) {
        for (std::uint32_t index = resourceScopes_.Size();
             index > 0U; --index) {
            const ResourceScopeRecord& scope =
                resourceScopes_[index - 1U];
            const Aero::ResourceDictionary* dictionary =
                scope.external != nullptr
                    ? scope.external
                    : &scope.resources;
            Base::Result<Aero::ResourceValue> value =
                dictionary->Lookup(resourceKey.Value());
            if (value) return value;
            if (value.GetStatus().code != Base::ErrorCode::NotFound) {
                return value.GetStatus();
            }
        }
        Base::Result<Aero::ResourceValue> committed =
            committedResources_.Lookup(resourceKey.Value());
        if (committed) return committed;
        if (committed.GetStatus().code != Base::ErrorCode::NotFound) {
            return committed.GetStatus();
        }
    }
    if (loadContext_ != nullptr && loadContext_->resources != nullptr) {
        Base::Result<Aero::ResourceValue> value =
            loadContext_->resources->Lookup(resourceKey.Value());
        if (value) {
            return value;
        }
        if (value.GetStatus().code != Base::ErrorCode::NotFound) {
            return value.GetStatus();
        }
    }
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        MessageStaticResourceNotFound.Data());
}

Base::Result<void> ObjectBuilder::CreateScopesForObject(
    std::uint32_t objectIndex,
    Frame& frame,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    if (objectIndex >= created_.Size()) {
        return Failure(
            InvalidStateStatus(),
            XamlObjectWriterDiagnosticCodes::InvalidWriterState,
            MessageInvalidWriterState,
            source);
    }

    const bool documentRoot = objectIndex == rootObjectIndex_;
    if (documentRoot || schema_->CreatesNameScope(created_[objectIndex].type)) {
        NameScopeRecord scope;
        scope.ownerObjectIndex = objectIndex;
        const std::uint32_t index = nameScopes_.Size();
        Base::Result<void> appendResult =
            nameScopes_.PushBack(std::move(scope));
        if (!appendResult) {
            return appendResult.GetStatus();
        }
        frame.nameScopeIndex = index;
        if (documentRoot) {
            documentNameScopeIndex_ = index;
        }
    }

    if (documentRoot ||
        schema_->CreatesResourceScope(created_[objectIndex].type)) {
        ResourceScopeRecord scope;
        scope.ownerObjectIndex = objectIndex;
        scope.external = schema_->ResolveResourceScope(
            created_[objectIndex].type,
            *created_[objectIndex].object);
        const std::uint32_t index = resourceScopes_.Size();
        Base::Result<void> appendResult =
            resourceScopes_.PushBack(std::move(scope));
        if (!appendResult) {
            return appendResult.GetStatus();
        }
        frame.resourceScopeIndex = index;
        if (documentRoot) {
            documentResourceScopeIndex_ = index;
        }
    }
    return {};
}

Base::Result<void> ObjectBuilder::ActivatePendingNamespaces(
    std::uint32_t& bindingStart) noexcept {
    bindingStart = namespaceBindings_.Size();
    for (PendingNamespaceRecord& pending : pendingNamespaces_) {
        NamespaceBindingRecord binding;
        Base::Result<void> prefixResult = binding.prefix.AssignUnchecked(
            pending.prefix.View());
        if (!prefixResult) {
            return prefixResult.GetStatus();
        }
        Base::Result<void> uriResult = binding.uri.AssignUnchecked(
            pending.uri.View());
        if (!uriResult) {
            return uriResult.GetStatus();
        }
        Base::Result<void> appendResult =
            namespaceBindings_.PushBack(std::move(binding));
        if (!appendResult) {
            return appendResult.GetStatus();
        }
    }
    pendingNamespaces_.Clear();
    return {};
}

void ObjectBuilder::PopNamespaceBindings(
    std::uint32_t bindingStart) noexcept {
    if (bindingStart == InvalidIndex) {
        return;
    }
    while (namespaceBindings_.Size() > bindingStart) {
        namespaceBindings_.PopBack();
    }
}

Base::Result<Base::StringView> ObjectBuilder::LookupNamespace(
    Base::StringView prefix) const noexcept {
    if (prefix == XmlPrefix) {
        return XmlNamespaceUri;
    }
    for (std::uint32_t index = namespaceBindings_.Size();
         index > 0U;
         --index) {
        const NamespaceBindingRecord& binding =
            namespaceBindings_[index - 1U];
        if (binding.prefix.View() == prefix) {
            return binding.uri.View();
        }
    }
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        "XAML namespace prefix is not bound in the active scope");
}

ExtensionServices ObjectBuilder::BuildExtensionServices(
    std::uint32_t targetObjectIndex,
    const ResolvedMember& member,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    ExtensionServices services;
    services.schema = schema_;
    if (targetObjectIndex < created_.Size()) {
        services.targetObject = created_[targetObjectIndex].object.Get();
        services.targetObjectType = created_[targetObjectIndex].type;
    }
    services.targetMember = member.id;
    services.targetValueType = member.valueType;
    if (rootObjectIndex_ < created_.Size()) {
        services.rootObject = created_[rootObjectIndex_].object.Get();
    }
    if (loadContext_ != nullptr) {
        services.templatedParent = loadContext_->templatedParent;
        services.baseUri = loadContext_->baseUri;
        services.effectiveValues = loadContext_->effectiveValues;
        services.bindings = loadContext_->bindings;
        services.fallbackResources = loadContext_->fallbackResources;
    }
    services.source = source;
    services.nameScope = FindActiveNameScope();
    services.namespaces =
        Detail::ResourcePrivate::CreateNamespaceScope(
            &ObjectBuilder::NamespaceLookupCallback,
            this);
    services.resources =
        Detail::ResourcePrivate::CreateResourceResolver(
            &ObjectBuilder::ResourceLookupCallback,
            this);
    serviceResourceChain_.Clear();
    for (std::uint32_t index = frames_.Size();
         index > 0U; --index) {
        const Frame& frame = frames_[index - 1U];
        if (frame.kind != FrameKind::Object ||
            frame.resourceScopeIndex == InvalidIndex ||
            frame.resourceScopeIndex >= resourceScopes_.Size()) {
            continue;
        }
        const Aero::ResourceDictionary* dictionary =
            resourceScopes_[frame.resourceScopeIndex].external;
        if (dictionary == nullptr) {
            dictionary =
                &resourceScopes_[
                    frame.resourceScopeIndex].resources;
        }
        bool duplicate = false;
        for (const Aero::ResourceDictionary* existing :
             serviceResourceChain_) {
            if (existing == dictionary) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            Base::Result<void> added =
                serviceResourceChain_.PushBack(dictionary);
            if (!added) {
                serviceResourceChain_.Clear();
                break;
            }
        }
    }
    if (loadContext_ != nullptr &&
        loadContext_->resources != nullptr) {
        bool duplicate = false;
        for (const Aero::ResourceDictionary* existing :
             serviceResourceChain_) {
            if (existing == loadContext_->resources) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            Base::Result<void> added =
                serviceResourceChain_.PushBack(
                    loadContext_->resources);
            if (!added) {
                serviceResourceChain_.Clear();
            }
        }
    }
    services.ambientResourceChain = {
        serviceResourceChain_.Data(),
        serviceResourceChain_.Size()};
    services.visualContent = &resultVisualContent_;
    services.deferredContentOwner =
        FindDeferredContentOwner();
    services.deferredContent =
        &deferredContent_;
    return services;
}

const Aero::NameScope*
ObjectBuilder::FindActiveNameScope() const noexcept {
    for (std::uint32_t index = frames_.Size(); index > 0U; --index) {
        const Frame& frame = frames_[index - 1U];
        if (frame.kind == FrameKind::Object &&
            frame.nameScopeIndex != InvalidIndex &&
            frame.nameScopeIndex < nameScopes_.Size()) {
            return &nameScopes_[frame.nameScopeIndex].names;
        }
    }
    return nullptr;
}

Base::Object*
ObjectBuilder::FindDeferredContentOwner() const noexcept {
    for (std::uint32_t index = frames_.Size();
         index > 0U;
         --index) {
        const Frame& frame = frames_[index - 1U];
        if (frame.kind != FrameKind::Object ||
            frame.objectIndex >= created_.Size()) {
            continue;
        }
        const CreatedObjectRecord& record =
            created_[frame.objectIndex];
        if (schema_->DefersVisualContent(
                record.type)) {
            return record.object.Get();
        }
    }
    return nullptr;
}

std::uint32_t ObjectBuilder::FindNameScopeIndexForObject(
    std::uint32_t objectIndex) const noexcept {
    const std::uint32_t objectFrame = FindObjectFrameIndex(objectIndex);
    if (objectFrame == InvalidIndex) {
        return InvalidIndex;
    }
    for (std::uint32_t index = objectFrame + 1U; index > 0U; --index) {
        const Frame& frame = frames_[index - 1U];
        if (frame.kind == FrameKind::Object &&
            frame.nameScopeIndex != InvalidIndex) {
            return frame.nameScopeIndex;
        }
    }
    return InvalidIndex;
}

std::uint32_t ObjectBuilder::FindResourceScopeIndexForParent() const noexcept {
    for (std::uint32_t index = frames_.Size(); index > 0U; --index) {
        const Frame& frame = frames_[index - 1U];
        if (frame.kind == FrameKind::Object &&
            frame.resourceScopeIndex != InvalidIndex) {
            return frame.resourceScopeIndex;
        }
    }
    return InvalidIndex;
}

std::uint32_t ObjectBuilder::FindObjectFrameIndex(
    std::uint32_t objectIndex) const noexcept {
    for (std::uint32_t index = frames_.Size(); index > 0U; --index) {
        const Frame& frame = frames_[index - 1U];
        if (frame.kind == FrameKind::Object &&
            frame.objectIndex == objectIndex) {
            return index - 1U;
        }
    }
    return InvalidIndex;
}

ObjectBuilder::MarkupValueKind ObjectBuilder::ParseMarkupValue(
    Base::StringView text,
    Base::StringView& extensionName,
    Base::StringView& argument) const noexcept {
    extensionName = {};
    argument = {};
    const Base::StringView value = TrimBuilderText(text);
    if (value.Empty() || value[0] != '{') {
        return MarkupValueKind::Literal;
    }
    if (value.SizeBytes() >= 2U && value[1] == '}') {
        argument = value.Substr(2U, value.SizeBytes() - 2U);
        return MarkupValueKind::EscapedLiteral;
    }
    if (value.SizeBytes() < 2U ||
        value[value.SizeBytes() - 1U] != '}') {
        return MarkupValueKind::Invalid;
    }

    const Base::StringView inner = TrimBuilderText(value.Substr(
        1U,
        value.SizeBytes() - 2U));
    if (inner == NullMarkup) {
        return MarkupValueKind::Null;
    }

    std::uint32_t nestedDepth = 0U;
    char quote = '\0';
    for (char character : inner) {
        if (quote != '\0') {
            if (character == quote) quote = '\0';
            continue;
        }
        if (character == '\'' || character == '"') {
            quote = character;
        } else if (character == '{') {
            ++nestedDepth;
        } else if (character == '}') {
            if (nestedDepth == 0U) {
                return MarkupValueKind::Invalid;
            }
            --nestedDepth;
        }
    }
    if (nestedDepth != 0U || quote != '\0') {
        return MarkupValueKind::Invalid;
    }

    std::uint32_t nameEnd = 0U;
    while (nameEnd < inner.SizeBytes() &&
           !IsAsciiWhitespace(inner[nameEnd]) && inner[nameEnd] != ',') {
        ++nameEnd;
    }
    if (nameEnd == 0U) {
        return MarkupValueKind::Invalid;
    }
    extensionName = inner.Substr(0U, nameEnd);

    argument = TrimBuilderText(inner.Substr(nameEnd, inner.SizeBytes() - nameEnd));
    if (!argument.Empty() && argument[0] == ',') {
        argument = TrimBuilderText(argument.Substr(1U, argument.SizeBytes() - 1U));
    }
    if (extensionName == StaticResourceMarkup) {
        if (argument.Empty()) {
            return MarkupValueKind::Invalid;
        }
        return MarkupValueKind::StaticResource;
    }
    return MarkupValueKind::Extension;
}

Base::Result<ProvidedValue> ObjectBuilder::EvaluateMarkupExtension(
    std::uint32_t targetObjectIndex,
    const ResolvedMember& member,
    Base::StringView extensionName,
    Base::StringView arguments,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    std::uint32_t colon = extensionName.SizeBytes();
    for (std::uint32_t index = 0U;
         index < extensionName.SizeBytes();
         ++index) {
        if (extensionName[index] != ':') {
            continue;
        }
        if (colon != extensionName.SizeBytes()) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    MessageInvalidMarkupExtension.Data()),
                XamlObjectWriterDiagnosticCodes::InvalidMarkupExtension,
                MessageInvalidMarkupExtension,
                source);
        }
        colon = index;
    }

    Base::StringView prefix;
    Base::StringView localName = extensionName;
    if (colon != extensionName.SizeBytes()) {
        if (colon == 0U || colon + 1U >= extensionName.SizeBytes()) {
            return Failure(
                Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    MessageInvalidMarkupExtension.Data()),
                XamlObjectWriterDiagnosticCodes::InvalidMarkupExtension,
                MessageInvalidMarkupExtension,
                source);
        }
        prefix = extensionName.Substr(0U, colon);
        localName = extensionName.Substr(
            colon + 1U,
            extensionName.SizeBytes() - colon - 1U);
    }

    Base::Result<Base::StringView> namespaceResult = LookupNamespace(prefix);
    if (!namespaceResult) {
        return Failure(
            namespaceResult.GetStatus(),
            XamlObjectWriterDiagnosticCodes::UnknownMarkupExtension,
            MessageUnknownMarkupExtension,
            source);
    }
    Base::Result<const Meta::TypeInfo*> typeResult =
        schema_->ResolveType(
            namespaceResult.Value(),
            localName);
    if (!typeResult) {
        return Failure(
            typeResult.GetStatus(),
            XamlObjectWriterDiagnosticCodes::UnknownMarkupExtension,
            MessageUnknownMarkupExtension,
            source);
    }

    const ExtensionServices services = BuildExtensionServices(
        targetObjectIndex,
        member,
        source);
    Base::Result<ProvidedValue> provided =
        schema_->ProvideMarkupExtensionValue(
            typeResult.Value()->Id(),
            arguments,
            services);
    if (!provided) {
        const bool missing =
            provided.GetStatus().code == Base::ErrorCode::Unsupported ||
            provided.GetStatus().code == Base::ErrorCode::NotFound;
        return Failure(
            provided.GetStatus(),
            missing
                ? XamlObjectWriterDiagnosticCodes::UnknownMarkupExtension
                : XamlObjectWriterDiagnosticCodes::MarkupExtensionFailed,
            missing
                ? MessageUnknownMarkupExtension
                : MessageMarkupExtensionFailed,
            source);
    }
    return provided;
}

bool ObjectBuilder::IsXamlDirective(
    const QualifiedName& name,
    Base::StringView localName) const noexcept {
    return name.NamespaceUri() == LanguageNamespaceUri() &&
        name.LocalName() == localName;
}

bool ObjectBuilder::IsXamlNullObject(
    const QualifiedName& name) const noexcept {
    return IsXamlDirective(name, DirectiveNull);
}

bool ObjectBuilder::HasPropertyElementSyntax(
    const QualifiedName& name) const noexcept {
    for (char character : name.LocalName()) {
        if (character == '.') {
            return true;
        }
    }
    return false;
}

bool ObjectBuilder::IsWhitespaceOnly(
    Base::StringView value) const noexcept {
    for (char character : value) {
        if (!IsAsciiWhitespace(character)) {
            return false;
        }
    }
    return true;
}

ObjectBuilder::AssignmentRecord* ObjectBuilder::FindAssignment(
    std::uint32_t objectIndex,
    Meta::MemberId member) noexcept {
    for (AssignmentRecord& assignment : assignments_) {
        if (assignment.objectIndex == objectIndex &&
            assignment.member == member) {
            return &assignment;
        }
    }
    return nullptr;
}

void ObjectBuilder::CommitDocumentScopes() noexcept {
    committedNames_.Clear();
    committedResources_.Clear();
    if (documentNameScopeIndex_ < nameScopes_.Size()) {
        committedNames_ = std::move(
            nameScopes_[documentNameScopeIndex_].names);
    }
    if (documentResourceScopeIndex_ < resourceScopes_.Size()) {
        committedResources_ = std::move(
            resourceScopes_[documentResourceScopeIndex_].resources);
    }
}

void ObjectBuilder::AbortTransaction() noexcept {
    root_.Reset();
    resultVisualContent_.ReleaseContent();
    resultVisualContent_.Clear();
    deferredContent_.ReleaseAll();
    for (std::uint32_t index = extensionEffects_.Size();
         index > 0U; --index) {
        extensionEffects_[index - 1U].Rollback();
    }
    extensionEffects_.Clear();
    for (std::uint32_t index = created_.Size(); index > 0U; --index) {
        CreatedObjectRecord& record = created_[index - 1U];
        if (record.beginCalled && record.object) {
            schema_->AbortInit(record.type, *record.object);
        }
    }
    ClearTransaction();
}

void ObjectBuilder::ClearTransaction() noexcept {
    deferredContent_.ReleaseAll();
    frames_.Clear();
    assignments_.Clear();
    deferredStaticResources_.Clear();
    extensionEffects_.Clear();
    nameScopes_.Clear();
    resourceScopes_.Clear();
    serviceResourceChain_.Clear();
    namespaceBindings_.Clear();
    pendingNamespaces_.Clear();
    created_.Clear();
    root_.Reset();
    rootObjectIndex_ = InvalidIndex;
    documentNameScopeIndex_ = InvalidIndex;
    documentResourceScopeIndex_ = InvalidIndex;
    ended_ = false;
    hasDeferredStaticResources_ = false;
}

Base::Status ObjectBuilder::Failure(
    Base::Status status,
    ::Aero::Diagnostics::DiagnosticCode diagnostic,
    Base::StringView message,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    if (diagnostics_ != nullptr) {
        Base::Result<::Aero::Diagnostics::Diagnostic> item = ::Aero::Diagnostics::Diagnostic::Create(
            diagnostic,
            ::Aero::Diagnostics::DiagnosticSeverity::Error,
            message,
            source,
            ::Aero::Diagnostics::InvalidDiagnosticObjectId,
            Meta::InvalidMemberId);
        if (!item) {
            return item.GetStatus();
        }
        Base::Result<void> reportResult = diagnostics_->Report(
            std::move(item).Value());
        if (!reportResult) {
            return reportResult.GetStatus();
        }
    }
    return status;
}

Base::Result<Base::StringView> ObjectBuilder::NamespaceLookupCallback(
    void* context,
    Base::StringView prefix) noexcept {
    if (context == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            MessageNamespaceState.Data());
    }
    return static_cast<ObjectBuilder*>(context)->LookupNamespace(prefix);
}

Base::Result<Aero::ResourceValue> ObjectBuilder::ResourceLookupCallback(
    void* context,
    Base::StringView key) noexcept {
    if (context == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            MessageStaticResourceNotFound.Data());
    }
    return static_cast<ObjectBuilder*>(context)->LookupResource(key);
}

} // namespace Aero::Markup


// ===== ObjectWriter =====



#include <utility>
#include "gui/BindingInternal.hpp"

namespace Aero::Markup {
namespace {

Base::Status InvalidContent(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidArgument, message);
}

Base::Status InvalidContentState(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState, message);
}

} // namespace

Base::Result<void> DeferredContentPlan::Stage(
    Base::Object& owner,
    Base::Object& parent,
    const Base::Ref<Base::Object>& child,
    ::Aero::Meta::Registry& metadata,
    Meta::MemberId member) noexcept {
    if (!child || member == Meta::InvalidMemberId ||
        !metadata.IsReady()) {
        return InvalidContentState(
            "Deferred XAML content edge is invalid");
    }
    Base::Result<void> retained =
        edges_.PushBack({
            &owner,
            &parent,
            child,
            &metadata,
            member,
            false});
    if (!retained) return retained.GetStatus();
    Base::Result<void> written =
        metadata.WriteContent(parent, member, child);
    if (!written) {
        edges_.PopBack();
        return written.GetStatus();
    }
    return {};
}

Base::Result<void> DeferredContentPlan::StageProperty(
    Base::Object& owner,
    Base::Object& parent,
    const Base::Ref<Base::Object>& child,
    ::Aero::Meta::Registry& metadata,
    Meta::MemberId member) noexcept {
    if (!child ||
        member == Meta::InvalidMemberId) {
        return InvalidContentState(
            "Deferred XAML structural property edge is invalid");
    }
    const Meta::PropertyInfo* property =
        metadata.Types().FindProperty(member);
    if (property == nullptr) {
        return InvalidContent(
            "Deferred XAML structural property was not found");
    }
    Base::Result<void> retained =
        edges_.PushBack({
            &owner,
            &parent,
            child,
            &metadata,
            member,
            true});
    if (!retained) return retained.GetStatus();
    const Meta::Value value =
        Meta::Value::FromObject(
            property->ValueType(), child);
    Base::Result<void> written =
        metadata.SetProperty(
            parent, member, value);
    if (!written) {
        edges_.PopBack();
        return written.GetStatus();
    }
    return {};
}

Base::Result<void> DeferredContentPlan::CopyForOwner(
    const Base::Object& owner,
    Base::Vector<DeferredContentEdge>& output) const noexcept {
    output.Clear();
    for (const DeferredContentEdge& edge : edges_) {
        if (edge.owner != &owner) continue;
        Base::Result<void> copied =
            output.PushBack(edge);
        if (!copied) {
            output.Clear();
            return copied.GetStatus();
        }
    }
    return {};
}

Base::Result<void> DeferredContentPlan::StageBinding(
    Base::Object& owner,
    Base::Object* source,
    ::Aero::DependencyObject& target,
    Aero::Internal::BindingEngine& manager,
    ::Aero::Meta::Registry& metadata,
    Meta::DependencyPropertyHandle targetProperty,
    Meta::DependencyPropertyHandle dataContextProperty,
    Base::StringView path,
    Base::StringView stringFormat,
    Data::BindingMode mode,
    Meta::UpdateSourceTrigger updateSourceTrigger,
    bool bindsToSource) noexcept {
    if (!targetProperty.IsValid() ||
        (path.Empty() && !bindsToSource) ||
        !metadata.IsReady()) {
        return InvalidContentState(
            "Deferred XAML Binding declaration is invalid");
    }
    DeferredBindingEdge edge;
    edge.owner = &owner;
    edge.source = source;
    edge.target = &target;
    edge.manager = &manager;
    edge.metadata = &metadata;
    edge.targetProperty = targetProperty;
    edge.dataContextProperty = dataContextProperty;
    edge.mode = mode;
    edge.bindsToSource = bindsToSource;
    edge.updateSourceTrigger = updateSourceTrigger;
    Base::Result<void> assigned =
        edge.path.Assign(path);
    if (!assigned) return assigned.GetStatus();
    assigned = edge.stringFormat.Assign(
        stringFormat);
    if (!assigned) return assigned.GetStatus();
    return bindings_.PushBack(std::move(edge));
}

Base::Result<void>
DeferredContentPlan::CopyBindingsForOwner(
    const Base::Object& owner,
    Base::Vector<DeferredBindingEdge>& output) const noexcept {
    output.Clear();
    for (const DeferredBindingEdge& edge : bindings_) {
        if (edge.owner != &owner) continue;
        Base::Result<void> copied =
            output.PushBack(edge);
        if (!copied) {
            output.Clear();
            return copied.GetStatus();
        }
    }
    return {};
}

void DeferredContentPlan::ReleaseOwner(
    Base::Object& owner) noexcept {
    for (std::uint32_t index = 0U;
         index < edges_.Size();
         ++index) {
        DeferredContentEdge& edge = edges_[index];
        if (edge.owner != &owner) continue;
        bool firstForParent = true;
        for (std::uint32_t earlier = 0U;
             earlier < index;
             ++earlier) {
            firstForParent =
                firstForParent &&
                (edges_[earlier].owner != &owner ||
                 edges_[earlier].parent !=
                     edge.parent ||
                 (edge.property &&
                  edges_[earlier].member !=
                      edge.member));
        }
        if (firstForParent &&
            edge.parent != nullptr &&
            edge.metadata != nullptr) {
            if (edge.property) {
                const Meta::PropertyInfo* property =
                    edge.metadata->Types().
                        FindProperty(edge.member);
                if (property != nullptr) {
                    (void)edge.metadata->SetProperty(
                        *edge.parent,
                        edge.member,
                        Meta::Value::NullObject(
                            property->ValueType()));
                }
            } else {
                (void)edge.metadata->ClearContent(
                    *edge.parent,
                    edge.member);
            }
        }
    }

    std::uint32_t output = 0U;
    for (std::uint32_t index = 0U;
         index < edges_.Size();
         ++index) {
        DeferredContentEdge& edge = edges_[index];
        if (edge.owner == &owner) continue;
        if (output != index) {
            edges_[output] = std::move(edge);
        }
        ++output;
    }
    (void)edges_.Resize(output);

    output = 0U;
    for (std::uint32_t index = 0U;
         index < bindings_.Size();
         ++index) {
        DeferredBindingEdge& edge = bindings_[index];
        if (edge.owner == &owner) continue;
        if (output != index) {
            bindings_[output] = std::move(edge);
        }
        ++output;
    }
    (void)bindings_.Resize(output);
}

void DeferredContentPlan::ReleaseAll() noexcept {
    while (!edges_.Empty() || !bindings_.Empty()) {
        Base::Object* owner = !edges_.Empty()
            ? edges_.Front().owner
            : bindings_.Front().owner;
        if (owner == nullptr) {
            edges_.Clear();
            return;
        }
        ReleaseOwner(*owner);
    }
}

ObjectWriter::ObjectWriter(
    ::Aero::Markup::Schema& schema,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept
    : schema_(&schema),
      diagnostics_(diagnostics) {}

Base::Result<LoaderResult> ObjectWriter::LoadDocument(
    NodeReader& reader) noexcept {
    ObjectBuilder state(*this);
    return state.Load(reader);
}

Base::Result<LoaderResult> ObjectWriter::LoadDocument(
    const CompiledDocument& document) noexcept {
    ObjectBuilder state(*this);
    return state.Load(document);
}

Base::Result<Aero::Visual*> ObjectWriter::ResolveVisual(
    ::Aero::Markup::Schema& schema,
    Base::Object& object,
    Meta::TypeId type) noexcept {
    if (object.RuntimeType() != type ||
        !schema.Types().IsDerivedFrom(
            type, Aero::Visual::StaticTypeId())) {
        return InvalidContent(
            "XAML object metadata is not compatible with Visual");
    }
    return static_cast<Aero::Visual*>(&object);
}

Base::Result<Aero::UIElement*> ObjectWriter::ResolveUIElement(
    ::Aero::Markup::Schema& schema,
    Base::Object& object,
    Meta::TypeId type) noexcept {
    Base::Result<Aero::Visual*> visual =
        ResolveVisual(schema, object, type);
    if (!visual) return visual.GetStatus();
    Aero::UIElement* element =
        visual.Value()->AsUIElement();
    if (element == nullptr) {
        return InvalidContent("XAML object is not a UIElement");
    }
    return element;
}

Base::Result<void> ObjectWriter::StageContent(
    ::Aero::Markup::Schema& schema,
    Base::Object& object,
    const Meta::Value& value,
    const ExtensionServices& services) noexcept {
    VisualContentPlan* plan = services.visualContent;
    if (plan == nullptr || services.targetObject != &object ||
        value.Kind() != Meta::ValueKind::Object ||
        value.IsNullObject() || !value.AsObject()) {
        return InvalidContentState(
            "XAML visual content requires a non-null object");
    }

    ::Aero::Meta::Registry* metadata = schema.Metadata();
    if (metadata == nullptr) {
        return InvalidContentState(
            "XAML content metadata is unavailable");
    }
    Base::Result<Meta::ContentInfo> contentResult =
        metadata->GetContentInfo(services.targetMember);
    const Meta::PropertyInfo* property =
        schema.Types().FindProperty(
            services.targetMember);
    const bool structuralProperty =
        property != nullptr &&
        (static_cast<std::uint32_t>(
             property->Flags()) &
         static_cast<std::uint32_t>(
             Meta::PropertyFlags::Structural)) !=
            0U &&
        metadata->CanWriteProperty(
            services.targetMember);
    if (!contentResult && !structuralProperty) {
        return InvalidContent(
            "XAML content target has no content metadata");
    }
    if (!structuralProperty) {
        const Meta::ContentInfo& content =
            contentResult.Value();
        if (!content.writable ||
            !content.clearable ||
            !content.IsVisual() ||
            !schema.Types().IsDerivedFrom(
                services.targetObjectType,
                content.ownerType)) {
            return InvalidContent(
                "XAML content target has no visual content facet");
        }
    }

    Base::Object* childObject = value.AsObject().Get();
    Base::Result<Aero::UIElement*> childResult =
        ResolveUIElement(schema, *childObject, value.Type());
    if (!childResult) return childResult.GetStatus();

    // A deferred template owns a visual root without itself being a Visual.
    // Commit that root through the template's content accessor; descendant
    // visual edges are staged below once their actual visual parent exists.
    if (services.deferredContentOwner == &object &&
        !schema.Types().IsDerivedFrom(
            services.targetObjectType,
            Aero::Visual::StaticTypeId())) {
        if (structuralProperty) {
            return InvalidContent(
                "A non-visual template root cannot use a visual structural property");
        }
        return metadata->WriteContent(
            object,
            services.targetMember,
            value.AsObject());
    }

    Base::Result<Aero::UIElement*> parentResult =
        ResolveUIElement(
            schema, object, services.targetObjectType);
    if (!parentResult) return parentResult.GetStatus();

    if (services.deferredContentOwner != nullptr) {
        if (services.deferredContent == nullptr) {
            return InvalidContentState(
                "Deferred XAML content plan is unavailable");
        }
        return structuralProperty
            ? services.deferredContent->
                  StageProperty(
                      *services.
                           deferredContentOwner,
                      object,
                      value.AsObject(),
                      *metadata,
                      services.targetMember)
            : services.deferredContent->Stage(
                  *services.deferredContentOwner,
                  object,
                  value.AsObject(),
                  *metadata,
                  services.targetMember);
    }

    Base::Result<void> reserved = plan->Reserve(
        plan->contentEdges.Size() + 1U,
        plan->mountEdges.Size() + 1U,
        plan->nodes.Size() + 2U);
    if (!reserved) return reserved.GetStatus();

    Base::Result<Aero::Visual*> parentNode =
        ResolveVisual(
            schema, object, services.targetObjectType);
    if (!parentNode) return parentNode.GetStatus();
    Base::Result<Aero::Visual*> childNode =
        ResolveVisual(schema, *childObject, value.Type());
    if (!childNode) return childNode.GetStatus();

    Base::Result<void> parentAdded =
        plan->AddNode(*parentNode.Value());
    if (!parentAdded) return parentAdded.GetStatus();
    Base::Result<void> childAdded =
        plan->AddNode(*childNode.Value());
    if (!childAdded) return childAdded.GetStatus();

    Base::Ref<Base::Object> parentOwner =
        Base::Ref<Base::Object>::FromBorrowed(object);
    Base::Result<void> tracked =
        plan->contentEdges.PushBack({
            std::move(parentOwner), value.AsObject(),
            metadata, services.targetMember,
            structuralProperty});
    if (!tracked) return tracked.GetStatus();

    tracked = plan->mountEdges.PushBack({
        parentResult.Value(), childResult.Value(), {}});
    if (!tracked) {
        plan->contentEdges.PopBack();
        return tracked.GetStatus();
    }

    Base::Result<void> written =
        structuralProperty
        ? metadata->SetProperty(
              object,
              services.targetMember,
              value)
        : metadata->WriteContent(
              object,
              services.targetMember,
              value.AsObject());
    if (!written) {
        plan->mountEdges.PopBack();
        plan->contentEdges.PopBack();
        return written.GetStatus();
    }
    return {};
}

} // namespace Aero::Markup
