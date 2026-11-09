#include <Aero/DependencyObject.hpp>
#include <Aero/Base/Assert.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Allocator.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Events.hpp>
#include <Aero/Threading.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Transform2D.hpp>
#include <Aero/Media/Transform3D.hpp>
#include <Aero/Media/Effects.hpp>
#include <Aero/Markup/XamlReader.hpp>
#include <Aero/Controls.hpp>
#include <Aero/Documents/Inlines.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/TryCast.hpp>
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/core/PropertyStore.hpp"
#include <new>

#include "gui/input/InputManager.hpp"

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

struct ActiveMutation {
    const DependencyObject* object = nullptr;
    MemberId property = InvalidMemberId;
};

constexpr std::uint32_t MaxMutationDepth = 32U;
thread_local ActiveMutation t_mutationStack[MaxMutationDepth]{};
thread_local std::uint32_t t_mutationDepth = 0U;
thread_local std::uint64_t t_nextValueRevision = 1U;

// Unified base-value precedence resolution shared by GetAnimationBaseValueInternal
// and RecomputeEffectiveValueCore.
Base::Result<void> EvaluateStoredBaseValue(
    DependencyObject& owner,
    const StoredValueEntry* storedEntry,
    const PropertyMetadata& metadata,
    DependencyPropertyHandle propertyHandle,
    PropertyValue& outBaseValue,
    PropertyValueSourceInfo* outSource) noexcept {
    PropertyValueSourceInfo source;
    if (storedEntry == nullptr) {
        outBaseValue = metadata.defaultValue;
        source.rank = PropertyValueRank::Default;
        if (outSource != nullptr) *outSource = source;
        return {};
    }

    const StoredValueEntry& stored = *storedEntry;
    PropertyProviderToken providerToken;
    PropertyValue providerValue;
    const bool hasProvider =
        stored.TryGetWinnerProvider(providerToken, providerValue);
    const FrameworkElement* fe = ::Aero::TryCast<FrameworkElement>(&owner);
    const bool isTemplateChild =
        fe != nullptr && fe->GetTemplatedParent() != nullptr;

    if (hasProvider &&
        (providerToken.rank > PropertyValueRank::Local ||
         (isTemplateChild &&
          providerToken.rank >= PropertyValueRank::TemplatedParentTrigger))) {
        outBaseValue = providerValue;
        source.rank = providerToken.rank;
        source.token = providerToken;
    } else if (stored.HasExpression()) {
        const PropertyExpression expression = stored.ExpressionOrEmpty();
        Base::Result<PropertyValue> evaluated =
            expression.evaluate(expression.context, owner, propertyHandle);
        if (!evaluated) return evaluated.GetStatus();
        if (evaluated.Value().IsUnset()) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "A property expression returned Unset");
        }
        outBaseValue = std::move(evaluated).Value();
        source.rank = PropertyValueRank::LocalExpression;
        source.token = {PropertyValueRank::Local, LocalValueProviderOrigin, 1U};
        source.expressionKind = expression.kind;
        source.hasExpression = true;
    } else if (stored.HasLocal()) {
        outBaseValue = stored.LocalValueOrUnset();
        source.rank = PropertyValueRank::Local;
        source.token = {PropertyValueRank::Local, LocalValueProviderOrigin, 0U};
    } else if (hasProvider) {
        outBaseValue = providerValue;
        source.rank = providerToken.rank;
        source.token = providerToken;
    } else if (stored.HasInherited()) {
        outBaseValue = stored.InheritedValueOrUnset();
        source.rank = PropertyValueRank::Inherited;
        source.isInherited = true;
    } else {
        outBaseValue = metadata.defaultValue;
        source.rank = PropertyValueRank::Default;
    }

    if (stored.HasCurrent()) {
        outBaseValue = stored.CurrentValueOrUnset();
        source.isCurrentValue = true;
    }
    if (outSource != nullptr) {
        *outSource = source;
    }
    return {};
}

} // namespace

PropertyInvalidationFlags DependencyObject::AccumulateInvalidations(
    FrameworkPropertyMetadataOptions metadataFlags) noexcept {
    PropertyInvalidationFlags change = PropertyInvalidationFlags::None;
    if (HasFlag(metadataFlags, FrameworkPropertyMetadataOptions::AffectsMeasure)) {
        change |= PropertyInvalidationFlags::Measure;
    }
    if (HasFlag(metadataFlags, FrameworkPropertyMetadataOptions::AffectsArrange)) {
        change |= PropertyInvalidationFlags::Arrange;
    }
    if (HasFlag(metadataFlags, FrameworkPropertyMetadataOptions::AffectsRender)) {
        change |= PropertyInvalidationFlags::Render;
    }
    if (HasFlag(metadataFlags, FrameworkPropertyMetadataOptions::Inherits)) {
        change |= PropertyInvalidationFlags::Inheritance;
    }
    if (HasFlag(metadataFlags, FrameworkPropertyMetadataOptions::AffectsParentMeasure)) {
        change |= PropertyInvalidationFlags::ParentMeasure;
    }
    if (HasFlag(metadataFlags, FrameworkPropertyMetadataOptions::AffectsParentArrange)) {
        change |= PropertyInvalidationFlags::ParentArrange;
    }
    invalidations_ |= change;
    return change;
}

bool DependencyObject::HasChangeHandlers() const noexcept {
    return rare_ != nullptr && !rare_->changeHandlers.Empty();
}

Meta::DependencyPropertyRegistry* DependencyObject::PropertyRegistryPtr() const noexcept {
    return Threading::ResolveObjectContextRegistry(ContextSlot());
}

