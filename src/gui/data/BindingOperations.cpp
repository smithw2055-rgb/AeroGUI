#include "gui/data/BindingCommon.hpp"

#include "gui/meta/TypeRegistryDetail.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/internal/AeroGuiInternal.hpp"
#include "gui/data/BindingEngine.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleEngine.hpp"
#include <Aero/Data/Binding.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/UIElement.hpp>
#include <Aero/LogicalTreeHelper.hpp>
#include <Aero/TryCast.hpp>
#include <Aero/Visual.hpp>
#include <Aero/VisualTreeHelper.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Media/Geometry.hpp>
#include <Aero/Media/SolidColorBrush.hpp>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <utility>

namespace Aero {

using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero::Data;





Base::Result<std::uint32_t> BindingEngine::Flush() noexcept {
    if (!dispatcher_->CheckAccess()) {
        return dispatcher_->VerifyAccess().GetStatus();
    }
    if (!initialized_) {
        return InvalidState("BindingEngine is not initialized");
    }
    if (flushing_) {
        return std::uint32_t{0U};
    }

    flushing_ = true;
    lastError_ = {};
    std::uint32_t updated = 0U;

    for (std::uint32_t iteration = 0U; iteration < 4U; ++iteration) {
        const std::uint32_t currentCount = bindings_.Size();
        std::uint32_t iterationUpdated = 0U;
        // Equip slots author DataContext="{Binding Player.Slots[i]}" plus
        // Content="{Binding}". Apply DataContext writes first so empty-path
        // Content bindings in the same Flush see the slot object.
        for (std::uint32_t pass = 0U; pass < 2U; ++pass) {
            for (std::uint32_t index = 0U; index < currentCount; ++index) {
                if (index >= bindings_.Size() || bindings_[index].handle.value == 0U) {
                    continue;
                }
                const bool writesDataContext =
                    bindings_[index].dataContextProperty.IsValid() &&
                    bindings_[index].descriptor.targetProperty == bindings_[index].dataContextProperty;
                if (pass == 0U && !writesDataContext) {
                    continue;
                }
                if (pass == 1U && writesDataContext) {
                    continue;
                }
                if (bindings_[index].descriptor.mode == BindingMode::OneTime && bindings_[index].applied) {
                    continue;
                }
                const bool metadataPath =
                    bindings_[index].sourceKind != BindingSourceKind::DependencyProperty;
                const bool hasNotify =
                    bindings_[index].notificationSubscription != 0U ||
                    bindings_[index].sourceDependencyProperty.IsValid();
                const bool unresolvedDataContext =
                    bindings_[index].sourceKind == BindingSourceKind::DataContext &&
                    (!bindings_[index].applied || bindings_[index].metadataSource == nullptr ||
                     (!bindings_[index].bindsToSource && !bindings_[index].pathPlan.IsValid()));
                const bool pollMetadata =
                    (metadataPath && !hasNotify) || unresolvedDataContext;
                if (bindings_[index].applied && !bindings_[index].sourceDirty &&
                    !bindings_[index].targetDirty && !pollMetadata) {
                    continue;
                }
                if (pollMetadata && !bindings_[index].pollHintEmitted) {
                    bindings_[index].pollHintEmitted = true;
                    ReportDiagnostic(
                        bindings_[index],
                        BindingDiagnosticStage::ResolveSource,
                        Base::Status::Failure(
                            Base::ErrorCode::Unsupported,
                            "Binding source does not implement NotifyPropertyChanged; "
                            "polling metadata path every frame"));
                }

                Base::Result<PropertyValue> source = ReadSource(bindings_[index]);
                bool usedFallback = false;
                if (!source) {
                    const BindingDiagnosticStage stage =
                        bindings_[index].sourceKind == BindingSourceKind::DataContext &&
                        source.GetStatus().code == Base::ErrorCode::NotFound
                            ? BindingDiagnosticStage::ResolveSource
                            : BindingDiagnosticStage::ReadSource;
                    ReportDiagnostic(bindings_[index], stage, source.GetStatus());
                    if (bindings_[index].descriptor.fallbackValue.IsUnset()) {
                        bindings_[index].applied = false;
                        bindings_[index].sourceDirty = true;
                        continue;
                    }
                    source = bindings_[index].descriptor.fallbackValue;
                    usedFallback = true;
                }
                Base::Result<PropertyValue> target =
                    bindings_[index].descriptor.target->GetValue(bindings_[index].descriptor.targetProperty);
                if (!target) {
                    ReportDiagnostic(
                        bindings_[index],
                        BindingDiagnosticStage::WriteTarget,
                        target.GetStatus());
                    continue;
                }

                const bool sourceChanged = !bindings_[index].applied ||
                    bindings_[index].sourceDirty ||
                    usedFallback ||
                    (pollMetadata &&
                     source.Value() != bindings_[index].lastSourceValue);
                const bool targetChanged =
                    bindings_[index].descriptor.updateSourceTrigger ==
                            UpdateSourceTrigger::Explicit ||
                        bindings_[index].descriptor.updateSourceTrigger ==
                            UpdateSourceTrigger::LostFocus
                    ? bindings_[index].forceSourceUpdate
                    : (!bindings_[index].applied || bindings_[index].targetDirty);
                if (!sourceChanged && !targetChanged) {
                    bindings_[index].sourceDirty = false;
                    if (bindings_[index].descriptor.updateSourceTrigger !=
                            UpdateSourceTrigger::LostFocus &&
                        bindings_[index].descriptor.updateSourceTrigger !=
                            UpdateSourceTrigger::Explicit) {
                        bindings_[index].targetDirty = false;
                    }
                    continue;
                }
                Base::Result<void> applied =
                    Base::Status::Failure(
                        Base::ErrorCode::InvalidState,
                        "Binding update was not attempted");
                switch (bindings_[index].descriptor.mode) {
                case BindingMode::OneTime:
                    if (!bindings_[index].applied) {
                        applied = ApplySourceToTarget(
                            bindings_[index],
                            source.Value(),
                            usedFallback,
                            target.Value());
                        if (applied) ++iterationUpdated;
                    }
                    break;
                case BindingMode::Default:
                case BindingMode::OneWay:
                    if (sourceChanged) {
                        applied = ApplySourceToTarget(
                            bindings_[index],
                            source.Value(),
                            usedFallback,
                            target.Value());
                        if (applied) ++iterationUpdated;
                    }
                    break;
                case BindingMode::OneWayToSource:
                    if (targetChanged) {
                        applied = ApplyTargetToSource(
                            bindings_[index],
                            target.Value(),
                            source.Value());
                        if (applied) ++iterationUpdated;
                    }
                    break;
                case BindingMode::TwoWay:
                    // A target edit (ToggleButton click) must write back even when
                    // metadata-path polling reports a unchanged source as "changed".
                    // Source still wins when the source property itself is dirty.
                    if (targetChanged && !bindings_[index].sourceDirty) {
                        applied = ApplyTargetToSource(
                            bindings_[index],
                            target.Value(),
                            source.Value());
                        if (applied) ++iterationUpdated;
                    } else if (sourceChanged) {
                        applied = ApplySourceToTarget(
                            bindings_[index],
                            source.Value(),
                            usedFallback,
                            target.Value());
                        if (applied) ++iterationUpdated;
                    } else if (targetChanged) {
                        applied = ApplyTargetToSource(
                            bindings_[index],
                            target.Value(),
                            source.Value());
                        if (applied) ++iterationUpdated;
                    }
                    break;
                }
                if (index >= bindings_.Size() || bindings_[index].handle.value == 0U) {
                    continue;
                }
                if (!applied && (sourceChanged || targetChanged)) {
                    bindings_[index].sourceDirty = true;
                    continue;
                }
                bindings_[index].lastSourceValue = source.Value();
                bindings_[index].lastTargetValue = target.Value();
                bindings_[index].applied = true;
                bindings_[index].lastStatus = {};
                bindings_[index].sourceDirty = false;
                bindings_[index].targetDirty = false;
                bindings_[index].forceSourceUpdate = false;
            }
        }
        updated += iterationUpdated;
        if (iterationUpdated == 0U && bindings_.Size() == currentCount) {
            break;
        }
    }
    flushing_ = false;
    if (hasPendingDetaches_) {
        hasPendingDetaches_ = false;
        std::uint32_t writeIndex = 0U;
        for (std::uint32_t readIndex = 0U; readIndex < bindings_.Size(); ++readIndex) {
            if (bindings_[readIndex].handle.value != 0U) {
                if (writeIndex != readIndex) {
                    bindings_[writeIndex] = std::move(bindings_[readIndex]);
                    static_cast<void>(handleIndexMap_.Set(bindings_[writeIndex].handle.value, writeIndex));
                }
                ++writeIndex;
            }
        }
        bindings_.Resize(writeIndex);
    }
    return updated;
}

Base::Result<std::uint32_t>
BindingEngine::InspectBindings(
    const DependencyObject& object,
    Base::Vector<BindingInspection>&
        output) const noexcept {
    output.Clear();
    for (const BindingRecord& record :
        bindings_) {
        Base::Object* source =
            record.sourceKind ==
                BindingSourceKind::
                    DependencyProperty
            ? static_cast<Base::Object*>(
                record.descriptor.source)
            : record.metadataSource;
        if (source != &object &&
            record.descriptor.target !=
                &object) {
            continue;
        }
        BindingInspection inspection;
        inspection.handle = record.handle;
        inspection.source = source;
        inspection.target =
            record.descriptor.target;
        inspection.sourceProperty =
            record.descriptor.
                sourceProperty;
        inspection.targetProperty =
            record.descriptor.
                targetProperty;
        inspection.mode =
            record.descriptor.mode;
        inspection.updateSourceTrigger =
            record.descriptor.
                updateSourceTrigger;
        inspection.usesDataContext =
            record.sourceKind ==
                BindingSourceKind::
                    DataContext;
        inspection.applied =
            record.applied;
        Base::Result<void> assigned =
            inspection.path.Assign(
                record.path.View());
        if (!assigned) {
            output.Clear();
            return assigned.GetStatus();
        }
        output.PushBack(
                std::move(inspection));
    }
    return output.Size();
}

void BindingEngine::DataBindHook(void* context) noexcept {
    BindingEngine* manager = static_cast<BindingEngine*>(context);
    if (manager == nullptr) return;
    constexpr std::uint32_t MaximumActivationWaves = 16U;
    for (std::uint32_t wave = 0U;
         wave < MaximumActivationWaves; ++wave) {
        // Apply already-attached bindings even when a deferred template
        // Binding fails to activate. Sample trees (Inventory, QuestLog)
        // attach ItemsSource="{Binding ...}" on the live tree, then expand
        // ControlTemplates that can reject individual deferred Bindings.
        Base::Result<std::uint32_t> flushed = manager->Flush();
        if (!flushed) {
            manager->RecordError(flushed.GetStatus());
        }
        Base::Result<std::uint32_t> activated =
            manager->ActivatePendingDeferred();
        if (!activated) {
            manager->RecordError(activated.GetStatus());
        }
        flushed = manager->Flush();
        if (!flushed) {
            manager->RecordError(flushed.GetStatus());
            return;
        }
        if (manager->pendingDeferredActivations_.Empty()) return;
    }
    manager->RecordError(Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "Deferred Binding activation exceeded the bounded DataBind waves"));
}

