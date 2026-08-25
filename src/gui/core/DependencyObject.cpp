// Auto-relocated base-class method definitions (WPF semantic kernel).
#include <Aero/DependencyObject.hpp>
#include <Aero/Base/Assert.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Allocator.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Events.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Transforms.hpp>
#include <Aero/Media/Effects.hpp>
#include <Aero/Markup/XamlReader.hpp>
#include <Aero/Controls.hpp>
#include <cstdio>
#include "gui/core/State.hpp" 
#include "gui/internal/PropertyStore.hpp"
#include <new>

#include "gui/input/InputState.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "gui/meta/MetadataState.hpp"

using namespace Aero;
using namespace Aero::Media;
using namespace Aero::Meta;
using namespace Aero::Threading;

namespace Aero {
namespace {

constexpr Base::Status ReadOnlyStatus() noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::ReadOnly,
        "Dependency property is read-only");
}

} // namespace

// from src/gui/core/PropertySystem.cpp

PropertyInvalidationFlags DependencyObject::AccumulateInvalidations(
    PropertyMetadataFlags metadataFlags) noexcept {
    PropertyInvalidationFlags change = PropertyInvalidationFlags::None;
    if (HasFlag(metadataFlags, PropertyMetadataFlags::AffectsMeasure)) {
        change |= PropertyInvalidationFlags::Measure;
    }
    if (HasFlag(metadataFlags, PropertyMetadataFlags::AffectsArrange)) {
        change |= PropertyInvalidationFlags::Arrange;
    }
    if (HasFlag(metadataFlags, PropertyMetadataFlags::AffectsRender)) {
        change |= PropertyInvalidationFlags::Render;
    }
    if (HasFlag(metadataFlags, PropertyMetadataFlags::Inherits)) {
        change |= PropertyInvalidationFlags::Inheritance;
    }
    if (HasFlag(metadataFlags, PropertyMetadataFlags::AffectsParentMeasure)) {
        change |= PropertyInvalidationFlags::ParentMeasure;
    }
    if (HasFlag(metadataFlags, PropertyMetadataFlags::AffectsParentArrange)) {
        change |= PropertyInvalidationFlags::ParentArrange;
    }
    invalidations_ |= change;
    return change;
}

// from src/gui/core/PropertySystem.cpp

void DependencyObject::NotifyValueChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    AERO_ASSERT(changeHandlerNotificationDepth_ != UINT32_MAX);
    ++changeHandlerNotificationDepth_;
    const std::uint32_t snapshotCount = changeHandlers_.Size();
    for (std::uint32_t index = 0U; index < snapshotCount; ++index) {
        if (index >= changeHandlers_.Size()) {
            break;
        }
        ChangeHandlerRecord& record = changeHandlers_[index];
        if (record.active && record.property == args.GetProperty()) {
            record.handler(*this, args);
        }
    }
    --changeHandlerNotificationDepth_;
    if (changeHandlerNotificationDepth_ != 0U) {
        return;
    }
    for (std::uint32_t index = 0U; index < changeHandlers_.Size();) {
        if (!changeHandlers_[index].active) {
            RemoveChangeHandler(index);
        } else {
            ++index;
        }
    }
}

// from src/gui/core/PropertySystem.cpp

void DependencyObject::RemoveChangeHandler(std::uint32_t index) noexcept {
    AERO_ASSERT(index < changeHandlers_.Size());
    for (std::uint32_t current = index + 1U;
         current < changeHandlers_.Size();
         ++current) {
        changeHandlers_[current - 1U] = std::move(changeHandlers_[current]);
    }
    changeHandlers_.PopBack();
}

// from src/gui/core/PropertySystem.cpp


// from src/gui/core/PropertySystem.cpp

Base::Result<void> DependencyObject::VerifyMutationAllowed() const noexcept {
    return {};
}

// from src/gui/core/PropertySystem.cpp

void DependencyObject::OnPropertyInvalidated(
    PropertyInvalidationFlags) noexcept {
}

void DependencyObject::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs&) noexcept {
}

// from src/gui/core/PropertySystem.cpp