void DependencyObject::NotifyReadOnlyRedirectedChange(
    DependencyPropertyHandle property,
    const PropertyValue& oldValue,
    const PropertyValue& newValue) noexcept {
    const bool hasHandlers = HasChangeHandlers();
    Meta::DependencyPropertyRegistry* const registry = PropertyRegistryPtr();
    if (hasHandlers && registry != nullptr) {
        if (const Meta::DependencyProperty* registered = registry->Find(property)) {
            property = registered->Handle();
        }
    }
    const DependencyPropertyChangedEventArgs args{
        property,
        oldValue,
        newValue,
        EffectiveValueSource::Local,
        EffectiveValueSource::Local};
    if (hasHandlers) {
        NotifyValueChanged(args);
    }
    OnPropertyChanged(args);
}

void DependencyObject::NotifyValueChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    if (rare_ == nullptr || rare_->changeHandlers.Empty()) {
        return;
    }
    AERO_ASSERT(rare_->changeHandlerNotificationDepth != UINT32_MAX);
    ++rare_->changeHandlerNotificationDepth;
    const std::uint32_t snapshotCount = rare_->changeHandlers.Size();
    for (std::uint32_t index = 0U; index < snapshotCount; ++index) {
        if (index >= rare_->changeHandlers.Size()) {
            break;
        }
        ChangeHandlerRecord& record = rare_->changeHandlers[index];
        if (record.active && record.property == args.GetProperty()) {
            record.handler(*this, args);
        }
    }
    --rare_->changeHandlerNotificationDepth;
    if (rare_->changeHandlerNotificationDepth != 0U) {
        return;
    }
    for (std::uint32_t index = 0U; index < rare_->changeHandlers.Size();) {
        if (!rare_->changeHandlers[index].active) {
            RemoveChangeHandler(index);
        } else {
            ++index;
        }
    }
    if (rare_->IsEmpty()) {
        delete rare_;
        rare_ = nullptr;
    }
}

void DependencyObject::RemoveChangeHandler(std::uint32_t index) noexcept {
    if (rare_ == nullptr) return;
    AERO_ASSERT(index < rare_->changeHandlers.Size());
    for (std::uint32_t current = index + 1U;
         current < rare_->changeHandlers.Size();
         ++current) {
        rare_->changeHandlers[current - 1U] = std::move(rare_->changeHandlers[current]);
    }
    rare_->changeHandlers.PopBack();
}

bool DependencyObject::VerifyMutationAllowed() const noexcept {
    return true;
}

void DependencyObject::OnPropertyInvalidated(
    PropertyInvalidationFlags) noexcept {
}

void DependencyObject::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs&) noexcept {
}

Base::Result<void> DependencyObject::ApplyChange(
    DependencyPropertyHandle propertyHandle, const DependencyPropertyKey* key,
    ChangeKind kind, const PropertyValue* requestedValue) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    Meta::DependencyPropertyRegistry* const registry = PropertyRegistryPtr();
    const Meta::DependencyProperty* property =
        registry->Find(propertyHandle);
    if (property == nullptr) return Base::Status::Failure(
        Base::ErrorCode::NotFound, "Dependency property was not found");
    propertyHandle = property->Handle();
    const PropertyMetadata* metadata = property->MetadataFor(DependencyObject::RuntimeType());
    if (metadata == nullptr) return Base::Status::Failure(
        Base::ErrorCode::NotFound, "Dependency property does not apply to this object type");
    if (kind != ChangeKind::ReCoerce && property->GetIsReadOnly() &&
        !registry->ValidateKey(propertyHandle, key)) return ReadOnlyStatus();
    if ((kind == ChangeKind::SetLocal || kind == ChangeKind::SetCurrent) &&
        requestedValue == nullptr) return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Dependency property set operation requires a value");
    Base::Result<DependencyMutationScope> mutationResult = BeginMutation(propertyHandle);
    if (!mutationResult) return mutationResult.GetStatus();
    DependencyMutationScope mutation = std::move(mutationResult).Value();
    StoredValueEntry* storedEntry = FindStoredEntry(propertyHandle);
    const bool hadEntry = storedEntry != nullptr;
    if (!hadEntry && kind == ChangeKind::Clear) return {};
    if (!hadEntry) {
        Base::Result<StoredValueEntry*> ensured =
            EnsureStoredEntryDirect(propertyHandle, *metadata);
        if (!ensured) return ensured.GetStatus();
        storedEntry = ensured.Value();
    }
    StoredValueEntry& entry = *storedEntry;
    const PropertyValue oldEffective = hadEntry ? entry.effectiveValue : metadata->defaultValue;
    const PropertyValueSourceInfo oldSourceInfo = hadEntry
        ? entry.SourceInfo() : PropertyValueSourceInfo{};
    const std::uint64_t oldRevision = oldSourceInfo.revision;
    const PropertyValue oldLocal = entry.LocalValueOrUnset();
    const PropertyValue oldCurrent = entry.CurrentValueOrUnset();
    const PropertyExpression oldExpression = entry.ExpressionOrEmpty();
    const bool oldHasLocal = entry.HasLocal();
    const bool oldHasCurrent = entry.HasCurrent();
    const bool oldHasExpression = entry.HasExpression();
    const bool removesExpression = oldHasExpression &&
        (kind == ChangeKind::SetLocal || kind == ChangeKind::Clear);
    switch (kind) {
    case ChangeKind::SetLocal:
        if (entry.rare == nullptr &&
            (entry.HasInherited() || entry.HasInlineProvider())) {
            Base::Result<StoredValueRare*> block =
                entry.EnsureRare(GetDispatcher().GetPropertySlab());
            if (!block) {
                if (!hadEntry) {
                    RemoveStoredEntry(CanonicalPropertyKey(propertyHandle));
                }
                return block.GetStatus();
            }
        }
        if (entry.rare != nullptr) {
            entry.rare->localExpression = {};
        }
        entry.SetHasExpression(false);
        // P2.1: inline Local set is infallible, no OOM rollback needed.
        entry.SetLocalValue(*requestedValue);
        entry.ClearCurrent();
        break;
    case ChangeKind::SetCurrent: {
        Base::Result<StoredValueRare*> block =
            entry.EnsureRare(GetDispatcher().GetPropertySlab());
        if (!block) {
            if (!hadEntry) {
                RemoveStoredEntry(CanonicalPropertyKey(propertyHandle));
            }
            return block.GetStatus();
        }
        block->currentValue = *requestedValue;
        entry.SetHasCurrent(true);
        break;
    }
    case ChangeKind::Clear:
        if (entry.rare != nullptr) {
            entry.rare->localExpression = {};
        }
        entry.SetHasExpression(false);
        entry.ClearLocal();
        entry.ClearCurrent();
        break;
    case ChangeKind::ReCoerce:
        break;
    }
    Base::Result<void> recomputed = RecomputeEffectiveValueCore(
        propertyHandle, *property, *metadata, oldEffective, oldSourceInfo, storedEntry);
    if (recomputed) {
        if (removesExpression && oldExpression.cleanup != nullptr) {
            oldExpression.cleanup(oldExpression.context);
        }
        return {};
    }
    storedEntry = FindStoredEntry(propertyHandle);
    const bool committed = storedEntry != nullptr &&
        storedEntry->SourceInfo().revision != oldRevision;
    if (committed) {
        if (removesExpression && oldExpression.cleanup != nullptr) {
            oldExpression.cleanup(oldExpression.context);
        }
        return recomputed.GetStatus();
    }
    if (storedEntry != nullptr) {
        storedEntry->effectiveValue = oldEffective;
        storedEntry->SetHasCurrent(oldHasCurrent);
        storedEntry->SetHasExpression(oldHasExpression);
        if (oldHasLocal) {
            storedEntry->SetLocalValue(oldLocal);
        } else {
            storedEntry->ClearLocal();
        }
        if (oldHasCurrent || oldHasExpression) {
            Base::Result<StoredValueRare*> block = storedEntry->EnsureRare(
                GetDispatcher().GetPropertySlab());
            if (!block) {
                Base::ReportOutOfMemory(
                    sizeof(StoredValueRare),
                    alignof(StoredValueRare),
                    Base::MemoryTag::Object);
            } else {
                storedEntry->rare->currentValue =
                    oldHasCurrent ? oldCurrent : PropertyValue::Unset();
                storedEntry->rare->localExpression =
                    oldHasExpression ? oldExpression : PropertyExpression{};
            }
        } else {
            storedEntry->ClearCurrent();
            if (storedEntry->rare != nullptr) {
                storedEntry->rare->localExpression = {};
            }
            storedEntry->DropRareIfUnused();
        }
        if (!hadEntry) RemoveStoredEntry(CanonicalPropertyKey(propertyHandle));
    }
    return recomputed.GetStatus();
}

