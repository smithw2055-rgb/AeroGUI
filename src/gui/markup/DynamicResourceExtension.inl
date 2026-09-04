// ===== DynamicResourceExtension =====




// Dynamic-resource markup-extension implementation.



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
          allocator(&Base::GetDefaultAllocator()) {
        const Meta::DependencyProperty* descriptor =
            dependencyObject.PropertyRegistry().Find(dependencyProperty);
        if (descriptor != nullptr) property = descriptor->Handle();
    }

    struct Source {
        const ResourceDictionary* identity = nullptr;
        ResourceDictionary resources;
        ResourceChangeSubscription subscription;
    };

    Meta::EffectiveValueEngine* engine = nullptr;
    ::Aero::DependencyObject* target = nullptr;
    Meta::DependencyPropertyHandle property;
    Base::String key;
    Base::Vector<Source> sources;
    Base::IAllocator* allocator = nullptr;
};

Base::Result<Meta::PropertyValue> ConvertDynamicResourceValue(
    const Meta::PropertyValue& value,
    const Meta::DependencyProperty& property) noexcept {
    if (property.AcceptsAnyValue() ||
        value.Type() == property.ValueType() ||
        value.IsNullObject()) {
        return value;
    }
    if (property.ValueType() == Meta::TypeOf<Aero::Length>()) {
        Base::Result<long double> number = ReadConstantBindingNumber(value);
        if (number) {
            return Meta::ValueCodec<Aero::Length>::Encode(
                Aero::Length::Pixels(
                    static_cast<double>(number.Value())));
        }
    }
    if (property.ValueType() == Meta::TypeOf<Aero::GridLength>()) {
        Base::Result<long double> number = ReadConstantBindingNumber(value);
        if (number) {
            return Meta::ValueCodec<Aero::GridLength>::Encode(
                Aero::GridLength::Pixel(
                    static_cast<double>(number.Value())));
        }
    }
    if (property.ValueType() == Meta::TypeOf<Base::Thickness>()) {
        Base::Result<long double> number = ReadConstantBindingNumber(value);
        if (number) {
            const double size = static_cast<double>(number.Value());
            return Meta::ValueCodec<Base::Thickness>::Encode(
                Base::Thickness{size, size, size, size});
        }
    }
    if (value.Kind() == Meta::ValueKind::String) {
        return Meta::PropertyValue::TryFromString(
            property.ValueType(), value.AsString());
    }
    return value;
}

Base::Result<Meta::PropertyValue> MissingDynamicResourceValue(
    ::Aero::DependencyObject& object,
    const Meta::DependencyProperty* descriptor) noexcept {
    if (descriptor != nullptr) {
        const Meta::PropertyMetadata* metadata =
            descriptor->MetadataFor(object.RuntimeType());
        if (metadata == nullptr) {
            metadata = descriptor->MetadataFor(
                descriptor->RegisteredOwnerType());
        }
        if (metadata != nullptr && !metadata->defaultValue.IsUnset()) {
            return metadata->defaultValue;
        }
        if (descriptor->ValueType() == Meta::TypeOf<Base::String>()) {
            Base::Result<Meta::PropertyValue> empty =
                Meta::PropertyValue::TryFromString(
                    descriptor->ValueType(), Base::StringView{});
            if (empty) {
                return empty;
            }
        }
        return Meta::PropertyValue::NullObject(descriptor->ValueType());
    }
    return Meta::PropertyValue::NullObject(
        Meta::TypeOf<Base::Object>());
}

Base::Result<Meta::PropertyValue> ConvertLookedUpDynamicResource(
    const Aero::ResourceValue& resource,
    const Meta::DependencyProperty* descriptor) noexcept {
    return descriptor != nullptr
        ? ConvertDynamicResourceValue(resource, *descriptor)
        : Base::Result<Meta::PropertyValue>(resource);
}

