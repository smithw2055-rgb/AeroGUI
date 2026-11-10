#include "gui/markup/XamlSchema.hpp"
#include "gui/core/ValueConversion.hpp"
#include "gui/data/BindingEngine.hpp"
#include "gui/data/BindingCommon.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/templates/TemplateInstance.hpp"

#include <cstdio>
#include <cmath>
#include <new>
#include <utility>

#include <Aero/Gui.hpp>
#include <Aero/Markup/XamlReader.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Controls/Control.hpp>
#include <Aero/Controls.hpp>
#include <Aero/Markup/MarkupExtension.hpp>
#include <Aero/DataTemplate.hpp>
#include <Aero/TryCast.hpp>
#include <Aero/Controls/Buttons.hpp>
#include <Aero/Controls/TextBlock.hpp>
#include <Aero/Documents/Inlines.hpp>
#include <Aero/FrameworkContentElement.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Freezable.hpp>
#include <Aero/UIElement.hpp>
#include <Aero/Media/Animation.hpp>
#include <Aero/Triggers.hpp>
#include <Aero/Interactivity/TriggerAction.hpp>
#include <Aero/Interactivity/InteractionTriggers.hpp>
#include <Aero/Media/Animation/TimerTrigger.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Images.hpp>
#include <Aero/Interactivity/Behavior.hpp>
#include <Aero/Data/BindingBase.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Style.hpp>
#include <Aero/VisualStateManager.hpp>
#include <Aero/Value.hpp>
#include "gui/core/DependencyObjectAccess.hpp"

// Markup-extension implementations. Kept as one TU because the historic
// .inl files share helper symbols (constant conversion, dynamic-resource
// lookup, CaptureControlTemplateChildName) that were written for a single
// amalgamated translation unit.
// ===== BindingExtension.inl =====
// ===== BindingExtension =====
// Binding markup-extension implementation.
namespace Aero::Markup {

struct BindingExtensionOptions {
    BindingExtensionOptions() noexcept = default;
    BindingExtensionOptions(
        Aero::BindingEngine* bindingManager,
        Meta::DependencyPropertyHandle dataContext) noexcept
        : bindings(bindingManager),
          dataContextProperty(dataContext) {}

    Aero::BindingEngine* bindings = nullptr;
    Meta::DependencyPropertyHandle dataContextProperty;
};

class BindingExtension {
public:
    explicit BindingExtension(
        const BindingExtensionOptions& options) noexcept;

    BindingExtension(const BindingExtension&) = delete;
    BindingExtension& operator=(const BindingExtension&) = delete;

    Base::Result<void> Register(
        Schema& schema,
        Meta::TypeId bindingExtensionType) noexcept;
    void SetDataContextProperty(
        Meta::DependencyPropertyHandle property) noexcept {
        options_.dataContextProperty = property;
    }

private:
    BindingExtensionOptions options_;

    friend Base::Result<void> RegisterBuiltinMarkupExtensions(Schema&) noexcept;
    static Base::Result<ProvidedValue> ProvideValue(
        Base::StringView arguments,
        const ExtensionServices& services,
        void* context) noexcept;
};

struct DynamicResourceExtensionOptions {
    DynamicResourceExtensionOptions() noexcept = default;
    DynamicResourceExtensionOptions(
        Meta::EffectiveValueEngine* effectiveValueEngine,
        Aero::ResourceDictionary* resourceDictionary) noexcept
        : effectiveValues(effectiveValueEngine),
          resources(resourceDictionary) {}

    Meta::EffectiveValueEngine* effectiveValues = nullptr;
    Aero::ResourceDictionary* resources = nullptr;
};

class DynamicResourceExtension {
public:
    explicit DynamicResourceExtension(
        const DynamicResourceExtensionOptions& options) noexcept;

    DynamicResourceExtension(const DynamicResourceExtension&) = delete;
    DynamicResourceExtension& operator=(
        const DynamicResourceExtension&) = delete;

    Base::Result<void> Register(
        Schema& schema,
        Meta::TypeId dynamicResourceExtensionType) noexcept;

private:
    DynamicResourceExtensionOptions options_;

    friend Base::Result<void> RegisterBuiltinMarkupExtensions(Schema&) noexcept;
    static Base::Result<ProvidedValue> ProvideValue(
        Base::StringView arguments,
        const ExtensionServices& services,
        void* context) noexcept;
};

class StaticResourceExtension {
public:
    StaticResourceExtension() noexcept = default;

    StaticResourceExtension(const StaticResourceExtension&) = delete;
    StaticResourceExtension& operator=(
        const StaticResourceExtension&) = delete;

    Base::Result<void> Register(
        Schema& schema,
        Meta::TypeId staticResourceExtensionType) noexcept;

private:
    friend Base::Result<void> RegisterBuiltinMarkupExtensions(Schema&) noexcept;
    static Base::Result<ProvidedValue> ProvideValue(
        Base::StringView arguments,
        const ExtensionServices& services,
        void* context) noexcept;
};

class TypeExtension {
public:
    TypeExtension() noexcept = default;

    TypeExtension(const TypeExtension&) = delete;
    TypeExtension& operator=(const TypeExtension&) = delete;

    Base::Result<void> Register(
        Schema& schema,
        Meta::TypeId markupExtensionType) noexcept;

private:
    friend Base::Result<void> RegisterBuiltinMarkupExtensions(Schema&) noexcept;
    static Base::Result<ProvidedValue> ProvideValue(
        Base::StringView arguments,
        const ExtensionServices& services,
        void* context) noexcept;
};

class TemplateBindingExtension {
public:
    TemplateBindingExtension() noexcept = default;