Base::Result<void> DependencyObject::DropEngineValueStateInternal(
    DependencyPropertyHandle propertyHandle) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    if (!VerifyMutationAllowed()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Cannot mutate a frozen Freezable");
    }
    auto* storedEntry = FindStoredEntry(propertyHandle);
    if (storedEntry == nullptr) return {};
    StoredValueEntry& entry = *storedEntry;
    entry.SetQueued(false);
    entry.SetDescendantsQueued(false);
    entry.ClearAllProviders();
    ReleaseExpression(entry);
    entry.ClearInherited();
    entry.ClearAnimation();
    entry.ClearCurrent();
    return RecomputeEffectiveValueInternal(propertyHandle);
}

Base::Result<void> DependencyObject::RecomputeEffectiveValueInternal(
    DependencyPropertyHandle propertyHandle) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    Meta::DependencyPropertyRegistry* const registry = PropertyRegistryPtr();
    const Meta::DependencyProperty* property =
        registry->Find(propertyHandle);
    const PropertyMetadata* metadata = property != nullptr
        ? property->MetadataFor(DependencyObject::RuntimeType()) : nullptr;
    if (property == nullptr || metadata == nullptr) return Base::Status::Failure(
        Base::ErrorCode::NotFound, "Dependency property does not apply to this object type");
    propertyHandle = property->Handle();
    Base::Result<DependencyMutationScope> mutationResult = BeginMutation(propertyHandle);
    if (!mutationResult) return mutationResult.GetStatus();
    DependencyMutationScope mutation = std::move(mutationResult).Value();
    auto* storedEntry = FindStoredEntry(propertyHandle);
    const PropertyValue oldEffective = storedEntry != nullptr
        ? storedEntry->effectiveValue : metadata->defaultValue;
    const PropertyValueSourceInfo oldSourceInfo = storedEntry != nullptr
        ? storedEntry->SourceInfo() : PropertyValueSourceInfo{};
    return RecomputeEffectiveValueCore(propertyHandle, *property, *metadata,
        oldEffective, oldSourceInfo, storedEntry);
}

Base::Result<PropertyValue>
DependencyObject::GetAnimationBaseValueInternal(
    DependencyPropertyHandle propertyHandle) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    Meta::DependencyPropertyRegistry* const registry = PropertyRegistryPtr();
    const Meta::DependencyProperty* property =
        registry->Find(propertyHandle);
    const PropertyMetadata* metadata = property != nullptr
        ? property->MetadataFor(DependencyObject::RuntimeType()) : nullptr;
    if (property == nullptr || metadata == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Dependency property does not apply to this object type");
    }
    propertyHandle = property->Handle();
    const StoredValueEntry* storedEntry =
        FindStoredEntry(propertyHandle);
    PropertyValue baseValue;
    Base::Result<void> resolvedBase = EvaluateStoredBaseValue(
        *this, storedEntry, *metadata, propertyHandle, baseValue, nullptr);
    if (!resolvedBase) return resolvedBase.GetStatus();
    return registry->EvaluateValue(
        *this, *property, *metadata, baseValue);
}