Base::Result<void> DependencyObject::ApplyChange(
    DependencyPropertyHandle propertyHandle, const DependencyPropertyKey* key,
    ChangeKind kind, const PropertyValue* requestedValue) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    const Meta::DependencyProperty* property =
        registry_->Find(propertyHandle);
    if (property == nullptr) return Base::Status::Failure(
        Base::ErrorCode::NotFound, "Dependency property was not found");
    propertyHandle = property->Handle();
    const PropertyMetadata* metadata = property->MetadataFor(runtimeType_);
    if (metadata == nullptr) return Base::Status::Failure(
        Base::ErrorCode::NotFound, "Dependency property does not apply to this object type");
    if (kind != ChangeKind::ReCoerce && property->GetIsReadOnly() &&
        !registry_->ValidateKey(propertyHandle, key)) return ReadOnlyStatus();
    if ((kind == ChangeKind::SetLocal || kind == ChangeKind::SetCurrent) &&
        requestedValue == nullptr) return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Dependency property set operation requires a value");
    Base::Result<MutationScope> mutationResult = BeginMutation(propertyHandle);
    if (!mutationResult) return mutationResult.GetStatus();
    MutationScope mutation = std::move(mutationResult).Value();
    StoredValueEntry* storedEntry = FindStoredEntry(propertyHandle);
    const bool hadEntry = storedEntry != nullptr;
    if (!hadEntry && kind == ChangeKind::Clear) return {};
    if (!hadEntry) {
        Base::Result<StoredValueEntry*> ensured = EnsureStoredEntry(propertyHandle);
        if (!ensured) return ensured.GetStatus();
        storedEntry = ensured.Value();
    }
    StoredValueEntry& entry = *storedEntry;
    const PropertyValue oldEffective = hadEntry ? entry.effectiveValue : metadata->defaultValue;
    const PropertyValueSourceInfo oldSourceInfo = hadEntry
        ? entry.sourceInfo : PropertyValueSourceInfo{};
    const std::uint64_t oldRevision = oldSourceInfo.revision;
    const PropertyValue oldLocal = entry.localValue;
    const PropertyValue oldCurrent = entry.currentValue;
    const PropertyExpression oldExpression = entry.localExpression;
    const bool oldHasLocal = entry.hasLocal;
    const bool oldHasCurrent = entry.hasCurrent;
    const bool oldHasExpression = entry.hasExpression;
    const bool removesExpression = oldHasExpression &&
        (kind == ChangeKind::SetLocal || kind == ChangeKind::Clear);
    switch (kind) {
    case ChangeKind::SetLocal:
        entry.localExpression = {}; entry.hasExpression = false;
        entry.localValue = *requestedValue; entry.hasLocal = true;
        entry.currentValue = PropertyValue::Unset(); entry.hasCurrent = false; break;
    case ChangeKind::SetCurrent:
        entry.currentValue = *requestedValue; entry.hasCurrent = true; break;
    case ChangeKind::Clear:
        entry.localExpression = {}; entry.hasExpression = false;
        entry.localValue = PropertyValue::Unset(); entry.currentValue = PropertyValue::Unset();
        entry.hasLocal = false; entry.hasCurrent = false; break;
    case ChangeKind::ReCoerce: break;
    }
    Base::Result<void> recomputed = RecomputeEffectiveValueCore(
        propertyHandle, *property, *metadata, oldEffective, oldSourceInfo);
    if (recomputed) {
        if (removesExpression && oldExpression.cleanup != nullptr) {
            oldExpression.cleanup(oldExpression.context);
        }
        return {};
    }
        storedEntry = FindStoredEntry(propertyHandle);
    const bool committed = storedEntry != nullptr &&
        storedEntry->sourceInfo.revision != oldRevision;
    if (committed) {
        if (removesExpression && oldExpression.cleanup != nullptr) {
            oldExpression.cleanup(oldExpression.context);
        }
        return recomputed.GetStatus();
    }
    if (storedEntry != nullptr) {
        storedEntry->localValue = oldLocal;
        storedEntry->currentValue = oldCurrent;
        storedEntry->localExpression = oldExpression;
        storedEntry->hasLocal = oldHasLocal;
        storedEntry->hasCurrent = oldHasCurrent;
        storedEntry->hasExpression = oldHasExpression;
        if (!hadEntry) RemoveStoredEntry(CanonicalPropertyKey(propertyHandle));
    }
    return recomputed.GetStatus();
}

// from src/gui/core/PropertySystem.cpp

Base::Result<void> DependencyObject::DropEngineValueStateInternal(
    DependencyPropertyHandle propertyHandle) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    Base::Result<void> writable = VerifyMutationAllowed();
    if (!writable) return writable.GetStatus();
        auto* storedEntry = FindStoredEntry(propertyHandle);
    if (storedEntry == nullptr) return {};
    StoredValueEntry& entry = *storedEntry;
    entry.baseProviders.Clear();
    ReleaseExpression(entry);
    entry.inheritedValue = PropertyValue::Unset();
    entry.animationValue = PropertyValue::Unset();
    entry.currentValue = PropertyValue::Unset();
    entry.hasInherited = false;
    entry.hasAnimation = false;
    entry.hasCurrent = false;
    return RecomputeEffectiveValueInternal(propertyHandle);
}

// from src/gui/core/PropertySystem.cpp