void BindingEngine::OnPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs& args) noexcept {
    bool lostFocusFlush = false;
    for (BindingRecord& record : bindings_) {
        if (record.sourceKind == BindingSourceKind::DependencyProperty &&
            record.descriptor.source == &object &&
            record.descriptor.sourceProperty == args.GetProperty()) {
            record.sourceDirty = true;
        }
        if (record.sourceDependencyProperty.IsValid() &&
            record.metadataSource == &object &&
            record.sourceDependencyProperty == args.GetProperty()) {
            record.sourceDirty = true;
        }
        if (record.sourceKind == BindingSourceKind::DataContext &&
            record.dataContextProperty == args.GetProperty() &&
            BindingOwnerSeesDataContextChange(
                record.dataContextOwner, object)) {
            ReleaseMetadataSource(record);
            record.metadataSource = nullptr;
            record.pathPlan = {};
            record.sourceDirty = true;
            record.applied = false;
        }
        if (record.descriptor.target == &object &&
            record.descriptor.targetProperty == args.GetProperty()) {
            record.targetDirty = true;
        }
        if (record.lostFocusSubscribed &&
            record.descriptor.target == &object &&
            args.GetProperty() ==
                UIElement::IsKeyboardFocusedProperty.Handle() &&
            record.descriptor.updateSourceTrigger ==
                UpdateSourceTrigger::LostFocus) {
            const PropertyValue& next = args.GetNewValue();
            const bool focused =
                next.Kind() == ValueKind::Boolean && next.AsBoolean();
            if (!focused) {
                record.forceSourceUpdate = true;
                record.targetDirty = true;
                lostFocusFlush = true;
            }
        }
    }
    if (lostFocusFlush && !flushing_ && initialized_) {
        static_cast<void>(Flush());
    }
}