Base::Result<void> DependencyObject::RecomputeEffectiveValueCore(
    DependencyPropertyHandle propertyHandle,
    const Meta::DependencyProperty& property,
    const PropertyMetadata& metadata, const PropertyValue& oldEffective,
    const PropertyValueSourceInfo& oldSourceInfo,
    StoredValueEntry* storedEntry) noexcept {
    if (storedEntry == nullptr) {
        storedEntry = FindStoredEntry(propertyHandle);
    }
    if (storedEntry == nullptr) {
        Base::Result<StoredValueEntry*> ensured = EnsureStoredEntryDirect(propertyHandle, metadata);
        if (!ensured) return ensured.GetStatus();
        storedEntry = ensured.Value();
    }
    const StoredValueEntry& stored = *storedEntry;
    const bool hasExpression = stored.HasExpression();
    const bool hasLocal = stored.HasLocal();
    const PropertyValue localValue = stored.LocalValueOrUnset();
    const bool hasCurrent = stored.HasCurrent();
    const bool hasInherited = stored.HasInherited();
    const PropertyValue inheritedValue = stored.InheritedValueOrUnset();
    const bool hasAnimation = stored.HasAnimation();
    const PropertyValue animationValue = stored.AnimationValueOrUnset();
    const bool hasProvider = stored.HasProviders();

    PropertyValue baseValue;
    PropertyValueSourceInfo source;
    Base::Result<void> resolvedBase = EvaluateStoredBaseValue(
        *this, &stored, metadata, propertyHandle, baseValue, &source);
    if (!resolvedBase) return resolvedBase.GetStatus();

    PropertyValue candidate = hasAnimation ? animationValue : baseValue;
    if (hasAnimation) {
        source.rank = PropertyValueRank::Animation;
        source.token = {PropertyValueRank::Animation, AnimationValueProviderOrigin, 0U};
        source.isAnimated = true;
    }
    Meta::DependencyPropertyRegistry* const registry = PropertyRegistryPtr();
    Base::Result<PropertyValue> evaluated = registry->EvaluateValue(
        *this, property, metadata, candidate);
    if (!evaluated) return evaluated.GetStatus();
    PropertyValue newEffective = std::move(evaluated).Value();
    source.isCoerced = newEffective != candidate;
    if (t_nextValueRevision == UINT64_MAX) return Base::Status::Failure(
        Base::ErrorCode::OutOfRange, "Dependency property value revision limit reached");
    if (newEffective != oldEffective) {
        Base::Result<void> consumerPrepared =
            (*this).PrepareConsumerChange(
                propertyHandle,
                oldEffective,
                newEffective);
        if (!consumerPrepared) return consumerPrepared.GetStatus();
    }
    source.revision = t_nextValueRevision++;
    if (hasExpression || source.isCoerced || newEffective != oldEffective) {
        storedEntry = FindStoredEntry(propertyHandle);
    }
    if (storedEntry == nullptr) return Base::Status::Failure(Base::ErrorCode::InternalError,
        "Dependency property entry disappeared during evaluation");
    StoredValueEntry& entry = *storedEntry;
    if (hasLocal &&
        (source.rank != PropertyValueRank::Local ||
         source.isCoerced || hasAnimation || hasCurrent || hasExpression)) {
        Base::Result<StoredValueRare*> block =
            entry.EnsureRare(GetDispatcher().GetPropertySlab());
        if (block) {
            block->localBackup = localValue;
        }
    } else if (entry.rare != nullptr) {
        entry.rare->localBackup = PropertyValue::Unset();
    }
    if (entry.rare == nullptr && source.isCoerced &&
        (hasInherited || hasProvider)) {
        Base::Result<StoredValueRare*> block =
            entry.EnsureRare(GetDispatcher().GetPropertySlab());
        if (block && hasInherited) {
            block->inheritedValue = inheritedValue;
        }
    }
    entry.effectiveValue = newEffective;
    {
        Base::Result<void> storedSource = entry.SetSourceInfo(
            source, GetDispatcher().GetPropertySlab());
        if (!storedSource) {
            return storedSource.GetStatus();
        }
    }
    entry.DropRareIfUnused();
    if (newEffective != oldEffective) {
        (*this).CommitConsumerChange(
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
        if (HasFlag(flags, PropertyInvalidationFlags::Inheritance) && registry != nullptr) {
            registry->NotifyInheritancePropertyChanged(*this, propertyHandle);
        }
        storedEntry = FindStoredEntry(propertyHandle);
    }
    if (storedEntry != nullptr) {
        if (!storedEntry->HasNonDefaultState(metadata.defaultValue)) {
            RemoveStoredEntry(CanonicalPropertyKey(propertyHandle));
        }
    }
    return {};
}

Base::Result<void> DependencyObject::ApplyInheritedValueInternal(
    DependencyPropertyHandle property, const PropertyValue* value) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    if (!VerifyMutationAllowed()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Cannot mutate a frozen Freezable");
    }
    StoredValueEntry* storedEntry = FindStoredEntry(property);
    if (storedEntry == nullptr && value == nullptr) return {};
    if (storedEntry == nullptr) {
        Base::Result<StoredValueEntry*> ensured = EnsureStoredEntry(property);
        if (!ensured) return ensured.GetStatus();
        storedEntry = ensured.Value();
    }
    StoredValueEntry& entry = *storedEntry;
    const bool changed = value == nullptr ? entry.HasInherited()
        : (!entry.HasInherited() || entry.InheritedValueOrUnset() != *value);
    if (value == nullptr) {
        entry.ClearInherited();
    } else {
        Base::Result<void> storedInherited = entry.SetInheritedValue(
            *value, GetDispatcher().GetPropertySlab());
        if (!storedInherited) {
            return storedInherited.GetStatus();
        }
    }
    if (changed) {
        entry.ClearCurrent();
    }
    return {};
}

