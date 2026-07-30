#include <Aero/Core/Property/EffectiveValueEngine.hpp>

#include <Aero/Base/Assert.hpp>

#include <cstdint>
#include <cstdio>
#include <utility>

namespace Aero::Core {
namespace {

constexpr std::uint32_t InvalidIndex = UINT32_MAX;

Base::Status InvalidProviderStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidArgument,
        "The provider is not a mutable value slot");
}

class FlushScope final {
public:
    explicit FlushScope(bool& flag) noexcept : flag_(&flag) {
        flag = true;
    }

    ~FlushScope() {
        *flag_ = false;
    }

    FlushScope(const FlushScope&) = delete;
    FlushScope& operator=(const FlushScope&) = delete;

private:
    bool* flag_ = nullptr;
};

} // namespace

EffectiveValueEngine::EffectiveValueEngine(
    Dispatcher& dispatcher,
    DependencyPropertyRegistry& registry) noexcept
    : dispatcher_(&dispatcher),
      registry_(&registry),
      entries_(),
      parents_(),
      inheritanceSubscriptions_(),
      inheritanceChangedHandler_(
          this,
          &EffectiveValueEngine::
              OnInheritancePropertyChanged) {}

EffectiveValueEngine::~EffectiveValueEngine() noexcept {
    if (phaseHook_.IsValid() && dispatcher_ != nullptr &&
        dispatcher_->CheckAccess()) {
        (void)dispatcher_->RemoveFrameHook(phaseHook_);
    }

    for (Entry& entry : entries_) {
        ReleaseExpression(entry.localExpression);
    }
    for (DependencyObject* object :
         inheritanceSubscriptions_) {
        if (object == nullptr) continue;
        for (const DependencyProperty& property :
             registry_->Properties()) {
            const PropertyMetadata* metadata =
                property.MetadataFor(
                    object->RuntimeType());
            if (metadata != nullptr &&
                HasFlag(
                    metadata->flags,
                    PropertyMetadataFlags::Inherits)) {
                static_cast<void>(
                    object->RemoveValueChangedHandler(
                        property.Handle(),
                        inheritanceChangedHandler_));
            }
        }
    }
}

Base::Result<void> EffectiveValueEngine::Initialize() noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) {
        return access.GetStatus();
    }
    if (phaseHook_.IsValid()) {
        return {};
    }
    if (!registry_->IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "DependencyPropertyRegistry must be frozen before the value engine");
    }

    Base::Result<DispatcherFrameHookHandle> hook =
        dispatcher_->RegisterFrameHook(
            DispatcherFramePhase::PropertyChanges,
            &EffectiveValueEngine::PropertyChangesHook,
            this,
            nullptr);
    if (!hook) {
        return hook.GetStatus();
    }
    phaseHook_ = hook.Value();
    return {};
}

Base::Result<void> EffectiveValueEngine::VerifyMutable() const noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) {
        return access.GetStatus();
    }
    if (!phaseHook_.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "EffectiveValueEngine is not initialized");
    }
    if (flushing_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Provider mutation is not allowed while values are being flushed");
    }
    return {};
}

std::uint32_t EffectiveValueEngine::FindEntryIndex(
    const DependencyObject& object,
    DependencyPropertyHandle property) const noexcept {
    for (std::uint32_t index = 0U; index < entries_.Size(); ++index) {
        if (entries_[index].object == &object &&
            entries_[index].property == property) {
            return index;
        }
    }
    return InvalidIndex;
}