void BindingEngine::OnMetadataPropertyChanged(
    Base::Object& object,
    MemberId property) noexcept {
    for (BindingRecord& record : bindings_) {
        if (record.sourceKind ==
                BindingSourceKind::DependencyProperty ||
            record.metadataSource != &object ||
            !record.pathPlan.IsValid() ||
            record.pathPlan.Segments().Empty()) {
            continue;
        }
        const BindingPathSegment& first =
            record.pathPlan.Segments()[0];
        if (property == InvalidMemberId ||
            first.member == property ||
            first.dynamic) {
            record.sourceDirty = true;
        }
    }
}

void BindingEngine::MetadataPropertyChanged(
    Base::Object& object,
    MemberId property,
    void* context) noexcept {
    BindingEngine* manager = static_cast<BindingEngine*>(context);
    if (manager != nullptr) {
        manager->OnMetadataPropertyChanged(object, property);
    }
}

Base::Result<void> BindingEngine::VerifyDescriptor(
    const BindingDescriptor& descriptor) const noexcept {
    if (descriptor.source == nullptr || descriptor.target == nullptr ||
        !descriptor.sourceProperty.IsValid() ||
        !descriptor.targetProperty.IsValid()) {
        return InvalidArgument("Binding descriptor is incomplete");
    }
    if (&descriptor.source->GetDispatcher() != dispatcher_ ||
        &descriptor.target->GetDispatcher() != dispatcher_) {
        return InvalidArgument("Binding source and target must use this Dispatcher");
    }
    Base::Result<PropertyValue> source =
        descriptor.source->GetValue(descriptor.sourceProperty);
    if (!source) {
        return source.GetStatus();
    }
    Base::Result<PropertyValue> target =
        descriptor.target->GetValue(descriptor.targetProperty);
    if (!target) {
        return target.GetStatus();
    }
    if (source.Value().Type() != target.Value().Type() &&
        descriptor.convert == nullptr &&
        !descriptor.converterResource &&
        !HasDefaultTargetConversion(
            source.Value().Type(), target.Value().Type())) {
        return InvalidArgument("Binding source and target property types differ");
    }
    if ((descriptor.mode == BindingMode::TwoWay ||
         descriptor.mode == BindingMode::OneWayToSource) &&
        source.Value().Type() != target.Value().Type() &&
        descriptor.convertBack == nullptr &&
        !descriptor.converterResource &&
        !HasDefaultTargetConversion(
            target.Value().Type(),
            source.Value().Type())) {
        return InvalidArgument(
            "Binding requires ConvertBack for different source and target types");
    }
    if (!descriptor.fallbackValue.IsUnset() &&
        descriptor.fallbackValue.Type() != target.Value().Type()) {
        return InvalidArgument(
            "Binding fallback value type differs from the target property");
    }
    if (!descriptor.targetNullValue.IsUnset() &&
        descriptor.targetNullValue.Type() != target.Value().Type()) {
        return InvalidArgument(
            "Binding target-null value type differs from the target property");
    }
    return {};
}