Base::Result<void> DependencyObject::RecomputeEffectiveValueInternal(
    DependencyPropertyHandle propertyHandle) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    const Meta::DependencyProperty* property =
        registry_->Find(propertyHandle);
    const PropertyMetadata* metadata = property != nullptr
        ? property->MetadataFor(runtimeType_) : nullptr;
    if (property == nullptr || metadata == nullptr) return Base::Status::Failure(
        Base::ErrorCode::NotFound, "Dependency property does not apply to this object type");
    propertyHandle = property->Handle();
    Base::Result<MutationScope> mutationResult = BeginMutation(propertyHandle);
    if (!mutationResult) return mutationResult.GetStatus();
    MutationScope mutation = std::move(mutationResult).Value();
        auto* storedEntry = FindStoredEntry(propertyHandle);
    const PropertyValue oldEffective = storedEntry != nullptr
        ? storedEntry->effectiveValue : metadata->defaultValue;
    const PropertyValueSourceInfo oldSourceInfo = storedEntry != nullptr
        ? storedEntry->sourceInfo : PropertyValueSourceInfo{};
    return RecomputeEffectiveValueCore(propertyHandle, *property, *metadata,
        oldEffective, oldSourceInfo);
}

// from src/gui/core/PropertySystem.cpp

Base::Result<void> DependencyObject::RecomputeEffectiveValueCore(
    DependencyPropertyHandle propertyHandle,
    const Meta::DependencyProperty& property,
    const PropertyMetadata& metadata, const PropertyValue& oldEffective,
    const PropertyValueSourceInfo& oldSourceInfo) noexcept {
    StoredValueEntry* storedEntry = FindStoredEntry(propertyHandle);
    if (storedEntry == nullptr) {
        Base::Result<StoredValueEntry*> ensured = EnsureStoredEntry(propertyHandle);
        if (!ensured) return ensured.GetStatus();
        storedEntry = ensured.Value();
    }
    const StoredValueEntry& stored = *storedEntry;
    const bool hasExpression = stored.hasExpression;
    const PropertyExpression expression = stored.localExpression;
    const bool hasLocal = stored.hasLocal;
    const PropertyValue localValue = stored.localValue;
    const bool hasCurrent = stored.hasCurrent;
    const PropertyValue currentValue = stored.currentValue;
    const bool hasInherited = stored.hasInherited;
    const PropertyValue inheritedValue = stored.inheritedValue;
    const bool hasAnimation = stored.hasAnimation;
    const PropertyValue animationValue = stored.animationValue;
    bool hasProvider = false;
    PropertyProviderToken providerToken;
    PropertyValue providerValue;
    const PropertyProviderContribution* provider = stored.baseProviders.Winner();
    if (provider != nullptr) { hasProvider = true; providerToken = provider->token; providerValue = provider->value; }

    PropertyValue baseValue;
    PropertyValueSourceInfo source;
    if (hasProvider && providerToken.rank > PropertyValueRank::Local) {
        baseValue = providerValue;
        source.rank = providerToken.rank;
        source.token = providerToken;
    } else if (hasExpression) {
        Base::Result<PropertyValue> evaluated = expression.evaluate(
            expression.context, *this, propertyHandle);
        if (!evaluated) return evaluated.GetStatus();
        if (evaluated.Value().IsUnset()) return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed, "A property expression returned Unset");
        baseValue = std::move(evaluated).Value();
        source.rank = PropertyValueRank::LocalExpression;
        source.token = {PropertyValueRank::Local, LocalValueProviderOrigin, 1U};
        source.expressionKind = expression.kind;
        source.hasExpression = true;
    } else if (hasLocal) {
        baseValue = localValue;
        source.rank = PropertyValueRank::Local;
        source.token = {PropertyValueRank::Local, LocalValueProviderOrigin, 0U};
    } else if (hasProvider) {
        baseValue = providerValue;
        source.rank = providerToken.rank;
        source.token = providerToken;
    } else if (hasInherited) {
        baseValue = inheritedValue;
        source.rank = PropertyValueRank::Inherited;
        source.isInherited = true;
    } else {
        baseValue = metadata.defaultValue;
        source.rank = PropertyValueRank::Default;
    }
    if (hasCurrent) { baseValue = currentValue; source.isCurrentValue = true; }
    PropertyValue candidate = hasAnimation ? animationValue : baseValue;
    if (hasAnimation) {
        source.rank = PropertyValueRank::Animation;
        source.token = {PropertyValueRank::Animation, AnimationValueProviderOrigin, 0U};
        source.isAnimated = true;
    }
    Base::Result<PropertyValue> evaluated = registry_->EvaluateValue(
        *this, property, metadata, candidate);
    if (!evaluated) return evaluated.GetStatus();
    PropertyValue newEffective = std::move(evaluated).Value();
    source.isCoerced = newEffective != candidate;
    if (nextValueRevision_ == UINT64_MAX) return Base::Status::Failure(
        Base::ErrorCode::OutOfRange, "Dependency property value revision limit reached");
    if (newEffective != oldEffective) {
        Base::Result<void> consumerPrepared =
            AeroGuiInternal::PrepareConsumerChange(
                *this,
                propertyHandle,
                oldEffective,
                newEffective);
        if (!consumerPrepared) return consumerPrepared.GetStatus();
    }
    source.revision = nextValueRevision_++;
    storedEntry = FindStoredEntry(propertyHandle);
    if (storedEntry == nullptr) return Base::Status::Failure(Base::ErrorCode::InternalError,
        "Dependency property entry disappeared during evaluation");
    StoredValueEntry& entry = *storedEntry;
    entry.baseValue = baseValue;
    entry.effectiveValue = newEffective;
    entry.sourceInfo = source;
    if (newEffective != oldEffective) {
        AeroGuiInternal::CommitConsumerChange(
            *this,
            propertyHandle,
            oldEffective,
            newEffective);
    }
    const EffectiveValueSource oldSource = ToLegacySource(oldSourceInfo);
    const EffectiveValueSource newSource = ToLegacySource(source);
    if (newEffective != oldEffective) {
        const PropertyInvalidationFlags flags = AccumulateInvalidations(metadata.flags);
        const DependencyPropertyChangedEventArgs args{
            propertyHandle,
            oldEffective,
            newEffective,
            oldSource,
            newSource};
        if (!metadata.changed.Empty()) metadata.changed(*this, args);
        NotifyValueChanged(args);
        OnPropertyChanged(args);
        OnPropertyInvalidated(flags);
        storedEntry = FindStoredEntry(propertyHandle);
    }
    if (storedEntry != nullptr) {
        const StoredValueEntry& finalEntry = *storedEntry;
        const bool shouldStore = finalEntry.hasLocal || finalEntry.hasCurrent ||
            finalEntry.hasExpression || finalEntry.hasInherited || finalEntry.hasAnimation ||
            !finalEntry.baseProviders.GetIsEmpty() ||
            finalEntry.effectiveValue != metadata.defaultValue;
        if (!shouldStore) RemoveStoredEntry(CanonicalPropertyKey(propertyHandle));
    }
    return {};
}