bool DependencyObject::ClearAnimationValueInternal(
    DependencyPropertyHandle property) noexcept {
    if (!VerifyReady() || !VerifyMutationAllowed()) return false;
    auto* storedEntry = FindStoredEntry(property);
    if (storedEntry == nullptr || !storedEntry->HasAnimation()) return false;
    storedEntry->ClearAnimation();
    return true;
}

Base::Result<void> DependencyObject::ApplyAnimationValueInternal(
    DependencyPropertyHandle property, const PropertyValue& value) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    if (!VerifyMutationAllowed()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Cannot mutate a frozen Freezable");
    }
    if (value.IsUnset()) return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
        "Animation values cannot be Unset");
    Base::Result<StoredValueEntry*> ensured = EnsureStoredEntry(property);
    if (!ensured) return ensured.GetStatus();
    Base::Result<StoredValueRare*> block = ensured->EnsureRare(
        GetDispatcher().GetPropertySlab());
    if (!block) return block.GetStatus();
    block->animationValue = value;
    ensured->SetHasAnimation(true);
    return {};
}

bool DependencyObject::InvalidateBaseValueInternal(
    DependencyPropertyHandle property) noexcept {
    if (!VerifyReady() || !VerifyMutationAllowed()) return false;
    auto* storedEntry = FindStoredEntry(property);
    if (storedEntry == nullptr || !storedEntry->HasCurrent()) return false;
    storedEntry->ClearCurrent();
    return true;
}

bool DependencyObject::ClearLocalExpressionInternal(
    DependencyPropertyHandle property) noexcept {
    if (!VerifyReady() || !VerifyMutationAllowed()) return false;
    auto* storedEntry = FindStoredEntry(property);
    if (storedEntry == nullptr || !storedEntry->HasExpression()) return false;
    ReleaseExpression(*const_cast<StoredValueEntry*>(storedEntry));
    storedEntry->ClearCurrent();
    return true;
}

Base::Result<void> DependencyObject::ApplyLocalExpressionInternal(
    DependencyPropertyHandle property, const PropertyExpression& expression) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    if (!VerifyMutationAllowed()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Cannot mutate a frozen Freezable");
    }
    if (!expression.IsValid()) return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
        "A property expression requires an evaluate callback");
    Base::Result<StoredValueEntry*> ensured = EnsureStoredEntry(property);
    if (!ensured) return ensured.GetStatus();
    StoredValueEntry& entry = *ensured.Value();
    PropertyExpression old;
    const bool hadOld = entry.HasExpression();
    if (hadOld) old = entry.ExpressionOrEmpty();
    Base::Result<StoredValueRare*> block =
        entry.EnsureRare(GetDispatcher().GetPropertySlab());
    if (!block) return block.GetStatus();
    block->localExpression = expression;
    entry.SetHasExpression(true);
    entry.ClearLocal();
    entry.ClearCurrent();
    if (hadOld && old.cleanup != nullptr) old.cleanup(old.context);
    return {};
}

bool DependencyObject::ClearProviderOriginInternal(
    DependencyPropertyHandle property, std::uint32_t origin) noexcept {
    if (!VerifyReady() || !VerifyMutationAllowed()) return false;
    auto* storedEntry = FindStoredEntry(property);
    if (storedEntry == nullptr) return false;
    const bool removed = storedEntry->RemoveProviderOrigin(origin) != 0U;
    if (removed) {
        storedEntry->ClearCurrent();
    }
    return removed;
}

bool DependencyObject::ClearProviderContributionInternal(
    DependencyPropertyHandle property, PropertyProviderToken token) noexcept {
    if (!VerifyReady() || !VerifyMutationAllowed()) return false;
    auto* storedEntry = FindStoredEntry(property);
    if (storedEntry == nullptr) return false;
    const bool removed = storedEntry->RemoveProviderToken(token);
    if (removed) {
        storedEntry->ClearCurrent();
    }
    return removed;
}

Base::Result<void> DependencyObject::ApplyProviderContributionInternal(
    DependencyPropertyHandle property, PropertyProviderToken token,
    const PropertyValue& value) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return ready.GetStatus();
    if (!VerifyMutationAllowed()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Cannot mutate a frozen Freezable");
    }
    Meta::DependencyPropertyRegistry* const registry = PropertyRegistryPtr();
    const Meta::DependencyProperty* registered = registry->Find(property);
    if (registered == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Dependency property was not found");
    }
    property = registered->Handle();
    const PropertyMetadata* metadata = registered->MetadataFor(DependencyObject::RuntimeType());
    if (metadata == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "Dependency property does not apply to this type");
    }
    Base::Result<void> valid = registry->ValidateValue(*registered, *metadata, value);
    if (!valid) return valid.GetStatus();
    StoredValueEntry* existing = FindStoredEntry(property);
    Base::Result<StoredValueEntry*> ensured = existing != nullptr
        ? Base::Result<StoredValueEntry*>(existing)
        : EnsureStoredEntryDirect(property, *metadata);
    if (!ensured) return ensured.GetStatus();
    StoredValueEntry& entry = *ensured.Value();
    entry.ClearCurrent();
    Base::Result<PropertyProviderSet*> providers = entry.EnsureProviders(
        GetDispatcher().GetPropertySlab());
    if (!providers) {
        return providers.GetStatus();
    }
    if (!providers->Set(token, value)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "A property contribution requires a valid token and value");
    }
    return {};
}