Base::Result<void> BindingEngine::VerifyDescriptor(
    const MetadataBindingDescriptor& descriptor) const noexcept {
    if (descriptor.metadata == nullptr ||
        !descriptor.metadata->IsReady() ||
        descriptor.target == nullptr ||
        !descriptor.targetProperty.IsValid() ||
        (descriptor.path.Empty() && !descriptor.bindsToSource) ||
        (descriptor.source == nullptr &&
         !descriptor.dataContextProperty.IsValid())) {
        return InvalidArgument("Metadata binding descriptor is incomplete");
    }
    if (&descriptor.target->GetDispatcher() != dispatcher_) {
        return InvalidArgument(
            "Binding target must use this Dispatcher");
    }
    Base::Result<PropertyValue> target =
        descriptor.target->GetValue(descriptor.targetProperty);
    if (!target) return target.GetStatus();
    if (!descriptor.fallbackValue.IsUnset() &&
        descriptor.fallbackValue.Type() != target.Value().Type()) {
        return InvalidArgument(
            "Binding fallback value type differs from the target property");
    }
    if (!descriptor.targetNullValue.IsUnset() &&
        descriptor.targetNullValue.Type() != target.Value().Type()) {
        return InvalidArgument(
            "Binding target-null value type differs from the target property");
    }
    if (descriptor.source != nullptr &&
        (descriptor.source->RuntimeType() == InvalidTypeId ||
        descriptor.metadata->Types().FindType(
            descriptor.source->RuntimeType()) == nullptr)) {
        return InvalidArgument(
            "Binding metadata source has no registered runtime type");
    }
    return {};
}