// from src/gui/core/PropertySystem.cpp

Base::Result<void> DependencyObject::ApplyInheritedValueInternal(
    DependencyPropertyHandle property, const PropertyValue* value) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    Base::Result<void> writable = VerifyMutationAllowed();
    if (!writable) return writable.GetStatus();
    StoredValueEntry* storedEntry = FindStoredEntry(property);
    if (storedEntry == nullptr && value == nullptr) return {};
    if (storedEntry == nullptr) {
        Base::Result<StoredValueEntry*> ensured = EnsureStoredEntry(property);
        if (!ensured) return ensured.GetStatus();
        storedEntry = ensured.Value();
    }
    StoredValueEntry& entry = *storedEntry;
    const bool changed = value == nullptr ? entry.hasInherited
        : (!entry.hasInherited || entry.inheritedValue != *value);
    if (value == nullptr) { entry.inheritedValue = PropertyValue::Unset(); entry.hasInherited = false; }
    else { entry.inheritedValue = *value; entry.hasInherited = true; }
    if (changed) { entry.currentValue = PropertyValue::Unset(); entry.hasCurrent = false; }
    return {};
}

// from src/gui/core/PropertySystem.cpp

Base::Result<bool> DependencyObject::ClearAnimationValueInternal(
    DependencyPropertyHandle property) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    Base::Result<void> writable = VerifyMutationAllowed();
    if (!writable) return writable.GetStatus();
        auto* storedEntry = FindStoredEntry(property);
    if (storedEntry == nullptr || !storedEntry->hasAnimation) return false;
    storedEntry->animationValue = PropertyValue::Unset();
    storedEntry->hasAnimation = false;
    return true;
}

// from src/gui/core/PropertySystem.cpp

Base::Result<void> DependencyObject::ApplyAnimationValueInternal(
    DependencyPropertyHandle property, const PropertyValue& value) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    Base::Result<void> writable = VerifyMutationAllowed();
    if (!writable) return writable.GetStatus();
    if (value.IsUnset()) return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
        "Animation values cannot be Unset");
    Base::Result<StoredValueEntry*> ensured = EnsureStoredEntry(property);
    if (!ensured) return ensured.GetStatus();
    ensured.Value()->animationValue = value;
    ensured.Value()->hasAnimation = true;
    return {};
}

// from src/gui/core/PropertySystem.cpp

Base::Result<bool> DependencyObject::InvalidateBaseValueInternal(
    DependencyPropertyHandle property) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    Base::Result<void> writable = VerifyMutationAllowed();
    if (!writable) return writable.GetStatus();
        auto* storedEntry = FindStoredEntry(property);
    if (storedEntry == nullptr || !storedEntry->hasCurrent) return false;
    storedEntry->currentValue = PropertyValue::Unset();
    storedEntry->hasCurrent = false;
    return true;
}

// from src/gui/core/PropertySystem.cpp