Base::Result<std::uint32_t> EffectiveValueEngine::EnsureEntry(
    DependencyObject& object,
    DependencyPropertyHandle property) noexcept {
    if (&object.GetDispatcher() != dispatcher_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "DependencyObject belongs to another Dispatcher");
    }

    const DependencyProperty* registered = registry_->Find(property);
    if (registered == nullptr ||
        registered->MetadataFor(object.RuntimeType()) == nullptr) {
        thread_local char message[384];
        const TypeInfo* objectType =
            registry_->Types().FindType(
                object.RuntimeType());
        const Base::StringView propertyName =
            registered != nullptr
            ? registered->Name()
            : Base::StringView("<unknown>");
        const Base::StringView typeName =
            objectType != nullptr
            ? objectType->Name()
            : Base::StringView("<unknown>");
        std::snprintf(
            message,
            sizeof(message),
            "Dependency property '%.*s' does not apply to object type '%.*s'",
            static_cast<int>(
                propertyName.SizeBytes()),
            propertyName.Data(),
            static_cast<int>(
                typeName.SizeBytes()),
            typeName.Data());
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            message);
    }

    const std::uint32_t existing = FindEntryIndex(object, property);
    if (existing != InvalidIndex) {
        return existing;
    }

    if (entries_.Size() == UINT32_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Effective value entry limit reached");
    }

    Entry entry;
    entry.object = &object;
    entry.property = property;
    Base::Result<void> appended = entries_.TryPushBack(std::move(entry));
    if (!appended) {
        return appended.GetStatus();
    }
    return entries_.Size() - 1U;
}

std::uint32_t EffectiveValueEngine::FindParentIndex(
    const DependencyObject& child) const noexcept {
    for (std::uint32_t index = 0U; index < parents_.Size(); ++index) {
        if (parents_[index].child == &child) {
            return index;
        }
    }
    return InvalidIndex;
}

DependencyObject* EffectiveValueEngine::InheritanceParent(
    const DependencyObject& child) const noexcept {
    const std::uint32_t index = FindParentIndex(child);
    return index != InvalidIndex ? parents_[index].parent : nullptr;
}

Base::Result<void> EffectiveValueEngine::SetInheritanceParent(
    DependencyObject& child,
    DependencyObject* parent) noexcept {
    Base::Result<void> ready = VerifyMutable();
    if (!ready) {
        return ready.GetStatus();
    }
    if (&child.GetDispatcher() != dispatcher_ ||
        (parent != nullptr && &parent->GetDispatcher() != dispatcher_)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Inheritance objects must belong to the value engine Dispatcher");
    }
    if (parent == &child) {
        return Base::Status::Failure(
            Base::ErrorCode::CycleDetected,
            "An object cannot inherit from itself");
    }

    DependencyObject* cursor = parent;
    while (cursor != nullptr) {
        if (cursor == &child) {
            return Base::Status::Failure(
                Base::ErrorCode::CycleDetected,
                "Inheritance parent assignment would create a cycle");
        }
        cursor = InheritanceParent(*cursor);
    }

    const std::uint32_t existing = FindParentIndex(child);
    DependencyObject* previousParent =
        existing != InvalidIndex
        ? parents_[existing].parent
        : nullptr;
    if (parent != nullptr) {
        Base::Result<void> subscribed =
            EnsureInheritanceSubscription(child);
        if (!subscribed) return subscribed.GetStatus();
        subscribed =
            EnsureInheritanceSubscription(*parent);
        if (!subscribed) return subscribed.GetStatus();
    }
    if (parent == nullptr) {
        if (existing != InvalidIndex) {
            RemoveParent(existing);
        }
    } else if (existing != InvalidIndex) {
        parents_[existing].parent = parent;
    } else {
        Base::Result<void> appended =
            parents_.TryPushBack(ParentLink{&child, parent});
        if (!appended) {
            return appended.GetStatus();
        }
    }

    const auto participates =
        [this](const DependencyObject& object) noexcept {
            for (const ParentLink& link : parents_) {
                if (link.child == &object ||
                    link.parent == &object) {
                    return true;
                }
            }
            return false;
        };
    if (!participates(child)) {
        RemoveInheritanceSubscription(child);
    }
    if (previousParent != nullptr &&
        previousParent != parent &&
        !participates(*previousParent)) {
        RemoveInheritanceSubscription(
            *previousParent);
    }

    for (std::uint32_t index = 0U; index < entries_.Size(); ++index) {
        if (entries_[index].object == &child) {
            Base::Result<void> queued = QueueEntry(index);
            if (!queued) {
                return queued.GetStatus();
            }
        }
    }
    return {};
}