    TemplateBindingExtension(
        const TemplateBindingExtension&) = delete;
    TemplateBindingExtension& operator=(
        const TemplateBindingExtension&) = delete;

    Base::Result<void> Register(
        Schema& schema,
        Meta::TypeId markupExtensionType) noexcept;

private:
    friend Base::Result<void> RegisterBuiltinMarkupExtensions(Schema&) noexcept;
    static Base::Result<ProvidedValue> ProvideValue(
        Base::StringView arguments,
        const ExtensionServices& services,
        void* context) noexcept;
};

class StaticExtension {
public:
    StaticExtension() noexcept = default;

    StaticExtension(const StaticExtension&) = delete;
    StaticExtension& operator=(const StaticExtension&) = delete;

    Base::Result<void> Register(
        Schema& schema,
        Meta::TypeId markupExtensionType) noexcept;

private:
    friend Base::Result<void> RegisterBuiltinMarkupExtensions(Schema&) noexcept;
    static Base::Result<ProvidedValue> ProvideValue(
        Base::StringView arguments,
        const ExtensionServices& services,
        void* context) noexcept;
};

Base::Result<long double> ReadConstantBindingNumber(
    const Meta::Value& value) noexcept;
Base::Result<Meta::Value> ConvertConstantBindingValue(
    const Meta::Value& value,
    Meta::TypeId targetType) noexcept;
Base::Result<void> CaptureControlTemplateChildName(
    Controls::ControlTemplate& controlTemplate,
    const Aero::NameScope* nameScope,
    Base::Object& target,
    Base::String& storage) noexcept;

} // namespace Aero::Markup

// ===== DynamicResourceExtension.inl =====
// ===== DynamicResourceExtension =====
// Dynamic-resource markup-extension implementation.
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
            DependencyObjectAccess::PropertyRegistry((dependencyObject)).Find(dependencyProperty);
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
} // namespace
Base::Result<Meta::PropertyValue> ConvertDynamicResourceValue(
    const Meta::PropertyValue& value,
    const Meta::DependencyProperty& property) noexcept {
    if (property.AcceptsAnyValue() ||
        value.Type() == property.ValueType() ||
        value.IsNullObject()) {
        return value;
    }
    if (property.ValueType() == Meta::TypeOf<Aero::Length>() ||
        property.ValueType() == Meta::TypeOf<Aero::GridLength>() ||
        property.ValueType() == Meta::TypeOf<Base::Thickness>()) {
        Base::Result<Meta::PropertyValue> metric =
            Base::ValueConversion::ConvertMetricValue(value, property.ValueType());
        if (metric) return metric;
    }
    if (value.Kind() == Meta::ValueKind::String) {
        return Meta::PropertyValue::TryFromString(
            property.ValueType(), value.AsString());
    }
    return value;
}