void DependencyObject::ReleaseExpression(StoredValueEntry& entry) noexcept {
    if (!entry.HasExpression()) return;
    const PropertyExpression expression = entry.ExpressionOrEmpty();
    if (entry.rare != nullptr) {
        entry.rare->localExpression = {};
    }
    entry.SetHasExpression(false);
    entry.DropRareIfUnused();
    if (expression.cleanup != nullptr) expression.cleanup(expression.context);
}

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

void DependencyObject::LeaveMutation() noexcept {
    AERO_ASSERT(t_mutationDepth > 0U && t_mutationStack[t_mutationDepth - 1U].object == this);
    if (t_mutationDepth > 0U) {
        --t_mutationDepth;
    }
}

Base::Result<DependencyMutationScope>
DependencyObject::BeginMutation(
    DependencyPropertyHandle property) noexcept {
    if (!VerifyMutationAllowed()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Cannot mutate a frozen Freezable");
    }
    for (std::uint32_t index = 0U; index < t_mutationDepth; ++index) {
        if (t_mutationStack[index].object == this &&
            t_mutationStack[index].property == property.value) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Recursive mutation of the same dependency property is not allowed");
        }
    }
    if (t_mutationDepth >= MaxMutationDepth) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Property mutation depth limit reached");
    }

    t_mutationStack[t_mutationDepth++] = {this, property.value};

    Base::Result<DispatcherReentrancyGuard> guard =
        GetDispatcher().EnterReentrancyGuard();
    if (!guard) {
        --t_mutationDepth;
        return guard.GetStatus();
    }

    return DependencyMutationScope(this, std::move(guard).Value());
}

PropertyInvalidationFlags DependencyObject::TakeInvalidations() noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) {
        return PropertyInvalidationFlags::None;
    }
    const PropertyInvalidationFlags result = invalidations_;
    invalidations_ = PropertyInvalidationFlags::None;
    return result;
}

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
    Meta::DependencyPropertyRegistry* const registry = PropertyRegistryPtr();
    const Meta::DependencyProperty* descriptor =
        registry != nullptr ? registry->Find(property) : nullptr;
    if (descriptor != nullptr) {
        property = descriptor->Handle();
    }
    if (rare_ == nullptr || rare_->changeHandlers.Empty()) {
        return false;
    }
    for (std::uint32_t count = rare_->changeHandlers.Size(); count > 0U; --count) {
        const std::uint32_t index = count - 1U;
        ChangeHandlerRecord& record = rare_->changeHandlers[index];
        if (record.property != property || record.handler != handler ||
            !record.active) {
            continue;
        }
        if (rare_->changeHandlerNotificationDepth != 0U) {
            record.active = false;
        } else {
            RemoveChangeHandler(index);
            if (rare_->IsEmpty()) {
                delete rare_;
                rare_ = nullptr;
            }
        }
        return true;
    }
    return false;
}

void DependencyObject::AddValueChangedHandler(
    DependencyPropertyHandle property,
    const DependencyPropertyChangedEventHandler& handler) noexcept {
    if (!VerifyReady()) return;
    Meta::DependencyPropertyRegistry* const registry = PropertyRegistryPtr();
    const Meta::DependencyProperty* descriptor = registry->Find(property);
    if (!property.IsValid() || handler.Empty() || descriptor == nullptr) {
        return;
    }
    property = descriptor->Handle();
    if (rare_ == nullptr) {
        rare_ = new (std::nothrow) DependencyObjectRare();
        if (rare_ == nullptr) return;
    }
    ChangeHandlerRecord record;
    record.property = property;
    record.handler = handler;
    record.active = true;
    (void)rare_->changeHandlers.PushBack(std::move(record));
}

void DependencyObject::CoerceValue(
    DependencyPropertyHandle property) noexcept {
    (void)ApplyChange(property, nullptr, ChangeKind::ReCoerce, nullptr);
}

void DependencyObject::ClearValue(
    const DependencyPropertyKey& key) noexcept {
    (void)ApplyChange(key.Property(), &key, ChangeKind::Clear, nullptr);
}

void DependencyObject::ClearValue(
    DependencyPropertyHandle property) noexcept {
    (void)ApplyChange(property, nullptr, ChangeKind::Clear, nullptr);
}

void DependencyObject::SetReadOnlyCurrentValue(
    DependencyPropertyHandle propertyHandle,
    const PropertyValue& value) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return;
    Meta::DependencyPropertyRegistry* const registry = PropertyRegistryPtr();
    const Meta::DependencyProperty* property =
        registry->Find(propertyHandle);
    if (property == nullptr) {
        return;
    }
    if (!property->GetIsReadOnly()) {
        return;
    }
    DependencyPropertyKey key;
    key.registry_ = registry;
    key.property_ = propertyHandle;
    key.secret_ = property->readOnlySecret_;
    (void)ApplyChange(
        propertyHandle, &key, ChangeKind::SetLocal, &value);
}