Base::Result<void> EffectiveValueEngine::SetProviderValue(
    DependencyObject& object,
    DependencyPropertyHandle property,
    EffectiveValueProvider provider,
    const PropertyValue& value) noexcept {
    Base::Result<void> ready = VerifyMutable();
    if (!ready) {
        return ready.GetStatus();
    }

    Base::Result<std::uint32_t> ensured = EnsureEntry(object, property);
    if (!ensured) {
        return ensured.GetStatus();
    }
    const std::uint32_t index = ensured.Value();

    ProviderSlot* slot = nullptr;
    switch (provider) {
    case EffectiveValueProvider::ThemeStyle:
        slot = &entries_[index].themeStyle;
        break;
    case EffectiveValueProvider::Style:
        slot = &entries_[index].style;
        break;
    case EffectiveValueProvider::Template:
        slot = &entries_[index].templated;
        break;
    case EffectiveValueProvider::Trigger:
        slot = &entries_[index].trigger;
        break;
    case EffectiveValueProvider::Animation:
        slot = &entries_[index].animation;
        break;
    default:
        return InvalidProviderStatus();
    }

    Base::Result<void> queued = QueueEntry(index);
    if (!queued) {
        return queued.GetStatus();
    }
    slot->value = value;
    slot->hasValue = true;
    return {};
}

Base::Result<void> EffectiveValueEngine::ClearProviderValue(
    DependencyObject& object,
    DependencyPropertyHandle property,
    EffectiveValueProvider provider) noexcept {
    Base::Result<void> ready = VerifyMutable();
    if (!ready) {
        return ready.GetStatus();
    }

    const std::uint32_t index = FindEntryIndex(object, property);
    if (index == InvalidIndex) {
        return {};
    }

    ProviderSlot* slot = nullptr;
    switch (provider) {
    case EffectiveValueProvider::ThemeStyle:
        slot = &entries_[index].themeStyle;
        break;
    case EffectiveValueProvider::Style:
        slot = &entries_[index].style;
        break;
    case EffectiveValueProvider::Template:
        slot = &entries_[index].templated;
        break;
    case EffectiveValueProvider::Trigger:
        slot = &entries_[index].trigger;
        break;
    case EffectiveValueProvider::Animation:
        slot = &entries_[index].animation;
        break;
    default:
        return InvalidProviderStatus();
    }

    if (!slot->hasValue) {
        return {};
    }
    Base::Result<void> queued = QueueEntry(index);
    if (!queued) {
        return queued.GetStatus();
    }
    slot->value = PropertyValue::Unset();
    slot->hasValue = false;
    return {};
}

Base::Result<void> EffectiveValueEngine::SetStyleValue(
    DependencyObject& object,
    DependencyPropertyHandle property,
    const PropertyValue& value) noexcept {
    return SetProviderValue(
        object, property, EffectiveValueProvider::Style, value);
}

Base::Result<void> EffectiveValueEngine::ClearStyleValue(
    DependencyObject& object,
    DependencyPropertyHandle property) noexcept {
    return ClearProviderValue(
        object, property, EffectiveValueProvider::Style);
}

Base::Result<void> EffectiveValueEngine::SetTemplateValue(
    DependencyObject& object,
    DependencyPropertyHandle property,
    const PropertyValue& value) noexcept {
    return SetProviderValue(
        object, property, EffectiveValueProvider::Template, value);
}

Base::Result<void> EffectiveValueEngine::ClearTemplateValue(
    DependencyObject& object,
    DependencyPropertyHandle property) noexcept {
    return ClearProviderValue(
        object, property, EffectiveValueProvider::Template);
}

Base::Result<void> EffectiveValueEngine::SetThemeStyleValue(
    DependencyObject& object,
    DependencyPropertyHandle property,
    const PropertyValue& value) noexcept {
    return SetProviderValue(
        object, property, EffectiveValueProvider::ThemeStyle, value);
}

Base::Result<void> EffectiveValueEngine::ClearThemeStyleValue(
    DependencyObject& object,
    DependencyPropertyHandle property) noexcept {
    return ClearProviderValue(
        object, property, EffectiveValueProvider::ThemeStyle);
}

Base::Result<void> EffectiveValueEngine::SetTriggerValue(
    DependencyObject& object,
    DependencyPropertyHandle property,
    const PropertyValue& value) noexcept {
    return SetProviderValue(
        object, property, EffectiveValueProvider::Trigger, value);
}