namespace {
Base::Result<Meta::PropertyValue> MissingDynamicResourceValue(
    ::Aero::DependencyObject& object,
    const Meta::DependencyProperty* descriptor) noexcept {
    if (descriptor != nullptr) {
        const Meta::PropertyMetadata* metadata =
            descriptor->MetadataFor(object);
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
        DependencyObjectAccess::PropertyRegistry((object)).Find(property);
    if (descriptor != nullptr) property = descriptor->Handle();
    if (state == nullptr ||
        state->target != &object || state->property != property) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "DynamicResource expression state is invalid");
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
    for (const DynamicResourceState::Source& source :
         state->sources) {
        if (source.identity == nullptr) {
            continue;
        }
        Aero::ResourceValue resource =
            source.resources.Lookup(state->key.View());
        if (resource.Kind() != Meta::ValueKind::Unset) {
            return ConvertLookedUpDynamicResource(
                resource, descriptor);
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

using DynamicResourceBucketMap =
    Base::HashMap<const ::Aero::DependencyObject*, Base::Vector<DynamicResourceState*>>;

DynamicResourceBucketMap& LiveDynamicResourcesByTarget() noexcept {
    static DynamicResourceBucketMap map;
    return map;
}

void TrackDynamicResource(DynamicResourceState* state) noexcept {
    if (state == nullptr || state->target == nullptr) return;
    DynamicResourceBucketMap& map = LiveDynamicResourcesByTarget();
    if (Base::Vector<DynamicResourceState*>* bucket = map.Find(state->target)) {
        for (DynamicResourceState* existing : *bucket) {
            if (existing == state) return;
        }
        bucket->PushBack(state);
        return;
    }
    Base::Vector<DynamicResourceState*> created;
    created.PushBack(state);
    static_cast<void>(map.Insert(state->target, std::move(created)));
}

void UntrackDynamicResource(DynamicResourceState* state) noexcept {
    if (state == nullptr || state->target == nullptr) return;
    DynamicResourceBucketMap& map = LiveDynamicResourcesByTarget();
    Base::Vector<DynamicResourceState*>* bucket = map.Find(state->target);
    if (bucket == nullptr) return;
    for (std::uint32_t index = 0U; index < bucket->Size(); ++index) {
        if ((*bucket)[index] != state) continue;
        if (index + 1U != bucket->Size()) {
            (*bucket)[index] = (*bucket)[bucket->Size() - 1U];
        }
        bucket->PopBack();
        if (bucket->Empty()) {
            map.Erase(state->target);
        }
        return;
    }
}

void CleanupDynamicResource(void* context) noexcept {
    DynamicResourceState* state = static_cast<DynamicResourceState*>(context);
    UntrackDynamicResource(state);
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
    void* context, const EffectServices& services) noexcept {
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
    chain.Reserve(
        state->resources.Size());
    for (const ResourceDictionary& resources : state->resources) {
        chain.PushBack(&resources);
    }
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
        state->sources.PushBack(std::move(source));
        return {};
    };
    FrameworkElement* current =
        ::Aero::TryCast<FrameworkElement>(&target);
    while (current != nullptr) {
        assigned = subscribe(&current->GetResources());
        if (!assigned) {
            CleanupDynamicResource(state);
            return assigned.GetStatus();
        }
        current = ::Aero::TryCast<FrameworkElement>(
            Data::GetVisualOrLogicalParent(current));
    }
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

    TrackDynamicResource(state);
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

Base::Result<void> AttachDeferredStyleDynamicResource(
    Meta::EffectiveValueEngine& engine,
    DependencyObject& target,
    Meta::DependencyPropertyHandle property,
    Base::StringView key) noexcept {
    return DynamicResource::Attach(
        engine,
        Base::Span<const ResourceDictionary* const>{},
        nullptr,
        target,
        property,
        key);
}

static bool IsSameOrDescendant(
    DependencyObject* target,
    DependencyObject* ancestor) noexcept {
    if (target == nullptr || ancestor == nullptr) return false;
    if (target == ancestor) return true;
    auto* current = ::Aero::TryCast<Media::Visual>(target);
    while (current != nullptr) {
        if (current == ancestor) return true;
        current = Data::GetVisualOrLogicalParent(current);
    }
    return false;
}

void NotifyDynamicResourceScopeChanged(
    DependencyObject& object) noexcept {
    DynamicResourceBucketMap& map = LiveDynamicResourcesByTarget();
    if (map.Empty()) return;
    for (auto& entry : map) {
        const DependencyObject* target = entry.Key();
        if (target == nullptr ||
            !IsSameOrDescendant(const_cast<DependencyObject*>(target), &object)) {
            continue;
        }
        for (DynamicResourceState* state : entry.Value()) {
            if (state != nullptr && state->engine != nullptr && state->target != nullptr) {
                static_cast<void>(
                    state->engine->Invalidate(*state->target, state->property));
            }
        }
    }
}

void NotifyAllDynamicResourcesChanged() noexcept {
    DynamicResourceBucketMap& map = LiveDynamicResourcesByTarget();
    if (map.Empty()) return;
    for (auto& entry : map) {
        for (DynamicResourceState* state : entry.Value()) {
            if (state != nullptr && state->engine != nullptr && state->target != nullptr) {
                static_cast<void>(
                    state->engine->Invalidate(*state->target, state->property));
            }
        }
    }
}

static Base::StringView ExtractResourceKeyArgument(Base::StringView arguments) noexcept {
    Base::StringView key = TrimAscii(arguments);
    constexpr Base::StringView ResourceKeyPrefix("ResourceKey");
    if (key.SizeBytes() >= ResourceKeyPrefix.SizeBytes() &&
        key.Substr(0U, ResourceKeyPrefix.SizeBytes()) == ResourceKeyPrefix) {
        Base::StringView rest = TrimAscii(key.Substr(
            ResourceKeyPrefix.SizeBytes(),
            key.SizeBytes() - ResourceKeyPrefix.SizeBytes()));
        if (!rest.Empty() && rest[0] == '=') {
            key = TrimAscii(rest.Substr(1U, rest.SizeBytes() - 1U));
        }
    }
    if (key.SizeBytes() >= 2U &&
        ((key[0] == '\'' && key[key.SizeBytes() - 1U] == '\'') ||
         (key[0] == '"' && key[key.SizeBytes() - 1U] == '"'))) {
        key = key.Substr(1U, key.SizeBytes() - 2U);
    }
    return key;
}

static Base::Result<Aero::ResourceValue> LookupResourceKeyInDictionary(
    const ResourceDictionary* dict,
    Base::StringView key,
    const Schema* schema) noexcept {
    if (dict == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::NotFound, "null dict");
    }
    Base::StringView typeName = key;
    bool isTypeKey = false;
    if (key.SizeBytes() >= 2U && key[0] == '{' && key[key.SizeBytes() - 1U] == '}') {
        Base::StringView inner = TrimAscii(key.Substr(1U, key.SizeBytes() - 2U));
        if (inner.SizeBytes() >= 7U && inner.Substr(0U, 7U) == "x:Type ") {
            typeName = TrimAscii(inner.Substr(7U, inner.SizeBytes() - 7U));
            isTypeKey = true;
        } else if (inner.SizeBytes() >= 5U && inner.Substr(0U, 5U) == "Type ") {
            typeName = TrimAscii(inner.Substr(5U, inner.SizeBytes() - 5U));
            isTypeKey = true;
        }
    }
    if (isTypeKey && schema != nullptr) {
        Base::StringView localName = typeName;
        for (std::uint32_t i = 0U; i < typeName.SizeBytes(); ++i) {
            if (typeName[i] == ':') {
                localName = TrimAscii(typeName.Substr(i + 1U, typeName.SizeBytes() - i - 1U));
                break;
            }
        }
        Base::Result<const Meta::TypeInfo*> resolved = schema->ResolveType(Meta::AeroNamespaceUri(), localName);
        if (!resolved) {
            resolved = schema->ResolveType(Base::StringView("http://schemas.microsoft.com/winfx/2006/xaml/presentation"), localName);
        }
        if (resolved && resolved.Value() != nullptr) {
            Aero::ResourceValue val = dict->Lookup(resolved.Value()->Id());
            if (val.Kind() != Meta::ValueKind::Unset) return val;
        }
    }
    Aero::ResourceValue val = dict->Lookup(key);
    if (val.Kind() != Meta::ValueKind::Unset) return val;
    if (isTypeKey) {
        val = dict->Lookup(typeName);
        if (val.Kind() != Meta::ValueKind::Unset) return val;
    }
    return Base::Status::Failure(Base::ErrorCode::NotFound, "not found");
}

DynamicResourceExtension::DynamicResourceExtension(
    const DynamicResourceExtensionOptions& options) noexcept
    : options_(options) {}

Base::Result<void> DynamicResourceExtension::Register(
    Schema& schema,
    Meta::TypeId dynamicResourceExtensionType) noexcept {
    return schema.AddMarkupExtension({
        dynamicResourceExtensionType,
        &DynamicResourceExtension::ProvideValue,
        this});
}

Base::Result<ProvidedValue> DynamicResourceExtension::ProvideValue(
    Base::StringView arguments,
    const ExtensionServices& services,
    void* context) noexcept {
    if (services.schema == nullptr || services.targetObject == nullptr ||
        services.targetMember == Meta::InvalidMemberId) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "DynamicResource markup extension has no target service context");
    }
    const Base::StringView key = ExtractResourceKeyArgument(arguments);
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
        // Keep the key live. DynamicResource values evaluate against the host tree/resources.
        Base::Result<Meta::Value> markerValue =
            Meta::Value::FromDynamicResourceKey(key);
        if (!markerValue) return markerValue.GetStatus();
        return ProvidedValue::FromValue(std::move(markerValue).Value());
    }
    Base::Result<::Aero::DependencyObject*> targetResult =
        services.schema->ResolvePropertyTarget(
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
            ::Aero::Controls::FrameworkTemplateState::AddDynamicResource(
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
            : (context != nullptr ? static_cast<DynamicResourceExtension*>(context)->options_.effectiveValues : nullptr);
    ResourceDictionary* fallbackResources =
        services.fallbackResources != nullptr
            ? services.fallbackResources
            : (context != nullptr ? static_cast<DynamicResourceExtension*>(context)->options_.resources : nullptr);

    Base::Result<Aero::ResourceValue> initialResource =
        Base::Status::Failure(Base::ErrorCode::NotFound, "Resource not found");
    const ResourceDictionary* matchedResource = nullptr;
    for (const ResourceDictionary* resDict : services.ambientResourceChain) {
        if (resDict == nullptr) continue;
        Base::Result<Aero::ResourceValue> found = resDict->Lookup(key);
        if (found) {
            initialResource = std::move(found);
            matchedResource = resDict;
            break;
        }
    }
    if (!initialResource && fallbackResources != nullptr) {
        initialResource = fallbackResources->Lookup(key);
    }
    if (!initialResource && services.resources.IsAvailable()) {
        initialResource = services.resources.Lookup(key);
    }
    if (initialResource) {
        const Meta::DependencyProperty* descriptor =
            DependencyObjectAccess::PropertyRegistry((*target)).Find(property);
        Base::Result<Meta::PropertyValue> converted =
            ConvertLookedUpDynamicResource(initialResource.Value(), descriptor);
        if (converted) {
            if (services.templatedParent != nullptr) {
                if (auto* fe = ::Aero::TryCast<FrameworkElement>(target)) {
                    static_cast<void>(fe->SetTemplateValue(property, std::move(converted).Value()));
                } else if (auto* fce = ::Aero::TryCast<FrameworkContentElement>(target)) {
                    static_cast<void>(fce->SetTemplateValue(property, std::move(converted).Value()));
                } else {
                    static_cast<void>(target->SetValue(property, std::move(converted).Value()));
                }
            } else {
                static_cast<void>(target->SetValue(property, std::move(converted).Value()));
            }
        }
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
    state->allocator = &allocator;
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
        DependencyObjectAccess::PropertyRegistry(target).Find(property);
    state->property = descriptor != nullptr
        ? descriptor->Handle()
        : property;
    Base::Result<void> reserved{};
    if (matchedResource != nullptr) {
        state->resources.Reserve(1U);
        Base::Result<ResourceDictionary> shared = matchedResource->Share();
        if (!shared) {
            reserved = shared.GetStatus();
        } else {
            state->resources.PushBack(std::move(shared).Value());
        }
    } else {
        state->resources.Reserve(services.ambientResourceChain.Size());
        for (std::uint32_t i = 0U; i < services.ambientResourceChain.Size(); ++i) {
            const ResourceDictionary* resource = services.ambientResourceChain[i];
            if (resource == nullptr) continue;
            bool duplicate = false;
            for (std::uint32_t j = 0U; j < i; ++j) {
                if (services.ambientResourceChain[j] == resource) {
                    duplicate = true;
                    break;
                }
            }
            if (duplicate) continue;
            Base::Result<ResourceDictionary> shared = resource->Share();
            if (!shared) {
                reserved = shared.GetStatus();
                break;
            }
            state->resources.PushBack(std::move(shared).Value());
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

// ===== StaticResourceExtension.inl =====
// ===== StaticResourceExtension =====
namespace Aero::Markup {
Base::Result<void> StaticResourceExtension::Register(
    Schema& schema,
    Meta::TypeId staticResourceExtensionType) noexcept {
    return schema.AddMarkupExtension({
        staticResourceExtensionType,
        &StaticResourceExtension::ProvideValue,
        nullptr});
}

Base::Result<ProvidedValue> StaticResourceExtension::ProvideValue(
    Base::StringView arguments,
    const ExtensionServices& services,
    void* /*context*/) noexcept {
    if (services.schema == nullptr || services.targetObject == nullptr ||
        services.targetMember == Meta::InvalidMemberId) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "StaticResource markup extension has no target service context");
    }
    const Base::StringView key = ExtractResourceKeyArgument(arguments);
    if (key.Empty() || key.Data() == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "StaticResource requires a resource key");
    }
    // A Setter is an authored style record rather than a DependencyObject.
    // Resolve its current resource value while the style dictionary is being
    // built; the style finalizer subsequently converts that value for the
    // target dependency property.
    const bool isNonDependencyTarget =
        services.targetObject == nullptr ||
        services.targetObject->RuntimeType() == Aero::Setter::StaticTypeId() ||
        services.targetObject->RuntimeType() == Aero::Style::StaticTypeId() ||
        (services.schema != nullptr && services.schema->Metadata() != nullptr &&
         !services.schema->Metadata()->Types().IsAssignableFrom(
             Meta::TypeOf<::Aero::DependencyObject>(),
             services.targetObject->RuntimeType()));
    if (isNonDependencyTarget) {
        if (services.resources.IsAvailable()) {
            Base::Result<Aero::ResourceValue> res = services.resources.Lookup(key);
            if (res) {
                return ProvidedValue::FromValue(std::move(res).Value());
            }
        }
        for (const ResourceDictionary* resources :
             services.ambientResourceChain) {
            if (resources == nullptr) continue;
            Base::Result<Aero::ResourceValue> resource =
                LookupResourceKeyInDictionary(resources, key, services.schema);
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
                LookupResourceKeyInDictionary(services.fallbackResources, key, services.schema);
            if (resource) {
                return ProvidedValue::FromValue(
                    std::move(resource).Value());
            }
            if (resource.GetStatus().code !=
                Base::ErrorCode::NotFound) {
                return resource.GetStatus();
            }
        }
        return ProvidedValue::FromValue(
            Meta::Value::NullObject(
                Meta::TypeOf<Base::Object>()));
    }
    Base::Result<::Aero::DependencyObject*> targetResult =
        services.schema->ResolvePropertyTarget(
            *services.targetObject);
    if (!targetResult) {
        return targetResult.GetStatus();
    }
    ::Aero::DependencyObject* target = targetResult.Value();
    const Meta::DependencyPropertyHandle property{services.targetMember};
    if (services.deferredContentOwner != nullptr &&
        services.deferredContentOwner->RuntimeType() ==
            Controls::ControlTemplate::StaticTypeId()) {
        Base::String targetName;
        Base::Result<void> captured = CaptureControlTemplateChildName(
            static_cast<Controls::ControlTemplate&>(
                *services.deferredContentOwner),
            services.nameScope,
            *target,
            targetName);
        if (!captured) return captured.GetStatus();
        Base::Result<void> retained =
            ::Aero::Controls::FrameworkTemplateState::AddDynamicResource(
                static_cast<Controls::ControlTemplate&>(
                    *services.deferredContentOwner),
                targetName.View(),
                key,
                property);
        return retained
            ? Base::Result<ProvidedValue>(ProvidedValue::Handled())
            : Base::Result<ProvidedValue>(retained.GetStatus());
    }
    const Meta::DependencyProperty* descriptor =
        DependencyObjectAccess::PropertyRegistry(target).Find(property);
    if (services.resources.IsAvailable()) {
        Base::Result<Aero::ResourceValue> res = services.resources.Lookup(key);
        if (res) {
            Base::Result<Meta::PropertyValue> converted =
                ConvertLookedUpDynamicResource(res.Value(), descriptor);
            if (converted) {
                return ProvidedValue::FromValue(std::move(converted).Value());
            }
            return ProvidedValue::FromValue(std::move(res).Value());
        }
    }
    auto resolveFrom =
        [&](const ResourceDictionary* resources)
            -> Base::Result<Meta::PropertyValue> {
        if (resources == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound, "StaticResource scope is absent");
        }
        Base::Result<Aero::ResourceValue> resource =
            resources->Lookup(key);
        if (resource) {
            return ConvertLookedUpDynamicResource(
                resource.Value(), descriptor);
        }
        return resource.GetStatus();
    };
    for (const ResourceDictionary* resources :
         services.ambientResourceChain) {
        Base::Result<Meta::PropertyValue> value = resolveFrom(resources);
        if (value) {
            return ProvidedValue::FromValue(
                std::move(value).Value());
        }
        if (value.GetStatus().code != Base::ErrorCode::NotFound) {
            return value.GetStatus();
        }
    }
    if (services.fallbackResources != nullptr) {
        Base::Result<Meta::PropertyValue> value =
            resolveFrom(services.fallbackResources);
        if (value) {
            return ProvidedValue::FromValue(
                std::move(value).Value());
        }
        if (value.GetStatus().code != Base::ErrorCode::NotFound) {
            return value.GetStatus();
        }
    }
    if (services.deferredContentOwner != nullptr) {
        Aero::ResourceDictionary* templateResources = nullptr;
        const Meta::TypeId ownerType =
            services.deferredContentOwner->RuntimeType();
        if (ownerType == ::Aero::DataTemplate::StaticTypeId() ||
            ownerType == ::Aero::HierarchicalDataTemplate::StaticTypeId()) {
            templateResources =
                &static_cast<::Aero::DataTemplate&>(
                    *services.deferredContentOwner).GetResources();
        } else if (
            ownerType == Controls::ItemsPanelTemplate::StaticTypeId()) {
            templateResources =
                &static_cast<Controls::ItemsPanelTemplate&>(
                    *services.deferredContentOwner).GetResources();
        }
        if (templateResources != nullptr) {
            Base::Result<Meta::PropertyValue> value =
                resolveFrom(templateResources);
            if (value) {
                return ProvidedValue::FromValue(
                    std::move(value).Value());
            }
            if (value.GetStatus().code !=
                Base::ErrorCode::NotFound) {
                return value.GetStatus();
            }
        }
    }
    // WPF StaticResource fails when the key is absent, but AeroGUI keeps the
    // tree alive by falling back to the property default so the gallery and
    // sample resources never crash the load.
    return ProvidedValue::FromValue(
        MissingDynamicResourceValue(*target, descriptor).Value());
}
} // namespace Aero::Markup

// ===== LocExtension.inl =====
// ===== LocExtension =====
namespace Aero::Markup {
namespace {

struct LocTarget {
    Base::WeakRef<::Aero::DependencyObject> object;
    Meta::DependencyPropertyHandle property;
    Meta::TypeId targetType = Meta::InvalidTypeId;
    Base::String key;
    Base::ResourceUri baseUri;
};

Base::Vector<LocTarget>& LocTargets() noexcept {
    static Base::Vector<LocTarget> targets;
    return targets;
}

void PruneExpiredLocTargets() noexcept {
    Base::Vector<LocTarget>& targets = LocTargets();
    std::uint32_t dst = 0U;
    for (std::uint32_t i = 0U; i < targets.Size(); ++i) {
        if (!targets[i].object.Lock()) continue;
        if (dst != i) {
            targets[dst] = std::move(targets[i]);
        }
        ++dst;
    }
    while (targets.Size() > dst) {
        targets.PopBack();
    }
}

struct CachedLocDictionary {
    Base::String canonicalUri;
    Base::Ref<Aero::ResourceDictionary> dictionary;
};

Base::Vector<CachedLocDictionary>& LocDictionaryCache() noexcept {
    static Base::Vector<CachedLocDictionary> cache;
    return cache;
}

Base::Result<Base::ResourceUri> ResolveLocDictionary(
    const Base::ResourceUri* baseUri,
    Base::StringView source) noexcept {
    if (baseUri != nullptr && !baseUri->Empty()) {
        return Base::ResourceUri::Resolve(*baseUri, source);
    }
    return Base::ResourceUri::Parse(source);
}

void NormalizeLocDictionaryImageUris(
    Aero::ResourceDictionary& dict,
    const Base::ResourceUri& dictionaryUri) noexcept {
    for (std::uint32_t i = 0U; i < dict.Size(); ++i) {
        Base::Result<Aero::ResourceEntrySnapshot> entry = dict.EntryAt(i);
        if (!entry) continue;
        const Meta::Value& val = entry.Value().value;
        if (val.Kind() != Meta::ValueKind::Object || !val.AsObject()) continue;
        if (auto* brush = ::Aero::TryCast<Media::ImageBrush>(val.AsObject().Get())) {
            if (auto* bitmap = ::Aero::TryCast<Media::BitmapImage>(brush->GetSource().Get())) {
                const Base::ResourceUri& imgUri = bitmap->GetUriSource();
                if (!imgUri.Empty() && !imgUri.IsAbsolute()) {
                    Base::Result<Base::ResourceUri> resolvedImg =
                        Base::ResourceUri::Resolve(dictionaryUri, imgUri.Canonical());
                    if (resolvedImg) {
                        bitmap->SetUriSource(resolvedImg.Value());
                    }
                }
            }
        }
    }
}

Base::Result<Base::Ref<Aero::ResourceDictionary>> LoadLocResourceDictionary(
    const Base::ResourceUri& dictionaryUri,
    const Base::ResourceUri* fallbackBaseUri) noexcept {
    const Base::StringView canonical = dictionaryUri.Canonical();
    for (const CachedLocDictionary& entry : LocDictionaryCache()) {
        if (entry.canonicalUri.View() == canonical && entry.dictionary) {
            return entry.dictionary;
        }
    }
    Aero::Gui* gui = Aero::Gui::Current();
    if (gui == nullptr || !gui->IsInitialized()) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "Gui is required to load localization ResourceDictionary");
    }
    XamlReader reader(*gui);
    Base::ResourceUri effectiveUri = dictionaryUri;
    Base::Result<XamlDocument> loaded = reader.Load(effectiveUri.Canonical());
    if (!loaded && fallbackBaseUri != nullptr && !fallbackBaseUri->Empty()) {
        Base::StringView relativePath = dictionaryUri.Path();
        if (!relativePath.Empty() && relativePath[0] == '/') {
            relativePath = relativePath.Substr(1U, relativePath.SizeBytes() - 1U);
        }
        if (!relativePath.Empty()) {
            Base::Result<Base::ResourceUri> resolvedFallback =
                Base::ResourceUri::Resolve(*fallbackBaseUri, relativePath);
            if (resolvedFallback) {
                effectiveUri = resolvedFallback.Value();
                for (const CachedLocDictionary& entry : LocDictionaryCache()) {
                    if (entry.canonicalUri.View() == effectiveUri.Canonical() && entry.dictionary) {
                        return entry.dictionary;
                    }
                }
                loaded = reader.Load(effectiveUri.Canonical());
            }
        }
    }
    if (!loaded) {
        return loaded.GetStatus();
    }
    Aero::ResourceDictionary* rootDict = loaded.Value().Root<Aero::ResourceDictionary>();
    if (rootDict == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Localization XAML root must be a ResourceDictionary");
    }
    const Base::ResourceUri& docUri = !loaded.Value().CanonicalUri().Empty()
        ? loaded.Value().CanonicalUri()
        : effectiveUri;
    NormalizeLocDictionaryImageUris(*rootDict, docUri);
    Base::Ref<Aero::ResourceDictionary> shared =
        Base::MakeRef<Aero::ResourceDictionary>(rootDict->Share());
    if (!shared) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Out of memory sharing localization ResourceDictionary");
    }
    CachedLocDictionary cacheEntry;
    if (cacheEntry.canonicalUri.Assign(effectiveUri.Canonical())) {
        cacheEntry.dictionary = shared;
        LocDictionaryCache().PushBack(std::move(cacheEntry));
    }
    return shared;
}

Base::Result<Meta::Value> LookupLocValueInDictionary(
    const Aero::ResourceDictionary& dict,
    Base::StringView key,
    Meta::TypeId targetType) noexcept {
    Aero::ResourceValue found = dict.Lookup(key);
    if (found.IsUnset()) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Localization resource key was not found");
    }
    if (found.Kind() == Meta::ValueKind::Object && found.AsObject() &&
        targetType != Meta::InvalidTypeId && found.Type() != targetType) {
        return Meta::Value::FromObject(targetType, found.AsObject());
    }
    return found;
}

} // namespace

