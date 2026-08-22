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
#include "gui/core/State.hpp"
#include "gui/core/State.hpp"
#include "gui/core/State.hpp"
#include "gui/core/State.hpp"
#include "gui/core/Impl.hpp"
#include "gui/input/InputState.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "gui/meta/MetadataState.hpp"

using namespace Aero;
using namespace Aero::Media;
using namespace Aero::Meta;
using namespace Aero::Threading;

namespace Aero {

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

void DependencyObject::RemoveEntry(std::uint32_t index) noexcept {
    AERO_ASSERT(index < values_.Size());
    ReleaseExpression(values_[index]);
    for (std::uint32_t current = index + 1U;
         current < values_.Size();
         ++current) {
        values_[current - 1U] = std::move(values_[current]);
    }
    values_.PopBack();
}

// from src/gui/core/PropertySystem.cpp

Base::Result<void> DependencyObject::VerifyMutationAllowed() const noexcept {
    return {};
}

// from src/gui/core/PropertySystem.cpp

void DependencyObject::OnPropertyInvalidated(
    PropertyInvalidationFlags) noexcept {
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
    std::uint32_t index = FindEntryIndex(propertyHandle);
    const bool hadEntry = index != InvalidIndex;
    if (!hadEntry && kind == ChangeKind::Clear) return {};
    if (!hadEntry) {
        Base::Result<std::uint32_t> ensured = EnsureEffectiveEntry(propertyHandle);
        if (!ensured) return ensured.GetStatus();
        index = ensured.Value();
    }
    EffectiveValueEntry& entry = values_[index];
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
    index = FindEntryIndex(propertyHandle);
    const bool committed = index != InvalidIndex &&
        values_[index].sourceInfo.revision != oldRevision;
    if (committed) {
        if (removesExpression && oldExpression.cleanup != nullptr) {
            oldExpression.cleanup(oldExpression.context);
        }
        return recomputed.GetStatus();
    }
    if (index != InvalidIndex) {
        values_[index].localValue = oldLocal;
        values_[index].currentValue = oldCurrent;
        values_[index].localExpression = oldExpression;
        values_[index].hasLocal = oldHasLocal;
        values_[index].hasCurrent = oldHasCurrent;
        values_[index].hasExpression = oldHasExpression;
        if (!hadEntry) RemoveEntry(index);
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
    const std::uint32_t index = FindEntryIndex(propertyHandle);
    if (index == InvalidIndex) return {};
    EffectiveValueEntry& entry = values_[index];
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
    const std::uint32_t index = FindEntryIndex(propertyHandle);
    const PropertyValue oldEffective = index != InvalidIndex
        ? values_[index].effectiveValue : metadata->defaultValue;
    const PropertyValueSourceInfo oldSourceInfo = index != InvalidIndex
        ? values_[index].sourceInfo : PropertyValueSourceInfo{};
    return RecomputeEffectiveValueCore(propertyHandle, *property, *metadata,
        oldEffective, oldSourceInfo);
}

// from src/gui/core/PropertySystem.cpp

Base::Result<void> DependencyObject::RecomputeEffectiveValueCore(
    DependencyPropertyHandle propertyHandle,
    const Meta::DependencyProperty& property,
    const PropertyMetadata& metadata, const PropertyValue& oldEffective,
    const PropertyValueSourceInfo& oldSourceInfo) noexcept {
    std::uint32_t index = FindEntryIndex(propertyHandle);
    if (index == InvalidIndex) {
        Base::Result<std::uint32_t> ensured = EnsureEffectiveEntry(propertyHandle);
        if (!ensured) return ensured.GetStatus();
        index = ensured.Value();
    }
    const EffectiveValueEntry& stored = values_[index];
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
    if (hasExpression) {
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
            Access::PrepareConsumerChange(
                *this,
                propertyHandle,
                oldEffective,
                newEffective);
        if (!consumerPrepared) return consumerPrepared.GetStatus();
    }
    source.revision = nextValueRevision_++;
    index = FindEntryIndex(propertyHandle);
    if (index == InvalidIndex) return Base::Status::Failure(Base::ErrorCode::InternalError,
        "Dependency property entry disappeared during evaluation");
    EffectiveValueEntry& entry = values_[index];
    entry.baseValue = baseValue;
    entry.effectiveValue = newEffective;
    entry.sourceInfo = source;
    if (newEffective != oldEffective) {
        Access::CommitConsumerChange(
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
        OnPropertyInvalidated(flags);
    }
    index = FindEntryIndex(propertyHandle);
    if (index != InvalidIndex) {
        const EffectiveValueEntry& finalEntry = values_[index];
        const bool shouldStore = finalEntry.hasLocal || finalEntry.hasCurrent ||
            finalEntry.hasExpression || finalEntry.hasInherited || finalEntry.hasAnimation ||
            !finalEntry.baseProviders.GetIsEmpty() ||
            finalEntry.effectiveValue != metadata.defaultValue;
        if (!shouldStore) RemoveEntry(index);
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
    std::uint32_t index = FindEntryIndex(property);
    if (index == InvalidIndex && value == nullptr) return {};
    if (index == InvalidIndex) {
        Base::Result<std::uint32_t> ensured = EnsureEffectiveEntry(property);
        if (!ensured) return ensured.GetStatus();
        index = ensured.Value();
    }
    EffectiveValueEntry& entry = values_[index];
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
    const std::uint32_t index = FindEntryIndex(property);
    if (index == InvalidIndex || !values_[index].hasAnimation) return false;
    values_[index].animationValue = PropertyValue::Unset();
    values_[index].hasAnimation = false;
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
    Base::Result<std::uint32_t> ensured = EnsureEffectiveEntry(property);
    if (!ensured) return ensured.GetStatus();
    values_[ensured.Value()].animationValue = value;
    values_[ensured.Value()].hasAnimation = true;
    return {};
}

// from src/gui/core/PropertySystem.cpp

Base::Result<bool> DependencyObject::InvalidateBaseValueInternal(
    DependencyPropertyHandle property) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    Base::Result<void> writable = VerifyMutationAllowed();
    if (!writable) return writable.GetStatus();
    const std::uint32_t index = FindEntryIndex(property);
    if (index == InvalidIndex || !values_[index].hasCurrent) return false;
    values_[index].currentValue = PropertyValue::Unset();
    values_[index].hasCurrent = false;
    return true;
}

// from src/gui/core/PropertySystem.cpp

Base::Result<bool> DependencyObject::ClearLocalExpressionInternal(
    DependencyPropertyHandle property) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    Base::Result<void> writable = VerifyMutationAllowed();
    if (!writable) return writable.GetStatus();
    const std::uint32_t index = FindEntryIndex(property);
    if (index == InvalidIndex || !values_[index].hasExpression) return false;
    ReleaseExpression(values_[index]);
    values_[index].currentValue = PropertyValue::Unset();
    values_[index].hasCurrent = false;
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
    Base::Result<std::uint32_t> ensured = EnsureEffectiveEntry(property);
    if (!ensured) return ensured.GetStatus();
    EffectiveValueEntry& entry = values_[ensured.Value()];
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
    const std::uint32_t index = FindEntryIndex(property);
    if (index == InvalidIndex) return false;
    const bool removed = values_[index].baseProviders.RemoveOrigin(origin) != 0U;
    if (removed) { values_[index].currentValue = PropertyValue::Unset(); values_[index].hasCurrent = false; }
    return removed;
}

// from src/gui/core/PropertySystem.cpp

Base::Result<bool> DependencyObject::ClearProviderContributionInternal(
    DependencyPropertyHandle property, PropertyProviderToken token) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    Base::Result<void> writable = VerifyMutationAllowed();
    if (!writable) return writable.GetStatus();
    const std::uint32_t index = FindEntryIndex(property);
    if (index == InvalidIndex) return false;
    const bool removed = values_[index].baseProviders.Remove(token);
    if (removed) { values_[index].currentValue = PropertyValue::Unset(); values_[index].hasCurrent = false; }
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
    Base::Result<std::uint32_t> ensured = EnsureEffectiveEntry(property);
    if (!ensured) return ensured.GetStatus();
    EffectiveValueEntry& entry = values_[ensured.Value()];
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

void DependencyObject::ReleaseExpression(EffectiveValueEntry& entry) noexcept {
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
        source.rank == PropertyValueRank::LocalExpression) return EffectiveValueSource::Local;
    return EffectiveValueSource::Current;
}

// from src/gui/core/PropertySystem.cpp


Base::Result<std::uint32_t> DependencyObject::EnsureEffectiveEntry(
    DependencyPropertyHandle propertyHandle) noexcept {
    const std::uint32_t existing = FindEntryIndex(propertyHandle);
    if (existing != InvalidIndex) return existing;
    const Meta::DependencyProperty* property =
        registry_->Find(propertyHandle);
    const PropertyMetadata* metadata = property != nullptr
        ? property->MetadataFor(runtimeType_) : nullptr;
    if (property == nullptr || metadata == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::NotFound,
            "Dependency property does not apply to this object type");
    }
    propertyHandle = property->Handle();
    if (values_.Size() == UINT32_MAX) {
        return Base::Status::Failure(Base::ErrorCode::OutOfRange,
            "DependencyObject sparse value table limit reached");
    }
    EffectiveValueEntry entry;
    entry.property = propertyHandle;
    entry.baseValue = metadata->defaultValue;
    entry.effectiveValue = metadata->defaultValue;
    Base::Result<void> appended = values_.PushBack(std::move(entry));
    if (!appended) return appended.GetStatus();
    return values_.Size() - 1U;
}

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

    const std::uint32_t oldIndex = FindEntryIndex(property);
    const PropertyValue oldEffective = oldIndex != InvalidIndex ? values_[oldIndex].effectiveValue : metadata->defaultValue;
    const PropertyValueSourceInfo oldSourceInfo = oldIndex != InvalidIndex ? values_[oldIndex].sourceInfo : PropertyValueSourceInfo{};

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
    const std::uint32_t index = FindEntryIndex(propertyHandle);
    return index != InvalidIndex ? values_[index].sourceInfo : PropertyValueSourceInfo{};
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

    const std::uint32_t index = FindEntryIndex(propertyHandle);
    if (index == InvalidIndex || !values_[index].hasLocal) {
        return PropertyValue::Unset();
    }
    return values_[index].localValue;
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

    const std::uint32_t index = FindEntryIndex(propertyHandle);
    return index != InvalidIndex
        ? values_[index].effectiveValue
        : metadata->defaultValue;
}

// from src/gui/core/PropertySystem.cpp

std::uint32_t DependencyObject::FindEntryIndex(
    DependencyPropertyHandle property) const noexcept {
    if (registry_ != nullptr) {
        const Meta::DependencyProperty* descriptor =
            registry_->Find(property);
        if (descriptor != nullptr) {
            property = descriptor->Handle();
        }
    }
    for (std::uint32_t index = 0U; index < values_.Size(); ++index) {
        DependencyPropertyHandle stored = values_[index].property;
        if (registry_ != nullptr) {
            const Meta::DependencyProperty* descriptor =
                registry_->Find(stored);
            if (descriptor != nullptr) {
                stored = descriptor->Handle();
            }
        }
        if (stored == property) {
            return index;
        }
    }
    return InvalidIndex;
}

// from src/gui/core/PropertySystem.cpp

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