void DependencyObject::SetTemplateValue(
    DependencyPropertyHandle property,
    const PropertyValue& value) noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return;
    if (!VerifyMutationAllowed()) return;
    Meta::DependencyPropertyRegistry* const registry = PropertyRegistryPtr();
    const Meta::DependencyProperty* registered = registry->Find(property);
    if (registered == nullptr) return;
    property = registered->Handle();
    const TypeId runtimeType = DependencyObject::RuntimeType();
    const PropertyMetadata* metadata = registered->MetadataFor(runtimeType);
    if (metadata == nullptr) return;
    if (!registry->ValidateValueFor(property, runtimeType, value)) return;

    StoredValueEntry* storedEntry = FindStoredEntry(property);
    const PropertyValue oldEffective = storedEntry != nullptr ? storedEntry->effectiveValue : metadata->defaultValue;
    const PropertyValueSourceInfo oldSourceInfo = storedEntry != nullptr ? storedEntry->SourceInfo() : PropertyValueSourceInfo{};

    if (storedEntry == nullptr) {
        Base::Result<StoredValueEntry*> ensured =
            EnsureStoredEntryDirect(property, *metadata);
        if (!ensured) return;
        storedEntry = ensured.Value();
    }
    constexpr PropertyProviderToken kTemplateToken{
        PropertyValueRank::TemplatedParentSetter, TemplatedParentSetterOrigin, 0U};
    if (storedEntry->rare == nullptr &&
        !storedEntry->HasLocal() &&
        !storedEntry->HasInherited() &&
        !storedEntry->HasCurrent() &&
        !storedEntry->HasExpression() &&
        !storedEntry->HasAnimation() &&
        !storedEntry->Queued() &&
        (!storedEntry->HasInlineProvider() ||
         (storedEntry->SourceInfo().token == kTemplateToken))) {
        storedEntry->SetInlineProviderContribution(kTemplateToken, value);
    } else {
        Base::Result<void> contribution = ApplyProviderContributionInternal(
            property, kTemplateToken, value);
        if (!contribution) return;
        storedEntry = FindStoredEntry(property);
    }

    (void)RecomputeEffectiveValueCore(
        property, *registered, *metadata, oldEffective, oldSourceInfo, storedEntry);
}

void DependencyObject::SetCurrentValue(
    const DependencyPropertyKey& key,
    const PropertyValue& value) noexcept {
    (void)ApplyChange(key.Property(), &key, ChangeKind::SetCurrent, &value);
}

void DependencyObject::SetCurrentValue(
    DependencyPropertyHandle property,
    const PropertyValue& value) noexcept {
    (void)ApplyChange(property, nullptr, ChangeKind::SetCurrent, &value);
}

void DependencyObject::SetValue(
    const DependencyPropertyKey& key,
    const PropertyValue& value) noexcept {
    (void)ApplyChange(key.Property(), &key, ChangeKind::SetLocal, &value);
}

void DependencyObject::SetValue(
    DependencyPropertyHandle property,
    const PropertyValue& value) noexcept {
    (void)ApplyChange(property, nullptr, ChangeKind::SetLocal, &value);
}

PropertyValueSourceInfo DependencyObject::GetValueSourceInfo(
    DependencyPropertyHandle propertyHandle) const noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready) return {};
    Meta::DependencyPropertyRegistry* const registry = PropertyRegistryPtr();
    const Meta::DependencyProperty* property =
        registry->Find(propertyHandle);
    if (property == nullptr || property->MetadataFor(DependencyObject::RuntimeType()) == nullptr) {
        return {};
    }
    auto* storedEntry = FindStoredEntry(propertyHandle);
    return storedEntry != nullptr ? storedEntry->SourceInfo() : PropertyValueSourceInfo{};
}

EffectiveValueSource DependencyObject::GetValueSource(
    DependencyPropertyHandle propertyHandle) const noexcept {
    const PropertyValueSourceInfo source = GetValueSourceInfo(propertyHandle);
    return ToLegacySource(source);
}

PropertyValue DependencyObject::ReadLocalValue(
    DependencyPropertyHandle propertyHandle) const noexcept {
    Base::Result<void> ready = VerifyReady();
    if (!ready || valueStore_ == nullptr) {
        return PropertyValue::Unset();
    }
    const StoredValueEntry* storedEntry = FindStoredEntry(propertyHandle);
    if (storedEntry == nullptr || !storedEntry->HasLocal()) {
        return PropertyValue::Unset();
    }
    Meta::DependencyPropertyRegistry* const registry = PropertyRegistryPtr();
    const Meta::DependencyProperty* property =
        registry->Find(propertyHandle);
    if (property == nullptr || property->MetadataFor(DependencyObject::RuntimeType()) == nullptr) {
        return PropertyValue::Unset();
    }
    return storedEntry->LocalValueOrUnset();
}

const PropertyValue* DependencyObject::FindEffectiveValuePtr(
    DependencyPropertyHandle propertyHandle) const noexcept {
    if (typeVerified_) {
        if (!CheckAccess()) {
            return nullptr;
        }
    } else {
        Base::Result<void> ready = VerifyReady();
        if (!ready) {
            return nullptr;
        }
    }

    // Canonical-handle hot path: probe the store by the incoming MemberId
    // before registry Find. Alias/legacy handles fall through.
    const PropertyStore* store = static_cast<const PropertyStore*>(valueStore_);
    if (store != nullptr) {
        if (const StoredValueEntry* hit =
                store->entries.Find(propertyHandle.value)) {
            return &hit->effectiveValue;
        }
    }

    Meta::DependencyPropertyRegistry* const registry = PropertyRegistryPtr();
    const Meta::DependencyProperty* property =
        registry != nullptr ? registry->Find(propertyHandle) : nullptr;
    if (property == nullptr) {
        return nullptr;
    }
    const PropertyMetadata* metadata = property->MetadataFor(DependencyObject::RuntimeType());
    if (metadata == nullptr) {
        return nullptr;
    }

    const MemberId canonical = property->Handle().value;
    if (store != nullptr && canonical != propertyHandle.value) {
        if (const StoredValueEntry* aliasHit = store->entries.Find(canonical)) {
            return &aliasHit->effectiveValue;
        }
    }
    if (const Meta::ComputedPropertyReadCallback computedReader = property->ComputedReader()) {
        thread_local PropertyValue t_computedScratch[4];
        thread_local std::uint32_t t_computedSlot = 0U;
        PropertyValue& slot = t_computedScratch[t_computedSlot++ & 3U];
        slot = computedReader(*this);
        return slot.IsUnset() ? nullptr : &slot;
    }
    return &metadata->defaultValue;
}