Base::Result<void> BindingEngine::ResolveMetadataSource(
    BindingRecord& record) noexcept {
    if (record.sourceKind == BindingSourceKind::MetadataObject) {
        return record.metadataSource != nullptr
            ? Base::Result<void>()
            : Base::Result<void>(InvalidState(
                "Binding source object is not resolved"));
    }
    if (record.sourceKind == BindingSourceKind::MetadataPath) {
        return record.metadataSource != nullptr &&
            record.pathPlan.IsValid()
            ? Base::Result<void>()
            : Base::Result<void>(InvalidState(
                "Binding metadata source is not resolved"));
    }
    if (record.dataContextOwner == nullptr) {
        ReleaseMetadataSource(record);
        record.metadataSource = nullptr;
        record.pathPlan = {};
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Binding target has no DataContext object");
    }
    Base::Result<PropertyValue> dataContext = ReadDataContextValue(
        *record.dataContextOwner,
        record.dataContextProperty);
    if (!dataContext ||
        dataContext.Value().Kind() != ValueKind::Object ||
        dataContext.Value().IsNullObject()) {
        DependencyObject* node = record.dataContextOwner;
        for (std::uint32_t depth = 0U; depth < 64U && node != nullptr; ++depth) {
            node = BindingParent(*node);
            if (node == nullptr) {
                continue;
            }
            Base::Result<PropertyValue> ancestor = ReadDataContextValue(
                *node,
                record.dataContextProperty);
            if (!ancestor) {
                continue;
            }
            if (ancestor.Value().Kind() == ValueKind::Object &&
                !ancestor.Value().IsNullObject()) {
                dataContext = std::move(ancestor);
                break;
            }
        }
    }
    if (!dataContext ||
        dataContext.Value().Kind() != ValueKind::Object ||
        dataContext.Value().IsNullObject()) {
        ReleaseMetadataSource(record);
        record.metadataSource = nullptr;
        record.pathPlan = {};
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Binding target has no DataContext object");
    }

    Base::Object* source = dataContext.Value().AsObject().Get();
    if (source == record.metadataSource &&
        (record.bindsToSource || record.pathPlan.IsValid())) {
        return {};
    }
    ReleaseMetadataSource(record);
    record.metadataSource = nullptr;
    record.pathPlan = {};

    if (record.bindsToSource) {
        const DependencyProperty* targetProperty =
            AeroGuiInternal::PropertyRegistry(record.descriptor.target).Find(
                record.descriptor.targetProperty);
        if (targetProperty == nullptr ||
            (!targetProperty->AcceptsAnyValue() &&
            (record.descriptor.convert == nullptr &&
             !record.descriptor.converterResource &&
             !record.metadata->Types().IsAssignableFrom(
                 targetProperty->ValueType(),
                 source->RuntimeType())))) {
            return BindingTypeMismatch(
                record.metadata->Types(),
                Base::StringView("."),
                source->RuntimeType(),
                *record.descriptor.target,
                targetProperty);
        }
        if (record.descriptor.mode == BindingMode::TwoWay ||
            record.descriptor.mode == BindingMode::OneWayToSource) {
            return Base::Status::Failure(
                Base::ErrorCode::ReadOnly,
                "Binding to the DataContext object does not support writeback");
        }
        record.metadataSource = source;
        record.sourceDirty = true;
        record.applied = false;
        return {};
    }

    BindingPathCompileError compileError;
    Base::Result<BindingPathPlan> compiled =
        BindingPathPlan::Compile(
            *record.metadata,
            source->RuntimeType(),
            record.path.View(),
            &compileError);
        if (!compiled) {
            return BindingPathFailure(
                record.metadata->Types(),
                record.path.View(),
                compileError,
                compiled.GetStatus());
        }
    const DependencyProperty* targetProperty =
        AeroGuiInternal::PropertyRegistry(record.descriptor.target).Find(
            record.descriptor.targetProperty);
    if (targetProperty == nullptr ||
        (record.descriptor.convert == nullptr &&
         !record.descriptor.converterResource &&
         !TargetAcceptsPathResult(
             record.metadata->Types(),
             targetProperty,
             compiled.Value().ResultType(),
             compiled.Value().HasDynamicResult()))) {
        return BindingTypeMismatch(
            record.metadata->Types(),
            record.path.View(),
            compiled.Value().ResultType(),
            *record.descriptor.target,
            targetProperty);
    }
    if ((record.descriptor.mode == BindingMode::TwoWay ||
         record.descriptor.mode == BindingMode::OneWayToSource) &&
        !compiled.Value().CanWrite()) {
        return Base::Status::Failure(
            Base::ErrorCode::ReadOnly,
            "Binding source path is not writable");
    }
    if ((record.descriptor.mode == BindingMode::TwoWay ||
         record.descriptor.mode == BindingMode::OneWayToSource) &&
        !compiled.Value().HasDynamicResult() &&
        targetProperty->ValueType() != compiled.Value().ResultType() &&
        record.descriptor.convertBack == nullptr &&
        !record.descriptor.converterResource &&
        !HasDefaultTargetConversion(
            targetProperty->ValueType(),
            compiled.Value().ResultType()) &&
        !CanRoundTripObjectValue(
            record.metadata->Types(),
            compiled.Value().ResultType(),
            targetProperty->ValueType())) {
        return InvalidArgument(
            "Binding requires ConvertBack for different source and target types");
    }
    record.metadataSource = source;
    record.pathPlan = std::move(compiled).Value();
    Base::Result<void> subscribed =
        SubscribeMetadataSource(record);
    if (!subscribed) {
        record.metadataSource = nullptr;
        record.pathPlan = {};
        return subscribed.GetStatus();
    }
    record.sourceDirty = true;
    record.applied = false;
    return {};
}