Base::Result<void> EffectiveValueEngine::ClearTriggerValue(
    DependencyObject& object,
    DependencyPropertyHandle property) noexcept {
    return ClearProviderValue(
        object, property, EffectiveValueProvider::Trigger);
}

Base::Result<void> EffectiveValueEngine::SetAnimationValue(
    DependencyObject& object,
    DependencyPropertyHandle property,
    const PropertyValue& value) noexcept {
    return SetProviderValue(
        object, property, EffectiveValueProvider::Animation, value);
}

Base::Result<void> EffectiveValueEngine::ClearAnimationValue(
    DependencyObject& object,
    DependencyPropertyHandle property) noexcept {
    return ClearProviderValue(
        object, property, EffectiveValueProvider::Animation);
}

Base::Result<void> EffectiveValueEngine::SetLocalExpression(
    DependencyObject& object,
    DependencyPropertyHandle property,
    const PropertyExpression& expression) noexcept {
    Base::Result<void> ready = VerifyMutable();
    if (!ready) {
        return ready.GetStatus();
    }
    if (!expression.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "A property expression requires an evaluate callback");
    }

    Base::Result<std::uint32_t> ensured = EnsureEntry(object, property);
    if (!ensured) {
        return ensured.GetStatus();
    }
    const std::uint32_t index = ensured.Value();
    Base::Result<void> queued = QueueEntry(index);
    if (!queued) {
        return queued.GetStatus();
    }

    PropertyExpression old;
    const bool hadOld = entries_[index].localExpression.hasExpression;
    if (hadOld) {
        old = entries_[index].localExpression.expression;
    }

    entries_[index].localExpression.expression = expression;
    entries_[index].localExpression.hasExpression = true;

    if (hadOld && old.cleanup != nullptr) {
        old.cleanup(old.context);
    }
    return {};
}

Base::Result<void> EffectiveValueEngine::ClearLocalExpression(
    DependencyObject& object,
    DependencyPropertyHandle property) noexcept {
    Base::Result<void> ready = VerifyMutable();
    if (!ready) {
        return ready.GetStatus();
    }

    const std::uint32_t index = FindEntryIndex(object, property);
    if (index == InvalidIndex ||
        !entries_[index].localExpression.hasExpression) {
        return {};
    }

    Base::Result<void> queued = QueueEntry(index);
    if (!queued) {
        return queued.GetStatus();
    }
    ReleaseExpression(entries_[index].localExpression);
    return {};
}

Base::Result<void> EffectiveValueEngine::Invalidate(
    DependencyObject& object,
    DependencyPropertyHandle property) noexcept {
    Base::Result<void> ready = VerifyMutable();
    if (!ready) {
        return ready.GetStatus();
    }

    Base::Result<std::uint32_t> ensured = EnsureEntry(object, property);
    if (!ensured) {
        return ensured.GetStatus();
    }
    Base::Result<void> queued = QueueEntry(ensured.Value());
    if (!queued) {
        return queued.GetStatus();
    }
    return QueueDescendants(object, property);
}

Base::Result<void> EffectiveValueEngine::QueueEntry(
    std::uint32_t index) noexcept {
    AERO_ASSERT(index < entries_.Size());
    Entry& entry = entries_[index];
    if (entry.queued) {
        return {};
    }
    if (nextQueueSequence_ == UINT64_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Effective value queue sequence limit reached");
    }
    entry.queued = true;
    entry.queueSequence = nextQueueSequence_++;
    return {};
}

Base::Result<void> EffectiveValueEngine::QueueDescendants(
    DependencyObject& parent,
    DependencyPropertyHandle property) noexcept {
    Base::Vector<DependencyObject*> frontier;
    Base::Result<void> root = frontier.TryPushBack(&parent);
    if (!root) {
        return root.GetStatus();
    }

    std::uint32_t cursor = 0U;
    while (cursor < frontier.Size()) {
        DependencyObject* current = frontier[cursor++];
        for (const ParentLink& link : parents_) {
            if (link.parent != current || link.child == nullptr) {
                continue;
            }
            const std::uint32_t entryIndex =
                FindEntryIndex(*link.child, property);
            if (entryIndex != InvalidIndex) {
                Base::Result<void> queued = QueueEntry(entryIndex);
                if (!queued) {
                    return queued.GetStatus();
                }
            }
            Base::Result<void> pushed = frontier.TryPushBack(link.child);
            if (!pushed) {
                return pushed.GetStatus();
            }
        }
    }
    return {};
}