Base::Result<bool> DependencyObject::ClearLocalExpressionInternal(
    DependencyPropertyHandle property) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    Base::Result<void> writable = VerifyMutationAllowed();
    if (!writable) return writable.GetStatus();
        auto* storedEntry = FindStoredEntry(property);
    if (storedEntry == nullptr || !storedEntry->hasExpression) return false;
    ReleaseExpression(*const_cast<StoredValueEntry*>(storedEntry));
    storedEntry->currentValue = PropertyValue::Unset();
    storedEntry->hasCurrent = false;
    return true;
}

// from src/gui/core/PropertySystem.cpp

Base::Result<void> DependencyObject::ApplyLocalExpressionInternal(
    DependencyPropertyHandle property, const PropertyExpression& expression) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    Base::Result<void> writable = VerifyMutationAllowed();
    if (!writable) return writable.GetStatus();
    if (!expression.IsValid()) return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
        "A property expression requires an evaluate callback");
    Base::Result<StoredValueEntry*> ensured = EnsureStoredEntry(property);
    if (!ensured) return ensured.GetStatus();
    StoredValueEntry& entry = *ensured.Value();
    PropertyExpression old;
    const bool hadOld = entry.hasExpression;
    if (hadOld) old = entry.localExpression;
    entry.localExpression = expression;
    entry.hasExpression = true;
    entry.localValue = PropertyValue::Unset();
    entry.hasLocal = false;
    entry.currentValue = PropertyValue::Unset();
    entry.hasCurrent = false;
    if (hadOld && old.cleanup != nullptr) old.cleanup(old.context);
    return {};
}

// from src/gui/core/PropertySystem.cpp

Base::Result<bool> DependencyObject::ClearProviderOriginInternal(
    DependencyPropertyHandle property, std::uint32_t origin) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    Base::Result<void> writable = VerifyMutationAllowed();
    if (!writable) return writable.GetStatus();
        auto* storedEntry = FindStoredEntry(property);
    if (storedEntry == nullptr) return false;
    const bool removed = storedEntry->baseProviders.RemoveOrigin(origin) != 0U;
    if (removed) { storedEntry->currentValue = PropertyValue::Unset(); storedEntry->hasCurrent = false; }
    return removed;
}

// from src/gui/core/PropertySystem.cpp

Base::Result<bool> DependencyObject::ClearProviderContributionInternal(
    DependencyPropertyHandle property, PropertyProviderToken token) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    Base::Result<void> writable = VerifyMutationAllowed();
    if (!writable) return writable.GetStatus();
        auto* storedEntry = FindStoredEntry(property);
    if (storedEntry == nullptr) return false;
    const bool removed = storedEntry->baseProviders.Remove(token);
    if (removed) { storedEntry->currentValue = PropertyValue::Unset(); storedEntry->hasCurrent = false; }
    return removed;
}

// from src/gui/core/PropertySystem.cpp

Base::Result<void> DependencyObject::ApplyProviderContributionInternal(
    DependencyPropertyHandle property, PropertyProviderToken token,
    const PropertyValue& value) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    Base::Result<void> writable = VerifyMutationAllowed();
    if (!writable) return writable.GetStatus();
    Base::Result<void> valid = registry_->ValidateValueFor(property, runtimeType_, value);
    if (!valid) return valid.GetStatus();
    Base::Result<StoredValueEntry*> ensured = EnsureStoredEntry(property);
    if (!ensured) return ensured.GetStatus();
    StoredValueEntry& entry = *ensured.Value();
    entry.currentValue = PropertyValue::Unset();
    entry.hasCurrent = false;
    if (!entry.baseProviders.Set(token, value)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "A property contribution requires a valid token and value");
    }
    return {};
}

// from src/gui/core/PropertySystem.cpp

void DependencyObject::ReleaseExpression(StoredValueEntry& entry) noexcept {
    if (!entry.hasExpression) return;
    const PropertyExpression expression = entry.localExpression;
    entry.localExpression = {};
    entry.hasExpression = false;
    if (expression.cleanup != nullptr) expression.cleanup(expression.context);
}

// from src/gui/core/PropertySystem.cpp

EffectiveValueSource DependencyObject::ToLegacySource(
    const PropertyValueSourceInfo& source) noexcept {
    if (source.isCurrentValue) return EffectiveValueSource::Current;
    if (source.rank == PropertyValueRank::Default) return EffectiveValueSource::Default;
    if (source.rank == PropertyValueRank::Local ||
        source.rank == PropertyValueRank::LocalExpression ||
        source.rank == PropertyValueRank::VisualState) {
        return EffectiveValueSource::Local;
    }
    return EffectiveValueSource::Current;
}

// from src/gui/core/PropertySystem.cpp



// from src/gui/core/PropertySystem.cpp

void DependencyObject::LeaveMutation() noexcept {
    AERO_ASSERT(!updateStack_.Empty());
    updateStack_.PopBack();
}

// from src/gui/core/PropertySystem.cpp