void BindingEngine::ReportDiagnostic(
    BindingRecord& record,
    BindingDiagnosticStage stage,
    Base::Status status) noexcept {
    lastError_ = status;
    record.lastStatus = status;
    record.conversionFailureStage = stage;
    if (record.descriptor.diagnostic != nullptr) {
        record.descriptor.diagnostic(
            {record.handle, stage, status},
            record.descriptor.diagnosticContext);
    }
}

Base::Result<void> BindingEngine::SubscribeMetadataSource(
    BindingRecord& record) noexcept {
    if (record.metadata == nullptr ||
        record.metadataSource == nullptr) {
        return InvalidArgument(
            "Binding metadata source subscription is incomplete");
    }
    Base::Result<std::uint64_t> subscribed =
        record.metadata->SubscribePropertyChanged(
        *record.metadataSource,
        &BindingEngine::MetadataPropertyChanged,
        this);
    if (!subscribed) return subscribed.GetStatus();
    record.notificationSubscription = subscribed.Value();
    record.sourceDependencyProperty = {};
    if (!record.bindsToSource &&
        !record.pathPlan.Segments().Empty()) {
        if (auto* sourceObject =
                ::Aero::TryCast<::Aero::DependencyObject>(
                    record.metadataSource)) {
            const BindingPathSegment& first =
                record.pathPlan.Segments()[0];
            if (!first.dynamic && first.member != InvalidMemberId) {
                DependencyPropertyHandle handle{first.member};
                if (AeroGuiInternal::PropertyRegistry(sourceObject).Find(handle) !=
                    nullptr) {
                    sourceObject->AddValueChangedHandler(
                        handle, propertyChangedHandler_);
                    record.sourceDependencyProperty = handle;
                }
            }
        }
    }
    return {};
}