Base::Result<void>
EffectiveValueEngine::EnsureInheritanceSubscription(
    DependencyObject& object) noexcept {
    for (DependencyObject* subscribed :
         inheritanceSubscriptions_) {
        if (subscribed == &object) return {};
    }
    for (const DependencyProperty& property :
         registry_->Properties()) {
        const PropertyMetadata* metadata =
            property.MetadataFor(object.RuntimeType());
        if (metadata == nullptr ||
            !HasFlag(
                metadata->flags,
                PropertyMetadataFlags::Inherits)) {
            continue;
        }
        Base::Result<void> added =
            object.TryAddValueChangedHandler(
                property.Handle(),
                inheritanceChangedHandler_);
        if (!added) {
            for (const DependencyProperty& rollback :
                 registry_->Properties()) {
                if (rollback.Handle() ==
                    property.Handle()) {
                    break;
                }
                const PropertyMetadata*
                    rollbackMetadata =
                        rollback.MetadataFor(
                            object.RuntimeType());
                if (rollbackMetadata != nullptr &&
                    HasFlag(
                        rollbackMetadata->flags,
                        PropertyMetadataFlags::
                            Inherits)) {
                    static_cast<void>(
                        object.RemoveValueChangedHandler(
                            rollback.Handle(),
                            inheritanceChangedHandler_));
                }
            }
            return added.GetStatus();
        }
        Base::Result<std::uint32_t> entry =
            EnsureEntry(object, property.Handle());
        if (!entry) {
            static_cast<void>(
                object.RemoveValueChangedHandler(
                    property.Handle(),
                    inheritanceChangedHandler_));
            return entry.GetStatus();
        }
        Base::Result<void> queued =
            QueueEntry(entry.Value());
        if (!queued) {
            static_cast<void>(
                object.RemoveValueChangedHandler(
                    property.Handle(),
                    inheritanceChangedHandler_));
            return queued.GetStatus();
        }
    }
    Base::Result<void> retained =
        inheritanceSubscriptions_.TryPushBack(
            &object);
    if (!retained) {
        for (const DependencyProperty& property :
             registry_->Properties()) {
            const PropertyMetadata* metadata =
                property.MetadataFor(
                    object.RuntimeType());
            if (metadata != nullptr &&
                HasFlag(
                    metadata->flags,
                    PropertyMetadataFlags::Inherits)) {
                static_cast<void>(
                    object.RemoveValueChangedHandler(
                        property.Handle(),
                        inheritanceChangedHandler_));
            }
        }
        return retained.GetStatus();
    }
    return {};
}

void EffectiveValueEngine::RemoveInheritanceSubscription(
    DependencyObject& object) noexcept {
    for (std::uint32_t index = 0U;
         index < inheritanceSubscriptions_.Size();
         ++index) {
        if (inheritanceSubscriptions_[index] !=
            &object) {
            continue;
        }
        for (const DependencyProperty& property :
             registry_->Properties()) {
            const PropertyMetadata* metadata =
                property.MetadataFor(
                    object.RuntimeType());
            if (metadata != nullptr &&
                HasFlag(
                    metadata->flags,
                    PropertyMetadataFlags::Inherits)) {
                static_cast<void>(
                    object.RemoveValueChangedHandler(
                        property.Handle(),
                        inheritanceChangedHandler_));
            }
        }
        for (std::uint32_t next = index + 1U;
             next < inheritanceSubscriptions_.Size();
             ++next) {
            inheritanceSubscriptions_[next - 1U] =
                inheritanceSubscriptions_[next];
        }
        inheritanceSubscriptions_.PopBack();
        return;
    }
}

void EffectiveValueEngine::OnInheritancePropertyChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs&
        args) noexcept {
    const DependencyProperty* property =
        registry_->Find(args.property);
    const PropertyMetadata* metadata =
        property != nullptr
        ? property->MetadataFor(
              object.RuntimeType())
        : nullptr;
    if (metadata == nullptr ||
        !HasFlag(
            metadata->flags,
            PropertyMetadataFlags::Inherits)) {
        return;
    }
    static_cast<void>(
        QueueDescendants(object, args.property));
}