Base::Result<DependencyObject::MutationScope>
DependencyObject::BeginMutation(
    DependencyPropertyHandle property) noexcept {
    Base::Result<void> writable = VerifyMutationAllowed();
    if (!writable) {
        return writable.GetStatus();
    }
    for (MemberId active : updateStack_) {
        if (active == property.value) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Recursive mutation of the same dependency property is not allowed");
        }
    }

    Base::Result<void> pushed = updateStack_.PushBack(property.value);
    if (!pushed) {
        return pushed.GetStatus();
    }

    Base::Result<DispatcherReentrancyGuard> guard =
        GetDispatcher().EnterReentrancyGuard();
    if (!guard) {
        updateStack_.PopBack();
        return guard.GetStatus();
    }

    return MutationScope(this, std::move(guard).Value());
}

// from src/gui/core/PropertySystem.cpp

PropertyInvalidationFlags DependencyObject::TakeInvalidations() noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) {
        return PropertyInvalidationFlags::None;
    }
    const PropertyInvalidationFlags result = invalidations_;
    invalidations_ = PropertyInvalidationFlags::None;
    return result;
}

// from src/gui/core/PropertySystem.cpp

bool DependencyObject::RemoveValueChangedHandler(
    DependencyPropertyHandle property,
    const DependencyPropertyChangedEventHandler& handler) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) {
        return false;
    }
    if (!property.IsValid() || handler.Empty()) {
        return false;
    }
    const Meta::DependencyProperty* descriptor =
        registry_ != nullptr ? registry_->Find(property) : nullptr;
    if (descriptor != nullptr) {
        property = descriptor->Handle();
    }
    for (std::uint32_t count = changeHandlers_.Size(); count > 0U; --count) {
        const std::uint32_t index = count - 1U;
        ChangeHandlerRecord& record = changeHandlers_[index];
        if (record.property != property || record.handler != handler ||
            !record.active) {
            continue;
        }
        if (changeHandlerNotificationDepth_ != 0U) {
            record.active = false;
        } else {
            RemoveChangeHandler(index);
        }
        return true;
    }
    return false;
}

// from src/gui/core/PropertySystem.cpp

void DependencyObject::AddValueChangedHandler(
    DependencyPropertyHandle property,
    const DependencyPropertyChangedEventHandler& handler) noexcept {
    Base::Result<void> added =
        AddValueChangedHandlerChecked(property, handler);
    if (!added) {
        Base::ReportOutOfMemory(
            sizeof(DependencyPropertyChangedEventHandler),
            alignof(DependencyPropertyChangedEventHandler),
            Base::MemoryTag::General);
    }
}

// from src/gui/core/PropertySystem.cpp

Base::Result<void> DependencyObject::AddValueChangedHandlerChecked(
    DependencyPropertyHandle property,
    const DependencyPropertyChangedEventHandler& handler) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) {
        return ready.GetStatus();
    }
    const Meta::DependencyProperty* descriptor =
        registry_->Find(property);
    if (!property.IsValid() || handler.Empty() ||
        descriptor == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Dependency property change handler registration is invalid");
    }
    property = descriptor->Handle();
    ChangeHandlerRecord record;
    record.property = property;
    record.handler = handler;
    record.active = true;
    return changeHandlers_.PushBack(std::move(record));
}

// from src/gui/core/PropertySystem.cpp

Base::Result<void> DependencyObject::CoerceValueChecked(
    DependencyPropertyHandle property) noexcept {
    return ApplyChange(property, nullptr, ChangeKind::ReCoerce, nullptr);
}

// from src/gui/core/PropertySystem.cpp

void DependencyObject::CoerceValue(
    DependencyPropertyHandle property) noexcept {
    static_cast<void>(CoerceValueChecked(property));
}

// from src/gui/core/PropertySystem.cpp

Base::Result<void> DependencyObject::ClearValueChecked(
    const DependencyPropertyKey& key) noexcept {
    return ApplyChange(key.Property(), &key, ChangeKind::Clear, nullptr);
}

// from src/gui/core/PropertySystem.cpp

void DependencyObject::ClearValue(
    const DependencyPropertyKey& key) noexcept {
    static_cast<void>(ClearValueChecked(key));
}

// from src/gui/core/PropertySystem.cpp

Base::Result<void> DependencyObject::ClearValueChecked(
    DependencyPropertyHandle property) noexcept {
    return ApplyChange(property, nullptr, ChangeKind::Clear, nullptr);
}

// from src/gui/core/PropertySystem.cpp

void DependencyObject::ClearValue(
    DependencyPropertyHandle property) noexcept {
    static_cast<void>(ClearValueChecked(property));
}

// from src/gui/core/PropertySystem.cpp

void DependencyObject::SetReadOnlyCurrentValue(
    DependencyPropertyHandle propertyHandle,
    const PropertyValue& value) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return;
    const Meta::DependencyProperty* property =
        registry_->Find(propertyHandle);
    if (property == nullptr) {
        return;
    }
    if (!property->GetIsReadOnly()) {
        return;
    }
    DependencyPropertyKey key;
    key.registry_ = registry_;
    key.property_ = propertyHandle;
    key.secret_ = property->readOnlySecret_;
    (void)ApplyChange(
        propertyHandle, &key, ChangeKind::SetLocal, &value);
}