Base::Result<void> LocExtension::Register(
    Schema& schema,
    Meta::TypeId markupExtensionType) noexcept {
    if (schema.IsFrozen() || markupExtensionType == Meta::InvalidTypeId) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Loc extension registration is invalid");
    }
    return schema.AddMarkupExtension({markupExtensionType, &ProvideValue, nullptr});
}

Base::Result<ProvidedValue> LocExtension::ProvideValue(
    Base::StringView arguments,
    const ExtensionServices& services,
    void* context) noexcept {
    const Base::StringView key = ExtractResourceKeyArgument(arguments);
    if (context != nullptr || key.Empty() ||
        services.targetMember == Meta::InvalidMemberId) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Loc requires a non-empty resource key and target property");
    }

    Base::Result<::Aero::DependencyObject*> targetResult =
        services.schema->ResolvePropertyTarget(*services.targetObject);
    if (!targetResult) return targetResult.GetStatus();
    ::Aero::DependencyObject* target = targetResult.Value();
    const Meta::DependencyPropertyHandle property{services.targetMember};
    if (!property.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Loc target property was not found");
    }

    PruneExpiredLocTargets();

    Base::Result<Base::ResourceUri> english = ResolveLocDictionary(
        services.baseUri, "Language-en.xaml");
    if (!english) return english.GetStatus();

    Base::Ref<Aero::ResourceDictionary> activeDict =
        target->GetValue(LocExtensionToken::ResourcesProperty);
    if (!activeDict) {
        Base::Result<Base::Ref<Aero::ResourceDictionary>> loaded =
            LoadLocResourceDictionary(english.Value(), services.baseUri);
        if (!loaded) return loaded.GetStatus();
        activeDict = std::move(loaded).Value();
    }

    Base::Result<Meta::Value> initial = LookupLocValueInDictionary(
        *activeDict, key, services.targetValueType);
    if (!initial) return initial.GetStatus();

    LocTarget subscription;
    subscription.object = Base::WeakRef<::Aero::DependencyObject>(
        Base::Ref<::Aero::DependencyObject>::FromBorrowed(*target));
    subscription.property = property;
    subscription.targetType = services.targetValueType;
    Base::Result<void> assignedKey = subscription.key.Assign(key);
    if (!assignedKey) return assignedKey.GetStatus();
    subscription.baseUri = english.Value();
    LocTargets().PushBack(std::move(subscription));
    return ProvidedValue::FromValue(std::move(initial).Value());
}