Base::Result<EffectiveValueEngine::Resolution>
EffectiveValueEngine::Resolve(Entry& entry) noexcept {
    const DependencyProperty* property = registry_->Find(entry.property);
    if (property == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Dependency property is no longer registered");
    }
    const PropertyMetadata* metadata =
        property->MetadataFor(entry.object->RuntimeType());
    if (metadata == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Dependency property metadata is unavailable for the object");
    }

    Resolution resolution;
    if (entry.animation.hasValue) {
        resolution.value = entry.animation.value;
        resolution.provider = EffectiveValueProvider::Animation;
        return resolution;
    }

    if (entry.localExpression.hasExpression) {
        const PropertyExpression expression =
            entry.localExpression.expression;
        Base::Result<PropertyValue> evaluated = expression.evaluate(
            expression.context, *entry.object, entry.property);
        if (!evaluated) {
            return evaluated.GetStatus();
        }
        if (evaluated.Value().IsUnset()) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "A property expression returned Unset");
        }
        resolution.value = std::move(evaluated).Value();
        resolution.provider = EffectiveValueProvider::LocalExpression;
        resolution.expressionKind = expression.kind;
        resolution.hasExpression = true;
        return resolution;
    }

    Base::Result<PropertyValue> local =
        entry.object->ReadLocalValue(entry.property);
    if (!local) {
        return local.GetStatus();
    }
    if (!local.Value().IsUnset()) {
        resolution.value = std::move(local).Value();
        resolution.provider = EffectiveValueProvider::Local;
        return resolution;
    }

    if (entry.trigger.hasValue) {
        resolution.value = entry.trigger.value;
        resolution.provider = EffectiveValueProvider::Trigger;
        return resolution;
    }
    if (entry.templated.hasValue) {
        resolution.value = entry.templated.value;
        resolution.provider = EffectiveValueProvider::Template;
        return resolution;
    }
    if (entry.style.hasValue) {
        resolution.value = entry.style.value;
        resolution.provider = EffectiveValueProvider::Style;
        return resolution;
    }
    if (entry.themeStyle.hasValue) {
        resolution.value = entry.themeStyle.value;
        resolution.provider = EffectiveValueProvider::ThemeStyle;
        return resolution;
    }

    if (HasFlag(metadata->flags, PropertyMetadataFlags::Inherits)) {
        DependencyObject* parent = InheritanceParent(*entry.object);
        while (parent != nullptr &&
               property->MetadataFor(
                   parent->RuntimeType()) == nullptr) {
            parent = InheritanceParent(*parent);
        }
        if (parent != nullptr) {
            Base::Result<PropertyValue> inherited =
                parent->GetValue(entry.property);
            if (!inherited) {
                return inherited.GetStatus();
            }
            resolution.value = std::move(inherited).Value();
            resolution.provider = EffectiveValueProvider::Inherited;
            return resolution;
        }
    }

    resolution.value = metadata->defaultValue;
    resolution.provider = EffectiveValueProvider::Default;
    return resolution;
}

Base::Result<void> EffectiveValueEngine::Apply(
    Entry& entry,
    const Resolution& resolution) noexcept {
    Base::Result<void> applied =
        entry.object->SetCurrentValue(entry.property, resolution.value);
    if (!applied) {
        return applied.GetStatus();
    }

    entry.diagnostics.provider = resolution.provider;
    entry.diagnostics.expressionKind = resolution.expressionKind;
    entry.diagnostics.hasExpression = resolution.hasExpression;
    entry.diagnostics.isInherited =
        resolution.provider == EffectiveValueProvider::Inherited;
    entry.diagnostics.isAnimated =
        resolution.provider == EffectiveValueProvider::Animation;
    entry.diagnostics.revision = nextRevision_++;

    const DependencyProperty* property = registry_->Find(entry.property);
    const PropertyMetadata* metadata = property != nullptr
        ? property->MetadataFor(entry.object->RuntimeType())
        : nullptr;
    if (metadata != nullptr &&
        HasFlag(metadata->flags, PropertyMetadataFlags::Inherits)) {
        Base::Result<void> descendants =
            QueueDescendants(*entry.object, entry.property);
        if (!descendants) {
            return descendants.GetStatus();
        }
    }
    return {};
}