// from src/gui/core/PropertySystem.cpp

Base::Result<void> DependencyObject::SetTemplateValueChecked(
    DependencyPropertyHandle property,
    const PropertyValue& value) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    Base::Result<void> writable = VerifyMutationAllowed();
    if (!writable) return writable.GetStatus();
    const Meta::DependencyProperty* registered = registry_->Find(property);
    if (registered == nullptr) return Base::Status::Failure(Base::ErrorCode::NotFound, "Dependency property is not registered");
    const PropertyMetadata* metadata = registered->MetadataFor(runtimeType_);
    if (metadata == nullptr) return Base::Status::Failure(Base::ErrorCode::NotFound, "Dependency property metadata is not registered for type");

    StoredValueEntry* storedEntry = FindStoredEntry(property);
    const PropertyValue oldEffective = storedEntry != nullptr ? storedEntry->effectiveValue : metadata->defaultValue;
    const PropertyValueSourceInfo oldSourceInfo = storedEntry != nullptr ? storedEntry->sourceInfo : PropertyValueSourceInfo{};

    Base::Result<void> contribution = ApplyProviderContributionInternal(
        property,
        PropertyProviderToken{PropertyValueRank::TemplatedParentSetter, FirstCanonicalProviderOrigin, 0U},
        value);
    if (!contribution) return contribution.GetStatus();

    return RecomputeEffectiveValueCore(
        property, *registered, *metadata, oldEffective, oldSourceInfo);
}

// from src/gui/core/PropertySystem.cpp

void DependencyObject::SetTemplateValue(
    DependencyPropertyHandle property,
    const PropertyValue& value) noexcept {
    static_cast<void>(SetTemplateValueChecked(property, value));
}

// from src/gui/core/PropertySystem.cpp

Base::Result<void> DependencyObject::SetCurrentValueChecked(
    const DependencyPropertyKey& key,
    const PropertyValue& value) noexcept {
    return ApplyChange(key.Property(), &key, ChangeKind::SetCurrent, &value);
}

// from src/gui/core/PropertySystem.cpp

void DependencyObject::SetCurrentValue(
    const DependencyPropertyKey& key,
    const PropertyValue& value) noexcept {
    static_cast<void>(SetCurrentValueChecked(key, value));
}

// from src/gui/core/PropertySystem.cpp

Base::Result<void> DependencyObject::SetCurrentValueChecked(
    DependencyPropertyHandle property,
    const PropertyValue& value) noexcept {
    return ApplyChange(property, nullptr, ChangeKind::SetCurrent, &value);
}

// from src/gui/core/PropertySystem.cpp

void DependencyObject::SetCurrentValue(
    DependencyPropertyHandle property,
    const PropertyValue& value) noexcept {
    static_cast<void>(SetCurrentValueChecked(property, value));
}

// from src/gui/core/PropertySystem.cpp

Base::Result<void> DependencyObject::SetValueChecked(
    const DependencyPropertyKey& key,
    const PropertyValue& value) noexcept {
    return ApplyChange(key.Property(), &key, ChangeKind::SetLocal, &value);
}

// from src/gui/core/PropertySystem.cpp

void DependencyObject::SetValue(
    const DependencyPropertyKey& key,
    const PropertyValue& value) noexcept {
    static_cast<void>(SetValueChecked(key, value));
}

// from src/gui/core/PropertySystem.cpp

Base::Result<void> DependencyObject::SetValueChecked(
    DependencyPropertyHandle property,
    const PropertyValue& value) noexcept {
    return ApplyChange(property, nullptr, ChangeKind::SetLocal, &value);
}

// from src/gui/core/PropertySystem.cpp

void DependencyObject::SetValue(
    DependencyPropertyHandle property,
    const PropertyValue& value) noexcept {
    static_cast<void>(SetValueChecked(property, value));
}

// from src/gui/core/PropertySystem.cpp


PropertyValueSourceInfo DependencyObject::GetValueSourceInfo(
    DependencyPropertyHandle propertyHandle) const noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return {};
    const Meta::DependencyProperty* property =
        registry_->Find(propertyHandle);
    if (property == nullptr || property->MetadataFor(runtimeType_) == nullptr)
        return {};
        auto* storedEntry = FindStoredEntry(propertyHandle);
    return storedEntry != nullptr ? storedEntry->sourceInfo : PropertyValueSourceInfo{};
}

// from src/gui/core/PropertySystem.cpp

EffectiveValueSource DependencyObject::GetValueSource(
    DependencyPropertyHandle propertyHandle) const noexcept {
    const PropertyValueSourceInfo source = GetValueSourceInfo(propertyHandle);
    return ToLegacySource(source);
}

// from src/gui/core/PropertySystem.cpp