Base::Result<Meta::PropertyValue> EvaluateDynamicResource(
    void* context,
    ::Aero::DependencyObject& object,
    Meta::DependencyPropertyHandle property) noexcept {
    DynamicResourceState* state = static_cast<DynamicResourceState*>(context);
    const Meta::DependencyProperty* descriptor =
        object.PropertyRegistry().Find(property);
    if (descriptor != nullptr) property = descriptor->Handle();
    if (state == nullptr ||
        state->target != &object || state->property != property) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "DynamicResource expression state is invalid");
    }
    for (const DynamicResourceState::Source& source :
         state->sources) {
        if (source.identity == nullptr) {
            continue;
        }
        Base::Result<Aero::ResourceValue> resource =
            source.resources.Lookup(state->key.View());
        if (resource) {
            return ConvertLookedUpDynamicResource(
                resource.Value(), descriptor);
        }
        if (resource.GetStatus().code !=
            Base::ErrorCode::NotFound) {
            return resource.GetStatus();
        }
    }
    FrameworkElement* element =
        ::Aero::TryCast<FrameworkElement>(&object);
    if (element != nullptr) {
        Base::Result<Aero::ResourceValue> resource =
            element->FindResource(state->key.View());
        if (resource) {
            return ConvertLookedUpDynamicResource(
                resource.Value(), descriptor);
        }
        if (resource.GetStatus().code !=
            Base::ErrorCode::NotFound) {
            return resource.GetStatus();
        }
    }
    // WPF DynamicResource does not fail the tree when the key is still
    // absent; the target keeps its default until a later resource change.
    return MissingDynamicResourceValue(object, descriptor);
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
        if (source.identity != nullptr) {
            static_cast<void>(
                source.resources.Unsubscribe(
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
    Base::Ref<::Aero::DependencyObject> targetOwner;
    ::Aero::DependencyObject* target = nullptr;
    Meta::DependencyPropertyHandle property;
    Base::Vector<ResourceDictionary> resources;
    ResourceDictionary fallbackResources;
    bool hasFallbackResources = false;
    Base::String key;
    Base::IAllocator* allocator = nullptr;
};


Base::Result<void> BindDynamicResourceRuntime(
    void* context, const EffectRuntimeServices& services) noexcept {
    auto* state = static_cast<DeferredDynamicResourceState*>(context);
    if (state == nullptr || services.effectiveValues == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "DynamicResource requires mounted View value services");
    }
    state->engine = services.effectiveValues;
    if (!state->hasFallbackResources && services.fallbackResources != nullptr) {
        Base::Result<ResourceDictionary> shared =
            services.fallbackResources->Share();
        if (!shared) return shared.GetStatus();
        state->fallbackResources = std::move(shared).Value();
        state->hasFallbackResources = true;
    }
    return {};
}

Base::Result<std::uint64_t> CommitDynamicResource(void* context) noexcept {
    auto* state = static_cast<DeferredDynamicResourceState*>(context);
    if (state == nullptr || state->engine == nullptr ||
        state->target == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Deferred DynamicResource state is invalid");
    }
    Base::Vector<const ResourceDictionary*> chain;
    Base::Result<void> prepared = chain.Reserve(
        state->resources.Size());
    if (prepared) {
        for (const ResourceDictionary& resources : state->resources) {
            prepared = chain.PushBack(&resources);
            if (!prepared) break;
        }
    }
    if (!prepared) return prepared.GetStatus();
    Base::Result<void> attached = DynamicResource::Attach(
        *state->engine,
        {chain.Data(), chain.Size()},
        state->hasFallbackResources
            ? &state->fallbackResources
            : nullptr,
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
        // Deferred resource effects are transaction-scoped. Teardown must
        // remove every queued effective-value record before targetOwner is
        // released; ClearLocalExpression would enqueue a final refresh and
        // leave a dangling object pointer in the engine.
        static_cast<void>(state->engine->DetachObject(*state->target));
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
    const Base::StringView normalizedKey = TrimAscii(key);
    if (!property.IsValid() || normalizedKey.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "DynamicResource requires a target property and a non-empty key");
    }
    for (const ResourceDictionary* resources :
         resourceChain) {
        if (resources == nullptr) continue;
        Base::Result<Aero::ResourceValue> existing =
            resources->Lookup(normalizedKey);
        if (!existing &&
            existing.GetStatus().code !=
                Base::ErrorCode::NotFound) {
            return existing.GetStatus();
        }
    }
    if (fallbackResources != nullptr) {
        Base::Result<Aero::ResourceValue> existing =
            fallbackResources->Lookup(normalizedKey);
        if (!existing &&
            existing.GetStatus().code !=
                Base::ErrorCode::NotFound) {
            return existing.GetStatus();
        }
    }

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
            if (source.identity == resources) return {};
        }
        Base::Result<ResourceChangeSubscription> subscription =
            resources->SubscribeChanged(
                &ResourceChanged, state);
        if (!subscription) {
            return subscription.GetStatus();
        }
        Base::Result<ResourceDictionary> shared =
            resources->Share();
        if (!shared) {
            static_cast<void>(resources->Unsubscribe(
                subscription.Value()));
            return shared.GetStatus();
        }
        DynamicResourceState::Source source;
        source.identity = resources;
        source.resources = std::move(shared).Value();
        source.subscription = subscription.Value();
        Base::Result<void> added =
            state->sources.PushBack(std::move(source));
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
    FrameworkElement* current =
        ::Aero::TryCast<FrameworkElement>(&target);
    while (current != nullptr) {
        assigned = subscribe(&current->GetResources());
        if (!assigned) {
            CleanupDynamicResource(state);
            return assigned.GetStatus();
        }
        current = ::Aero::TryCast<FrameworkElement>(
            current->GetLogicalParent());
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
    return SchemaPrivate::AddMarkupExtension(schema, {
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
    Base::StringView key = TrimAscii(arguments);
    constexpr Base::StringView ResourceKeyPrefix("ResourceKey=");
    if (key.SizeBytes() > ResourceKeyPrefix.SizeBytes() &&
        key.Substr(0U, ResourceKeyPrefix.SizeBytes()) ==
            ResourceKeyPrefix) {
        key = TrimAscii(key.Substr(
            ResourceKeyPrefix.SizeBytes(),
            key.SizeBytes() - ResourceKeyPrefix.SizeBytes()));
    }
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
        SchemaPrivate::ResolvePropertyTarget(
            *services.schema,
            *services.targetObject);
    if (!targetResult) {
        return targetResult.GetStatus();
    }
    ::Aero::DependencyObject* target = targetResult.Value();
    const Meta::DependencyPropertyHandle property{services.targetMember};
    if (services.deferredContentOwner != nullptr &&
        services.deferredContentOwner->RuntimeType() ==
            Controls::ControlTemplate::StaticTypeId()) {
        auto& controlTemplate =
            static_cast<Controls::ControlTemplate&>(
                *services.deferredContentOwner);
        Base::String targetName;
        Base::Result<void> captured = CaptureControlTemplateChildName(
            controlTemplate,
            services.nameScope,
            *target,
            targetName);
        if (!captured) return captured.GetStatus();
        Base::Result<void> retained =
            ::Aero::Controls::TemplatePrivate::AddDynamicResource(
                controlTemplate,
                targetName.View(),
                key,
                property);
        return retained
            ? Base::Result<ProvidedValue>(ProvidedValue::Handled())
            : Base::Result<ProvidedValue>(retained.GetStatus());
    }
    Meta::EffectiveValueEngine* effectiveValues =
        services.effectiveValues != nullptr
        ? services.effectiveValues
        : extension->options_.effectiveValues;
    ResourceDictionary* fallbackResources =
        services.fallbackResources != nullptr
        ? services.fallbackResources
        : extension->options_.resources;
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
    state->targetOwner =
        Base::Ref<::Aero::DependencyObject>::TryFromBorrowed(*target);
    if (!state->targetOwner) {
        CleanupDeferredDynamicResource(state);
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "DynamicResource target is not reference-counted");
    }
    state->target = state->targetOwner.Get();
    const Meta::DependencyProperty* descriptor =
        target->PropertyRegistry().Find(property);
    state->property = descriptor != nullptr
        ? descriptor->Handle()
        : property;
    state->allocator = &allocator;
    Base::Result<void> reserved = state->resources.Reserve(
        services.ambientResourceChain.Size());
    if (reserved) {
        for (const ResourceDictionary* resource :
             services.ambientResourceChain) {
            if (resource == nullptr) continue;
            Base::Result<ResourceDictionary> shared =
                resource->Share();
            if (!shared) {
                reserved = shared.GetStatus();
                break;
            }
            reserved = state->resources.PushBack(
                std::move(shared).Value());
            if (!reserved) break;
        }
    }
    if (reserved && fallbackResources != nullptr) {
        Base::Result<ResourceDictionary> shared =
            fallbackResources->Share();
        if (!shared) {
            reserved = shared.GetStatus();
        } else {
            state->fallbackResources =
                std::move(shared).Value();
            state->hasFallbackResources = true;
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
        &CleanupDeferredDynamicResource,
        nullptr,
        &BindDynamicResourceRuntime);
}

} // namespace Aero::Markup
