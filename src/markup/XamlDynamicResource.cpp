#include <Aero/Markup/XamlDynamicResource.hpp>

#include <new>

namespace Aero::Markup {
namespace {

struct DynamicResourceState final {
    DynamicResourceState(
        Core::EffectiveValueEngine& effectiveValues,
        ResourceDictionary& dictionary,
        Core::DependencyObject& dependencyObject,
        Core::DependencyPropertyHandle dependencyProperty) noexcept
        : engine(&effectiveValues),
          resources(&dictionary),
          target(&dependencyObject),
          property(dependencyProperty),
          key(),
          allocator(&Base::GetDefaultAllocator()) {}

    Core::EffectiveValueEngine* engine = nullptr;
    ResourceDictionary* resources = nullptr;
    Core::DependencyObject* target = nullptr;
    Core::DependencyPropertyHandle property;
    Base::String key;
    ResourceChangeSubscription subscription;
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
    if (state == nullptr || state->resources == nullptr ||
        state->target != &object || state->property != property) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "DynamicResource expression state is invalid");
    }
    Base::Result<XamlResourceValue> resource = state->resources->Lookup(
        state->key.View());
    if (!resource) {
        return resource.GetStatus();
    }
    return Core::PropertyValue::FromObject(
        resource.Value().type, resource.Value().object);
}

void ResourceChanged(
    void* context,
    Base::StringView key,
    ResourceChangeKind kind,
    std::uint64_t) noexcept {
    DynamicResourceState* state = static_cast<DynamicResourceState*>(context);
    if (state == nullptr || state->engine == nullptr || state->target == nullptr) {
        return;
    }
    if (kind != ResourceChangeKind::Cleared && key != state->key.View()) {
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
    if (state->resources != nullptr) {
        static_cast<void>(state->resources->Unsubscribe(state->subscription));
    }
    state->~DynamicResourceState();
    allocator->Deallocate(
        state,
        sizeof(DynamicResourceState),
        alignof(DynamicResourceState),
        Base::MemoryTag::Markup);
}

Base::Result<XamlValue> ToXamlValue(
    const XamlResourceValue& value) noexcept {
    if (!value.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "DynamicResource resolved an invalid resource value");
    }
    return XamlValue::FromObject(value.type, value.object);
}

} // namespace

Base::Result<void> DynamicResource::Attach(
    Core::EffectiveValueEngine& effectiveValues,
    ResourceDictionary& resources,
    Core::DependencyObject& target,
    Core::DependencyPropertyHandle property,
    Base::StringView key) noexcept {
    const Base::StringView normalizedKey = TrimAscii(key);
    if (!property.IsValid() || normalizedKey.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "DynamicResource requires a target property and non-empty key");
    }
    Base::Result<XamlResourceValue> existing = resources.Lookup(normalizedKey);
    if (!existing) {
        return existing.GetStatus();
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
        effectiveValues, resources, target, property);
    Base::Result<void> assigned = state->key.TryAssign(normalizedKey);
    if (!assigned) {
        state->~DynamicResourceState();
        stateAllocator->Deallocate(
            memory, sizeof(DynamicResourceState), alignof(DynamicResourceState),
            Base::MemoryTag::Markup);
        return assigned.GetStatus();
    }
    Base::Result<ResourceChangeSubscription> subscribed =
        resources.SubscribeChanged(&ResourceChanged, state);
    if (!subscribed) {
        state->~DynamicResourceState();
        stateAllocator->Deallocate(
            memory, sizeof(DynamicResourceState), alignof(DynamicResourceState),
            Base::MemoryTag::Markup);
        return subscribed.GetStatus();
    }
    state->subscription = subscribed.Value();

    const Core::PropertyExpression expression{
        state,
        &EvaluateDynamicResource,
        &CleanupDynamicResource,
        Core::PropertyExpressionKind::DynamicResource};
    Base::Result<void> installed = effectiveValues.SetLocalExpression(
        target, property, expression);
    if (!installed) {
        CleanupDynamicResource(state);
        return installed.GetStatus();
    }
    return {};
}

XamlDynamicResourceExtension::XamlDynamicResourceExtension(
    const XamlDynamicResourceExtensionOptions& options) noexcept
    : options_(options) {}

Base::Result<void> XamlDynamicResourceExtension::Register(
    XamlSchemaContext& schema,
    Core::TypeId dynamicResourceExtensionType) noexcept {
    if (options_.effectiveValues == nullptr || options_.resources == nullptr ||
        options_.asDependencyObject == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "DynamicResource markup extension options are incomplete");
    }
    return schema.TryRegisterMarkupExtension({
        dynamicResourceExtensionType,
        &XamlDynamicResourceExtension::ProvideValue,
        this});
}

Base::Result<XamlValue> XamlDynamicResourceExtension::ProvideValue(
    Base::StringView arguments,
    const XamlServiceProvider& services,
    void* context) noexcept {
    XamlDynamicResourceExtension* extension =
        static_cast<XamlDynamicResourceExtension*>(context);
    if (extension == nullptr || extension->options_.effectiveValues == nullptr ||
        extension->options_.resources == nullptr ||
        extension->options_.asDependencyObject == nullptr ||
        services.targetObject == nullptr ||
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
    Core::DependencyObject* target = extension->options_.asDependencyObject(
        *services.targetObject, extension->options_.castContext);
    if (target == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "DynamicResource target must be a DependencyObject instance");
    }
    const Core::DependencyPropertyHandle property{services.targetMember};
    Base::Result<XamlResourceValue> resource = extension->options_.resources->Lookup(key);
    if (!resource) {
        return resource.GetStatus();
    }
    Base::Result<void> attached = DynamicResource::Attach(
        *extension->options_.effectiveValues,
        *extension->options_.resources,
        *target,
        property,
        key);
    if (!attached) {
        return attached.GetStatus();
    }
    return ToXamlValue(resource.Value());
}

} // namespace Aero::Markup