void LocExtension::OnSourceChanged(
    ::Aero::DependencyObject& object,
    const Meta::DependencyPropertyChangedEventArgs& args) noexcept {
    PruneExpiredLocTargets();
    const Meta::Value& source = args.GetNewValue();
    if (source.Type() != Meta::TypeOf<Base::ResourceUri>()) return;
    Base::Result<Base::ResourceUri> dictionary =
        Meta::ValueCodec<Base::ResourceUri>::Decode(source);
    if (!dictionary || dictionary.Value().Empty()) {
        object.ClearValue(LocExtensionToken::ResourcesProperty);
        return;
    }

    const Base::ResourceUri* fallbackBaseUri = nullptr;
    for (const LocTarget& target : LocTargets()) {
        Base::Ref<::Aero::DependencyObject> locked = target.object.Lock();
        if (!locked || target.baseUri.Empty()) continue;
        if (IsSameOrDescendant(locked.Get(), &object)) {
            fallbackBaseUri = &target.baseUri;
            break;
        }
        if (fallbackBaseUri == nullptr) {
            fallbackBaseUri = &target.baseUri;
        }
    }

    Base::ResourceUri resolved = dictionary.Value();
    if (!resolved.IsAbsolute() && fallbackBaseUri != nullptr) {
        Base::Result<Base::ResourceUri> relative =
            ResolveLocDictionary(fallbackBaseUri, resolved.Canonical());
        if (relative) {
            resolved = std::move(relative).Value();
        }
    }

    Base::Result<Base::Ref<Aero::ResourceDictionary>> loadedDict =
        LoadLocResourceDictionary(resolved, fallbackBaseUri);
    if (!loadedDict || !loadedDict.Value()) return;

    object.SetValue(LocExtensionToken::ResourcesProperty, loadedDict.Value());

    for (LocTarget& target : LocTargets()) {
        Base::Ref<::Aero::DependencyObject> locked = target.object.Lock();
        if (!locked || !target.property.IsValid()) continue;
        if (!IsSameOrDescendant(locked.Get(), &object) &&
            locked->GetValue(LocExtensionToken::ResourcesProperty)) {
            continue;
        }
        Base::Ref<Aero::ResourceDictionary> targetDict =
            locked->GetValue(LocExtensionToken::ResourcesProperty);
        if (!targetDict) {
            targetDict = loadedDict.Value();
        }
        Base::Result<Meta::Value> value = LookupLocValueInDictionary(
            *targetDict, target.key.View(), target.targetType);
        if (value) {
            locked->SetValue(target.property, value.Value());
        }
    }
}
} // namespace Aero::Markup


