#include <Aero/Markup/Extensions/XamlDynamicResource.hpp>

#include <new>

namespace Aero::Markup {
namespace {

struct DynamicResourceState final {
    DynamicResourceState(
        Core::EffectiveValueEngine& effectiveValues,
        Core::DependencyObject& dependencyObject,
        Core::DependencyPropertyHandle dependencyProperty) noexcept
        : engine(&effectiveValues),
          target(&dependencyObject),
          property(dependencyProperty),
          key(),
          sources(),
          allocator(&Base::GetDefaultAllocator()) {}

    struct Source final {
        ResourceDictionary* resources = nullptr;
        ResourceChangeSubscription subscription;
    };

    Core::EffectiveValueEngine* engine = nullptr;
    Core::DependencyObject* target = nullptr;
    Core::DependencyPropertyHandle property;
    Base::String key;
    Base::Vector<Source> sources;
    Base::IAllocator* allocator = nullptr;
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

Base::Result<Core::PropertyValue> EvaluateDynamicResource(
    void* context,
    Core::DependencyObject& object,
    Core::DependencyPropertyHandle property) noexcept {
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
        Base::Result<XamlResourceValue> resource =
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
    ResourceChangeKind,
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


} // namespace

Base::Result<void> DynamicResource::Attach(
    Core::EffectiveValueEngine& effectiveValues,
    ResourceDictionary& resources,
    Core::DependencyObject& target,
    Core::DependencyPropertyHandle property,
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

Base::Result<Core::PropertyExpression> DynamicResource::CreateExpression(
    Core::EffectiveValueEngine& effectiveValues,
    Base::Span<const ResourceDictionary* const> resourceChain,
    ResourceDictionary* fallbackResources,
    Core::DependencyObject& target,
    Core::DependencyPropertyHandle property,
    Base::StringView key) noexcept {
    const Base::StringView normalizedKey = TrimAscii(key);
    if (!property.IsValid() || normalizedKey.Empty() ||
        (resourceChain.Empty() &&
         fallbackResources == nullptr)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "DynamicResource requires resources, a target property, and a non-empty key");
    }
    Base::Result<XamlResourceValue> existing =
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
    Base::Result<void> assigned = state->key.TryAssign(normalizedKey);
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
            state->sources.TryPushBack({
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

    return Core::PropertyExpression{
        state,
        &EvaluateDynamicResource,
        &CleanupDynamicResource,
        Core::PropertyExpressionKind::DynamicResource};
}

Base::Result<void> DynamicResource::Attach(
    Core::EffectiveValueEngine& effectiveValues,
    Base::Span<const ResourceDictionary* const> resourceChain,
    ResourceDictionary* fallbackResources,
    Core::DependencyObject& target,
    Core::DependencyPropertyHandle property,
    Base::StringView key) noexcept {
    Base::Result<Core::PropertyExpression> expression = CreateExpression(
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

XamlDynamicResourceExtension::XamlDynamicResourceExtension(
    const XamlDynamicResourceExtensionOptions& options) noexcept
    : options_(options) {}

Base::Result<void> XamlDynamicResourceExtension::Register(
    XamlSchemaContext& schema,
    Core::TypeId dynamicResourceExtensionType) noexcept {
    if (options_.effectiveValues == nullptr || options_.resources == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "DynamicResource markup extension options are incomplete");
    }
    return schema.TryRegisterMarkupExtension({
        dynamicResourceExtensionType,
        &XamlDynamicResourceExtension::ProvideValue,
        this});
}

Base::Result<XamlProvidedValue> XamlDynamicResourceExtension::ProvideValue(
    Base::StringView arguments,
    const XamlServiceProvider& services,
    void* context) noexcept {
    XamlDynamicResourceExtension* extension =
        static_cast<XamlDynamicResourceExtension*>(context);
    if (extension == nullptr || extension->options_.effectiveValues == nullptr ||
        extension->options_.resources == nullptr ||
        services.schema == nullptr || services.targetObject == nullptr ||
        services.targetMember == Core::InvalidMemberId) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "DynamicResource markup extension has no target service context");
    }
    const Base::StringView key = TrimAscii(arguments);
    if (key.Empty() || key.Data() == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "DynamicResource requires a resource key");
    }
    Base::Result<Core::DependencyObject*> targetResult =
        services.schema->ResolvePropertyTarget(
            *services.targetObject);
    if (!targetResult) {
        return targetResult.GetStatus();
    }
    Core::DependencyObject* target = targetResult.Value();
    const Core::DependencyPropertyHandle property{services.targetMember};
    Base::Result<Core::PropertyExpression> expression =
        DynamicResource::CreateExpression(
            *extension->options_.effectiveValues,
            services.ambientResourceChain,
            extension->options_.resources,
            *target,
            property,
            key);
    if (!expression) return expression.GetStatus();
    return XamlProvidedValue::Expression(
        *extension->options_.effectiveValues,
        expression.Value());
}

} // namespace Aero::Markup