PropertyValue DependencyObject::ReadLocalValue(
    DependencyPropertyHandle propertyHandle) const noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) {
        return PropertyValue::Unset();
    }

    const Meta::DependencyProperty* property =
        registry_->Find(propertyHandle);
    if (property == nullptr || property->MetadataFor(runtimeType_) == nullptr) {
        return PropertyValue::Unset();
    }

        auto* storedEntry = FindStoredEntry(propertyHandle);
    if (storedEntry == nullptr || !storedEntry->hasLocal) {
        return PropertyValue::Unset();
    }
    return storedEntry->localValue;
}

// from src/gui/core/PropertySystem.cpp

PropertyValue DependencyObject::GetValue(
    DependencyPropertyHandle propertyHandle) const noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) {
        return PropertyValue::Unset();
    }

    const Meta::DependencyProperty* property =
        registry_->Find(propertyHandle);
    if (property == nullptr) {
        return PropertyValue::Unset();
    }
    const PropertyMetadata* metadata = property->MetadataFor(runtimeType_);
    if (metadata == nullptr) {
        return PropertyValue::Unset();
    }

        auto* storedEntry = FindStoredEntry(propertyHandle);
    return storedEntry != nullptr
        ? storedEntry->effectiveValue
        : metadata->defaultValue;
}

// from src/gui/core/PropertySystem.cpp


// from src/gui/core/PropertySystem.cpp


MemberId DependencyObject::CanonicalPropertyKey(
    DependencyPropertyHandle property) const noexcept {
    if (registry_ != nullptr) {
        const Meta::DependencyProperty* descriptor = registry_->Find(property);
        if (descriptor != nullptr) {
            return descriptor->Handle().value;
        }
    }
    return property.value;
}

const StoredValueEntry* DependencyObject::FindStoredEntry(
    DependencyPropertyHandle property) const noexcept {
    const PropertyStore* store = static_cast<const PropertyStore*>(valueStore_);
    if (store == nullptr) {
        return nullptr;
    }
    return store->entries.Find(CanonicalPropertyKey(property));
}

StoredValueEntry* DependencyObject::FindStoredEntry(
    DependencyPropertyHandle property) noexcept {
    return const_cast<StoredValueEntry*>(
        static_cast<const DependencyObject*>(this)->FindStoredEntry(property));
}

Base::Result<StoredValueEntry*> DependencyObject::EnsureStoredEntry(
    DependencyPropertyHandle propertyHandle) noexcept {
    StoredValueEntry* existing = FindStoredEntry(propertyHandle);
    if (existing != nullptr) {
        return existing;
    }
    const Meta::DependencyProperty* property = registry_->Find(propertyHandle);
    const PropertyMetadata* metadata = property != nullptr
        ? property->MetadataFor(runtimeType_) : nullptr;
    if (property == nullptr || metadata == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::NotFound,
            "Dependency property does not apply to this object type");
    }
    propertyHandle = property->Handle();
    if (valueStore_ == nullptr) {
        valueStore_ = new (std::nothrow) PropertyStore();
        if (valueStore_ == nullptr) {
            return Base::Status::Failure(Base::ErrorCode::OutOfMemory,
                "DependencyObject value store allocation failed");
        }
    }
    PropertyStore* store = static_cast<PropertyStore*>(valueStore_);
    StoredValueEntry entry;
    entry.baseValue = metadata->defaultValue;
    entry.effectiveValue = metadata->defaultValue;
    Base::Result<Base::HashMap<MemberId, StoredValueEntry>::InsertResult> inserted =
        store->entries.Insert(propertyHandle.value, std::move(entry));
    if (!inserted) {
        if (store->entries.Empty()) {
            delete store;
            valueStore_ = nullptr;
        }
        return inserted.GetStatus();
    }
    return &inserted.Value().entry->Value();
}

void DependencyObject::RemoveStoredEntry(MemberId key) noexcept {
    PropertyStore* store = static_cast<PropertyStore*>(valueStore_);
    if (store == nullptr) {
        return;
    }
    StoredValueEntry* entry = store->entries.Find(key);
    if (entry != nullptr) {
        ReleaseExpression(*entry);
        store->entries.Erase(key);
    }
}

std::uint32_t DependencyObject::StoredValueCount() const noexcept {
    const PropertyStore* store = static_cast<const PropertyStore*>(valueStore_);
    return store != nullptr ? store->entries.Size() : 0U;
}

Base::Result<void> DependencyObject::VerifyReady() const noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) {
        return access.GetStatus();
    }
    if (!objectServicesAvailable_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "DependencyObject was created without an ObjectFactoryScope");
    }
    if (registry_ == nullptr || !registry_->IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Dependency property registry is not frozen");
    }
    if (runtimeType_ == InvalidTypeId ||
        registry_->Types().FindType(runtimeType_) == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "DependencyObject runtime type is not registered");
    }
    return {};
}
} // namespace Aero {