PropertyValue DependencyObject::GetValue(
    DependencyPropertyHandle propertyHandle) const noexcept {
    const PropertyValue* val = FindEffectiveValuePtr(propertyHandle);
    return val != nullptr ? *val : PropertyValue::Unset();
}

Base::StringView DependencyObject::GetStringView(
    DependencyPropertyHandle propertyHandle) const noexcept {
    const PropertyValue* val = FindEffectiveValuePtr(propertyHandle);
    if (val != nullptr && val->Kind() == Base::ValueKind::String) {
        return val->AsString();
    }
    return {};
}

MemberId DependencyObject::CanonicalPropertyKey(
    DependencyPropertyHandle property) const noexcept {
    // P2.3: single-hash alias resolution; no descriptor load. The store is
    // always keyed by canonical handle (ApplyChange canonicalizes via
    // Find()->Handle() before EnsureStoredEntryDirect).
    Meta::DependencyPropertyRegistry* const registry = PropertyRegistryPtr();
    if (registry != nullptr) {
        return registry->CanonicalHandle(property);
    }
    return property.value;
}

const StoredValueEntry* DependencyObject::FindStoredEntry(
    DependencyPropertyHandle property) const noexcept {
    const PropertyStore* store = static_cast<const PropertyStore*>(valueStore_);
    if (store == nullptr) {
        return nullptr;
    }
    if (const StoredValueEntry* hit = store->entries.Find(property.value)) {
        return hit;
    }
    if (property.HasGlobalIndex()) {
        return nullptr;
    }
    const MemberId canonical = CanonicalPropertyKey(property);
    if (canonical == property.value) {
        return nullptr;
    }
    return store->entries.Find(canonical);
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
    Meta::DependencyPropertyRegistry* const registry = PropertyRegistryPtr();
    const Meta::DependencyProperty* property = registry->Find(propertyHandle);
    const PropertyMetadata* metadata = property != nullptr
        ? property->MetadataFor(DependencyObject::RuntimeType()) : nullptr;
    if (property == nullptr || metadata == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::NotFound,
            "Dependency property does not apply to this object type");
    }
    return EnsureStoredEntryDirect(property->Handle(), *metadata);
}

Base::Result<StoredValueEntry*> DependencyObject::EnsureStoredEntryDirect(
    DependencyPropertyHandle canonicalHandle,
    const PropertyMetadata& metadata) noexcept {
    if (valueStore_ == nullptr) {
        // P2.4: the store block comes from the owning Dispatcher's slab.
        // Release needs no context: delete routes via the block header.
        void* memory = GetDispatcher().GetPropertySlab().AllocateStore(
            sizeof(PropertyStore));
        if (memory == nullptr) {
            return Base::Status::Failure(Base::ErrorCode::OutOfMemory,
                "DependencyObject value store allocation failed");
        }
        valueStore_ = new (memory) PropertyStore();
    }
    PropertyStore* store = static_cast<PropertyStore*>(valueStore_);
    StoredValueEntry entry;
    entry.effectiveValue = metadata.defaultValue;
    auto inserted =
        store->entries.Insert(canonicalHandle.value, std::move(entry));
    return &inserted.entry->Value();
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
        if (store->entries.Empty()) {
            delete store;
            valueStore_ = nullptr;
        }
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
    if (typeVerified_) {
        return {};
    }
    if (!objectServicesAvailable_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "DependencyObject was created without an ObjectFactoryScope");
    }
    Meta::DependencyPropertyRegistry* const registry = PropertyRegistryPtr();
    if (registry == nullptr || !registry->IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Dependency property registry is not frozen");
    }
    const TypeId runtimeType = DependencyObject::RuntimeType();
    if (runtimeType == InvalidTypeId ||
        registry->Types().FindType(runtimeType) == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "DependencyObject runtime type is not registered");
    }
    typeVerified_ = 1U;
    return {};
}


DependencyObjectRare* DependencyObject::EnsureRare() noexcept {
    if (rare_ == nullptr) {
        rare_ = new (std::nothrow) DependencyObjectRare();
    }
    return rare_;
}

void DependencyObject::CleanupRareIfEmpty() noexcept {
    if (rare_ != nullptr && rare_->IsEmpty()) {
        delete rare_;
        rare_ = nullptr;
    }
}

DependencyObject* DependencyObject::ExplicitInheritanceParent() const noexcept {
    return (hasExplicitInheritanceParent_ && rare_ != nullptr)
        ? rare_->inheritanceParent
        : nullptr;
}

void DependencyObject::SetExplicitInheritanceParent(DependencyObject* parent) noexcept {
    if (parent == nullptr) {
        hasExplicitInheritanceParent_ = 0U;
        if (rare_ != nullptr) {
            rare_->inheritanceParent = nullptr;
            CleanupRareIfEmpty();
        }
        return;
    }
    if (EnsureRare() == nullptr) {
        return;
    }
    rare_->inheritanceParent = parent;
    hasExplicitInheritanceParent_ = 1U;
}

DependencyObject* DependencyObject::DistinctLogicalParentFromRare() const noexcept {
    return rare_ != nullptr ? rare_->distinctLogicalParent : nullptr;
}

void DependencyObject::SetDistinctLogicalParentInRare(DependencyObject* parent) noexcept {
    if (parent == nullptr) {
        if (rare_ != nullptr) {
            rare_->distinctLogicalParent = nullptr;
            if (rare_->IsEmpty()) {
                delete rare_;
                rare_ = nullptr;
            }
        }
        return;
    }
    if (rare_ == nullptr) {
        rare_ = new (std::nothrow) DependencyObjectRare();
        if (rare_ == nullptr) {
            return;
        }
    }
    rare_->distinctLogicalParent = parent;
}

} // namespace Aero
