#include "Extensions.hpp"
#include "../ui/RuntimeManagers.hpp"

// Dynamic-resource markup-extension implementation.
#include "SchemaInternal.hpp"

#include <Aero/Style.hpp>

#include <new>
#include <utility>

namespace Aero::Markup {
using Aero::ResourceChangeSubscription;
using Aero::ResourceDictionary;

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


struct DeferredDynamicResourceState final {
    Core::EffectiveValueEngine* engine = nullptr;
    Core::DependencyObject* target = nullptr;
    Core::DependencyPropertyHandle property;
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

DynamicResourceExtension::DynamicResourceExtension(
    const DynamicResourceExtensionOptions& options) noexcept
    : options_(options) {}

Base::Result<void> DynamicResourceExtension::Register(
    Schema& schema,
    Core::TypeId dynamicResourceExtensionType) noexcept {
    return Detail::SchemaAccess::AddMarkupExtension(schema, {
        dynamicResourceExtensionType,
        &DynamicResourceExtension::ProvideValue,
        this});
}

Base::Result<ProvidedValue> DynamicResourceExtension::ProvideValue(
    Base::StringView arguments,
    const ExtensionContext& services,
    void* context) noexcept {
    DynamicResourceExtension* extension =
        static_cast<DynamicResourceExtension*>(context);
    if (extension == nullptr ||
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
            Core::Value::NullObject(
                Core::TypeOf<Base::Object>()));
    }
    Base::Result<Core::DependencyObject*> targetResult =
        Detail::SchemaAccess::ResolvePropertyTarget(
            *services.schema,
            *services.targetObject);
    if (!targetResult) {
        return targetResult.GetStatus();
    }
    Core::DependencyObject* target = targetResult.Value();
    const Core::DependencyPropertyHandle property{services.targetMember};
    Core::EffectiveValueEngine* effectiveValues =
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
    Base::Result<void> reserved = state->resources.TryReserve(
        services.ambientResourceChain.Size());
    if (reserved) {
        for (const ResourceDictionary* resource :
             services.ambientResourceChain) {
            reserved = state->resources.TryPushBack(resource);
            if (!reserved) break;
        }
    }
    if (reserved) reserved = state->key.TryAssign(key);
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