void BindingEngine::ReleaseMetadataSource(
    BindingRecord& record) noexcept {
    if (record.sourceDependencyProperty.IsValid() &&
        record.metadataSource != nullptr) {
        if (auto* sourceObject =
                ::Aero::TryCast<::Aero::DependencyObject>(
                    record.metadataSource)) {
            (void)sourceObject->RemoveValueChangedHandler(
                record.sourceDependencyProperty,
                propertyChangedHandler_);
        }
    }
    record.sourceDependencyProperty = {};
    if (record.notificationSubscription != 0U &&
        record.metadata != nullptr &&
        record.metadataSource != nullptr) {
        (void)record.metadata->UnsubscribePropertyChanged(
            *record.metadataSource,
            record.notificationSubscription);
    }
    record.notificationSubscription = 0U;
}

void BindingEngine::CleanupRecord(BindingRecord& record) noexcept {
    if (record.sourceKind == BindingSourceKind::DependencyProperty) {
        if (record.descriptor.source != nullptr && record.descriptor.sourceProperty.IsValid()) {
            (void)record.descriptor.source->RemoveValueChangedHandler(
                record.descriptor.sourceProperty,
                propertyChangedHandler_);
        }
    } else {
        ReleaseMetadataSource(record);
        if (record.sourceKind == BindingSourceKind::DataContext &&
            record.dataContextOwner != nullptr && record.dataContextProperty.IsValid()) {
            (void)record.dataContextOwner->RemoveValueChangedHandler(
                record.dataContextProperty,
                propertyChangedHandler_);
        }
    }
    if (record.descriptor.target != nullptr && record.descriptor.targetProperty.IsValid()) {
        (void)record.descriptor.target->RemoveValueChangedHandler(
            record.descriptor.targetProperty, propertyChangedHandler_);
    }
    UnsubscribeLostFocus(record);
}

void BindingEngine::RemoveAt(std::uint32_t index) noexcept {
    if (index >= bindings_.Size()) return;
    handleIndexMap_.Erase(bindings_[index].handle.value);
    CleanupRecord(bindings_[index]);
    for (std::uint32_t current = index + 1U;
         current < bindings_.Size();
         ++current) {
        bindings_[current - 1U] = std::move(bindings_[current]);
        static_cast<void>(handleIndexMap_.Set(bindings_[current - 1U].handle.value, current - 1U));
    }
    bindings_.PopBack();
}

} // namespace Aero