// ===== TypeExtension.inl =====
// ===== TypeExtension =====
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
    return schema.AddMarkupExtension({
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
        services.schema->ConvertText(
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

// ===== StaticExtension.inl =====
// ===== StaticExtension =====
namespace Aero::Markup {
namespace {
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
    return schema.AddMarkupExtension({
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

    const Base::StringView expression = TrimAscii(arguments);
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
    Base::StringView prefix;
    Base::StringView typeName;
    switch (SplitQualifiedXamlName(qualifiedType, prefix, typeName)) {
    case QualifiedNameSplitStatus::MultipleColons:
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "x:Static type has multiple namespace separators");
    case QualifiedNameSplitStatus::MalformedPrefix:
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "x:Static type name is invalid");
    case QualifiedNameSplitStatus::Ok:
        break;
    }

    Base::Result<Base::StringView> xamlNamespace =
        services.namespaces.Lookup(prefix);
    if (!xamlNamespace) return xamlNamespace.GetStatus();
    Base::Result<const Meta::TypeInfo*> type =
        services.schema->ResolveType(
            xamlNamespace.Value(),
            typeName);
    if (!type) return type.GetStatus();
    if (type->Kind() == Meta::MetadataTypeKind::Enum) {
        const Meta::EnumValueInfo* value =
            services.schema->Types().FindEnumValue(
                type->Id(), memberName);
        if (value == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "x:Static enum member was not found");
        }

        const bool signedEnum =
            (static_cast<std::uint32_t>(type->Flags()) &
             static_cast<std::uint32_t>(Meta::TypeFlags::SignedEnum)) != 0U;
        Meta::Value result = signedEnum
            ? Meta::Value::FromSignedInteger(
                  type->Id(),
                  static_cast<std::int64_t>(value->RawValue()))
            : Meta::Value::FromUnsignedInteger(
                  type->Id(),
                  value->RawValue());
        return ProvidedValue::FromValue(std::move(result));
    }

    if (services.targetValueType != Meta::InvalidTypeId &&
        services.schema->Types().IsAssignableFrom(
            services.targetValueType,
            Input::RoutedCommand::StaticTypeId())) {
        Base::Result<Base::Ref<Input::RoutedCommand>> command =
            Input::RoutedCommand::ResolveStatic(
                type->Id(), memberName);
        if (!command) return command.GetStatus();
        Meta::Value result = Meta::Value::FromObject(
            services.targetValueType,
            Base::Ref<Base::Object>(command.Value()));
        return ProvidedValue::FromValue(std::move(result));
    }

    return Base::Status::Failure(
        Base::ErrorCode::Unsupported,
        "x:Static member is not a registered enum or routed command");
}

Base::Result<void> RegisterBuiltinMarkupExtensions(
    Schema& schema) noexcept {
    ResourceExtension resourceExtension;
    Base::Result<void> status = resourceExtension.Register(schema);
    if (!status) return status;
    status = schema.AddMarkupExtension({
        Meta::MakeTypeId(Meta::AeroNamespaceUri(), Base::StringView("DynamicResource")),
        &DynamicResourceExtension::ProvideValue,
        nullptr});
    if (!status) return status;
    status = schema.AddMarkupExtension({
        Meta::MakeTypeId(Meta::AeroNamespaceUri(), Base::StringView("Binding")),
        &BindingExtension::ProvideValue,
        nullptr});
    if (!status) return status;
    status = schema.AddMarkupExtension({
        Meta::MakeTypeId(LanguageNamespaceUri(), Base::StringView("Static")),
        &StaticExtension::ProvideValue,
        nullptr});
    if (!status) return status;
    status = schema.AddMarkupExtension({
        Meta::MakeTypeId(Meta::AeroNamespaceUri(), Base::StringView("StaticResourceExtension")),
        &StaticResourceExtension::ProvideValue,
        nullptr});
    if (!status) return status;
    status = schema.AddMarkupExtension({
        Meta::MakeTypeId(LanguageNamespaceUri(), Base::StringView("Type")),
        &TypeExtension::ProvideValue,
        nullptr});
    if (!status) return status;
    status = schema.AddMarkupExtension({
        Meta::MakeTypeId(Meta::AeroNamespaceUri(), Base::StringView("Loc")),
        &LocExtension::ProvideValue,
        nullptr});
    if (!status) return status;
    status = schema.AddMarkupExtension({
        Meta::MakeTypeId(Meta::AeroNamespaceUri(), Base::StringView("TemplateBinding")),
        &TemplateBindingExtension::ProvideValue,
        nullptr});
    return status;
}
} // namespace Aero::Markup