Base::Result<std::uint32_t> EffectiveValueEngine::Flush() noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) {
        return access.GetStatus();
    }
    if (!phaseHook_.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "EffectiveValueEngine is not initialized");
    }
    if (flushing_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Effective value flushing is already active");
    }

    FlushScope scope(flushing_);
    const std::uint64_t boundary = nextQueueSequence_ - 1U;
    std::uint32_t processed = 0U;

    while (true) {
        std::uint32_t selected = InvalidIndex;
        std::uint64_t selectedSequence = UINT64_MAX;
        for (std::uint32_t index = 0U; index < entries_.Size(); ++index) {
            const Entry& entry = entries_[index];
            if (entry.queued && entry.queueSequence <= boundary &&
                entry.queueSequence < selectedSequence) {
                selected = index;
                selectedSequence = entry.queueSequence;
            }
        }
        if (selected == InvalidIndex) {
            break;
        }

        Entry& entry = entries_[selected];
        entry.queued = false;

        Base::Result<Resolution> resolved = Resolve(entry);
        if (!resolved) {
            entry.queued = true;
            return resolved.GetStatus();
        }
        Base::Result<void> applied = Apply(entry, resolved.Value());
        if (!applied) {
            entry.queued = true;
            return applied.GetStatus();
        }
        ++processed;
    }
    return processed;
}

Base::Result<EffectiveValueDiagnostics> EffectiveValueEngine::Diagnostics(
    const DependencyObject& object,
    DependencyPropertyHandle property) const noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) {
        return access.GetStatus();
    }
    const std::uint32_t index = FindEntryIndex(object, property);
    if (index == InvalidIndex) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "The property is not tracked by the effective value engine");
    }
    return entries_[index].diagnostics;
}

Base::Result<void> EffectiveValueEngine::DetachObject(
    DependencyObject& object) noexcept {
    Base::Result<void> ready = VerifyMutable();
    if (!ready) {
        return ready.GetStatus();
    }

    std::uint32_t entry = 0U;
    while (entry < entries_.Size()) {
        if (entries_[entry].object == &object) {
            RemoveEntry(entry);
        } else {
            ++entry;
        }
    }

    std::uint32_t parent = 0U;
    while (parent < parents_.Size()) {
        if (parents_[parent].child == &object ||
            parents_[parent].parent == &object) {
            RemoveParent(parent);
        } else {
            ++parent;
        }
    }
    RemoveInheritanceSubscription(object);
    return {};
}

std::uint32_t EffectiveValueEngine::PendingPropertyCount() const noexcept {
    std::uint32_t count = 0U;
    for (const Entry& entry : entries_) {
        if (entry.queued) {
            ++count;
        }
    }
    return count;
}

void EffectiveValueEngine::ReleaseExpression(
    ExpressionSlot& slot) noexcept {
    if (!slot.hasExpression) {
        return;
    }
    const PropertyExpression expression = slot.expression;
    slot.expression = {};
    slot.hasExpression = false;
    if (expression.cleanup != nullptr) {
        expression.cleanup(expression.context);
    }
}

void EffectiveValueEngine::RemoveEntry(std::uint32_t index) noexcept {
    AERO_ASSERT(index < entries_.Size());
    ReleaseExpression(entries_[index].localExpression);
    for (std::uint32_t current = index + 1U;
         current < entries_.Size();
         ++current) {
        entries_[current - 1U] = std::move(entries_[current]);
    }
    entries_.PopBack();
}

void EffectiveValueEngine::RemoveParent(std::uint32_t index) noexcept {
    AERO_ASSERT(index < parents_.Size());
    for (std::uint32_t current = index + 1U;
         current < parents_.Size();
         ++current) {
        parents_[current - 1U] = parents_[current];
    }
    parents_.PopBack();
}

void EffectiveValueEngine::PropertyChangesHook(void* context) noexcept {
    auto* engine = static_cast<EffectiveValueEngine*>(context);
    (void)engine->Flush();
}

} // namespace Aero::Core
